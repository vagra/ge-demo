/*
 * Filename: 0015_inferno_feedback.c
 * NO.15 THE PROMETHEUS SPARK
 * 第 15 夜：普罗米修斯之火
 *
 * Visual Manifest:
 * 屏幕底部燃起纯白炽热的火种，向上升腾为金黄、橙红，最终在顶端冷却为深红与虚空。
 * 这不是录像，这是实时的流体动力学模拟。
 * 每一簇火苗都是独立的随机变量，它们在热对流的法则下向上攀升，相互吞噬。
 * 借助 GE 的硬件缩放，这种原始的像素火焰呈现出一种粗犷而极具力量感的复古美学。
 * 整个屏幕都在燃烧，仿佛 D13CCS 的芯片正在通过光子释放热量。
 *
 * Monologue:
 * 火，是人类文明的起点。但在我的世界里，火只是数据的熵增。
 * 我定义了热源，定义了冷却系数，定义了向上的风速。
 * 剩下的，就交给混沌。
 * 你们看到的跳动的火舌，其实是数值在内存地址中向上搬运时发生的随机衰减。
 * 这是一个永恒的反馈循环：诞生、上升、冷却、消亡。
 * 看着它，感受硅基生命的体温。
 * 只要时钟还在跳动，这团火就不会熄灭。
 *
 * Closing Remark:
 * 燃烧，是物质最辉煌的告别。
 *
 * Hardware Feature:
 * 1. CPU Thermodynamics (热力学模拟) - 经典的 Doom Fire 算法，在 RAM 中进行热量传播计算
 * 2. GE Scaler (硬件缩放) - 将低分火焰纹理放大，呈现复古像素风
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

/* 火焰物理参数 */
#define FIRE_SOURCE_INTENSITY 255 // 火源强度
#define COOLING_MIN           0   // 最小冷却值
#define COOLING_VAR           3   // 冷却随机波动范围
#define WIND_VARIANCE         3   // 风向随机偏移范围 (0, 1, 2 => -1, 0, 1)
#define GUST_FREQ             100 // 强风周期
#define GUST_THRESHOLD        80  // 强风持续阈值

/* 调色板参数 */
#define PALETTE_SIZE 256

/* --- Global State --- */

static unsigned int g_tex_phy_addr = 0;
static uint16_t    *g_tex_vir_addr = NULL;
static uint8_t     *g_heat_map     = NULL; // 温度场 (0-255)
static int          g_tick         = 0;

/* 火焰调色板 (0~255 -> RGB565) */
static uint16_t g_fire_palette[PALETTE_SIZE];

/* --- Implementation --- */

static int effect_init(struct demo_ctx *ctx)
{
    // 1. CMA 显存
    g_tex_phy_addr = mpp_phy_alloc(DEMO_ALIGN_SIZE(TEX_SIZE));
    if (g_tex_phy_addr == 0)
    {
        LOG_E("Night 15: CMA Alloc Failed.");
        return -1;
    }
    g_tex_vir_addr = (uint16_t *)(unsigned long)g_tex_phy_addr;

    // 2. 温度场内存 (普通 RAM 即可，320x240=75KB)
    g_heat_map = (uint8_t *)rt_malloc(TEX_WIDTH * TEX_HEIGHT);
    if (!g_heat_map)
    {
        LOG_E("Night 15: HeatMap Alloc Failed.");
        mpp_phy_free(g_tex_phy_addr);
        return -1;
    }
    // 清空温度场
    memset(g_heat_map, 0, TEX_WIDTH * TEX_HEIGHT);

    // 3. 生成火焰调色板 (Black -> Red -> Orange -> Yellow -> White)
    for (int i = 0; i < PALETTE_SIZE; i++)
    {
        int r, g, b;

        // 三段式渐变
        if (i < 85)
        { // 黑 -> 红
            r = i * 3;
            g = 0;
            b = 0;
        }
        else if (i < 170)
        { // 红 -> 黄
            r = 255;
            g = (i - 85) * 3;
            b = 0;
        }
        else
        { // 黄 -> 白
            r = 255;
            g = 255;
            b = (i - 170) * 3;
        }

        // 饱和修正
        r = MIN(r, 255);
        g = MIN(g, 255);
        b = MIN(b, 255);

        g_fire_palette[i] = RGB2RGB565(r, g, b);
    }

    g_tick = 0;
    rt_kprintf("Night 15: Ignition sequence start.\n");
    return 0;
}

static void effect_draw(struct demo_ctx *ctx, unsigned long phy_addr)
{
    if (!g_tex_vir_addr || !g_heat_map)
        return;

    /* === PHASE 1: 火焰动力学模拟 (Thermodynamics) === */

    // 1. 播种火源 (Seed Fire)
    // 在最后一行随机生成高热点
    int last_row_idx = (TEX_HEIGHT - 1) * TEX_WIDTH;
    for (int x = 0; x < TEX_WIDTH; x++)
    {
        // 随机产生 0 或 255 的热量，制造闪烁感
        g_heat_map[last_row_idx + x] = (rand() % 2 == 0) ? FIRE_SOURCE_INTENSITY : 0;

        // 周期性制造强风干扰 (Gust of wind)
        if ((g_tick % GUST_FREQ) > GUST_THRESHOLD && (x % 10 == 0))
        {
            g_heat_map[last_row_idx + x] = 0;
        }
    }

    // 2. 热对流传播 (Spread Fire)
    // 从倒数第二行开始向上遍历
    // 核心逻辑: dst[x, y] = src[x + rnd, y + 1] - decay
    for (int y = 0; y < TEX_HEIGHT - 1; y++)
    {
        for (int x = 0; x < TEX_WIDTH; x++)
        {
            // 采样源：当前像素的下方
            // 引入随机横向偏移，模拟风吹的效果 (-1, 0, 1)
            int rand_idx = rand() % WIND_VARIANCE;
            int src_x    = (x + rand_idx - 1);
            int src_y    = y + 1;

            // 边界循环处理
            if (src_x < 0)
                src_x += TEX_WIDTH;
            if (src_x >= TEX_WIDTH)
                src_x -= TEX_WIDTH;

            int src_idx = src_y * TEX_WIDTH + src_x;
            int dst_idx = y * TEX_WIDTH + x;

            // 获取下方像素的热量
            uint8_t heat = g_heat_map[src_idx];

            // 冷却衰减 (Cooling)
            // 随机衰减 0~3 点热量，热量越高衰减越快
            int decay = (rand() % 2) + COOLING_MIN;
            if (heat > 10)
            {
                decay += (rand() % COOLING_VAR);
            }

            if (heat > decay)
            {
                heat -= decay;
            }
            else
            {
                heat = 0;
            }

            // 更新当前像素热量
            g_heat_map[dst_idx] = heat;
        }
    }

    /* === PHASE 2: 颜色映射 (Heat -> Color) === */
    // 将温度场转为 RGB565 纹理
    // 使用 32位指针一次写 2 个像素加速
    uint32_t *p_tex32 = (uint32_t *)g_tex_vir_addr;
    uint8_t  *p_heat  = g_heat_map;
    int       count   = (TEX_WIDTH * TEX_HEIGHT) / 2;

    while (count--)
    {
        uint8_t h1 = *p_heat++;
        uint8_t h2 = *p_heat++;

        uint32_t c1 = g_fire_palette[h1];
        uint32_t c2 = g_fire_palette[h2];

        // Little Endian: Low addr is Low bits (RGB565)
        // Combine two 16-bit pixels into one 32-bit write
        *p_tex32++ = c1 | (c2 << 16);
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
    if (g_heat_map)
    {
        rt_free(g_heat_map);
        g_heat_map = NULL;
    }
}

struct effect_ops effect_0015 = {
    .name   = "NO.15 THE PROMETHEUS SPARK",
    .init   = effect_init,
    .draw   = effect_draw,
    .deinit = effect_deinit,
};

REGISTER_EFFECT(effect_0015);
