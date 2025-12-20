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
 */

#include "demo_engine.h"
#include "mpp_mem.h"
#include "aic_hal_ge.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/*
 * === 混合渲染架构 (Water Ripple Simulation) ===
 * 1. 纹理: 320x240 RGB565 (150KB)
 * 2. 核心: 2D Wave Equation (Heightmap Simulation)
 *    我们需要两个高度场缓冲区 (Buffer1, Buffer2)，每帧交换。
 *    每个像素的高度值取决于周围 4 个像素的高度和上一帧的高度。
 *    这是 Demoscene 中最经典的 "Water Effect"。
 */
#define TEX_W    320
#define TEX_H    240
#define TEX_SIZE (TEX_W * TEX_H * 2)

/* 高度场缓冲区大小 (int16_t 以支持负波谷) */
#define MAP_SIZE (TEX_W * TEX_H * sizeof(int16_t))

static unsigned int g_tex_phy_addr = 0;
static uint16_t    *g_tex_vir_addr = NULL;
static int          g_tick         = 0;

/* 两个高度图：当前帧和上一帧 */
static int16_t *g_buf1 = NULL;
static int16_t *g_buf2 = NULL;

/* 预计算调色板 (根据高度映射颜色) */
static uint16_t palette[256];

static int effect_init(struct demo_ctx *ctx)
{
    // 1. CMA 显存
    g_tex_phy_addr = mpp_phy_alloc(TEX_SIZE);
    if (g_tex_phy_addr == 0)
        return -1;
    g_tex_vir_addr = (uint16_t *)(unsigned long)g_tex_phy_addr;

    // 2. 高度图内存 (普通 RAM)
    g_buf1 = (int16_t *)rt_malloc(MAP_SIZE);
    g_buf2 = (int16_t *)rt_malloc(MAP_SIZE);

    if (!g_buf1 || !g_buf2)
    {
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

        // 高度值 i: 0 (深) ~ 255 (高)
        // 我们希望波峰(高)是亮的，波谷(低)是暗的
        // 平静水面 (128) 应该是中性色

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

        palette[i] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
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

    // 交换指针
    if (g_tick % 2 == 0)
    {
        curr = g_buf2;
        prev = g_buf1;
    }

    // 1. 制造扰动 (Raindrops)
    // 每隔几帧随机滴落水滴
    if (g_tick % 4 == 0)
    {
        int rx = (rand() % (TEX_W - 4)) + 2;
        int ry = (rand() % (TEX_H - 4)) + 2;
        // 激起波浪：设置一个较高的负值或正值
        // 500 是能量强度
        prev[ry * TEX_W + rx] = 1000;
    }

    // 也可以加入一个移动的扰动源 (像手指划过水面)
    int tx = (TEX_W / 2) + (int)(sinf(g_tick * 0.05f) * 100.0f);
    int ty = (TEX_H / 2) + (int)(cosf(g_tick * 0.03f) * 80.0f);
    // 边界检查
    if (tx >= 2 && tx < TEX_W - 2 && ty >= 2 && ty < TEX_H - 2)
    {
        prev[ty * TEX_W + tx] = 1000;
    }

    // 2. 波传播算法 (Wave Propagation)
    // 这种算法非常适合 CPU，只有加减移位
    // Val = (Left + Right + Up + Down) / 2 - Val_Prev
    // Val -= Val >> 5 (Damping/阻尼，防止能量无限震荡)

    // 优化：避免边界检查，直接从 1 循环到 W-1
    for (int y = 1; y < TEX_H - 1; y++)
    {
        // 预计算行指针
        int16_t *p_curr = &curr[y * TEX_W + 1];
        int16_t *p_prev = &prev[y * TEX_W + 1]; // 这里的 prev 其实是“再上一帧”的数据，用于计算

        // 上下行的指针
        int16_t *p_up   = &prev[(y - 1) * TEX_W + 1];
        int16_t *p_down = &prev[(y + 1) * TEX_W + 1];

        for (int x = 1; x < TEX_W - 1; x++)
        {
            // 核心公式
            // 取周围 4 个点的平均值 (用移位代替除法)
            // p_prev[x-1] 是左，p_prev[x+1] 是右
            int16_t val = (p_up[x] + p_down[x] + p_prev[x - 1] + p_prev[x + 1]) >> 1;

            // 减去当前位置上一时刻的值 (惯性)
            val -= p_curr[x]; // 注意：因为 buffer 交换了，p_curr 此时存的是“再上一帧”的值

            // 阻尼衰减 (Damping)
            // val -= val >> 5; // 相当于 val = val * 31 / 32
            val -= (val >> 5);

            // 写回
            p_curr[x] = val;
        }
    }

    /*
     * === PHASE 2: 渲染 (Rendering) ===
     * 将高度图映射为颜色
     */
    uint16_t *p_pixel  = g_tex_vir_addr;
    int16_t  *p_height = curr; // 使用最新的高度图

    // 跳过第一行和最后一行
    p_pixel += TEX_W;
    p_height += TEX_W;

    for (int i = TEX_W; i < TEX_W * (TEX_H - 1); i++)
    {
        int16_t val = *p_height++;

        // 简单的折射模拟：计算高度差 (斜率)
        // 这里的 val 范围大约是 -1000 ~ +1000
        // 我们将其压缩到 0~255

        int idx = 128 + (val >> 2); // 偏移 128 作为海平面

        // 夹紧
        if (idx < 0)
            idx = 0;
        if (idx > 255)
            idx = 255;

        *p_pixel++ = palette[idx];
    }

    /* === CRITICAL: Cache Flush === */
    aicos_dcache_clean_range((void *)g_tex_vir_addr, TEX_SIZE);

    /* === PHASE 3: GE Hardware Scaling === */
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
    if (g_buf1)
        rt_free(g_buf1);
    if (g_buf2)
        rt_free(g_buf2);
    g_buf1 = NULL;
    g_buf2 = NULL;
}

struct effect_ops effect_0017 = {
    .name   = "NO.17 THE QUANTUM SURFACE",
    .init   = effect_init,
    .draw   = effect_draw,
    .deinit = effect_deinit,
};

REGISTER_EFFECT(effect_0017);
