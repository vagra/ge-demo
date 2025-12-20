/*
 * Filename: 0009_rotozoom_plane_v2.c
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
 */

#include "demo_engine.h"
#include "mpp_mem.h"
#include "aic_hal_ge.h"
#include <math.h>

/*
 * === 混合渲染架构 ===
 * 1. 纹理: 320x240 RGB565
 * 2. 优化重点: 调整 Zoom 范围和纹理生成算法，消除混叠噪点。
 */
#define TEX_W    320
#define TEX_H    240
#define TEX_SIZE (TEX_W * TEX_H * 2)

static unsigned int g_tex_phy_addr = 0;
static uint16_t    *g_tex_vir_addr = NULL;
static int          g_tick         = 0;

/* LUTs */
static int      sin_lut[512]; // Q12
static uint16_t palette[256];

static int effect_init(struct demo_ctx *ctx)
{
    g_tex_phy_addr = mpp_phy_alloc(TEX_SIZE);
    if (g_tex_phy_addr == 0)
        return -1;
    g_tex_vir_addr = (uint16_t *)(unsigned long)g_tex_phy_addr;

    // 初始化正弦表
    for (int i = 0; i < 512; i++)
    {
        sin_lut[i] = (int)(sinf(i * 3.14159f / 256.0f) * 4096.0f);
    }

    // 初始化调色板：高对比度的霓虹色
    for (int i = 0; i < 256; i++)
    {
        // 使用更平滑的色彩过渡
        int hue = i;
        int r   = (int)(128 + 127 * sin(hue * 0.05f));
        int g   = (int)(128 + 127 * sin(hue * 0.05f + 2.09f)); // +120度
        int b   = (int)(128 + 127 * sin(hue * 0.05f + 4.18f)); // +240度

        // 增加棋盘格的明暗对比
        if ((i / 32) % 2 == 0)
        {
            r = r * 3 / 4;
            g = g * 3 / 4;
            b = b * 3 / 4;
        }

        palette[i] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
    }

    g_tick = 0;
    rt_kprintf("Night 9: Rotozoom Anti-Aliased.\n");
    return 0;
}

#define GET_SIN(idx) (sin_lut[(idx) & 511])
#define GET_COS(idx) (sin_lut[((idx) + 128) & 511])

static void effect_draw(struct demo_ctx *ctx, unsigned long phy_addr)
{
    if (!g_tex_vir_addr)
        return;

    /*
     * === PHASE 1: 仿射纹理映射 (稳定版) ===
     */

    // 1. 角度：缓慢旋转
    int angle = g_tick * 2;

    // 2. 缩放：限制在 0.5x 到 2.0x 之间，严禁接近 0
    // Q12: 4096 = 1.0x
    // Range: 2048 (0.5x) ~ 6144 (1.5x)
    int zoom = 4096 + (GET_SIN(g_tick) >> 1);

    // 计算步长 (反比于 Zoom)
    int s = (GET_SIN(angle) * 4096) / zoom;
    int c = (GET_COS(angle) * 4096) / zoom;

    // 纹理中心移动 (Pan)
    int center_u = g_tick * 32;
    int center_v = g_tick * 48;

    uint16_t *p_pixel = g_tex_vir_addr;
    int       half_w  = TEX_W / 2;
    int       half_h  = TEX_H / 2;

    for (int y = 0; y < TEX_H; y++)
    {
        int dy = y - half_h;

        // 增量计算起始点
        int start_u = (-half_w * c - dy * s) + (center_u << 12);
        int start_v = (-half_w * s + dy * c) + (center_v << 12);

        int u = start_u;
        int v = start_v;

        for (int x = 0; x < TEX_W; x++)
        {
            /*
             * 纹理生成逻辑 (Texture Pattern)
             * 改用大尺度的 XOR 棋盘格，避免高频噪点
             */

            // 降低纹理频率：右移 16位 而不是 12位
            // 相当于把纹理放大了 16 倍，这样每个“格子”在屏幕上就很大
            int tx = u >> 16;
            int ty = v >> 16;

            // 经典 XOR 纹理，但在宏观尺度上
            int val = (tx ^ ty) & 0xFF;

            // 查表
            *p_pixel++ = palette[val];

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
    blt.src_buf.stride[0]   = TEX_W * 2;
    blt.src_buf.size.width  = TEX_W;
    blt.src_buf.size.height = TEX_H;
    blt.src_buf.format      = MPP_FMT_RGB_565;
    blt.src_buf.crop_en     = 0;

    blt.dst_buf.buf_type    = MPP_PHY_ADDR;
    blt.dst_buf.phy_addr[0] = phy_addr;
    blt.dst_buf.stride[0]   = ctx->info.stride;
    blt.dst_buf.size.width  = ctx->info.width;
    blt.dst_buf.size.height = ctx->info.height;
    blt.dst_buf.format      = ctx->info.format;

    blt.dst_buf.crop_en     = 1;
    blt.dst_buf.crop.x      = 0;
    blt.dst_buf.crop.y      = 0;
    blt.dst_buf.crop.width  = ctx->info.width;
    blt.dst_buf.crop.height = ctx->info.height;

    blt.ctrl.flags    = 0;
    blt.ctrl.alpha_en = 0;

    mpp_ge_bitblt(ctx->ge, &blt);
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
