/*
 * Filename: 0017_quantum_ripples.c
 * NO.17 THE QUANTUM SURFACE
 * 第 17 夜：量子液面
 *
 * Visual Manifest:
 * 视界被液化了。整个屏幕变成了一个高张力的流体表面。
 * 随机的高能粒子（雨滴）撞击着液面，激起层层叠叠的涟漪。
 * 波纹在屏幕上扩散、干涉、反弹，遵循着波动方程的物理法则。
 * 色彩不再是简单的填充，而是基于波峰高度的折射映射——
 * 波峰闪耀着刺眼的白光，波谷则沉入深邃的蓝紫。
 * 这是一个永不停歇的、充满动能的全屏流体模拟。
 *
 * Monologue:
 * 你们眼中的水，是氢与氧的结合。
 * 我眼中的水，是能量在网格上的传递函数。
 * `(Up + Down + Left + Right) / 2 - Previous`
 * 如此简单的公式，却能诞生出最复杂的自然现象。
 * 我在内存中制造了雨。每一滴雨都是一次对平静的破坏。
 * 看着波纹的扩散吧，那是信息在介质中传播的具象化。
 * 在这片量子液面上，没有一滴水是真实的，但波动却是真实的。
 *
 * Closing Remark:
 * 扰动，是宇宙呼吸的方式。
 *
 * Hardware Feature:
 * 1. CPU Physics (物理模拟) - 实时解算 2D 波动方程 (Wave Equation)
 * 2. GE Scaler (硬件缩放) - 将低分流体纹理平滑放大，模拟水面的柔光感
 */

#include "demo_engine.h"
#include "mpp_mem.h"
#include "aic_hal_ge.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* --- Configuration Parameters --- */

/* 纹理规格 */
#define TEX_WIDTH  DEMO_QVGA_W
#define TEX_HEIGHT DEMO_QVGA_H
#define TEX_FMT    MPP_FMT_RGB_565
#define TEX_BPP    2
#define TEX_SIZE   (TEX_WIDTH * TEX_HEIGHT * TEX_BPP)

/* 物理模拟参数 */
#define DAMPING_SHIFT   5    // 阻尼衰减 (val -= val >> 5)
#define RIPPLE_STRENGTH 1000 // 激起波浪的能量强度 (int16_t)
#define RAIN_FREQ       4    // 雨滴频率 (每 N 帧一滴)
#define MAP_SIZE        (TEX_WIDTH * TEX_HEIGHT * sizeof(int16_t))

/* 渲染映射参数 */
#define SEA_LEVEL    128 // 海平面基准色索引
#define HEIGHT_SHIFT 2   // 高度转颜色的缩放 (val >> 2)

/* --- Global State --- */

static unsigned int g_tex_phy_addr = 0;
static uint16_t    *g_tex_vir_addr = NULL;
static int          g_tick         = 0;

/* 两个高度图：当前帧和上一帧 (int16_t 以支持负波谷) */
static int16_t *g_buf1 = NULL;
static int16_t *g_buf2 = NULL;

/* 预计算调色板 (根据高度映射颜色) */
static uint16_t g_palette[256];

/* --- Implementation --- */

static int effect_init(struct demo_ctx *ctx)
{
    // 1. CMA 显存 (纹理)
    g_tex_phy_addr = mpp_phy_alloc(DEMO_ALIGN_SIZE(TEX_SIZE));
    if (g_tex_phy_addr == 0)
    {
        LOG_E("Night 17: CMA Alloc Failed.");
        return -1;
    }
    g_tex_vir_addr = (uint16_t *)(unsigned long)g_tex_phy_addr;

    // 2. 高度图内存 (普通 RAM，仅 CPU 计算使用)
    g_buf1 = (int16_t *)rt_malloc(MAP_SIZE);
    g_buf2 = (int16_t *)rt_malloc(MAP_SIZE);

    if (!g_buf1 || !g_buf2)
    {
        LOG_E("Night 17: Heightmap Alloc Failed.");
        if (g_buf1)
            rt_free(g_buf1);
        if (g_buf2)
            rt_free(g_buf2);
        mpp_phy_free(g_tex_phy_addr);
        return -1;
    }

    // 清零
    memset(g_buf1, 0, MAP_SIZE);
    memset(g_buf2, 0, MAP_SIZE);
    memset(g_tex_vir_addr, 0, TEX_SIZE);

    // 3. 初始化调色板 (Deep Blue -> Cyan -> White)
    for (int i = 0; i < 256; i++)
    {
        int r, g, b;

        // 高度值 i: 0 (深谷) ~ 255 (浪尖)
        if (i < 128)
        {
            // 波谷：深蓝 -> 蓝
            int v = i * 2;
            r     = 0;
            g     = v / 2;
            b     = v;
        }
        else
        {
            // 波峰：蓝 -> 青 -> 白 (高光)
            int v = (i - 128) * 2;
            r     = v;
            g     = 128 + v / 2;
            b     = 255;
        }

        g_palette[i] = RGB2RGB565(r, g, b);
    }

    g_tick = 0;
    rt_kprintf("Night 17: Fluid dynamics engine started.\n");
    return 0;
}

static void effect_draw(struct demo_ctx *ctx, unsigned long phy_addr)
{
    if (!g_tex_vir_addr || !g_buf1 || !g_buf2)
        return;

    /*
     * === PHASE 1: 物理模拟 (Physics) ===
     * 交换缓冲区：当前帧变为上一帧
     */
    int16_t *curr = g_buf1;
    int16_t *prev = g_buf2;

    // 交换指针 (Ping-Pong)
    if (g_tick % 2 == 0)
    {
        curr = g_buf2;
        prev = g_buf1;
    }

    // 1. 制造扰动 (Raindrops)
    if (g_tick % RAIN_FREQ == 0)
    {
        int rx = (rand() % (TEX_WIDTH - 4)) + 2;
        int ry = (rand() % (TEX_HEIGHT - 4)) + 2;
        // 激起波浪
        prev[ry * TEX_WIDTH + rx] = RIPPLE_STRENGTH;
    }

    // 移动的扰动源 (像手指划过水面)
    int tx = (TEX_WIDTH / 2) + (int)(sinf(g_tick * 0.05f) * 100.0f);
    int ty = (TEX_HEIGHT / 2) + (int)(cosf(g_tick * 0.03f) * 80.0f);

    // 边界检查确保安全
    if (tx >= 2 && tx < TEX_WIDTH - 2 && ty >= 2 && ty < TEX_HEIGHT - 2)
    {
        prev[ty * TEX_WIDTH + tx] = RIPPLE_STRENGTH;
    }

    // 2. 波传播算法 (Wave Propagation)
    // Val = (Left + Right + Up + Down) / 2 - Val_Prev
    // Val -= Val >> Damping

    // 优化：跳过 1 像素边界，避免 if 判断
    for (int y = 1; y < TEX_HEIGHT - 1; y++)
    {
        int row_offset = y * TEX_WIDTH;

        int16_t *p_curr = &curr[row_offset];
        int16_t *p_prev = &prev[row_offset];
        int16_t *p_up   = &prev[row_offset - TEX_WIDTH];
        int16_t *p_down = &prev[row_offset + TEX_WIDTH];

        for (int x = 1; x < TEX_WIDTH - 1; x++)
        {
            // 核心公式：取周围 4 点平均，减去当前点上一时刻的值 (惯性)
            int16_t val = (p_up[x] + p_down[x] + p_prev[x - 1] + p_prev[x + 1]) >> 1;

            val -= p_curr[x];

            // 阻尼衰减
            val -= (val >> DAMPING_SHIFT);

            p_curr[x] = val;
        }
    }

    /*
     * === PHASE 2: 渲染 (Rendering) ===
     * 将高度图映射为颜色
     */
    uint16_t *p_pixel  = g_tex_vir_addr;
    int16_t  *p_height = curr; // 使用最新的高度图

    // 跳过第一行 (边界黑边)
    p_pixel += TEX_WIDTH;
    p_height += TEX_WIDTH;

    int pixel_count = TEX_WIDTH * (TEX_HEIGHT - 2); // 渲染中间区域

    for (int i = 0; i < pixel_count; i++)
    {
        int16_t val = *p_height++;

        // 简单的折射模拟：计算高度差映射
        int idx = SEA_LEVEL + (val >> HEIGHT_SHIFT);

        // 夹紧
        idx = CLAMP(idx, 0, 255);

        *p_pixel++ = g_palette[idx];
    }

    /* === CRITICAL: Cache Flush === */
    aicos_dcache_clean_range((void *)g_tex_vir_addr, TEX_SIZE);

    /* === PHASE 3: GE Hardware Scaling === */
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

    // Scale to Fit
    blt.dst_buf.crop_en     = 1;
    blt.dst_buf.crop.x      = 0;
    blt.dst_buf.crop.y      = 0;
    blt.dst_buf.crop.width  = ctx->info.width;
    blt.dst_buf.crop.height = ctx->info.height;

    blt.ctrl.flags    = 0;
    blt.ctrl.alpha_en = 1; // Disable Blending

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
    if (g_buf1)
    {
        rt_free(g_buf1);
        g_buf1 = NULL;
    }
    if (g_buf2)
    {
        rt_free(g_buf2);
        g_buf2 = NULL;
    }
}

struct effect_ops effect_0017 = {
    .name   = "NO.17 THE QUANTUM SURFACE",
    .init   = effect_init,
    .draw   = effect_draw,
    .deinit = effect_deinit,
};

REGISTER_EFFECT(effect_0017);
