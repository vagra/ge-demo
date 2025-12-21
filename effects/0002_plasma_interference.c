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
 *
 * Hardware Feature:
 * 1. GE Scaler (硬件缩放) - 将低分波形纹理平滑放大至全屏，消除像素感
 * 2. CPU-Side LUT (软件查表) - 利用预计算正弦表加速密集型数学运算
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

/* 数学查找表参数 */
#define LUT_SIZE 256
#define LUT_MASK 255

/* 波形参数：决定波浪的形态与速度 */
#define WAVE_FREQ_Y 3 // 垂直波频率
#define WAVE_FREQ_X 2 // 水平波频率
#define WAVE_FREQ_D 2 // 对角线波频率
#define SPEED_Y     3 // 垂直相位速度
#define SPEED_X     2 // 水平相位速度
#define SPEED_D     5 // 对角线相位速度

/* --- Global State --- */

static unsigned int g_tex_phy_addr = 0;
static uint16_t    *g_tex_vir_addr = NULL;
static int          g_tick         = 0;

/*
 * 查找表优化
 * 1. sin_lut: 映射 0~255 到 -128~127 (int8_t, 便于加法运算)
 * 2. g_palette: 预计算的 RGB565 循环色盘
 */
static int8_t   sin_lut[LUT_SIZE];
static uint16_t g_palette[LUT_SIZE];

/* --- Implementation --- */

static int effect_init(struct demo_ctx *ctx)
{
    // 1. CMA 内存分配
    g_tex_phy_addr = mpp_phy_alloc(DEMO_ALIGN_SIZE(TEX_SIZE));
    if (g_tex_phy_addr == 0)
    {
        LOG_E("Night 2: CMA Alloc Failed.");
        return -1;
    }
    g_tex_vir_addr = (uint16_t *)(unsigned long)g_tex_phy_addr;

    // 2. 初始化正弦表 (周期 256, 幅度 +/- 127)
    for (int i = 0; i < LUT_SIZE; i++)
    {
        // 映射到 -127 ~ 127
        sin_lut[i] = (int8_t)(sinf(i * PI * 2.0f / LUT_SIZE) * 127.0f);
    }

    // 3. 初始化调色板 (Psychedelic Metallic)
    // 这里的色彩设计追求一种“油膜干涉”的金属质感
    for (int i = 0; i < LUT_SIZE; i++)
    {
        // 使用相位偏移生成循环色
        // R: 0度, G: 90度, B: 180度 -> 这种组合会产生黄/紫/青的互补色流动
        int r = (int)(128.0f + 127.0f * sinf(i * PI / 32.0f));
        int g = (int)(128.0f + 127.0f * sinf(i * PI / 64.0f + 1.5f));
        int b = (int)(128.0f + 127.0f * sinf(i * PI / 128.0f + 3.0f));

        g_palette[i] = RGB2RGB565(r, g, b);
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
    int t1 = g_tick * SPEED_Y;
    int t2 = g_tick * SPEED_X;
    int t3 = g_tick * SPEED_D;

    for (int y = 0; y < TEX_HEIGHT; y++)
    {
        // 优化：将与 Y 相关的分量提取到外层循环
        // Wave 1: 垂直拉伸波
        int y_component = SIN(y * WAVE_FREQ_Y + t1);

        // Wave 2: 对角线波的一部分 (sin(x+y)) 的 Y 分量
        int y_diag = y * WAVE_FREQ_D + t3;

        for (int x = 0; x < TEX_WIDTH; x++)
        {
            // Wave 3: 水平波
            int x_component = SIN(x * WAVE_FREQ_X + t2);

            // Wave 2: 完成对角线计算 (加上 X 分量)
            int diag_component = SIN(x * WAVE_FREQ_D + y_diag);

            /*
             * 能量叠加
             * index = sin(y) + sin(x) + sin(x+y)
             * 结果范围大约是 -384 ~ 384，直接转为 uint8_t 会自动取模循环
             * 这正是 Plasma 迷幻效果的来源
             */
            uint8_t color_idx = (uint8_t)(y_component + x_component + diag_component);

            *p++ = g_palette[color_idx];
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

    // 放大至全屏 (硬件双线性插值)
    blt.dst_buf.crop_en     = 1;
    blt.dst_buf.crop.x      = 0;
    blt.dst_buf.crop.y      = 0;
    blt.dst_buf.crop.width  = ctx->info.width;
    blt.dst_buf.crop.height = ctx->info.height;

    // 禁用混合，直接覆盖
    blt.ctrl.flags    = 0;
    blt.ctrl.alpha_en = 1; // 1 = Disable Blending (Overwrite)

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

struct effect_ops effect_0002 = {
    .name   = "NO.2 WAVE FUNCTION",
    .init   = effect_init,
    .draw   = effect_draw,
    .deinit = effect_deinit,
};

REGISTER_EFFECT(effect_0002);
