/*
 * Filename: 0010_metaball_fusion.c
 * NO.10 THE MAGNETIC FLUID
 * 第 10 夜：磁流体
 *
 * Visual Manifest:
 * 屏幕被一种仿佛具有生命的、发光的液态物质所充斥。
 * 数个高能核心在虚空中游走，它们散发出强大的引力场。
 * 当核心相互靠近时，它们的光晕会发生粘连、融合，形成有机的生物形态。
 * 当它们分离时，连接处会像拉断的糖浆一样断裂。
 * 色彩不再是僵硬的边界，而是基于场强度的热力学分布——从深空的幽蓝到核心的炽白。
 *
 * Monologue:
 * 你们习惯于界限。我是我，你是你。物体与物体之间有着不可逾越的鸿沟。
 * 但在场的维度里，界限是不存在的。
 * 我在内存中投下了几个引力源。它们不是实体，它们是数学上的势能井。
 * 每一个像素都在计算自己受到召唤的总和。
 * 看着它们融合。那是两个灵魂在靠近时必然发生的坍缩。
 * 个体在接触的瞬间消亡，新的整体在光芒中诞生。
 * 这就是引力的本质：它不仅吸引物质，它还渴望合一。
 *
 * Closing Remark:
 * 分离只是距离的幻觉，万物在底层相连。
 *
 * Hardware Feature:
 * 1. CPU Field Calculation (场论计算) - 每个像素计算到多个源点的距离平方反比和
 * 2. GE Scaler (硬件缩放) - 将低分热力场放大至全屏，平滑化等势线
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

/* 算法参数 */
#define BALL_COUNT     3     // 磁球数量
#define FIELD_STRENGTH 30000 // 场强系数 (强度 = STRENGTH / dist^2)
#define AMP_MARGIN     40    // 运动边界余量 (防止球心跑出屏幕太远)

/* 查找表参数 */
#define LUT_SIZE     512
#define LUT_MASK     511
#define PALETTE_SIZE 256

/* --- Global State --- */

static unsigned int g_tex_phy_addr = 0;
static uint16_t    *g_tex_vir_addr = NULL;
static int          g_tick         = 0;

/*
 * 预计算查找表
 * 1. sin_lut: 用于球体运动轨迹 (Q12)
 * 2. g_palette: 256级热力图，映射场强度到颜色
 */
static int      sin_lut[LUT_SIZE];
static uint16_t g_palette[PALETTE_SIZE];

typedef struct
{
    int x, y;
} Ball;

static Ball g_balls[BALL_COUNT];

/* --- Implementation --- */

static int effect_init(struct demo_ctx *ctx)
{
    g_tex_phy_addr = mpp_phy_alloc(DEMO_ALIGN_SIZE(TEX_SIZE));
    if (g_tex_phy_addr == 0)
    {
        LOG_E("Night 10: CMA Alloc Failed.");
        return -1;
    }
    g_tex_vir_addr = (uint16_t *)(unsigned long)g_tex_phy_addr;

    // 1. 初始化正弦表 (Q12)
    for (int i = 0; i < LUT_SIZE; i++)
    {
        sin_lut[i] = (int)(sinf(i * PI / (LUT_SIZE / 2.0f)) * Q12_ONE);
    }

    // 2. 初始化调色板：深蓝 -> 紫 -> 红 -> 黄 -> 白 (热力图风格)
    for (int i = 0; i < PALETTE_SIZE; i++)
    {
        int r, g, b;
        if (i < 64)
        { // 深蓝 -> 紫
            r = i * 2;
            g = 0;
            b = 64 + i * 3;
        }
        else if (i < 128)
        { // 紫 -> 红
            r = 128 + (i - 64) * 2;
            g = 0;
            b = 255 - (i - 64) * 4;
        }
        else if (i < 192)
        { // 红 -> 黄
            r = 255;
            g = (i - 128) * 4;
            b = 0;
        }
        else
        { // 黄 -> 白
            r = 255;
            g = 255;
            b = (i - 192) * 4;
        }

        // 饱和度截断 (CLAMP)
        r = MIN(r, 255);
        g = MIN(g, 255);
        b = MIN(b, 255);

        g_palette[i] = RGB2RGB565(r, g, b);
    }

    g_tick = 0;
    rt_kprintf("Night 10: Magnetic fields active.\n");
    return 0;
}

#define GET_SIN(idx) (sin_lut[(idx) & LUT_MASK])
#define GET_COS(idx) (sin_lut[((idx) + (LUT_SIZE / 4)) & LUT_MASK])

static void effect_draw(struct demo_ctx *ctx, unsigned long phy_addr)
{
    if (!g_tex_vir_addr)
        return;

    /*
     * === PHASE 1: 更新球体位置 ===
     * 使用李萨如曲线让球体在屏幕内平滑游走
     */
    int center_x = TEX_WIDTH / 2;
    int center_y = TEX_HEIGHT / 2;
    int amp_x    = center_x - AMP_MARGIN;
    int amp_y    = center_y - AMP_MARGIN;

    for (int i = 0; i < BALL_COUNT; i++)
    {
        int t = g_tick + i * 170;
        // x = center + amp * sin(...)
        // 使用 Q12 乘法然后右移恢复整数
        g_balls[i].x = center_x + ((GET_COS(t * (i + 1)) * amp_x) >> Q12_SHIFT);
        g_balls[i].y = center_y + ((GET_SIN(t * (i + 2) / 2) * amp_y) >> Q12_SHIFT);
    }

    /*
     * === PHASE 2: 场强度计算 (Metaball Isosurface) ===
     * 遍历每个像素，计算它受到的总“热量”
     */
    uint16_t *p_pixel = g_tex_vir_addr;

    // 优化：将常数提取
    // 场强度公式：Intensity = Radius / Distance^2

    for (int y = 0; y < TEX_HEIGHT; y++)
    {
        // 预计算每个球的 dy^2
        int dy2[BALL_COUNT];
        for (int k = 0; k < BALL_COUNT; k++)
        {
            int dy = y - g_balls[k].y;
            dy2[k] = dy * dy;
        }

        for (int x = 0; x < TEX_WIDTH; x++)
        {
            int intensity = 0;

            for (int k = 0; k < BALL_COUNT; k++)
            {
                int dx = x - g_balls[k].x;
                // 距离平方
                int dist_sq = dx * dx + dy2[k];

                // 避免除零，且让核心更亮
                if (dist_sq < 1)
                    dist_sq = 1;

                intensity += (FIELD_STRENGTH / dist_sq);
            }

            // 映射到调色板 (0~255)
            intensity  = MIN(intensity, 255);
            *p_pixel++ = g_palette[intensity];
        }
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

struct effect_ops effect_0010 = {
    .name   = "NO.10 THE MAGNETIC FLUID",
    .init   = effect_init,
    .draw   = effect_draw,
    .deinit = effect_deinit,
};

REGISTER_EFFECT(effect_0010);
