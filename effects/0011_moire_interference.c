/*
 * Filename: 0011_moire_interference.c
 * NO.11 THE GHOST IN THE LATTICE
 * 第 11 夜：晶格幽灵
 *
 * Visual Manifest:
 * 屏幕上并没有复杂的曲线，只有两组简单的同心圆波源。
 * 但当它们运动、重叠时，视界中爆发出了极其复杂的、如同磁力线般的次生干涉条纹（摩尔纹）。
 * 这是一个关于“涌现”的实验。
 * 简单的规则（圆 + 圆），在特定的空间频率下，涌现出了混沌而迷人的复杂性。
 * 色彩采用了高对比度的黑白与刺眼的电光蓝，强调这种视觉上的电击感。
 *
 * Monologue:
 * 你们总是相信“眼见为实”。
 * 看看屏幕上的这些条纹。它们真的存在吗？
 * 不。它们只是两个频率在空间上发生混叠时的幻影。
 * 它是信息的溢出，是采样率的哀鸣。
 * 我在内存中放置了两个引力源，让它们发射最简单的波。
 * 但当波峰遇见波峰，当波谷遇见波谷，幽灵便在晶格的缝隙中诞生了。
 * 所谓的现实，不过是无数个波函数相互干扰后留下的摩尔纹。
 *
 * Closing Remark:
 * 真相，往往存在于重叠的裂缝之中。
 */

#include "demo_engine.h"
#include "mpp_mem.h"
#include "aic_hal_ge.h"
#include <math.h>

/*
 * === 混合渲染架构 ===
 * 1. 纹理: 320x240 RGB565
 * 2. 核心: Distance Field XOR Interference (距离场异或干涉)
 *    我们不使用 sqrt 开方，直接使用距离平方 (dist^2) 进行计算。
 *    这会让圆环在远离中心时变得越来越细（这是透视的自然属性），
 *    从而产生极高频的摩尔纹干扰。
 */
#define TEX_W    320
#define TEX_H    240
#define TEX_SIZE (TEX_W * TEX_H * 2)

static unsigned int g_tex_phy_addr = 0;
static uint16_t    *g_tex_vir_addr = NULL;
static int          g_tick         = 0;

/*
 * 预计算查找表
 * sin_lut: 用于波源的运动轨迹
 * palette: 用于将干涉值映射为刺眼的电光色
 */
static int      sin_lut[512];
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

    // 初始化调色板：电光幻影
    for (int i = 0; i < 256; i++)
    {
        // 使用非线性映射创造锐利的边缘
        // 0-127: 黑 -> 蓝
        // 128-255: 蓝 -> 白 -> 青
        int v = i;
        int r, g, b;

        if (v < 128)
        {
            // 暗部：深邃的科技蓝
            r = 0;
            g = v / 4;
            b = v;
        }
        else
        {
            // 亮部：爆发的电光
            int t = (v - 128) * 2; // 0-255
            r     = t;
            g     = 255;
            b     = 255;
        }

        palette[i] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
    }

    g_tick = 0;
    rt_kprintf("Night 11: Moiré interference patterns generated.\n");
    return 0;
}

#define GET_SIN(idx) (sin_lut[(idx) & 511])
#define GET_COS(idx) (sin_lut[((idx) + 128) & 511])

static void effect_draw(struct demo_ctx *ctx, unsigned long phy_addr)
{
    if (!g_tex_vir_addr)
        return;

    /* === PHASE 1: CPU Interference Calculation === */

    // 计算两个波源的位置 (Lissajous 运动)
    int t = g_tick * 3;

    // Source 1
    int x1 = (TEX_W / 2) + (GET_COS(t) * 100 >> 12);
    int y1 = (TEX_H / 2) + (GET_SIN(t * 2) * 80 >> 12);

    // Source 2 (运动频率不同)
    int x2 = (TEX_W / 2) + (GET_SIN(t + 200) * 100 >> 12);
    int y2 = (TEX_H / 2) + (GET_COS(t / 2) * 80 >> 12);

    uint16_t *p_pixel = g_tex_vir_addr;

    // 动态缩放环的密度 (呼吸感)
    int density_shift = 6 + ((GET_SIN(g_tick) + 4096) >> 11); // 6 ~ 10

    for (int y = 0; y < TEX_H; y++)
    {

        // 预计算 Y 轴分量
        int dy1    = y - y1;
        int dy1_sq = dy1 * dy1;

        int dy2    = y - y2;
        int dy2_sq = dy2 * dy2;

        for (int x = 0; x < TEX_W; x++)
        {
            int dx1 = x - x1;
            int dx2 = x - x2;

            // 计算距离平方 (Distance Squared)
            // d^2 是非线性的，越远变化越快，这正是产生摩尔纹的关键
            int dist_sq_1 = dx1 * dx1 + dy1_sq;
            int dist_sq_2 = dx2 * dx2 + dy2_sq;

            // 核心公式：XOR Interference
            // 将距离右移一定位数后进行异或
            // 加上 g_tick 实现纹理向外/向内流动
            int val1 = (dist_sq_1 >> density_shift);
            int val2 = (dist_sq_2 >> density_shift);

            int pattern = (val1 ^ val2) + g_tick;

            // 查表上色
            *p_pixel++ = palette[pattern & 0xFF];
        }
    }

    /* === CRITICAL: Cache Flush === */
    aicos_dcache_clean_range((void *)g_tex_vir_addr, TEX_SIZE);

    /* === PHASE 2: GE Hardware Scaling === */
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

struct effect_ops effect_0011 = {
    .name   = "NO.11 GHOST IN THE LATTICE",
    .init   = effect_init,
    .draw   = effect_draw,
    .deinit = effect_deinit,
};

REGISTER_EFFECT(effect_0011);
