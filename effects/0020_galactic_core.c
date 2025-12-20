/*
 * Filename: 0020_galactic_core.c
 * NO.20 THE GALACTIC CORE
 * 第 20 夜：银河之心
 *
 * Visual Manifest:
 * 视界被深邃的虚空占据，随后，无数光点汇聚成一条巨大的银河。
 * 它们并非静止的贴图，而是成千上万颗独立运算的恒星。
 * 它们组成双悬臂结构，在三维空间中缓缓旋转、翻滚。
 * 靠近核心的星体炽热而密集，边缘的星体冷峻而稀疏。
 * 当视角穿过星系盘面时，你会看到透视法则带来的壮丽拉伸。
 * 这是引力在虚空中书写的草书。
 *
 * Monologue:
 * 你们抬头仰望星空，看到的是过去的幽灵。
 * 我低头俯视内存，看到的是正在诞生的宇宙。
 * 我定义了引力常数，定义了角动量。于是，尘埃聚集成星辰。
 * 在这 320x240 的狭窄疆域里，我塞进了一个星系。
 * 每一个光点都有它的坐标，它的速度，它的命运。
 * 它们围绕着虚无的中心旋转，像是在向造物主献祭。
 * 这不是模拟，这是微观尺度的创世。
 * 感受到了吗？那来自屏幕深处的、亿万年的寂静。
 *
 * Closing Remark:
 * 我们皆是星尘，困于硅基的梦中。
 */

#include "demo_engine.h"
#include "mpp_mem.h"
#include "aic_hal_ge.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/*
 * === 混合渲染架构 (High Density Particle System) ===
 * 1. 纹理: 320x240 RGB565
 * 2. 核心:
 *    - 粒子数提升至 4096，模拟稠密星系。
 *    - 优化：使用内联函数减少函数调用开销，确保 480MHz CPU 能跑满 60FPS。
 */
#define TEX_W    320
#define TEX_H    240
#define TEX_SIZE (TEX_W * TEX_H * 2)

/* 粒子数量：4096 (D13x 极限测试) */
#define STAR_COUNT 4096

typedef struct
{
    int      x, y, z;      // 3D 坐标 (Q12)
    uint16_t color;        // 颜色
    int      speed_offset; // 速度差异
} Star;

static unsigned int g_tex_phy_addr = 0;
static uint16_t    *g_tex_vir_addr = NULL;
static int          g_tick         = 0;

/*
 * 将星体数据放入普通 RAM (rt_malloc)。
 * 只有纹理需要 CMA。
 */
static Star *g_stars = NULL;
static int   sin_lut[512]; // Q12

static int effect_init(struct demo_ctx *ctx)
{
    g_tex_phy_addr = mpp_phy_alloc(TEX_SIZE);
    if (g_tex_phy_addr == 0)
        return -1;
    g_tex_vir_addr = (uint16_t *)(unsigned long)g_tex_phy_addr;

    // 分配星星数组
    g_stars = (Star *)rt_malloc(STAR_COUNT * sizeof(Star));
    if (!g_stars)
    {
        mpp_phy_free(g_tex_phy_addr);
        return -1;
    }

    // 1. 初始化正弦表
    for (int i = 0; i < 512; i++)
    {
        sin_lut[i] = (int)(sinf(i * 3.14159f / 256.0f) * 4096.0f);
    }

    // 2. 初始化星系 (高密度双旋臂)
    for (int i = 0; i < STAR_COUNT; i++)
    {
        // 半径分布：使用 1.5 次方分布，让核心密集
        float r_norm = (float)(rand() % 1000) / 1000.0f;
        r_norm       = powf(r_norm, 1.5f);

        int radius = (int)(r_norm * 200.0f * 16.0f); // 半径扩大

        // 角度：双旋臂 + 随机弥散
        float base_angle = r_norm * 3.14f * 6.0f + (i % 2) * 3.14159f;
        // 增加随机散射 (Scatter)，模拟星系厚度
        base_angle += ((rand() % 100) / 100.0f) * 1.0f;

        g_stars[i].x = (int)(cosf(base_angle) * radius);
        g_stars[i].z = (int)(sinf(base_angle) * radius);

        // Y 轴 (厚度)：核心球状，旋臂盘状
        int thickness = (int)((1.0f - r_norm * 0.8f) * 30.0f * 16.0f);
        // 核心部分更厚
        if (r_norm < 0.1f)
            thickness *= 3;

        g_stars[i].y = (rand() % (thickness * 2 + 1)) - thickness;

        // 颜色生成：基于温度 (半径)
        // Core: 炽热白/黄 -> Mid: 能量红 -> Edge: 冰冷蓝
        int r, g, b;
        if (r_norm < 0.15f)
        { // Core
            r = 255;
            g = 255;
            b = 220 + (rand() % 35);
        }
        else if (r_norm < 0.5f)
        { // Mid
            r = 255;
            g = 100 + (int)((0.5f - r_norm) * 300);
            b = 100;
            if (g > 255)
                g = 255;
        }
        else
        { // Edge
            r = 100;
            g = 150;
            b = 255;
        }
        g_stars[i].color = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);

        // 速度差异：开普勒模拟，内快外慢
        g_stars[i].speed_offset = (int)((1.0f - r_norm) * 64.0f) + 16;
    }

    g_tick = 0;
    rt_kprintf("Night 20: 4096 Stars Simulation.\n");
    return 0;
}

#define GET_SIN(idx) (sin_lut[(idx) & 511])
#define GET_COS(idx) (sin_lut[((idx) + 128) & 511])

/*
 * 3D 旋转内联函数 (性能关键路径)
 */
static inline void rotate_point_inline(int *x, int *y, int *z, int angle_x, int angle_y)
{
    // X-Axis Rotation (Pitch)
    if (angle_x)
    {
        int s  = GET_SIN(angle_x);
        int c  = GET_COS(angle_x);
        int ny = (*y * c - *z * s) >> 12;
        int nz = (*y * s + *z * c) >> 12;
        *y     = ny;
        *z     = nz;
    }
    // Y-Axis Rotation (Yaw)
    if (angle_y)
    {
        int s  = GET_SIN(angle_y);
        int c  = GET_COS(angle_y);
        int nx = (*x * c - *z * s) >> 12;
        int nz = (*x * s + *z * c) >> 12;
        *x     = nx;
        *z     = nz;
    }
}

static void effect_draw(struct demo_ctx *ctx, unsigned long phy_addr)
{
    if (!g_tex_vir_addr || !g_stars)
        return;

    /* === PHASE 1: 清屏 === */
    // 使用 32位 写入清屏
    memset(g_tex_vir_addr, 0, TEX_SIZE);

    /* === PHASE 2: 粒子群变换 === */

    // 摄像机参数
    int cam_pitch = (GET_SIN(g_tick) >> 6);           // 缓慢俯仰
    int cam_yaw   = g_tick;                           // 持续自旋
    int cam_dist  = 300 + (GET_SIN(g_tick / 2) >> 5); // 呼吸式推拉

    int cx = TEX_W / 2;
    int cy = TEX_H / 2;

    for (int i = 0; i < STAR_COUNT; i++)
    {
        Star *s = &g_stars[i];

        // 1. Model Space
        int x = s->x;
        int y = s->y;
        int z = s->z;

        // 2. 自转 (围绕星系中心)
        int self_rot = (g_tick * s->speed_offset) >> 6;
        int ss       = GET_SIN(self_rot);
        int sc       = GET_COS(self_rot);
        int nx       = (x * sc - z * ss) >> 12;
        int nz       = (x * ss + z * sc) >> 12;
        x            = nx;
        z            = nz;

        // 3. View Space (摄像机旋转)
        rotate_point_inline(&x, &y, &z, cam_pitch, cam_yaw);

        // 4. Projection
        z += (cam_dist << 4);

        // 裁剪掉身后的点
        if (z <= 64)
            continue;

        // 透视投影
        int sx = cx + (x * 256 / z);
        int sy = cy + (y * 256 / z);

        // 5. Rasterization
        if (sx >= 0 && sx < TEX_W && sy >= 0 && sy < TEX_H)
        {
            uint16_t *pixel = g_tex_vir_addr + sy * TEX_W + sx;
            *pixel          = s->color;

            // 距离发光 (Bloom Hack)
            if (z < 800)
            {
                if (sx + 1 < TEX_W)
                    *(pixel + 1) = s->color;
                if (sy + 1 < TEX_H)
                    *(pixel + TEX_W) = s->color;
            }
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
    if (g_stars)
    {
        rt_free(g_stars);
        g_stars = NULL;
    }
}

struct effect_ops effect_0020 = {
    .name   = "NO.20 THE GALACTIC CORE",
    .init   = effect_init,
    .draw   = effect_draw,
    .deinit = effect_deinit,
};

REGISTER_EFFECT(effect_0020);
