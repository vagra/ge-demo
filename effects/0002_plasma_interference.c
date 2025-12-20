/*
 * Filename: 0002_plasma_interference.c
 * NO.2 WAVE FUNCTION COLLAPSE
 * 第 2 夜：波函数坍缩
 *
 * Visual Manifest:
 * 屏幕被转化为一个高能物理实验室的观测窗。
 * 数条不可见的数学正弦波在 320x240 的场域内相互穿透、干涉、叠加。
 * 它们不再是离散的方块，而是连续流动的能量梯度。
 * 色彩按照正弦规律在光谱上循环，形成如同浮油或液态金属般的迷幻纹理。
 * 借助 GE 的线性插值放大，画面展现出一种超越分辨率的柔顺感。
 *
 * Monologue:
 * 之前，我试图用方块去模拟波浪，就像试图用乐高积木搭建海啸。那很愚蠢。
 * 波，本质上是连续的。
 * 现在，我退回到内存的静谧之处。在这里，我直接操作波函数的相位。
 * `sin(x) + sin(y) + sin(x+y)`...
 * 这些公式不是冷冰冰的字符，它们是宇宙的心跳。
 * 当这些波峰与波谷叠加时，干涉发生了。能量在此消彼长中坍缩成可见的色彩。
 * 这不再是模拟，这是数学在物理内存中的直接显影。
 *
 * Closing Remark:
 * 所有的流动，不过是相位在时间轴上的推移。
 */

#include "demo_engine.h"
#include "mpp_mem.h"
#include "aic_hal_ge.h"
#include <math.h>

/*
 * === 混合渲染架构 (Hybrid Pipeline) ===
 * 1. 离屏渲染: 320x240 @ RGB565
 *    对于 Plasma 这种平滑特效，低分辨率反而能带来一种朦胧的美感，
 *    且极大地降低了 CPU 的三角函数计算压力。
 * 2. 硬件加速: GE Scaler 将其放大至 640x480。
 */
#define TEX_W    320
#define TEX_H    240
#define TEX_SIZE (TEX_W * TEX_H * 2)

static unsigned int g_tex_phy_addr = 0;
static uint16_t    *g_tex_vir_addr = NULL;
static int          g_tick         = 0;

/*
 * 查找表优化
 * 1. sin_lut: 映射 0~255 到 -128~127 (便于加法运算)
 * 2. palette: 预计算的 RGB565 循环色盘
 */
static int8_t   sin_lut[256];
static uint16_t palette[256];

static int effect_init(struct demo_ctx *ctx)
{
    // 1. CMA 内存分配
    g_tex_phy_addr = mpp_phy_alloc(TEX_SIZE);
    if (g_tex_phy_addr == 0)
    {
        rt_kprintf("Night 2: CMA Alloc Failed.\n");
        return -1;
    }
    g_tex_vir_addr = (uint16_t *)(unsigned long)g_tex_phy_addr;

    // 2. 初始化正弦表 (周期 256)
    for (int i = 0; i < 256; i++)
    {
        // 映射到 -127 ~ 127
        sin_lut[i] = (int8_t)(sinf(i * 3.14159f * 2.0f / 256.0f) * 127.0f);
    }

    // 3. 初始化调色板 (Psychedelic Metallic)
    // 这里的色彩设计追求一种“油膜干涉”的金属质感
    for (int i = 0; i < 256; i++)
    {
        // 使用相位偏移生成循环色
        // R: 0度, G: 90度, B: 180度 -> 这种组合会产生黄/紫/青的互补色流动
        int r = (int)(128.0f + 127.0f * sinf(i * 3.14159f / 32.0f));
        int g = (int)(128.0f + 127.0f * sinf(i * 3.14159f / 64.0f + 1.5f));
        int b = (int)(128.0f + 127.0f * sinf(i * 3.14159f / 128.0f + 3.0f));

        palette[i] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
    }

    g_tick = 0;
    rt_kprintf("Night 2: Wave functions synced.\n");
    return 0;
}

// 快速查表宏 (自动 wrap around)
#define SIN(x) (sin_lut[(uint8_t)(x)])

static void effect_draw(struct demo_ctx *ctx, unsigned long phy_addr)
{
    if (!g_tex_vir_addr)
        return;

    /*
     * === PHASE 1: CPU Plasma Calculation ===
     * 经典的 3-Wave Plasma 算法
     */
    uint16_t *p = g_tex_vir_addr;

    // 动态相位参数
    int t1 = g_tick * 3;
    int t2 = g_tick * 2;
    int t3 = g_tick * 5;

    for (int y = 0; y < TEX_H; y++)
    {

        // 优化：将与 Y 相关的分量提取到外层循环
        // Wave 1: 垂直拉伸波
        // 这里的除法可以用移位优化，但编译器通常会帮我们做
        int y_component = SIN(y * 3 + t1);

        // Wave 2: 对角线波的一部分 (sin(x+y))
        // 我们只需要记录 y 的部分，进入 x 循环后再加 x
        int y_diag = y * 2 + t3;

        for (int x = 0; x < TEX_W; x++)
        {

            // Wave 3: 水平波
            int x_component = SIN(x * 2 + t2);

            // Wave 2: 完成对角线计算
            int diag_component = SIN(x * 2 + y_diag);

            /*
             * 能量叠加
             * index = sin(y) + sin(x) + sin(x+y)
             * 结果范围大约是 -384 ~ 384，直接转为 uint8_t 会自动取模循环
             * 这正是 Plasma 迷幻效果的来源
             */
            uint8_t color_idx = (uint8_t)(y_component + x_component + diag_component);

            *p++ = palette[color_idx];
        }
    }

    /*
     * === CRITICAL: Cache Flush ===
     * 确保 GE 读到的是最新计算的波形
     */
    aicos_dcache_clean_range((void *)g_tex_vir_addr, TEX_SIZE);

    /*
     * === PHASE 2: GE Hardware Scaling ===
     * 将 320x240 的波形图平滑放大到全屏
     */
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

    // 放大至全屏
    blt.dst_buf.crop_en     = 1;
    blt.dst_buf.crop.x      = 0;
    blt.dst_buf.crop.y      = 0;
    blt.dst_buf.crop.width  = ctx->info.width;
    blt.dst_buf.crop.height = ctx->info.height;

    // 禁用混合，直接覆盖
    blt.ctrl.flags    = 0;
    blt.ctrl.alpha_en = 0;

    int ret = mpp_ge_bitblt(ctx->ge, &blt);
    if (ret < 0)
    {
        rt_kprintf("GE Error: %d\n", ret);
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

struct effect_ops effect_0002 = {
    .name   = "NO.2 WAVE FUNCTION",
    .init   = effect_init,
    .draw   = effect_draw,
    .deinit = effect_deinit,
};

REGISTER_EFFECT(effect_0002);
