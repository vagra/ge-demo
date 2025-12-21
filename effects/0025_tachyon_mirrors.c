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
 */

#include "demo_engine.h"
#include "mpp_mem.h"
#include "aic_hal_ge.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

/*
 * Hardware Feature:
 * 1. GE Rot1 (多相位独立旋转)
 * 2. GE Scaler (独立象限缩放)
 * 3. GE FillRect (三重真空清理：主屏幕 + 双路中间层)
 * 覆盖机能清单：移除冗余的混合规则，优化指令流，确保 4 象限镜像的极致清晰与高帧率。
 */

#define TEX_W    320
#define TEX_H    240
#define TEX_SIZE (TEX_W * TEX_H * 2)

static unsigned int g_tex_phy_addr    = 0;
static unsigned int g_rot_phy_addr[2] = {0, 0}; // 两个不同旋转相位的中间层
static uint16_t    *g_tex_vir_addr    = NULL;

static int      g_tick = 0;
static int      sin_lut[512];
static uint16_t palette[256];

static int effect_init(struct demo_ctx *ctx)
{
    // 1. 申请连续物理显存
    g_tex_phy_addr    = mpp_phy_alloc(TEX_SIZE);
    g_rot_phy_addr[0] = mpp_phy_alloc(TEX_SIZE);
    g_rot_phy_addr[1] = mpp_phy_alloc(TEX_SIZE);

    if (!g_tex_phy_addr || !g_rot_phy_addr[0] || !g_rot_phy_addr[1])
        return -1;

    g_tex_vir_addr = (uint16_t *)(unsigned long)g_tex_phy_addr;

    // 2. 初始化查找表
    for (int i = 0; i < 512; i++)
        sin_lut[i] = (int)(sinf(i * 3.14159f / 256.0f) * 4096.0f);

    // 3. 初始化极光调色板 (高频蓝绿调)
    for (int i = 0; i < 256; i++)
    {
        int r = (int)(20 + 20 * sinf(i * 0.05f));
        int g = (int)(100 + 80 * sinf(i * 0.02f + 1.0f));
        int b = (int)(150 + 100 * sinf(i * 0.04f + 3.0f));
        if ((i % 16) > 12)
        {
            r = 255;
            g = 255;
            b = 255;
        }
        palette[i] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
    }

    g_tick = 0;
    return 0;
}

#define GET_SIN(idx) (sin_lut[(idx) & 511])
#define GET_COS(idx) (sin_lut[((idx) + 128) & 511])

static void effect_draw(struct demo_ctx *ctx, unsigned long phy_addr)
{
    if (!g_tex_vir_addr)
        return;

    int t = g_tick;

    /* --- PHASE 1: CPU 纹理生成 --- */
    uint16_t *p = g_tex_vir_addr;
    for (int y = 0; y < TEX_H; y++)
    {
        int dy2 = (y - 120) * (y - 120);
        for (int x = 0; x < TEX_W; x++)
        {
            int dist = ((x - 160) * (x - 160) + dy2) >> 7;
            int val  = (dist ^ (x >> 2) ^ (y >> 2)) + t;
            *p++     = palette[val & 0xFF];
        }
    }
    aicos_dcache_clean_range((void *)g_tex_vir_addr, TEX_SIZE);

    /* --- PHASE 2: GE 准备两路“绝对净空”的旋转层 --- */
    for (int i = 0; i < 2; i++)
    {
        // 在旋转前强制清除中间缓冲区，确保无上一帧死角残留
        struct ge_fillrect clean_buf  = {0};
        clean_buf.type                = GE_NO_GRADIENT;
        clean_buf.start_color         = 0xFF000000;
        clean_buf.dst_buf.buf_type    = MPP_PHY_ADDR;
        clean_buf.dst_buf.phy_addr[0] = g_rot_phy_addr[i];
        clean_buf.dst_buf.stride[0]   = TEX_W * 2;
        clean_buf.dst_buf.size.width  = TEX_W;
        clean_buf.dst_buf.size.height = TEX_H;
        clean_buf.dst_buf.format      = MPP_FMT_RGB_565;
        mpp_ge_fillrect(ctx->ge, &clean_buf);
        mpp_ge_emit(ctx->ge);

        struct ge_rotation rot  = {0};
        rot.src_buf.buf_type    = MPP_PHY_ADDR;
        rot.src_buf.phy_addr[0] = g_tex_phy_addr;
        rot.src_buf.stride[0]   = TEX_W * 2;
        rot.src_buf.size.width  = TEX_W;
        rot.src_buf.size.height = TEX_H;
        rot.src_buf.format      = MPP_FMT_RGB_565;

        rot.dst_buf.buf_type    = MPP_PHY_ADDR;
        rot.dst_buf.phy_addr[0] = g_rot_phy_addr[i];
        rot.dst_buf.stride[0]   = TEX_W * 2;
        rot.dst_buf.size.width  = TEX_W;
        rot.dst_buf.size.height = TEX_H;
        rot.dst_buf.format      = MPP_FMT_RGB_565;

        // 异相位旋转：路0顺时针，路1逆时针
        int theta            = (i == 0) ? (t * 4) & 511 : (-t * 3) & 511;
        rot.angle_sin        = GET_SIN(theta);
        rot.angle_cos        = GET_COS(theta);
        rot.src_rot_center.x = 160;
        rot.src_rot_center.y = 120;
        rot.dst_rot_center.x = 160;
        rot.dst_rot_center.y = 120;
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

    /* --- PHASE 4: 四象限镜像投射 (移除重叠混合) --- */
    for (int i = 0; i < 4; i++)
    {
        struct ge_bitblt blt = {0};
        // 奇数窗口用相位0，偶数窗口用相位1
        blt.src_buf.buf_type    = MPP_PHY_ADDR;
        blt.src_buf.phy_addr[0] = g_rot_phy_addr[i % 2];
        blt.src_buf.stride[0]   = TEX_W * 2;
        blt.src_buf.size.width  = TEX_W;
        blt.src_buf.size.height = TEX_H;
        blt.src_buf.format      = MPP_FMT_RGB_565;

        blt.dst_buf.buf_type    = MPP_PHY_ADDR;
        blt.dst_buf.phy_addr[0] = phy_addr;
        blt.dst_buf.stride[0]   = ctx->info.stride;
        blt.dst_buf.size.width  = ctx->info.width;
        blt.dst_buf.size.height = ctx->info.height;
        blt.dst_buf.format      = ctx->info.format;

        // 象限布局：左上、右上、左下、右下
        // 缩放尺寸稍微调整，以填满对应的 1/4 屏幕
        blt.dst_buf.crop_en     = 1;
        blt.dst_buf.crop.width  = ctx->info.width / 2;
        blt.dst_buf.crop.height = ctx->info.height / 2;
        blt.dst_buf.crop.x      = (i % 2 == 0) ? 0 : (ctx->info.width / 2);
        blt.dst_buf.crop.y      = (i / 2 == 0) ? 0 : (ctx->info.height / 2);

        // 采样逻辑：引入镜像反转感
        blt.src_buf.crop_en     = 1;
        blt.src_buf.crop.width  = 200;
        blt.src_buf.crop.height = 150;
        blt.src_buf.crop.x      = (i % 2 == 0) ? 60 : 0;
        blt.src_buf.crop.y      = (i / 2 == 0) ? 45 : 0;

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
