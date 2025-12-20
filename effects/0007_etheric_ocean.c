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
 */

#include "demo_engine.h"
#include "mpp_mem.h"
#include "aic_hal_ge.h"
#include <math.h>

/*
 * === 混合渲染架构 (Full-Screen Focus) ===
 * 1. 纹理: 320x240 RGB565 (150KB CMA Memory)
 * 2. 算法: Old School Plasma (四重正弦叠加)
 *    为了保证全屏无死角，我们计算每一个像素的能量值。
 */
#define TEX_W    320
#define TEX_H    240
#define TEX_SIZE (TEX_W * TEX_H * 2)

static unsigned int g_tex_phy_addr = 0;
static uint16_t    *g_tex_vir_addr = NULL;
static int          g_tick         = 0;

/*
 * 两个查找表：
 * 1. sin_lut: 用于波形计算
 * 2. palette_lut: 用于将波形能量映射为绚丽的颜色 (256色 -> RGB565)
 */
static int      sin_lut[512];
static uint16_t palette_lut[256];

static int effect_init(struct demo_ctx *ctx)
{
    // 1. CMA 内存分配
    g_tex_phy_addr = mpp_phy_alloc(TEX_SIZE);
    if (g_tex_phy_addr == 0)
    {
        return -1;
    }
    g_tex_vir_addr = (uint16_t *)(unsigned long)g_tex_phy_addr;

    // 2. 初始化正弦表 (Q8 定点数, 256 = 1.0)
    // 这种精度对于 Plasma 这种模糊效果足够了，且运算更快
    for (int i = 0; i < 512; i++)
    {
        sin_lut[i] = (int)(sinf(i * 3.14159f / 256.0f) * 256.0f);
    }

    // 3. 初始化调色板 (Psychedelic Colors)
    // 生成一条连续的、高饱和度的色带
    for (int i = 0; i < 256; i++)
    {
        // 利用正弦波生成平滑循环的 RGB
        // 偏移量 0, 85, 170 对应 0, 120, 240 度相位差
        int r = (int)(128.0f + 127.0f * sinf(i * 3.14159f / 32.0f));
        int g = (int)(128.0f + 127.0f * sinf(i * 3.14159f / 64.0f + 2.0f));
        int b = (int)(128.0f + 127.0f * sinf(i * 3.14159f / 128.0f + 4.0f));

        // 转换为 RGB565
        palette_lut[i] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
    }

    g_tick = 0;
    rt_kprintf("Night 7: Etheric Ocean (Plasma) started.\n");
    return 0;
}

// 快速查表宏
#define GET_SIN(idx) (sin_lut[(idx) & 511])

static void effect_draw(struct demo_ctx *ctx, unsigned long phy_addr)
{
    if (!g_tex_vir_addr)
        return;

    /* === PHASE 1: CPU Plasma Calculation === */

    // 动态相位参数，让波浪动起来
    int t1 = g_tick * 3;
    int t2 = g_tick * 5;
    int t3 = g_tick * 2;
    int t4 = g_tick * 7;

    uint16_t *p_pixel = g_tex_vir_addr;

    for (int y = 0; y < TEX_H; y++)
    {

        // 优化：将与 Y 相关的计算提出来
        // Wave 1: 纵向波
        int v1 = GET_SIN(y * 3 + t1);
        // Wave 2: 另一种纵向拉伸
        int v2 = GET_SIN(y * 2 + t3);

        for (int x = 0; x < TEX_W; x++)
        {

            // Wave 3: 横向波
            int v3 = GET_SIN(x * 3 + t2);
            // Wave 4: 对角线波 (x+y)
            int v4 = GET_SIN((x + y) * 2 + t4);

            /*
             * 能量合成公式：
             * 将四个维度的波叠加。
             * 结果范围大约是 -1024 ~ +1024。
             * 我们需要将其映射到 0~255 的调色板索引。
             */
            int energy = v1 + v2 + v3 + v4;

            // 归一化并取模
            // (energy + 1024) >> 3 大约将 2048 的范围压缩到 256
            uint8_t color_idx = (uint8_t)((energy + 1024) >> 3);

            // 查表写入
            *p_pixel++ = palette_lut[color_idx];
        }
    }

    /* === CRITICAL: Cache Flush === */
    aicos_dcache_clean_range((void *)g_tex_vir_addr, TEX_SIZE);

    /* === PHASE 2: GE Hardware Scaling === */
    struct ge_bitblt blt = {0};

    // Source (320x240 RGB565)
    blt.src_buf.buf_type    = MPP_PHY_ADDR;
    blt.src_buf.phy_addr[0] = g_tex_phy_addr;
    blt.src_buf.stride[0]   = TEX_W * 2;
    blt.src_buf.size.width  = TEX_W;
    blt.src_buf.size.height = TEX_H;
    blt.src_buf.format      = MPP_FMT_RGB_565;
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

struct effect_ops effect_0007 = {
    .name   = "NO.7 THE ETHERIC OCEAN",
    .init   = effect_init,
    .draw   = effect_draw,
    .deinit = effect_deinit,
};

REGISTER_EFFECT(effect_0007);
