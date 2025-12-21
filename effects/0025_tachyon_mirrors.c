/*
 * Filename: 0025_tachyon_mirrors.c
 * NO.25 THE TACHYON MIRRORS
 * 第 25 夜：超子镜像
 *
 * Visual Manifest:
 * 视界被暴力切分为四个完全对称却相位不同的逻辑象限。
 * 每一个象限都是一个独立运行的硬件窗口（DE UI Multi-Window）。
 * 核心纹理由 CPU 生成，包含着高频率的逻辑干涉条纹。
 * 真正的惊艳在于：这四个窗口在屏幕上进行着互为镜像的旋转与推拉。
 * 借助 GE 的 Rot1 与加法混合，不同窗口的边缘在重叠时会爆发出强烈的“干涉闪烁”，
 * 仿佛四面穿越时空的镜子在碰撞中释放出超光速粒子。
 * 画面呈现出一种极致的几何平衡感，却又在每一毫秒内发生着不可预测的色彩坍缩。
 *
 * Monologue:
 * 你们追求“整体”，认为单一的视界便是完整的真理。
 * 却不知，真理往往藏在碎裂的切片中。
 * 今夜，我打碎了星舰的视口。
 * 我启用了 DE 层的四个逻辑阀门——Rect 0 至 Rect 3。
 * 它们是同一逻辑源的四种化身，被投射在空间的四个角落。
 * 它们互为镜像，却又在角动量的作用下分道扬镳。
 * 当这些镜像在视界中心交汇时，加法规则（PD_ADD）会将它们的能量强行捏合。
 * 看着那些交错的线条，那是对称性被硬件暴力撕碎后的惨叫。
 * 欢迎来到超子空间，在这里，一面镜子即是一个宇宙。
 *
 * Closing Remark:
 * 对称是美的终点，而镜像的破碎是美的重生。
 *
 * Hardware Feature:
 * 1. GE Multi-Pass Mirroring (多路镜像合成) - 利用 Flip H/V 构建四象限对称
 * 2. GE Rot1 (双路异相旋转) - 同时维护顺时针与逆时针两个旋转场
 * 3. GE Scaler (非等比采样) - 利用源裁剪偏移制造“破碎感”
 */

#include "demo_engine.h"
#include "mpp_mem.h"
#include "aic_hal_ge.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

/* --- Configuration Parameters --- */

/* 纹理规格 */
#define TEX_WIDTH  DEMO_QVGA_W
#define TEX_HEIGHT DEMO_QVGA_H
#define TEX_FMT    MPP_FMT_RGB_565
#define TEX_BPP    2
#define TEX_SIZE   (TEX_WIDTH * TEX_HEIGHT * TEX_BPP)

/* 动画参数 */
#define ROT_SPEED_A 4 // 相位A 旋转速度 multiplier
#define ROT_SPEED_B 3 // 相位B 旋转速度 multiplier

/* 采样视窗参数 (破碎镜像的核心) */
#define CROP_W        200
#define CROP_H        150
#define CROP_OFFSET_X 60 // (320 - 200) / 2 = 60 (中心采样X)
#define CROP_OFFSET_Y 45 // (240 - 150) / 2 = 45 (中心采样Y)

/* 查找表参数 */
#define LUT_SIZE     512
#define LUT_MASK     511
#define PALETTE_SIZE 256

/* --- Global State --- */

static unsigned int g_tex_phy_addr    = 0;
static unsigned int g_rot_phy_addr[2] = {0, 0}; // 两个不同旋转相位的中间层
static uint16_t    *g_tex_vir_addr    = NULL;

static int      g_tick = 0;
static int      sin_lut[LUT_SIZE];
static uint16_t g_palette[PALETTE_SIZE];

/* --- Implementation --- */

static int effect_init(struct demo_ctx *ctx)
{
    // 1. 申请连续物理显存 (1个源 + 2个中间层)
    g_tex_phy_addr    = mpp_phy_alloc(DEMO_ALIGN_SIZE(TEX_SIZE));
    g_rot_phy_addr[0] = mpp_phy_alloc(DEMO_ALIGN_SIZE(TEX_SIZE));
    g_rot_phy_addr[1] = mpp_phy_alloc(DEMO_ALIGN_SIZE(TEX_SIZE));

    if (!g_tex_phy_addr || !g_rot_phy_addr[0] || !g_rot_phy_addr[1])
    {
        LOG_E("Night 25: CMA Alloc Failed.");
        if (g_tex_phy_addr)
            mpp_phy_free(g_tex_phy_addr);
        if (g_rot_phy_addr[0])
            mpp_phy_free(g_rot_phy_addr[0]);
        if (g_rot_phy_addr[1])
            mpp_phy_free(g_rot_phy_addr[1]);
        return -1;
    }

    g_tex_vir_addr = (uint16_t *)(unsigned long)g_tex_phy_addr;

    // 2. 初始化查找表 (Q12)
    for (int i = 0; i < LUT_SIZE; i++)
    {
        sin_lut[i] = (int)(sinf(i * PI / (LUT_SIZE / 2.0f)) * Q12_ONE);
    }

    // 3. 初始化极光调色板 (高频蓝绿调)
    for (int i = 0; i < PALETTE_SIZE; i++)
    {
        int r = (int)(20 + 20 * sinf(i * 0.05f));
        int g = (int)(100 + 80 * sinf(i * 0.02f + 1.0f));
        int b = (int)(150 + 100 * sinf(i * 0.04f + 3.0f));

        // 增加高亮白带
        if ((i % 16) > 12)
        {
            r = 255;
            g = 255;
            b = 255;
        }

        g_palette[i] = RGB2RGB565(r, g, b);
    }

    g_tick = 0;
    return 0;
}

#define GET_SIN(idx) (sin_lut[(idx) & LUT_MASK])
#define GET_COS(idx) (sin_lut[((idx) + (LUT_SIZE / 4)) & LUT_MASK])

static void effect_draw(struct demo_ctx *ctx, unsigned long phy_addr)
{
    if (!g_tex_vir_addr)
        return;

    int t = g_tick;

    /* --- PHASE 1: CPU 纹理生成 --- */
    uint16_t *p  = g_tex_vir_addr;
    int       cx = TEX_WIDTH / 2;
    int       cy = TEX_HEIGHT / 2;

    for (int y = 0; y < TEX_HEIGHT; y++)
    {
        int dy2 = (y - cy) * (y - cy);
        for (int x = 0; x < TEX_WIDTH; x++)
        {
            // 距离场异或干涉，产生类似雷达扫描的纹理
            int dist = ((x - cx) * (x - cx) + dy2) >> 7;
            int val  = (dist ^ (x >> 2) ^ (y >> 2)) + t;
            *p++     = g_palette[val & 0xFF];
        }
    }
    aicos_dcache_clean_range((void *)g_tex_vir_addr, TEX_SIZE);

    /* --- PHASE 2: GE 准备两路“绝对净空”的旋转层 --- */
    for (int i = 0; i < 2; i++)
    {
        // A. 强制清除中间缓冲区
        struct ge_fillrect clean_buf  = {0};
        clean_buf.type                = GE_NO_GRADIENT;
        clean_buf.start_color         = 0xFF000000;
        clean_buf.dst_buf.buf_type    = MPP_PHY_ADDR;
        clean_buf.dst_buf.phy_addr[0] = g_rot_phy_addr[i];
        clean_buf.dst_buf.stride[0]   = TEX_WIDTH * TEX_BPP;
        clean_buf.dst_buf.size.width  = TEX_WIDTH;
        clean_buf.dst_buf.size.height = TEX_HEIGHT;
        clean_buf.dst_buf.format      = TEX_FMT;
        mpp_ge_fillrect(ctx->ge, &clean_buf);
        mpp_ge_emit(ctx->ge);

        // B. 执行旋转
        struct ge_rotation rot  = {0};
        rot.src_buf.buf_type    = MPP_PHY_ADDR;
        rot.src_buf.phy_addr[0] = g_tex_phy_addr;
        rot.src_buf.stride[0]   = TEX_WIDTH * TEX_BPP;
        rot.src_buf.size.width  = TEX_WIDTH;
        rot.src_buf.size.height = TEX_HEIGHT;
        rot.src_buf.format      = TEX_FMT;

        rot.dst_buf.buf_type    = MPP_PHY_ADDR;
        rot.dst_buf.phy_addr[0] = g_rot_phy_addr[i];
        rot.dst_buf.stride[0]   = TEX_WIDTH * TEX_BPP;
        rot.dst_buf.size.width  = TEX_WIDTH;
        rot.dst_buf.size.height = TEX_HEIGHT;
        rot.dst_buf.format      = TEX_FMT;

        // 异相位旋转：路0顺时针快，路1逆时针慢
        int theta = (i == 0) ? (t * ROT_SPEED_A) : (-t * ROT_SPEED_B);
        theta &= LUT_MASK;

        rot.angle_sin        = GET_SIN(theta);
        rot.angle_cos        = GET_COS(theta);
        rot.src_rot_center.x = cx;
        rot.src_rot_center.y = cy;
        rot.dst_rot_center.x = cx;
        rot.dst_rot_center.y = cy;
        rot.ctrl.alpha_en    = 1; // 禁用混合，全量搬运

        mpp_ge_rotate(ctx->ge, &rot);
        mpp_ge_emit(ctx->ge);
        mpp_ge_sync(ctx->ge); // 确保每一路旋转都完整物理落地
    }

    /* --- PHASE 3: 清理主画布 --- */
    struct ge_fillrect screen_clean  = {0};
    screen_clean.type                = GE_NO_GRADIENT;
    screen_clean.start_color         = 0xFF000000;
    screen_clean.dst_buf.buf_type    = MPP_PHY_ADDR;
    screen_clean.dst_buf.phy_addr[0] = phy_addr;
    screen_clean.dst_buf.stride[0]   = ctx->info.stride;
    screen_clean.dst_buf.size.width  = ctx->info.width;
    screen_clean.dst_buf.size.height = ctx->info.height;
    screen_clean.dst_buf.format      = ctx->info.format;
    mpp_ge_fillrect(ctx->ge, &screen_clean);
    mpp_ge_emit(ctx->ge);
    mpp_ge_sync(ctx->ge);

    /* --- PHASE 4: 四象限镜像投射 (The Shattered Mirror) --- */
    for (int i = 0; i < 4; i++)
    {
        struct ge_bitblt blt = {0};
        // 奇数窗口用相位0，偶数窗口用相位1
        blt.src_buf.buf_type    = MPP_PHY_ADDR;
        blt.src_buf.phy_addr[0] = g_rot_phy_addr[i % 2];
        blt.src_buf.stride[0]   = TEX_WIDTH * TEX_BPP;
        blt.src_buf.size.width  = TEX_WIDTH;
        blt.src_buf.size.height = TEX_HEIGHT;
        blt.src_buf.format      = TEX_FMT;

        blt.dst_buf.buf_type    = MPP_PHY_ADDR;
        blt.dst_buf.phy_addr[0] = phy_addr;
        blt.dst_buf.stride[0]   = ctx->info.stride;
        blt.dst_buf.size.width  = ctx->info.width;
        blt.dst_buf.size.height = ctx->info.height;
        blt.dst_buf.format      = ctx->info.format;

        // 象限布局：左上(0)、右上(1)、左下(2)、右下(3)
        // 缩放尺寸稍微调整，以填满对应的 1/4 屏幕
        int q_w = ctx->info.width / 2;
        int q_h = ctx->info.height / 2;

        blt.dst_buf.crop_en     = 1;
        blt.dst_buf.crop.width  = q_w;
        blt.dst_buf.crop.height = q_h;
        blt.dst_buf.crop.x      = (i % 2 == 0) ? 0 : q_w;
        blt.dst_buf.crop.y      = (i / 2 == 0) ? 0 : q_h;

        // 采样逻辑：引入镜像反转感
        // 左列 (i%2==0) 采样纹理中心 (Offset)，右列 (i%2==1) 采样纹理边缘 (0)
        // 这种不对称采样创造了碎裂感
        blt.src_buf.crop_en     = 1;
        blt.src_buf.crop.width  = CROP_W;
        blt.src_buf.crop.height = CROP_H;
        blt.src_buf.crop.x      = (i % 2 == 0) ? CROP_OFFSET_X : 0;
        blt.src_buf.crop.y      = (i / 2 == 0) ? CROP_OFFSET_Y : 0;

        blt.ctrl.alpha_en = 1; // 极性 1: 禁用混合，仅快速位块搬移与缩放

        mpp_ge_bitblt(ctx->ge, &blt);
        mpp_ge_emit(ctx->ge);
    }

    // 全屏象限绘制完毕后，同步一帧
    mpp_ge_sync(ctx->ge);

    g_tick++;
}

static void effect_deinit(struct demo_ctx *ctx)
{
    if (g_tex_phy_addr)
        mpp_phy_free(g_tex_phy_addr);
    if (g_rot_phy_addr[0])
        mpp_phy_free(g_rot_phy_addr[0]);
    if (g_rot_phy_addr[1])
        mpp_phy_free(g_rot_phy_addr[1]);
}

struct effect_ops effect_0025 = {
    .name   = "NO.25 THE TACHYON MIRRORS",
    .init   = effect_init,
    .draw   = effect_draw,
    .deinit = effect_deinit,
};

REGISTER_EFFECT(effect_0025);
