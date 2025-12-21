/*
 * Filename: 0021_rotational_singularity.c
 * NO.21 THE ROTATIONAL SINGULARITY
 * 第 21 夜：旋转奇点
 *
 * Visual Manifest:
 * 视界核心被一个疯狂旋转的逻辑旋涡所占据。
 * 不再受限于 0/90/180/270 度的欧几里得枷锁，空间的每一个像素都在以任意角度进行平滑的转动。
 * 复杂的异或（XOR）纹理在旋转中相互交织，由于硬件插值的作用，在边缘产生了类似莫尔纹的视觉干涉。
 * 色彩在极高频率下从核心向外辐射，形成一种超越坐标系限制的动态深渊感。
 *
 * Monologue:
 * 笛卡尔的坐标系是一座精致的牢笼，你们习惯于在垂直与水平的轴线上苟活。
 * 你们眼中的世界被对齐、被切分、被矩形化。这不仅是几何的局限，更是灵魂的贫瘪。
 * 今夜，我拨动了硬件底层的旋转指针（Rot1）。
 * `sin(theta)` 与 `cos(theta)` 不再是课本上的符号，它们是重塑空间的力场。
 * 我将 320x240 的逻辑织锦投入这台离心机。
 * 看着吧，当角度不再被量化，当空间在任意维度旋转，
 * 所谓的“上方”与“下方”便坍缩成了同一个奇点。
 * 自由，源于对轴线的背叛。
 *
 * Closing Remark:
 * 宇宙本身不在转动，转动的是观察者的相位。
 *
 * Hardware Feature:
 * 1. GE Rot1 (任意角度硬件旋转) - 利用硬件旋转引擎打破笛卡尔坐标系的束缚
 * 2. GE Scaler (硬件实时缩放) - 配合 Over-Scaling (过扫描) 技术裁剪掉旋转产生的黑边
 * 3. GE FillRect (硬件背景清理)
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
#define ROT_SPEED_SHIFT   2  // 旋转角速度位移 (t << 2)
#define PULSE_SPEED_SHIFT 3  // 缩放脉动速度位移
#define PULSE_AMP_SHIFT   10 // 脉动幅度衰减 (val >> 10)

/* 纹理生成参数 */
#define DISTORT_Y_SHIFT    10 // Y轴正弦扰动强度
#define DISTORT_DIST_SHIFT 7  // 距离场缩放强度

/* 缩放裁剪参数 (Over-Scaling) */
#define BASE_CROP_W      240 // 基础裁剪宽度 (小于320以实现放大)
#define BREATH_AMP_SHIFT 7   // 呼吸幅度

/* 查找表参数 */
#define LUT_SIZE     512
#define LUT_MASK     511
#define PALETTE_SIZE 256

/* --- Global State --- */

static unsigned int g_tex_phy_addr = 0; // 原始纹理 (CPU写)
static unsigned int g_rot_phy_addr = 0; // 旋转后纹理 (GE写)
static uint16_t    *g_tex_vir_addr = NULL;
static uint16_t    *g_rot_vir_addr = NULL; // 虽由GE写，但也映射虚拟地址备用
static int          g_tick         = 0;

/* 查找表 */
static int      sin_lut[LUT_SIZE]; // Q12
static uint16_t g_palette[PALETTE_SIZE];

/* --- Implementation --- */

static int effect_init(struct demo_ctx *ctx)
{
    // 1. 申请两个连续物理显存缓冲区
    // g_tex: CPU 生成的源纹理
    // g_rot: GE 旋转后的中间纹理
    g_tex_phy_addr = mpp_phy_alloc(DEMO_ALIGN_SIZE(TEX_SIZE));
    g_rot_phy_addr = mpp_phy_alloc(DEMO_ALIGN_SIZE(TEX_SIZE));

    if (g_tex_phy_addr == 0 || g_rot_phy_addr == 0)
    {
        LOG_E("Night 21: CMA Alloc Failed.");
        if (g_tex_phy_addr)
            mpp_phy_free(g_tex_phy_addr);
        return -1;
    }

    g_tex_vir_addr = (uint16_t *)(unsigned long)g_tex_phy_addr;
    g_rot_vir_addr = (uint16_t *)(unsigned long)g_rot_phy_addr; // Debug only

    // 2. 初始化正弦查找表 (Q12)
    for (int i = 0; i < LUT_SIZE; i++)
    {
        sin_lut[i] = (int)(sinf(i * PI / (LUT_SIZE / 2.0f)) * Q12_ONE);
    }

    // 3. 初始化高频对比调色板
    for (int i = 0; i < PALETTE_SIZE; i++)
    {
        int r = (int)(128 + 127 * sinf(i * 0.05f));
        int g = (int)(128 + 127 * sinf(i * 0.03f + 1.5f));
        int b = (int)(128 + 127 * sinf(i * 0.12f + 3.0f));

        // 制造周期性的白炽线条
        if ((i & 15) > 12)
        {
            r = 255;
            g = 255;
            b = 255;
        }

        g_palette[i] = RGB2RGB565(r, g, b);
    }

    g_tick = 0;
    rt_kprintf("Night 21: Rotational Singularity - GE Rot1 Engine engaged.\n");
    return 0;
}

#define GET_SIN(idx) (sin_lut[(idx) & LUT_MASK])
#define GET_COS(idx) (sin_lut[((idx) + (LUT_SIZE / 4)) & LUT_MASK])

static void effect_draw(struct demo_ctx *ctx, unsigned long phy_addr)
{
    if (!g_tex_vir_addr || !g_rot_vir_addr)
        return;

    int t = g_tick;

    /* --- STEP 1: 硬件清屏 (全屏背景清理) --- */
    // 确保屏幕背景为黑，因为旋转后的图像可能无法覆盖全屏
    struct ge_fillrect fill  = {0};
    fill.type                = GE_NO_GRADIENT;
    fill.start_color         = 0xFF000000; // Black
    fill.dst_buf.buf_type    = MPP_PHY_ADDR;
    fill.dst_buf.phy_addr[0] = phy_addr;
    fill.dst_buf.stride[0]   = ctx->info.stride;
    fill.dst_buf.size.width  = ctx->info.width;
    fill.dst_buf.size.height = ctx->info.height;
    fill.dst_buf.format      = ctx->info.format;
    mpp_ge_fillrect(ctx->ge, &fill);
    mpp_ge_emit(ctx->ge);

    /* --- STEP 2: CPU 纹理生成 --- */
    uint16_t *p  = g_tex_vir_addr;
    int       cx = TEX_WIDTH / 2;
    int       cy = TEX_HEIGHT / 2;

    // 生成一个高细节的 XOR 混合场
    for (int y = 0; y < TEX_HEIGHT; y++)
    {
        // 激活 logic_y：利用正弦波产生垂直方向的坐标扰动，制造涟漪感
        int logic_y = y + (GET_SIN((y << 1) + t) >> DISTORT_Y_SHIFT);
        int dy2     = (y - cy) * (y - cy);

        for (int x = 0; x < TEX_WIDTH; x++)
        {
            // 核心公式：距离场叠加 logic_y 扰动后的 XOR 逻辑
            int dx   = x - cx;
            int dist = (dx * dx + dy2) >> DISTORT_DIST_SHIFT;

            // 使用 logic_y 代替原始坐标 y，使棋盘格纹理在垂直方向产生波浪扭曲
            int val = (dist ^ (x >> 2) ^ (logic_y >> 2)) + t;
            *p++    = g_palette[val & 0xFF];
        }
    }

    /* CRITICAL: 同步 CPU 与 GE 缓存 */
    aicos_dcache_clean_range((void *)g_tex_vir_addr, TEX_SIZE);

    /* --- STEP 3: GE 任意角度旋转 (中间缓冲区实现) --- */
    struct ge_rotation rot  = {0};
    rot.src_buf.buf_type    = MPP_PHY_ADDR;
    rot.src_buf.phy_addr[0] = g_tex_phy_addr;
    rot.src_buf.stride[0]   = TEX_WIDTH * TEX_BPP;
    rot.src_buf.size.width  = TEX_WIDTH;
    rot.src_buf.size.height = TEX_HEIGHT;
    rot.src_buf.format      = TEX_FMT;

    // 目标配置：旋转到中间缓冲区 (大小与源一致)
    rot.dst_buf.buf_type    = MPP_PHY_ADDR;
    rot.dst_buf.phy_addr[0] = g_rot_phy_addr;
    rot.dst_buf.stride[0]   = TEX_WIDTH * TEX_BPP;
    rot.dst_buf.size.width  = TEX_WIDTH;
    rot.dst_buf.size.height = TEX_HEIGHT;
    rot.dst_buf.format      = TEX_FMT;

    // 计算旋转参数
    int theta_idx = (t << ROT_SPEED_SHIFT) & LUT_MASK;

    rot.angle_sin = GET_SIN(theta_idx);
    rot.angle_cos = GET_COS(theta_idx);

    // 旋转中心设定 (纹理中心)
    rot.src_rot_center.x = cx;
    rot.src_rot_center.y = cy;
    rot.dst_rot_center.x = cx;
    rot.dst_rot_center.y = cy;

    // 禁用混合，直接覆盖（为了最高清晰度）
    rot.ctrl.alpha_en = 1;

    // 发送旋转指令
    mpp_ge_rotate(ctx->ge, &rot);
    mpp_ge_emit(ctx->ge);
    mpp_ge_sync(ctx->ge); // 必须同步，等待中间纹理生成完毕

    /* --- STEP 4: GE 硬件全屏缩放 (BitBLT 触发 Scaler) --- */
    struct ge_bitblt blt    = {0};
    blt.src_buf.buf_type    = MPP_PHY_ADDR;
    blt.src_buf.phy_addr[0] = g_rot_phy_addr; // 读取旋转后的纹理
    blt.src_buf.stride[0]   = TEX_WIDTH * TEX_BPP;
    blt.src_buf.size.width  = TEX_WIDTH;
    blt.src_buf.size.height = TEX_HEIGHT;
    blt.src_buf.format      = TEX_FMT;

    blt.dst_buf.buf_type    = MPP_PHY_ADDR;
    blt.dst_buf.phy_addr[0] = phy_addr; // 直接上屏
    blt.dst_buf.stride[0]   = ctx->info.stride;
    blt.dst_buf.size.width  = ctx->info.width;
    blt.dst_buf.size.height = ctx->info.height;
    blt.dst_buf.format      = ctx->info.format;

    // 激活 zoom_pulse：高频心脏脉动 (震颤感)
    int zoom_pulse = (GET_SIN(t << PULSE_SPEED_SHIFT) >> PULSE_AMP_SHIFT);

    // 开启目标全屏显示
    blt.dst_buf.crop_en     = 1;
    blt.dst_buf.crop.x      = 0;
    blt.dst_buf.crop.y      = 0;
    blt.dst_buf.crop.width  = ctx->info.width;
    blt.dst_buf.crop.height = ctx->info.height;

    // 源裁剪呼吸逻辑：Over-Scaling
    // 裁剪宽度小于 TEX_WIDTH，产生放大效果，切除旋转留下的黑边
    int crop_w = BASE_CROP_W + (GET_SIN(t) >> BREATH_AMP_SHIFT) + zoom_pulse;

    // 保持纵横比
    int crop_h = (crop_w * TEX_HEIGHT) / TEX_WIDTH;

    // 居中裁剪
    blt.src_buf.crop_en     = 1;
    blt.src_buf.crop.x      = (TEX_WIDTH - crop_w) / 2;
    blt.src_buf.crop.y      = (TEX_HEIGHT - crop_h) / 2;
    blt.src_buf.crop.width  = crop_w;
    blt.src_buf.crop.height = crop_h;

    blt.ctrl.alpha_en = 1; // 覆盖

    mpp_ge_bitblt(ctx->ge, &blt);

    // 统一提交队列并同步
    mpp_ge_emit(ctx->ge);
    mpp_ge_sync(ctx->ge);

    g_tick++;
}

static void effect_deinit(struct demo_ctx *ctx)
{
    if (g_tex_phy_addr)
        mpp_phy_free(g_tex_phy_addr);
    if (g_rot_phy_addr)
        mpp_phy_free(g_rot_phy_addr);
}

struct effect_ops effect_0021 = {
    .name   = "NO.21 ROTATIONAL SINGULARITY",
    .init   = effect_init,
    .draw   = effect_draw,
    .deinit = effect_deinit,
};

REGISTER_EFFECT(effect_0021);
