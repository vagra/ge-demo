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
 *
 * Hardware Feature:
 * 1. High-Density 3D Math (高密度3D运算) - 4096 粒子实时 3D 旋转投影
 * 2. GE Scaler (硬件缩放) - 将粒子点阵平滑放大，模拟望远镜视角
 */

#include "demo_engine.h"
#include "mpp_mem.h"
#include "aic_hal_ge.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* --- Configuration Parameters --- */

/* 纹理规格 */
#define TEX_WIDTH  DEMO_QVGA_W
#define TEX_HEIGHT DEMO_QVGA_H
#define TEX_FMT    MPP_FMT_RGB_565
#define TEX_BPP    2
#define TEX_SIZE   (TEX_WIDTH * TEX_HEIGHT * TEX_BPP)

/* 星系参数 */
#define STAR_COUNT     4096   // 粒子数量极限
#define GALAXY_RADIUS  200.0f // 星系半径
#define GALAXY_ARMS    2      // 旋臂数量
#define ARM_TWIST      6.0f   // 旋臂缠绕圈数
#define CORE_THICKNESS 30.0f  // 盘面厚度

/* 颜色阈值 (0.0 ~ 1.0) */
#define THRESH_CORE 0.15f
#define THRESH_MID  0.5f

/* 摄像机参数 */
#define CAM_DIST_BASE 300 // 基础距离
#define PROJ_SCALE    256 // 透视缩放系数

/* 查找表 */
#define LUT_SIZE 512
#define LUT_MASK 511

/* --- Global State --- */

static unsigned int g_tex_phy_addr = 0;
static uint16_t    *g_tex_vir_addr = NULL;
static int          g_tick         = 0;

typedef struct
{
    int      x, y, z;      // 3D 坐标 (Q12)
    uint16_t color;        // 颜色
    int      speed_offset; // 速度差异
} Star;

/* 星体数据放入普通 RAM (rt_malloc) */
static Star *g_stars = NULL;
static int   sin_lut[LUT_SIZE]; // Q12

/* --- Implementation --- */

static int effect_init(struct demo_ctx *ctx)
{
    // 1. CMA 显存
    g_tex_phy_addr = mpp_phy_alloc(DEMO_ALIGN_SIZE(TEX_SIZE));
    if (g_tex_phy_addr == 0)
    {
        LOG_E("Night 20: CMA Alloc Failed.");
        return -1;
    }
    g_tex_vir_addr = (uint16_t *)(unsigned long)g_tex_phy_addr;

    // 2. 分配星星数组 (RAM)
    g_stars = (Star *)rt_malloc(STAR_COUNT * sizeof(Star));
    if (!g_stars)
    {
        LOG_E("Night 20: Star Alloc Failed.");
        mpp_phy_free(g_tex_phy_addr);
        return -1;
    }

    // 3. 初始化正弦表 (Q12)
    for (int i = 0; i < LUT_SIZE; i++)
    {
        sin_lut[i] = (int)(sinf(i * PI / (LUT_SIZE / 2.0f)) * Q12_ONE);
    }

    // 4. 初始化星系 (高密度双旋臂)
    for (int i = 0; i < STAR_COUNT; i++)
    {
        // 半径分布：使用 1.5 次方分布，让核心密集
        float r_norm = (float)(rand() % 1000) / 1000.0f;
        r_norm       = powf(r_norm, 1.5f);

        int radius = (int)(r_norm * GALAXY_RADIUS * 16.0f); // *16 for extra precision scale

        // 角度：双旋臂 + 随机弥散
        float base_angle = r_norm * PI * ARM_TWIST + (i % GALAXY_ARMS) * PI;
        // 增加随机散射 (Scatter)，模拟星系厚度
        base_angle += ((rand() % 100) / 100.0f) * 1.0f;

        g_stars[i].x = (int)(cosf(base_angle) * radius);
        g_stars[i].z = (int)(sinf(base_angle) * radius);

        // Y 轴 (厚度)：核心球状，旋臂盘状
        int thickness = (int)((1.0f - r_norm * 0.8f) * CORE_THICKNESS * 16.0f);
        // 核心部分更厚
        if (r_norm < 0.1f)
            thickness *= 3;

        g_stars[i].y = (rand() % (thickness * 2 + 1)) - thickness;

        // 颜色生成：基于温度 (半径)
        int r, g, b;
        if (r_norm < THRESH_CORE)
        { // Core: 炽热白/黄
            r = 255;
            g = 255;
            b = 220 + (rand() % 35);
        }
        else if (r_norm < THRESH_MID)
        { // Mid: 能量红
            r = 255;
            g = 100 + (int)((THRESH_MID - r_norm) * 300);
            b = 100;
            g = MIN(g, 255);
        }
        else
        { // Edge: 冰冷蓝
            r = 100;
            g = 150;
            b = 255;
        }
        g_stars[i].color = RGB2RGB565(r, g, b);

        // 速度差异：开普勒模拟，内快外慢
        g_stars[i].speed_offset = (int)((1.0f - r_norm) * 64.0f) + 16;
    }

    g_tick = 0;
    rt_kprintf("Night 20: 4096 Stars Simulation.\n");
    return 0;
}

#define GET_SIN(idx) (sin_lut[(idx) & LUT_MASK])
#define GET_COS(idx) (sin_lut[((idx) + (LUT_SIZE / 4)) & LUT_MASK])

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
        int ny = (*y * c - *z * s) >> Q12_SHIFT;
        int nz = (*y * s + *z * c) >> Q12_SHIFT;
        *y     = ny;
        *z     = nz;
    }
    // Y-Axis Rotation (Yaw)
    if (angle_y)
    {
        int s  = GET_SIN(angle_y);
        int c  = GET_COS(angle_y);
        int nx = (*x * c - *z * s) >> Q12_SHIFT;
        int nz = (*x * s + *z * c) >> Q12_SHIFT;
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
    int cam_pitch = (GET_SIN(g_tick) >> 6);                     // 缓慢俯仰
    int cam_yaw   = g_tick;                                     // 持续自旋
    int cam_dist  = CAM_DIST_BASE + (GET_SIN(g_tick / 2) >> 5); // 呼吸式推拉

    int cx = TEX_WIDTH / 2;
    int cy = TEX_HEIGHT / 2;

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
        int nx       = (x * sc - z * ss) >> Q12_SHIFT;
        int nz       = (x * ss + z * sc) >> Q12_SHIFT;
        x            = nx;
        z            = nz;

        // 3. View Space (摄像机旋转)
        rotate_point_inline(&x, &y, &z, cam_pitch, cam_yaw);

        // 4. Projection
        // 增加摄像机距离
        z += (cam_dist << 4);

        // 裁剪掉身后的点
        if (z <= 64)
            continue;

        // 透视投影: screen_x = x / z * scale
        int sx = cx + (x * PROJ_SCALE / z);
        int sy = cy + (y * PROJ_SCALE / z);

        // 5. Rasterization
        if (sx >= 0 && sx < TEX_WIDTH && sy >= 0 && sy < TEX_HEIGHT)
        {
            uint16_t *pixel = g_tex_vir_addr + sy * TEX_WIDTH + sx;
            *pixel          = s->color;

            // 距离发光 (Bloom Hack) - 模拟近大远小
            // 越近的点 (z越小) 绘制越大
            if (z < 800)
            {
                if (sx + 1 < TEX_WIDTH)
                    *(pixel + 1) = s->color;

                if (sy + 1 < TEX_HEIGHT)
                    *(pixel + TEX_WIDTH) = s->color;

                // 增加一点柔化
                if (z < 400 && sx + 1 < TEX_WIDTH && sy + 1 < TEX_HEIGHT)
                    *(pixel + TEX_WIDTH + 1) = s->color;
            }
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

    // Scale to Fit
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
