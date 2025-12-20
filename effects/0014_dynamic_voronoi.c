/*
 * Filename: 0014_dynamic_voronoi.c
 * NO.14 THE CRYSTALLINE CELL
 * 第 14 夜：晶格细胞
 *
 * Visual Manifest:
 * 屏幕被切割成无数个多边形晶胞，仿佛显微镜下的生物组织切片，又像是未来都市的动态规划图。
 * 数个看不见的核心在屏幕上游走，它们的影响力范围不断挤压、吞噬彼此。
 * 我们不仅渲染了区域，更渲染了“边界”——那是第一最近邻与第二最近邻势均力敌的地方。
 * 亮白色的光线在晶格的缝隙中游走，勾勒出数学上完美的分割线。
 *
 * Monologue:
 * 空间本无界限，是引力定义了归属。
 * 我投下了 16 个游荡的灵魂（Seeds）。
 * 对于屏幕上的每一个像素，这都是一场关于忠诚的拷问：谁离你最近？
 * 这种简单的邻近法则，自发地将虚空切割成了完美的晶体结构。
 * 看着那些发光的边缘，那是两个力场达到平衡的瞬间。
 * 所谓的边界，不过是两种势力势均力敌的停火线。
 *
 * Closing Remark:
 * 定义你的核心，世界自然会为你留出位置。
 */

#include "demo_engine.h"
#include "mpp_mem.h"
#include "aic_hal_ge.h"
#include <math.h>
#include <stdlib.h>

/*
 * === 混合渲染架构 (Cellular Noise) ===
 * 1. 纹理: 320x240 RGB565
 * 2. 核心: Dynamic Voronoi / Worley Noise
 *    对于每个像素，计算到 N 个特征点的距离。
 *    为了性能，我们使用曼哈顿距离 (Manhattan Distance: |dx|+|dy|)，
 *    它比欧几里得距离更快，且能产生独特的菱形/科技感纹理。
 */
#define TEX_W    320
#define TEX_H    240
#define TEX_SIZE (TEX_W * TEX_H * 2)

/* 种子点数量 */
#define SEED_COUNT 12

typedef struct
{
    int x, y;
    int vx, vy;
} Seed;

static unsigned int g_tex_phy_addr = 0;
static uint16_t    *g_tex_vir_addr = NULL;
static int          g_tick         = 0;

static Seed     g_seeds[SEED_COUNT];
static uint16_t palette[256];

static int effect_init(struct demo_ctx *ctx)
{
    g_tex_phy_addr = mpp_phy_alloc(TEX_SIZE);
    if (g_tex_phy_addr == 0)
        return -1;
    g_tex_vir_addr = (uint16_t *)(unsigned long)g_tex_phy_addr;

    // 1. 初始化种子点
    for (int i = 0; i < SEED_COUNT; i++)
    {
        g_seeds[i].x  = rand() % TEX_W;
        g_seeds[i].y  = rand() % TEX_H;
        g_seeds[i].vx = (rand() % 5) - 2; // -2 ~ 2
        g_seeds[i].vy = (rand() % 5) - 2;
        if (g_seeds[i].vx == 0)
            g_seeds[i].vx = 1;
        if (g_seeds[i].vy == 0)
            g_seeds[i].vy = 1;
    }

    // 2. 初始化调色板 (Crystal Blue -> White)
    for (int i = 0; i < 256; i++)
    {
        int r, g, b;

        // 基于距离差的着色：
        // 0 (边界) -> 255 (晶体中心)

        // 边界发光 (Electric Blue Border)
        if (i < 16)
        {
            int boost = (16 - i) * 16; // 255 -> 0
            r         = boost;
            g         = boost + 100;
            b         = 255;
        }
        else
        {
            // 晶体内部 (Dark to Blue Gradient)
            int v = i - 16;
            r     = 0;
            g     = v / 2;
            b     = 64 + v / 2;
        }

        // 饱和度截断
        if (r > 255)
            r = 255;
        if (g > 255)
            g = 255;
        if (b > 255)
            b = 255;

        palette[i] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
    }

    g_tick = 0;
    rt_kprintf("Night 14: Cellular tessellation active.\n");
    return 0;
}

static void effect_draw(struct demo_ctx *ctx, unsigned long phy_addr)
{
    if (!g_tex_vir_addr)
        return;

    /* === PHASE 1: 更新种子位置 === */
    for (int i = 0; i < SEED_COUNT; i++)
    {
        g_seeds[i].x += g_seeds[i].vx;
        g_seeds[i].y += g_seeds[i].vy;

        // 碰壁反弹
        if (g_seeds[i].x < 0 || g_seeds[i].x >= TEX_W)
        {
            g_seeds[i].vx = -g_seeds[i].vx;
            g_seeds[i].x += g_seeds[i].vx;
        }
        if (g_seeds[i].y < 0 || g_seeds[i].y >= TEX_H)
        {
            g_seeds[i].vy = -g_seeds[i].vy;
            g_seeds[i].y += g_seeds[i].vy;
        }
    }

    /* === PHASE 2: 沃罗诺伊图计算 (Voronoi) === */
    uint16_t *p_pixel = g_tex_vir_addr;

    for (int y = 0; y < TEX_H; y++)
    {
        for (int x = 0; x < TEX_W; x++)
        {

            // 寻找第一近 (d1) 和第二近 (d2) 的距离
            // 初始化为一个较大的值
            int d1 = 0xFFFF;
            int d2 = 0xFFFF;

            for (int i = 0; i < SEED_COUNT; i++)
            {
                // 曼哈顿距离：|x1-x2| + |y1-y2|
                // 纯整数运算，极快，且产生漂亮的棱形边界
                int dist = abs(x - g_seeds[i].x) + abs(y - g_seeds[i].y);

                if (dist < d1)
                {
                    d2 = d1;
                    d1 = dist;
                }
                else if (dist < d2)
                {
                    d2 = dist;
                }
            }

            /*
             * 视觉魔法：Worley Noise F2 - F1
             * 边界处 d1 == d2，所以 d2 - d1 == 0。
             * 越靠近晶胞中心，d2 远大于 d1，差值越大。
             */
            int val = d2 - d1;

            // 放大对比度
            // val 范围通常在 0 ~ 100 之间
            if (val > 255)
                val = 255;

            // 颜色查表
            *p_pixel++ = palette[val];
        }
    }

    /* === CRITICAL: Cache Flush === */
    aicos_dcache_clean_range((void *)g_tex_vir_addr, TEX_SIZE);

    /* === PHASE 3: GE Scaling === */
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

struct effect_ops effect_0014 = {
    .name   = "NO.14 THE CRYSTALLINE CELL",
    .init   = effect_init,
    .draw   = effect_draw,
    .deinit = effect_deinit,
};

REGISTER_EFFECT(effect_0014);
