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
 *
 * Hardware Feature:
 * 1. CPU Cellular Noise (细胞噪声) - 实时计算 Manhattan Voronoi 图 (Distance Difference)
 * 2. GE Scaler (硬件缩放) - 将低分晶格纹理无损放大至全屏
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
#define SEED_COUNT   12 // 种子点数量
#define MAX_SPEED    2  // 种子最大移动速度 (+/-)
#define BORDER_WIDTH 16 // 晶格边界发光宽度 (阈值)

/* 调色板参数 */
#define PALETTE_SIZE 256

/* --- Global State --- */

static unsigned int g_tex_phy_addr = 0;
static uint16_t    *g_tex_vir_addr = NULL;
static int          g_tick         = 0;

typedef struct
{
    int x, y;
    int vx, vy;
} Seed;

static Seed     g_seeds[SEED_COUNT];
static uint16_t g_palette[PALETTE_SIZE];

/* --- Implementation --- */

static int effect_init(struct demo_ctx *ctx)
{
    // 1. CMA 显存
    g_tex_phy_addr = mpp_phy_alloc(DEMO_ALIGN_SIZE(TEX_SIZE));
    if (g_tex_phy_addr == 0)
    {
        LOG_E("Night 14: CMA Alloc Failed.");
        return -1;
    }
    g_tex_vir_addr = (uint16_t *)(unsigned long)g_tex_phy_addr;

    // 2. 初始化种子点
    for (int i = 0; i < SEED_COUNT; i++)
    {
        g_seeds[i].x  = rand() % TEX_WIDTH;
        g_seeds[i].y  = rand() % TEX_HEIGHT;
        g_seeds[i].vx = (rand() % (MAX_SPEED * 2 + 1)) - MAX_SPEED; // -2 ~ 2
        g_seeds[i].vy = (rand() % (MAX_SPEED * 2 + 1)) - MAX_SPEED;

        // 防止静止
        if (g_seeds[i].vx == 0)
            g_seeds[i].vx = 1;
        if (g_seeds[i].vy == 0)
            g_seeds[i].vy = 1;
    }

    // 3. 初始化调色板 (Crystal Blue -> White)
    // 基于 Worley Noise 的特征值 (F2 - F1) 进行着色
    // 值越小表示越接近边界
    for (int i = 0; i < PALETTE_SIZE; i++)
    {
        int r, g, b;

        if (i < BORDER_WIDTH)
        {
            // 边界发光 (Electric Blue Border)
            // 越接近 0 (边界中心)，越亮
            int boost = (BORDER_WIDTH - i) * 16;
            r         = boost;
            g         = boost + 100;
            b         = 255;
        }
        else
        {
            // 晶体内部 (Dark to Blue Gradient)
            int v = i - BORDER_WIDTH;
            r     = 0;
            g     = v / 2;
            b     = 64 + v / 2;
        }

        // 饱和度截断
        r = MIN(r, 255);
        g = MIN(g, 255);
        b = MIN(b, 255);

        g_palette[i] = RGB2RGB565(r, g, b);
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
        if (g_seeds[i].x < 0 || g_seeds[i].x >= TEX_WIDTH)
        {
            g_seeds[i].vx = -g_seeds[i].vx;
            g_seeds[i].x += g_seeds[i].vx;
        }
        if (g_seeds[i].y < 0 || g_seeds[i].y >= TEX_HEIGHT)
        {
            g_seeds[i].vy = -g_seeds[i].vy;
            g_seeds[i].y += g_seeds[i].vy;
        }
    }

    /* === PHASE 2: 沃罗诺伊图计算 (Voronoi) === */
    uint16_t *p_pixel = g_tex_vir_addr;

    for (int y = 0; y < TEX_HEIGHT; y++)
    {
        for (int x = 0; x < TEX_WIDTH; x++)
        {
            // 寻找第一近 (d1) 和第二近 (d2) 的距离
            int d1 = 0x7FFF;
            int d2 = 0x7FFF;

            for (int i = 0; i < SEED_COUNT; i++)
            {
                // 曼哈顿距离：|x1-x2| + |y1-y2|
                // 纯整数运算，极快，且产生漂亮的棱形边界
                int dist = ABS(x - g_seeds[i].x) + ABS(y - g_seeds[i].y);

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

            // 限制范围以查表
            val = MIN(val, 255);

            // 颜色查表
            *p_pixel++ = g_palette[val];
        }
    }

    /* === CRITICAL: Cache Flush === */
    aicos_dcache_clean_range((void *)g_tex_vir_addr, TEX_SIZE);

    /* === PHASE 3: GE Scaling === */
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
}

struct effect_ops effect_0014 = {
    .name   = "NO.14 THE CRYSTALLINE CELL",
    .init   = effect_init,
    .draw   = effect_draw,
    .deinit = effect_deinit,
};

REGISTER_EFFECT(effect_0014);
