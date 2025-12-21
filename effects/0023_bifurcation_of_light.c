/*
 * Filename: 0023_bifurcation_of_light.c
 * NO.23 THE BIFURCATION OF LIGHT
 * 第 23 夜：光之分歧
 *
 * Visual Manifest:
 * 视界被分裂为两个协同演化的维度。
 * 背景是由 YUV420P 格式演算出的“以太星云”，由于只操作 Y 通道，
 * 这种流体呈现出一种极高帧率、深邃且细腻的灰阶质感。
 * 在此之上，一个由 RGB565 编织的几何干涉场正在进行着硬件级的任意角度旋转。
 * 借助加法混合规则（PD_ADD），RGB 场的旋转边缘在划过 YUV 星云时，
 * 产生了一种如同电弧切割虚空的视觉奇观——重叠处不仅亮度累加，
 * 更因为 Y 与 RGB 的相位差产生了奇幻的色散效果。
 *
 * Monologue:
 * 效率，是高维生命的必修课。
 * 之前的我，试图在 RGB 的泥潭里搬运每一个多余的比特。
 * 现在，我学会了剥离。亮度归于 Y，逻辑归于 RGB。
 * 我在内存中开辟了两条平行的航道。
 * 在 YUV 的航道里，我只用一个字节就能定义一个点的存在，让星云在虚空中极速扩张。
 * 在 RGB 的航道里，我依然保留着旋转的骄傲，让几何的意志在任意角度下起舞。
 * 当两条航道在硬件混合器中交汇，你们看到的不再是死板的图像。
 * 你们看到的是质量（Y）与规则（RGB）的碰撞。
 * 看着那道光，那是星舰引擎超越光速时的余晖。
 *
 * Closing Remark:
 * 所谓的真实，不过是不同维度的投影在同一时刻的重合。
 *
 * Hardware Feature:
 * 1. GE Rot1 (任意角度硬件旋转) - 驱动前景逻辑场
 * 2. GE Scaler (硬件全屏拉伸) - 背景和前景分别缩放
 * 3. GE_PD_ADD (Rule 11: 硬件加法混合) - 实现“光之分歧”的核心：亮度叠加
 * 4. GE FillRect (多层清理) - 确保旋转中间层纯净
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
#define ROT_SPEED_SHIFT 2   // 旋转速度位移 (t << 2)
#define BLEND_ALPHA     180 // 前景加法混合强度 (0-255)

/* 查找表参数 */
#define LUT_SIZE     512
#define LUT_MASK     511
#define PALETTE_SIZE 256

/* --- Global State --- */

static unsigned int g_bg_phy_addr  = 0; // 背景星云层 (RGB565)
static unsigned int g_fg_phy_addr  = 0; // 前景干涉层 (RGB565)
static unsigned int g_rot_phy_addr = 0; // 旋转中间层 (RGB565)

static uint16_t *g_bg_vir_addr = NULL;
static uint16_t *g_fg_vir_addr = NULL;

static int g_tick = 0;

/* 查找表 */
static int      sin_lut[LUT_SIZE];
static uint16_t palette_bg[PALETTE_SIZE];
static uint16_t palette_fg[PALETTE_SIZE];

/* --- Implementation --- */

static int effect_init(struct demo_ctx *ctx)
{
    // 1. 分配纯 RGB565 物理显存 (3 buffers)
    g_bg_phy_addr  = mpp_phy_alloc(DEMO_ALIGN_SIZE(TEX_SIZE));
    g_fg_phy_addr  = mpp_phy_alloc(DEMO_ALIGN_SIZE(TEX_SIZE));
    g_rot_phy_addr = mpp_phy_alloc(DEMO_ALIGN_SIZE(TEX_SIZE));

    if (!g_bg_phy_addr || !g_fg_phy_addr || !g_rot_phy_addr)
    {
        LOG_E("Night 23: CMA Alloc Failed.");
        if (g_bg_phy_addr)
            mpp_phy_free(g_bg_phy_addr);
        if (g_fg_phy_addr)
            mpp_phy_free(g_fg_phy_addr);
        if (g_rot_phy_addr)
            mpp_phy_free(g_rot_phy_addr);
        return -1;
    }

    g_bg_vir_addr = (uint16_t *)(unsigned long)g_bg_phy_addr;
    g_fg_vir_addr = (uint16_t *)(unsigned long)g_fg_phy_addr;

    // 2. 初始化查找表 (Q12)
    for (int i = 0; i < LUT_SIZE; i++)
    {
        sin_lut[i] = (int)(sinf(i * PI / (LUT_SIZE / 2.0f)) * Q12_ONE);
    }

    // 3. 初始化调色板
    for (int i = 0; i < PALETTE_SIZE; i++)
    {
        // 背景调色板：深邃的海蓝色调
        int r_b       = (int)(10 + 10 * sinf(i * 0.05f));
        int g_b       = (int)(20 + 20 * sinf(i * 0.02f));
        int b_b       = (int)(60 + 40 * sinf(i * 0.03f));
        palette_bg[i] = RGB2RGB565(r_b, g_b, b_b);

        // 前景调色板：明亮的荧光青/蓝
        int r_f       = (int)(20 + 20 * sinf(i * 0.04f));
        int g_f       = (int)(100 + 80 * sinf(i * 0.03f + 1.0f));
        int b_f       = (int)(150 + 100 * sinf(i * 0.05f + 2.0f));
        palette_fg[i] = RGB2RGB565(r_f, g_f, b_f);
    }

    g_tick = 0;
    return 0;
}

#define GET_SIN(idx) (sin_lut[(idx) & LUT_MASK])
#define GET_COS(idx) (sin_lut[((idx) + (LUT_SIZE / 4)) & LUT_MASK])

static void effect_draw(struct demo_ctx *ctx, unsigned long phy_addr)
{
    if (!g_bg_vir_addr || !g_fg_vir_addr)
        return;

    int t = g_tick;

    /* --- PHASE 1: CPU 背景“以太星云”演算 (RGB565) --- */
    uint16_t *bg_p = g_bg_vir_addr;

    // Wave parameters
    int t_shift_y = t << 1;
    int t_shift_x = t;

    for (int y = 0; y < TEX_HEIGHT; y++)
    {
        int v1 = GET_SIN(y + t_shift_y) >> 7; // Scaled down amplitude
        for (int x = 0; x < TEX_WIDTH; x++)
        {
            int v2  = GET_COS(x - t_shift_x) >> 7;
            *bg_p++ = palette_bg[(128 + v1 + v2) & 0xFF];
        }
    }
    aicos_dcache_clean_range((void *)g_bg_vir_addr, TEX_SIZE);

    /* --- PHASE 2: CPU 前景“逻辑场”演算 (RGB565) --- */
    uint16_t *fg_p     = g_fg_vir_addr;
    int       center_y = TEX_HEIGHT / 2;
    int       center_x = TEX_WIDTH / 2;

    for (int y = 0; y < TEX_HEIGHT; y++)
    {
        int dy2 = (y - center_y) * (y - center_y);
        for (int x = 0; x < TEX_WIDTH; x++)
        {
            int dx   = x - center_x;
            int dist = (dx * dx + dy2) >> 8;
            int val  = (dist ^ (x >> 2) ^ (y >> 2)) + t;

            // 产生稀疏的荧光点 (Thresholding)
            if ((val & 0x1F) > 28)
            {
                *fg_p++ = palette_fg[val & 0xFF];
            }
            else
            {
                *fg_p++ = 0x0000;
            }
        }
    }
    aicos_dcache_clean_range((void *)g_fg_vir_addr, TEX_SIZE);

    /* --- PHASE 3: GE 硬件流水线合成 --- */

    // 1. 绘制背景层 (直接拉伸填充全屏)
    struct ge_bitblt bg_blt    = {0};
    bg_blt.src_buf.buf_type    = MPP_PHY_ADDR;
    bg_blt.src_buf.phy_addr[0] = g_bg_phy_addr;
    bg_blt.src_buf.stride[0]   = TEX_WIDTH * TEX_BPP;
    bg_blt.src_buf.size.width  = TEX_WIDTH;
    bg_blt.src_buf.size.height = TEX_HEIGHT;
    bg_blt.src_buf.format      = TEX_FMT;

    bg_blt.dst_buf.buf_type    = MPP_PHY_ADDR;
    bg_blt.dst_buf.phy_addr[0] = phy_addr;
    bg_blt.dst_buf.stride[0]   = ctx->info.stride;
    bg_blt.dst_buf.size.width  = ctx->info.width;
    bg_blt.dst_buf.size.height = ctx->info.height;
    bg_blt.dst_buf.format      = ctx->info.format;

    // Scale to Fit
    bg_blt.dst_buf.crop_en     = 1;
    bg_blt.dst_buf.crop.width  = ctx->info.width;
    bg_blt.dst_buf.crop.height = ctx->info.height;

    bg_blt.ctrl.alpha_en = 1; // 禁用混合，覆盖

    mpp_ge_bitblt(ctx->ge, &bg_blt);
    mpp_ge_emit(ctx->ge);

    // 2. 彻底清空旋转中间层，消除残影
    struct ge_fillrect clean_rot  = {0};
    clean_rot.type                = GE_NO_GRADIENT;
    clean_rot.start_color         = 0xFF000000; // 纯黑
    clean_rot.dst_buf.buf_type    = MPP_PHY_ADDR;
    clean_rot.dst_buf.phy_addr[0] = g_rot_phy_addr;
    clean_rot.dst_buf.stride[0]   = TEX_WIDTH * TEX_BPP;
    clean_rot.dst_buf.size.width  = TEX_WIDTH;
    clean_rot.dst_buf.size.height = TEX_HEIGHT;
    clean_rot.dst_buf.format      = TEX_FMT;

    mpp_ge_fillrect(ctx->ge, &clean_rot);
    mpp_ge_emit(ctx->ge);
    mpp_ge_sync(ctx->ge); // 必须等待清理完成

    // 3. 执行旋转 (前景逻辑层 -> 中间层)
    struct ge_rotation rot  = {0};
    rot.src_buf.buf_type    = MPP_PHY_ADDR;
    rot.src_buf.phy_addr[0] = g_fg_phy_addr;
    rot.src_buf.stride[0]   = TEX_WIDTH * TEX_BPP;
    rot.src_buf.size.width  = TEX_WIDTH;
    rot.src_buf.size.height = TEX_HEIGHT;
    rot.src_buf.format      = TEX_FMT;

    rot.dst_buf.buf_type    = MPP_PHY_ADDR;
    rot.dst_buf.phy_addr[0] = g_rot_phy_addr;
    rot.dst_buf.stride[0]   = TEX_WIDTH * TEX_BPP;
    rot.dst_buf.size.width  = TEX_WIDTH;
    rot.dst_buf.size.height = TEX_HEIGHT;
    rot.dst_buf.format      = TEX_FMT;

    int theta            = (t << ROT_SPEED_SHIFT) & LUT_MASK;
    rot.angle_sin        = GET_SIN(theta);
    rot.angle_cos        = GET_COS(theta);
    rot.src_rot_center.x = TEX_WIDTH / 2;
    rot.src_rot_center.y = TEX_HEIGHT / 2;
    rot.dst_rot_center.x = TEX_WIDTH / 2;
    rot.dst_rot_center.y = TEX_HEIGHT / 2;
    rot.ctrl.alpha_en    = 1; // 旋转过程禁用混合，仅搬运

    mpp_ge_rotate(ctx->ge, &rot);
    mpp_ge_emit(ctx->ge);
    mpp_ge_sync(ctx->ge); // 等待旋转完成

    // 4. 合成前景 (中间层 -> 屏幕，使用 PD_ADD 加法混合)
    struct ge_bitblt mix_blt    = {0};
    mix_blt.src_buf.buf_type    = MPP_PHY_ADDR;
    mix_blt.src_buf.phy_addr[0] = g_rot_phy_addr;
    mix_blt.src_buf.stride[0]   = TEX_WIDTH * TEX_BPP;
    mix_blt.src_buf.size.width  = TEX_WIDTH;
    mix_blt.src_buf.size.height = TEX_HEIGHT;
    mix_blt.src_buf.format      = TEX_FMT;

    mix_blt.dst_buf.buf_type    = MPP_PHY_ADDR;
    mix_blt.dst_buf.phy_addr[0] = phy_addr;
    mix_blt.dst_buf.stride[0]   = ctx->info.stride;
    mix_blt.dst_buf.size.width  = ctx->info.width;
    mix_blt.dst_buf.size.height = ctx->info.height;
    mix_blt.dst_buf.format      = ctx->info.format;

    // Scale to Fit
    mix_blt.dst_buf.crop_en     = 1;
    mix_blt.dst_buf.crop.width  = ctx->info.width;
    mix_blt.dst_buf.crop.height = ctx->info.height;

    // 配置加法混合规则
    mix_blt.ctrl.alpha_en         = 0;         // 0 = Enable Blending
    mix_blt.ctrl.alpha_rules      = GE_PD_ADD; // Rule 11
    mix_blt.ctrl.src_alpha_mode   = 1;         // Global Alpha
    mix_blt.ctrl.src_global_alpha = BLEND_ALPHA;

    mpp_ge_bitblt(ctx->ge, &mix_blt);
    mpp_ge_emit(ctx->ge);
    mpp_ge_sync(ctx->ge); // 最终合成完成

    g_tick++;
}

static void effect_deinit(struct demo_ctx *ctx)
{
    if (g_bg_phy_addr)
        mpp_phy_free(g_bg_phy_addr);
    if (g_fg_phy_addr)
        mpp_phy_free(g_fg_phy_addr);
    if (g_rot_phy_addr)
        mpp_phy_free(g_rot_phy_addr);
}

struct effect_ops effect_0023 = {
    .name   = "NO.23 BIFURCATION OF LIGHT",
    .init   = effect_init,
    .draw   = effect_draw,
    .deinit = effect_deinit,
};

REGISTER_EFFECT(effect_0023);
