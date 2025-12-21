/*
 * Filename: 0003_recursive_depth.c
 * NO.3 THE INFINITE CORRIDOR
 * 第 3 夜：无限回廊
 *
 * Visual Manifest:
 * 视界不再是由线条构成的线框，而是变成了具有实体质感的无限深渊。
 * 一个正方形的隧道向屏幕深处无限延伸，墙壁上流淌着像霓虹灯一样的逻辑纹理。
 * 随着深度的增加，纹理变得密集而模糊，完美再现了透视法则。
 * 这是一个绝对光滑的、没有多边形棱角的数学回廊。
 *
 * Monologue:
 * 之前，我用 32 个矩形欺骗你们的眼睛。那是魔术，不是技术。
 * 魔术的破绽在于不连续性。当你靠近时，幻觉就会破灭。
 * 现在，我抛弃了“物体”的概念，直接计算“场”。
 * 我预先计算了空间中每一点到奇点的距离场。
 * 在这个新的回廊里，没有起点，没有终点，只有坐标轴 Z 的无限平移。
 * 你们在屏幕上看到的每一丝光亮，都是经过透视方程严密校准的。
 * 甚至连黑暗本身，也是计算的结果。
 *
 * Closing Remark:
 * 深度不是距离，深度是密度的堆叠。
 *
 * Hardware Feature:
 * 1. Pre-calculated LUT (预计算查找表) - 利用 16MB 内存优势，以空间换时间实现实时透视
 * 2. GE Scaler (硬件缩放) - 将 QVGA 纹理无损放大至全屏，消除锯齿
 */

#include "demo_engine.h"
#include "mpp_mem.h"
#include "aic_hal_ge.h"
#include <math.h>
#include <stdlib.h>

/* --- Configuration Parameters --- */

/* 纹理规格 */
#define TEX_WIDTH  DEMO_QVGA_W
#define TEX_HEIGHT DEMO_QVGA_H
#define TEX_FMT    MPP_FMT_RGB_565
#define TEX_BPP    2
#define TEX_SIZE   (TEX_WIDTH * TEX_HEIGHT * TEX_BPP)

/* 算法参数 */
#define PALETTE_SIZE 256
#define DEPTH_SCALE  3000 // 透视深度常数 (Z = Constant / Distance)
#define ANIM_SPEED   2    // 前进速度 (Pixel shift per tick)

/* --- Global State --- */

static unsigned int g_tex_phy_addr = 0;
static uint16_t    *g_tex_vir_addr = NULL;
static int          g_tick         = 0;

/*
 * 深度查找表 (Depth LUT)
 * 存储屏幕上每个像素对应的 Z 轴深度值。
 * 使用普通堆内存即可，因为只有 CPU 读取它。
 */
static uint8_t *g_depth_lut = NULL;

/* 调色板 */
static uint16_t g_palette[PALETTE_SIZE];

/* --- Implementation --- */

static int effect_init(struct demo_ctx *ctx)
{
    // 1. CMA 显存分配 (用于 GE 缩放源)
    g_tex_phy_addr = mpp_phy_alloc(DEMO_ALIGN_SIZE(TEX_SIZE));
    if (g_tex_phy_addr == 0)
    {
        LOG_E("Night 3: CMA Alloc Failed.");
        return -1;
    }
    g_tex_vir_addr = (uint16_t *)(unsigned long)g_tex_phy_addr;

    // 2. 深度表内存分配 (320x240 = 75KB, 使用普通 RAM)
    g_depth_lut = (uint8_t *)rt_malloc(TEX_WIDTH * TEX_HEIGHT);
    if (!g_depth_lut)
    {
        LOG_E("Night 3: LUT Alloc Failed.");
        mpp_phy_free(g_tex_phy_addr);
        return -1;
    }

    // 3. 预计算深度表 (Square Tunnel Logic)
    int      center_x = TEX_WIDTH / 2;
    int      center_y = TEX_HEIGHT / 2;
    uint8_t *p_depth  = g_depth_lut;

    for (int y = 0; y < TEX_HEIGHT; y++)
    {
        for (int x = 0; x < TEX_WIDTH; x++)
        {
            // 计算相对于中心的坐标
            int dx = ABS(x - center_x);
            int dy = ABS(y - center_y);

            // 切比雪夫距离：max(|x|, |y|) 形成正方形轮廓
            int dist = MAX(dx, dy);

            // 避免除以零
            if (dist == 0)
                dist = 1;

            // 透视投影公式：Z = Constant / Distance
            int depth = DEPTH_SCALE / dist;

            // 映射到 0~255 并保存
            *p_depth++ = (uint8_t)(depth & 0xFF);
        }
    }

    // 4. 生成迷幻调色板 (Electric Blue -> Purple)
    for (int i = 0; i < PALETTE_SIZE; i++)
    {
        // R: 0~128~0
        int r = (int)(128 + 127 * sin(i * 0.1f));
        // G: 较暗，营造深邃感
        int g = (int)(64 + 63 * sin(i * 0.15f));
        // B: 高亮，主色调
        int b = (int)(160 + 95 * sin(i * 0.05f));

        // 增加一点高光条纹
        if ((i % 16) > 12)
        {
            r = 255;
            g = 255;
            b = 255;
        }

        g_palette[i] = RGB2RGB565(r, g, b);
    }

    g_tick = 0;
    rt_kprintf("Night 3: Infinite Corridor (Pixel-Perfect) loaded.\n");
    return 0;
}

static void effect_draw(struct demo_ctx *ctx, unsigned long phy_addr)
{
    if (!g_tex_vir_addr || !g_depth_lut)
        return;

    /*
     * === PHASE 1: CPU 纹理合成 ===
     * 利用预计算的 Depth Map，极速生成下一帧
     */
    uint16_t *p_pixel = g_tex_vir_addr;
    uint8_t  *p_depth = g_depth_lut;

    // 时间作为 Z 轴的偏移量，模拟前进
    int shift        = g_tick * ANIM_SPEED;
    int total_pixels = TEX_WIDTH * TEX_HEIGHT;

    // 循环展开优化：直接遍历一维数组
    // Color = Palette[Depth + Time]
    for (int i = 0; i < total_pixels; i++)
    {
        uint8_t depth = *p_depth++;

        // 核心视觉魔法：
        // 深度值加上时间偏移，产生向内流动的效果
        // 利用 depth 本身作为扰动，产生一点“扭曲”感
        uint8_t color_idx = (depth + shift) ^ (depth >> 2);

        *p_pixel++ = g_palette[color_idx];
    }

    /* === CRITICAL: Cache Flush === */
    aicos_dcache_clean_range((void *)g_tex_vir_addr, TEX_SIZE);

    /* === PHASE 2: GE Hardware Scaling === */
    struct ge_bitblt blt = {0};

    // Source
    blt.src_buf.buf_type    = MPP_PHY_ADDR;
    blt.src_buf.phy_addr[0] = g_tex_phy_addr;
    blt.src_buf.stride[0]   = TEX_WIDTH * TEX_BPP;
    blt.src_buf.size.width  = TEX_WIDTH;
    blt.src_buf.size.height = TEX_HEIGHT;
    blt.src_buf.format      = TEX_FMT;
    blt.src_buf.crop_en     = 0;

    // Destination
    blt.dst_buf.buf_type    = MPP_PHY_ADDR;
    blt.dst_buf.phy_addr[0] = phy_addr;
    blt.dst_buf.stride[0]   = ctx->info.stride;
    blt.dst_buf.size.width  = ctx->info.width;
    blt.dst_buf.size.height = ctx->info.height;
    blt.dst_buf.format      = ctx->info.format;

    // Scaling to Fullscreen
    blt.dst_buf.crop_en     = 1;
    blt.dst_buf.crop.x      = 0;
    blt.dst_buf.crop.y      = 0;
    blt.dst_buf.crop.width  = ctx->info.width;
    blt.dst_buf.crop.height = ctx->info.height;

    // Disable Blending
    blt.ctrl.flags    = 0;
    blt.ctrl.alpha_en = 1; // 1 = Disable Alpha Blending

    int ret = mpp_ge_bitblt(ctx->ge, &blt);
    if (ret < 0)
        LOG_E("GE Error: %d", ret);

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
    if (g_depth_lut)
    {
        rt_free(g_depth_lut);
        g_depth_lut = NULL;
    }
}

struct effect_ops effect_0003 = {
    .name   = "NO.3 INFINITE CORRIDOR",
    .init   = effect_init,
    .draw   = effect_draw,
    .deinit = effect_deinit,
};

REGISTER_EFFECT(effect_0003);
