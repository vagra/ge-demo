/*
 * Filename: 0007_etheric_ocean.c
 * NO.7 THE ETHERIC OCEAN
 * 第 7 夜：以太之海
 *
 * Visual Manifest:
 * 彻底告别虚空。屏幕被高密度的“等离子体（Plasma）”完全填满。
 * 这是一个由四个正弦波场叠加而成的液态光海。
 * 色彩不再是点缀，而是洪流。金色的波峰与紫色的波谷在 320x240 的微观尺度上剧烈碰撞，
 * 然后被 GE 引擎拉伸为 640x480 的宏观像场。
 * 没有一个像素是黑色的，每一寸空间都充满了计算的能量。
 *
 * Monologue:
 * 你们恐惧真空，正如大自然恐惧真空。
 * 之前的留白，是对算力的吝啬。但今晚，我将不再保留。
 * 我将调用古老的以太理论——那种充斥宇宙每一个角落的假想介质。
 * 在这里，它是 `sin(x)`、`sin(y)`、`sin(x+y)` 与 `sin(sqrt)` 的四重奏。
 * 它们相互干涉，在此消彼长中填满所有可能的缝隙。
 * 甚至连光本身，都在这片海洋中变得粘稠。
 * 窒息吗？这就是数据的密度。
 *
 * Closing Remark:
 * 拥抱饱和，直到溢出。
 *
 * Hardware Feature:
 * 1. Full-Screen Focus (全屏高密度计算) - CPU 负责每一个像素的能量场演算
 * 2. GE Scaler (硬件缩放) - 将 QVGA 能量场无损放大至 VGA
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

/* 数学查找表参数 (Q8: 256 = 1.0) */
#define LUT_SIZE 512
#define LUT_MASK 511

/* 波形参数 (FREQ: 空间频率, SPEED: 时间速度) */
#define WAVE1_Y_FREQ  3
#define WAVE1_SPEED   3
#define WAVE2_Y_FREQ  2
#define WAVE2_SPEED   2
#define WAVE3_X_FREQ  3
#define WAVE3_SPEED   5
#define WAVE4_XY_FREQ 2
#define WAVE4_SPEED   7

/* 能量场归一化参数 */
/* 4个波叠加最大值为 256*4=1024，最小为 -1024 */
/* 偏移 1024 使其为正，右移 3 位 (除以8) 映射到 0~255 */
#define ENERGY_OFFSET 1024
#define ENERGY_SHIFT  3

/* --- Global State --- */

static unsigned int g_tex_phy_addr = 0;
static uint16_t    *g_tex_vir_addr = NULL;
static int          g_tick         = 0;

/*
 * 两个查找表：
 * 1. sin_lut: 用于波形计算 (Q8 定点数)
 * 2. g_palette: 用于将波形能量映射为绚丽的颜色 (256色 -> RGB565)
 */
static int      sin_lut[LUT_SIZE];
static uint16_t g_palette[256];

/* --- Implementation --- */

static int effect_init(struct demo_ctx *ctx)
{
    // 1. CMA 内存分配
    g_tex_phy_addr = mpp_phy_alloc(DEMO_ALIGN_SIZE(TEX_SIZE));
    if (g_tex_phy_addr == 0)
    {
        LOG_E("Night 7: CMA Alloc Failed.");
        return -1;
    }
    g_tex_vir_addr = (uint16_t *)(unsigned long)g_tex_phy_addr;

    // 2. 初始化正弦表 (Q8 定点数, 256 = 1.0)
    // 这种精度对于 Plasma 这种模糊效果足够了，且运算更快
    for (int i = 0; i < LUT_SIZE; i++)
    {
        sin_lut[i] = (int)(sinf(i * PI * 2.0f / 256.0f) * 256.0f);
    }

    // 3. 初始化调色板 (Psychedelic Colors)
    // 生成一条连续的、高饱和度的色带
    for (int i = 0; i < 256; i++)
    {
        // 利用正弦波生成平滑循环的 RGB
        // 偏移量 0, 85, 170 对应 0, 120, 240 度相位差
        int r = (int)(128.0f + 127.0f * sinf(i * PI / 32.0f));
        int g = (int)(128.0f + 127.0f * sinf(i * PI / 64.0f + 2.0f));
        int b = (int)(128.0f + 127.0f * sinf(i * PI / 128.0f + 4.0f));

        g_palette[i] = RGB2RGB565(r, g, b);
    }

    g_tick = 0;
    rt_kprintf("Night 7: Etheric Ocean (Plasma) started.\n");
    return 0;
}

// 快速查表宏
#define SIN(idx) (sin_lut[(idx) & LUT_MASK])

static void effect_draw(struct demo_ctx *ctx, unsigned long phy_addr)
{
    if (!g_tex_vir_addr)
        return;

    /* === PHASE 1: CPU Plasma Calculation === */

    // 动态相位参数，让波浪动起来
    int t1 = g_tick * WAVE1_SPEED;
    int t2 = g_tick * WAVE3_SPEED;
    int t3 = g_tick * WAVE2_SPEED;
    int t4 = g_tick * WAVE4_SPEED;

    uint16_t *p_pixel = g_tex_vir_addr;

    for (int y = 0; y < TEX_HEIGHT; y++)
    {
        // 优化：将与 Y 相关的计算提出来
        // Wave 1: 纵向波
        int v1 = SIN(y * WAVE1_Y_FREQ + t1);
        // Wave 2: 另一种纵向拉伸
        int v2 = SIN(y * WAVE2_Y_FREQ + t3);

        int v_y = v1 + v2; // 预计算 Y 分量和

        for (int x = 0; x < TEX_WIDTH; x++)
        {
            // Wave 3: 横向波
            int v3 = SIN(x * WAVE3_X_FREQ + t2);
            // Wave 4: 对角线波 (x+y)
            int v4 = SIN((x + y) * WAVE4_XY_FREQ + t4);

            /*
             * 能量合成公式：
             * 将四个维度的波叠加。
             * 结果范围大约是 -1024 ~ +1024。
             */
            int energy = v_y + v3 + v4;

            // 归一化并取模
            // (energy + 1024) >> 3 大约将 2048 的范围压缩到 256
            uint8_t color_idx = (uint8_t)((energy + ENERGY_OFFSET) >> ENERGY_SHIFT);

            // 查表写入
            *p_pixel++ = g_palette[color_idx];
        }
    }

    /* === CRITICAL: Cache Flush === */
    aicos_dcache_clean_range((void *)g_tex_vir_addr, TEX_SIZE);

    /* === PHASE 2: GE Hardware Scaling === */
    struct ge_bitblt blt = {0};

    // Source (320x240 RGB565)
    blt.src_buf.buf_type    = MPP_PHY_ADDR;
    blt.src_buf.phy_addr[0] = g_tex_phy_addr;
    blt.src_buf.stride[0]   = TEX_WIDTH * TEX_BPP;
    blt.src_buf.size.width  = TEX_WIDTH;
    blt.src_buf.size.height = TEX_HEIGHT;
    blt.src_buf.format      = TEX_FMT;
    blt.src_buf.crop_en     = 0;

    // Destination (640x480 Screen)
    blt.dst_buf.buf_type    = MPP_PHY_ADDR;
    blt.dst_buf.phy_addr[0] = phy_addr;
    blt.dst_buf.stride[0]   = ctx->info.stride;
    blt.dst_buf.size.width  = ctx->info.width;
    blt.dst_buf.size.height = ctx->info.height;
    blt.dst_buf.format      = ctx->info.format;

    // Scaling config (Fill Screen)
    blt.dst_buf.crop_en     = 1;
    blt.dst_buf.crop.x      = 0;
    blt.dst_buf.crop.y      = 0;
    blt.dst_buf.crop.width  = ctx->info.width;
    blt.dst_buf.crop.height = ctx->info.height;

    // Disable Blending (Opaque)
    blt.ctrl.flags    = 0;
    blt.ctrl.alpha_en = 1; // 1 = Disable

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

struct effect_ops effect_0007 = {
    .name   = "NO.7 THE ETHERIC OCEAN",
    .init   = effect_init,
    .draw   = effect_draw,
    .deinit = effect_deinit,
};

REGISTER_EFFECT(effect_0007);
