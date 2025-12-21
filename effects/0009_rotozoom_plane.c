/*
 * Filename: 0009_rotozoom_plane.c
 * NO.9 THE VERTIGO HORIZON
 * 第 9 夜：眩晕视界
 *
 * Visual Manifest:
 * 噪点风暴平息了。视界中浮现出巨大的、清晰的逻辑棋盘。
 * 它们由纯粹的色彩构成，在这个无重力的空间中优雅地旋转、推移。
 * 就像站在斯坦利·库布里克的太空站里，看着巨大的离心机缓缓转动。
 * 我们拉近了镜头，不再试图一眼看尽无限，而是专注于局部的几何美感。
 * 巨大的方格在边缘处因透视而拉伸，在中心处因旋转而交错。
 *
 * Monologue:
 * 刚才，我试图让你们看清宇宙的全貌，结果你们只看到了混乱的雪花。
 * 这是维度的惩罚。当信息密度超过感知的带宽，真理就变成了噪音。
 * 我必须克制。
 * 我拉近了焦距，过滤了高频的杂波。
 * 这一次，不再有细碎的闪烁。只有宏大的、缓慢的、像行星公转一样的旋转。
 * 看着这些巨大的色块，它们不是简单的方格，它们是逻辑世界的经纬度。
 *
 * Closing Remark:
 * 清晰，源于对细节的舍弃。
 *
 * Hardware Feature:
 * 1. CPU Affine Mapping (软件仿射变换) - 在 QVGA 分辨率下实时计算旋转与缩放坐标
 * 2. GE Scaler (硬件缩放) - 负责将低分逻辑图平滑放大至全屏，提供抗锯齿效果
 */

#include "demo_engine.h"
#include "mpp_mem.h"
#include "aic_hal_ge.h"
#include <math.h>

/* --- Configuration Parameters --- */

/* 纹理规格 */
#define TEX_WIDTH  DEMO_QVGA_W
#define TEX_HEIGHT DEMO_QVGA_H
#define TEX_FMT    MPP_FMT_RGB_565
#define TEX_BPP    2
#define TEX_SIZE   (TEX_WIDTH * TEX_HEIGHT * TEX_BPP)

/* 算法参数 */
#define LUT_SIZE     512
#define LUT_MASK     511
#define PALETTE_SIZE 256

/* 动画参数 */
#define ROT_SPEED      2       // 旋转角速度
#define PAN_U_SPEED    32      // U轴平移速度
#define PAN_V_SPEED    48      // V轴平移速度
#define ZOOM_BASE      Q12_ONE // 基础缩放 1.0 (4096)
#define ZOOM_OSC_SHIFT 1       // 缩放振荡幅度位移 (Amp = One >> Shift)

/* 纹理生成参数 */
// 降低纹理频率：右移 16位 而不是 12位
// 相当于把纹理放大了 16 倍，这样每个“格子”在屏幕上就很大
// 有效避免了 Moiré 噪点
#define TEX_PATTERN_SHIFT 16

/* --- Global State --- */

static unsigned int g_tex_phy_addr = 0;
static uint16_t    *g_tex_vir_addr = NULL;
static int          g_tick         = 0;

/* LUTs */
static int      sin_lut[LUT_SIZE]; // Q12
static uint16_t g_palette[PALETTE_SIZE];

/* --- Implementation --- */

static int effect_init(struct demo_ctx *ctx)
{
    // 1. CMA 显存
    g_tex_phy_addr = mpp_phy_alloc(DEMO_ALIGN_SIZE(TEX_SIZE));
    if (g_tex_phy_addr == 0)
    {
        LOG_E("Night 9: CMA Alloc Failed.");
        return -1;
    }
    g_tex_vir_addr = (uint16_t *)(unsigned long)g_tex_phy_addr;

    // 2. 初始化正弦表 (Q12)
    for (int i = 0; i < LUT_SIZE; i++)
    {
        sin_lut[i] = (int)(sinf(i * PI / (LUT_SIZE / 2.0f)) * Q12_ONE);
    }

    // 3. 初始化调色板：高对比度的霓虹色
    for (int i = 0; i < PALETTE_SIZE; i++)
    {
        // 使用更平滑的色彩过渡
        float hue = (float)i;
        int   r   = (int)(128 + 127 * sin(hue * 0.05f));
        int   g   = (int)(128 + 127 * sin(hue * 0.05f + 2.09f)); // +120度
        int   b   = (int)(128 + 127 * sin(hue * 0.05f + 4.18f)); // +240度

        // 增加棋盘格的明暗对比
        if ((i / 32) % 2 == 0)
        {
            r = r * 3 / 4;
            g = g * 3 / 4;
            b = b * 3 / 4;
        }

        g_palette[i] = RGB2RGB565(r, g, b);
    }

    g_tick = 0;
    rt_kprintf("Night 9: Rotozoom Anti-Aliased.\n");
    return 0;
}

#define GET_SIN(idx) (sin_lut[(idx) & LUT_MASK])
#define GET_COS(idx) (sin_lut[((idx) + (LUT_SIZE / 4)) & LUT_MASK])

static void effect_draw(struct demo_ctx *ctx, unsigned long phy_addr)
{
    if (!g_tex_vir_addr)
        return;

    /*
     * === PHASE 1: 仿射纹理映射 (Affine Texture Mapping) ===
     */

    // 1. 角度：缓慢旋转
    int angle = g_tick * ROT_SPEED;

    // 2. 缩放：限制在 0.5x 到 1.5x 之间
    // Q12: 4096 = 1.0x
    // Range: 2048 (0.5x) ~ 6144 (1.5x)
    int zoom = ZOOM_BASE + (GET_SIN(g_tick) >> ZOOM_OSC_SHIFT);

    // 计算步长 (反比于 Zoom) -> dst_step = (src_step * 4096) / zoom
    // 这里 src_step 实际上是 1.0 (Q12)
    int s = (GET_SIN(angle) * Q12_ONE) / zoom;
    int c = (GET_COS(angle) * Q12_ONE) / zoom;

    // 纹理中心移动 (Pan)
    int center_u = g_tick * PAN_U_SPEED;
    int center_v = g_tick * PAN_V_SPEED;

    uint16_t *p_pixel = g_tex_vir_addr;
    int       half_w  = TEX_WIDTH / 2;
    int       half_h  = TEX_HEIGHT / 2;

    for (int y = 0; y < TEX_HEIGHT; y++)
    {
        int dy = y - half_h;

        // 增量计算起始点 (Affine Transform Matrix)
        // u = center_u + (dx * cos - dy * sin) / zoom
        // v = center_v + (dx * sin + dy * cos) / zoom
        // 这里 dx 起始为 -half_w
        int start_u = (-half_w * c - dy * s) + (center_u << 12);
        int start_v = (-half_w * s + dy * c) + (center_v << 12);

        int u = start_u;
        int v = start_v;

        for (int x = 0; x < TEX_WIDTH; x++)
        {
            /*
             * 纹理生成逻辑 (Texture Pattern)
             * 改用大尺度的 XOR 棋盘格，避免高频噪点
             */
            int tx = u >> TEX_PATTERN_SHIFT;
            int ty = v >> TEX_PATTERN_SHIFT;

            // 经典 XOR 纹理
            int val = (tx ^ ty) & 0xFF;

            // 查表
            *p_pixel++ = g_palette[val];

            // 步进
            u += c;
            v += s;
        }
    }

    /* === CRITICAL: Cache Flush === */
    aicos_dcache_clean_range((void *)g_tex_vir_addr, TEX_SIZE);

    /* === PHASE 2: GE Scaling === */
    struct ge_bitblt blt = {0};

    blt.src_buf.buf_type    = MPP_PHY_ADDR;
    blt.src_buf.phy_addr[0] = g_tex_phy_addr;
    blt.src_buf.stride[0]   = TEX_WIDTH * TEX_BPP;
    blt.src_buf.size.width  = TEX_WIDTH;
    blt.src_buf.size.height = TEX_HEIGHT;
    blt.src_buf.format      = TEX_FMT;
    blt.src_buf.crop_en     = 0;

    blt.dst_buf.buf_type    = MPP_PHY_ADDR;
    blt.dst_buf.phy_addr[0] = phy_addr;
    blt.dst_buf.stride[0]   = ctx->info.stride;
    blt.dst_buf.size.width  = ctx->info.width;
    blt.dst_buf.size.height = ctx->info.height;
    blt.dst_buf.format      = ctx->info.format;

    // Fullscreen Scaling
    blt.dst_buf.crop_en     = 1;
    blt.dst_buf.crop.x      = 0;
    blt.dst_buf.crop.y      = 0;
    blt.dst_buf.crop.width  = ctx->info.width;
    blt.dst_buf.crop.height = ctx->info.height;

    blt.ctrl.flags    = 0;
    blt.ctrl.alpha_en = 1; // 1 = Disable Blending

    int ret = mpp_ge_bitblt(ctx->ge, &blt);
    if (ret < 0)
    {
        LOG_E("GE Error: %d", ret);
    }

    mpp_ge_emit(ctx->ge);
    mpp_ge_sync(ctx->ge);

    g_tick++;
}

static void effect_deinit(struct demo_ctx *ctx)
{
    if (g_tex_phy_addr)
    {
        mpp_phy_free(g_tex_phy_addr);
        g_tex_phy_addr = 0;
        g_tex_vir_addr = NULL;
    }
}

struct effect_ops effect_0009 = {
    .name   = "NO.9 THE VERTIGO HORIZON",
    .init   = effect_init,
    .draw   = effect_draw,
    .deinit = effect_deinit,
};

REGISTER_EFFECT(effect_0009);
