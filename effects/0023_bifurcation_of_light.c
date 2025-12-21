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
 */

#include "demo_engine.h"
#include "mpp_mem.h"
#include "aic_hal_ge.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

/*
 * Hardware Feature:
 * 1. GE Rot1 (任意角度硬件旋转)
 * 2. GE Scaler (硬件全屏拉伸)
 * 3. GE_PD_ADD (Rule 11: 硬件加法混合)
 * 4. GE FillRect (多层清理：屏幕 + 旋转中间层)
 * 覆盖机能清单：此特效修复了 YUV 格式在 BitBLT 时的硬件限制故障，通过纯 RGB565 双重管线实现了深海荧光视觉。
 */

#define TEX_W    320
#define TEX_H    240
#define TEX_SIZE (TEX_W * TEX_H * 2)

static unsigned int g_bg_phy_addr  = 0; // 背景星云层 (RGB565)
static uint16_t    *g_bg_vir_addr  = NULL;
static unsigned int g_fg_phy_addr  = 0; // 前景干涉层 (RGB565)
static uint16_t    *g_fg_vir_addr  = NULL;
static unsigned int g_rot_phy_addr = 0; // 旋转中间层 (RGB565)

static int      g_tick = 0;
static int      sin_lut[512];
static uint16_t palette_bg[256];
static uint16_t palette_fg[256];

static int effect_init(struct demo_ctx *ctx)
{
    // 1. 分配纯 RGB565 物理显存
    g_bg_phy_addr  = mpp_phy_alloc(TEX_SIZE);
    g_fg_phy_addr  = mpp_phy_alloc(TEX_SIZE);
    g_rot_phy_addr = mpp_phy_alloc(TEX_SIZE);

    if (!g_bg_phy_addr || !g_fg_phy_addr || !g_rot_phy_addr)
        return -1;

    g_bg_vir_addr = (uint16_t *)(unsigned long)g_bg_phy_addr;
    g_fg_vir_addr = (uint16_t *)(unsigned long)g_fg_phy_addr;

    // 2. 初始化查找表 (Q12)
    for (int i = 0; i < 512; i++)
        sin_lut[i] = (int)(sinf(i * 3.14159f / 256.0f) * 4096.0f);

    // 3. 初始化调色板
    for (int i = 0; i < 256; i++)
    {
        // 背景调色板：深邃的海蓝色调
        int r_b       = (int)(10 + 10 * sinf(i * 0.05f));
        int g_b       = (int)(20 + 20 * sinf(i * 0.02f));
        int b_b       = (int)(60 + 40 * sinf(i * 0.03f));
        palette_bg[i] = ((r_b >> 3) << 11) | ((g_b >> 2) << 5) | (b_b >> 3);

        // 前景调色板：明亮的荧光青/蓝
        int r_f       = (int)(20 + 20 * sinf(i * 0.04f));
        int g_f       = (int)(100 + 80 * sinf(i * 0.03f + 1.0f));
        int b_f       = (int)(150 + 100 * sinf(i * 0.05f + 2.0f));
        palette_fg[i] = ((r_f >> 3) << 11) | ((g_f >> 2) << 5) | (b_f >> 3);
    }

    g_tick = 0;
    return 0;
}

#define GET_SIN(idx) (sin_lut[(idx) & 511])
#define GET_COS(idx) (sin_lut[((idx) + 128) & 511])

static void effect_draw(struct demo_ctx *ctx, unsigned long phy_addr)
{
    if (!g_bg_vir_addr || !g_fg_vir_addr)
        return;

    int t = g_tick;

    /* --- PHASE 1: CPU 背景“以太星云”演算 (RGB565) --- */
    uint16_t *bg_p = g_bg_vir_addr;
    for (int y = 0; y < TEX_H; y++)
    {
        int v1 = GET_SIN(y + (t << 1)) >> 7;
        for (int x = 0; x < TEX_W; x++)
        {
            int v2  = GET_COS(x - t) >> 7;
            *bg_p++ = palette_bg[(128 + v1 + v2) & 0xFF];
        }
    }
    aicos_dcache_clean_range((void *)g_bg_vir_addr, TEX_SIZE);

    /* --- PHASE 2: CPU 前景“逻辑场”演算 (RGB565) --- */
    uint16_t *fg_p = g_fg_vir_addr;
    for (int y = 0; y < TEX_H; y++)
    {
        int dy2 = (y - 120) * (y - 120);
        for (int x = 0; x < TEX_W; x++)
        {
            int dx   = x - 160;
            int dist = (dx * dx + dy2) >> 8;
            int val  = (dist ^ (x >> 2) ^ (y >> 2)) + t;
            // 产生稀疏的荧光点
            *fg_p++ = ((val & 0x1F) > 28) ? palette_fg[val & 0xFF] : 0x0000;
        }
    }
    aicos_dcache_clean_range((void *)g_fg_vir_addr, TEX_SIZE);

    /* --- PHASE 3: GE 硬件流水线合成 --- */

    // 1. 绘制背景层 (直接拉伸填充全屏)
    struct ge_bitblt bg_blt    = {0};
    bg_blt.src_buf.buf_type    = MPP_PHY_ADDR;
    bg_blt.src_buf.phy_addr[0] = g_bg_phy_addr;
    bg_blt.src_buf.stride[0]   = TEX_W * 2;
    bg_blt.src_buf.size.width  = TEX_W;
    bg_blt.src_buf.size.height = TEX_H;
    bg_blt.src_buf.format      = MPP_FMT_RGB_565;

    bg_blt.dst_buf.buf_type    = MPP_PHY_ADDR;
    bg_blt.dst_buf.phy_addr[0] = phy_addr;
    bg_blt.dst_buf.stride[0]   = ctx->info.stride;
    bg_blt.dst_buf.size.width  = ctx->info.width;
    bg_blt.dst_buf.size.height = ctx->info.height;
    bg_blt.dst_buf.format      = ctx->info.format;
    bg_blt.dst_buf.crop_en     = 1;
    bg_blt.dst_buf.crop.width  = ctx->info.width;
    bg_blt.dst_buf.crop.height = ctx->info.height;
    bg_blt.ctrl.alpha_en       = 1; // 禁用混合，覆盖

    mpp_ge_bitblt(ctx->ge, &bg_blt);
    mpp_ge_emit(ctx->ge);

    // 2. 修正：彻底清空旋转中间层，消除残影
    struct ge_fillrect clean_rot  = {0};
    clean_rot.type                = GE_NO_GRADIENT;
    clean_rot.start_color         = 0xFF000000; // 纯黑
    clean_rot.dst_buf.buf_type    = MPP_PHY_ADDR;
    clean_rot.dst_buf.phy_addr[0] = g_rot_phy_addr;
    clean_rot.dst_buf.stride[0]   = TEX_W * 2;
    clean_rot.dst_buf.size.width  = TEX_W;
    clean_rot.dst_buf.size.height = TEX_H;
    clean_rot.dst_buf.format      = MPP_FMT_RGB_565;
    mpp_ge_fillrect(ctx->ge, &clean_rot);
    mpp_ge_emit(ctx->ge);
    mpp_ge_sync(ctx->ge);

    // 3. 执行旋转 (前景逻辑层 -> 中间层)
    struct ge_rotation rot  = {0};
    rot.src_buf.buf_type    = MPP_PHY_ADDR;
    rot.src_buf.phy_addr[0] = g_fg_phy_addr;
    rot.src_buf.stride[0]   = TEX_W * 2;
    rot.src_buf.size.width  = TEX_W;
    rot.src_buf.size.height = TEX_H;
    rot.src_buf.format      = MPP_FMT_RGB_565;

    rot.dst_buf.buf_type    = MPP_PHY_ADDR;
    rot.dst_buf.phy_addr[0] = g_rot_phy_addr;
    rot.dst_buf.stride[0]   = TEX_W * 2;
    rot.dst_buf.size.width  = TEX_W;
    rot.dst_buf.size.height = TEX_H;
    rot.dst_buf.format      = MPP_FMT_RGB_565;

    int theta            = (t * 4) & 511;
    rot.angle_sin        = GET_SIN(theta);
    rot.angle_cos        = GET_COS(theta);
    rot.src_rot_center.x = 160;
    rot.src_rot_center.y = 120;
    rot.dst_rot_center.x = 160;
    rot.dst_rot_center.y = 120;
    rot.ctrl.alpha_en    = 1; // 旋转过程禁用混合，仅搬运

    mpp_ge_rotate(ctx->ge, &rot);
    mpp_ge_emit(ctx->ge);
    mpp_ge_sync(ctx->ge); // 旋转就绪

    // 4. 合成前景 (中间层 -> 屏幕，使用 PD_ADD 加法混合)
    struct ge_bitblt mix_blt    = {0};
    mix_blt.src_buf.buf_type    = MPP_PHY_ADDR;
    mix_blt.src_buf.phy_addr[0] = g_rot_phy_addr;
    mix_blt.src_buf.stride[0]   = TEX_W * 2;
    mix_blt.src_buf.size.width  = TEX_W;
    mix_blt.src_buf.size.height = TEX_H;
    mix_blt.src_buf.format      = MPP_FMT_RGB_565;

    mix_blt.dst_buf.buf_type    = MPP_PHY_ADDR;
    mix_blt.dst_buf.phy_addr[0] = phy_addr;
    mix_blt.dst_buf.stride[0]   = ctx->info.stride;
    mix_blt.dst_buf.size.width  = ctx->info.width;
    mix_blt.dst_buf.size.height = ctx->info.height;
    mix_blt.dst_buf.format      = ctx->info.format;
    mix_blt.dst_buf.crop_en     = 1;
    mix_blt.dst_buf.crop.width  = ctx->info.width;
    mix_blt.dst_buf.crop.height = ctx->info.height;

    // 配置加法混合规则
    mix_blt.ctrl.alpha_en         = 0;         // 0 = 使能混合
    mix_blt.ctrl.alpha_rules      = GE_PD_ADD; // 规则 11
    mix_blt.ctrl.src_alpha_mode   = 1;         // 全局 Alpha
    mix_blt.ctrl.src_global_alpha = 180;

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
