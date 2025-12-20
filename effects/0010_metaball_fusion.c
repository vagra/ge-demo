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
 */

#include "demo_engine.h"
#include "mpp_mem.h"
#include "aic_hal_ge.h"
#include <math.h>

/*
 * === 混合渲染架构 ===
 * 1. 纹理: 320x240 RGB565
 * 2. 算法: Metaballs (Isosurfaces)
 *    对于屏幕上的每一点，计算它到所有球体中心的距离平方反比之和。
 *    Sum = R / (dx^2 + dy^2)
 *    这种计算对 CPU 压力较大，但 320x240 分辨率下，RISC-V 480MHz 可以轻松驾驭 3-5 个球体。
 */
#define TEX_W     320
#define TEX_H     240
#define TEX_SIZE  (TEX_W * TEX_H * 2)
#define NUM_BALLS 3

static unsigned int g_tex_phy_addr = 0;
static uint16_t    *g_tex_vir_addr = NULL;
static int          g_tick         = 0;

/*
 * 预计算查找表
 * 1. sin_lut: 用于球体运动轨迹
 * 2. palette: 256级热力图，映射场强度到颜色
 */
static int      sin_lut[512];
static uint16_t palette[256];

typedef struct
{
    int x, y;
    int dx, dy;
} Ball;

static Ball balls[NUM_BALLS];

static int effect_init(struct demo_ctx *ctx)
{
    g_tex_phy_addr = mpp_phy_alloc(TEX_SIZE);
    if (g_tex_phy_addr == 0)
        return -1;
    g_tex_vir_addr = (uint16_t *)(unsigned long)g_tex_phy_addr;

    // 初始化正弦表
    for (int i = 0; i < 512; i++)
    {
        sin_lut[i] = (int)(sinf(i * 3.14159f / 256.0f) * 4096.0f);
    }

    // 初始化调色板：深蓝 -> 紫 -> 红 -> 黄 -> 白 (热力图风格)
    for (int i = 0; i < 256; i++)
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
    rt_kprintf("Night 10: Magnetic fields active.\n");
    return 0;
}

#define GET_SIN(idx) (sin_lut[(idx) & 511])
#define GET_COS(idx) (sin_lut[((idx) + 128) & 511])

static void effect_draw(struct demo_ctx *ctx, unsigned long phy_addr)
{
    if (!g_tex_vir_addr)
        return;

    /*
     * === PHASE 1: 更新球体位置 ===
     * 使用李萨如曲线让球体在屏幕内平滑游走
     */
    for (int i = 0; i < NUM_BALLS; i++)
    {
        int t = g_tick + i * 170;
        // 映射到 320x240 空间 (留出边缘)
        // x = center + amp * sin(...)
        balls[i].x = (TEX_W / 2) + (GET_COS(t * (i + 1)) * (TEX_W / 2 - 40) >> 12);
        balls[i].y = (TEX_H / 2) + (GET_SIN(t * (i + 2) / 2) * (TEX_H / 2 - 40) >> 12);
    }

    /*
     * === PHASE 2: 场强度计算 ===
     * 遍历每个像素，计算它受到的总“热量”
     */
    uint16_t *p_pixel = g_tex_vir_addr;

    // 优化：将常数提取
    // 场强度公式：Intensity = Radius / Distance^2
    // 为了避开除法，我们使用定点数乘法近似：Intensity = Radius * (1/Distance^2)
    // 但查表法太占内存。这里直接用整数除法，D13x 的除法器性能尚可。
    // 更好的方法：Intensity = Sum( 2048 / (dx*dx + dy*dy + 1) )

    for (int y = 0; y < TEX_H; y++)
    {

        // 预计算每个球的 dy^2
        int dy2[NUM_BALLS];
        for (int k = 0; k < NUM_BALLS; k++)
        {
            int dy = y - balls[k].y;
            dy2[k] = dy * dy;
        }

        for (int x = 0; x < TEX_W; x++)
        {
            int intensity = 0;

            for (int k = 0; k < NUM_BALLS; k++)
            {
                int dx = x - balls[k].x;
                // 距离平方
                int dist_sq = dx * dx + dy2[k];

                // 避免除零，且让核心更亮
                // 40000 是一个经验系数，决定球的大小
                if (dist_sq < 1)
                    dist_sq = 1;
                intensity += (30000 / dist_sq);
            }

            // 映射到调色板 (0~255)
            if (intensity > 255)
                intensity = 255;
            *p_pixel++ = palette[intensity];
        }
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
}

struct effect_ops effect_0010 = {
    .name   = "NO.10 THE MAGNETIC FLUID",
    .init   = effect_init,
    .draw   = effect_draw,
    .deinit = effect_deinit,
};

REGISTER_EFFECT(effect_0010);
