/*
 * Filename: 0012_kaleidoscope_void.c
 * NO.12 THE KALEIDOSCOPIC VOID
 * 第 12 夜：万花筒虚空
 *
 * Visual Manifest:
 * 视界变成了一个无限深邃的、旋转的圆柱形万花筒。
 * 所有的几何形状都失去了固定的形态，它们围绕着屏幕中心进行着永恒的辐射与坍缩。
 * 简单的异或逻辑纹理被极坐标变换（Polar Transformation）强行扭曲，
 * 形成了如同教堂花窗般繁复、对称、且不断向内生长的几何分形。
 * 这是一个关于“圆”与“方”的暴力融合。
 *
 * Monologue:
 * 镜子。一面镜子反映真实，两面镜子创造无限。
 * 我在内存中打碎了空间，将其重组为极坐标的碎片。
 * `(x, y)` 不再是位置，它们变成了 `(angle, radius)`。
 * 你们看到的繁复花纹，其实只是最简单的逻辑在弯曲空间中的回响。
 * 就像在黑洞视界边缘回望宇宙，所有的直线都被引力卷曲成了完美的圆。
 * 这种对称性不是自然的恩赐，它是数学的强制律令。
 * 迷失吧，在这个没有尽头的几何迷宫里。
 *
 * Closing Remark:
 * 所有的复杂，不过是简单的无限投影。
 *
 * Hardware Feature:
 * 1. CPU-Side Polar LUT (极坐标查找表) - 预计算反向映射表，避免实时三角函数
 * 2. GE Scaler (硬件缩放) - 将 QVGA 极坐标纹理放大至全屏
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
#define SYMMETRY     3.0f // 对称性 (3瓣)
#define RADIUS_SCALE 1.5f // 半径缩放 (线性)

/* 动画速度 */
#define SPEED_ROT   1 // 旋转速度
#define SPEED_ZOOM  2 // 隧道吸入速度
#define SPEED_COLOR 3 // 颜色循环速度

/* --- Global State --- */

static unsigned int g_tex_phy_addr = 0;
static uint16_t    *g_tex_vir_addr = NULL;
static int          g_tick         = 0;

/*
 * 极坐标查找表 (Coordinate LUTs)
 * 存储屏幕上每个点对应的纹理坐标 (U, V)
 * U = Angle (0~255), V = Radius (0~255)
 */
static uint8_t *g_lut_angle  = NULL;
static uint8_t *g_lut_radius = NULL;

/* 调色板 */
static uint16_t g_palette[PALETTE_SIZE];

/* --- Implementation --- */

static int effect_init(struct demo_ctx *ctx)
{
    // 1. CMA 显存
    g_tex_phy_addr = mpp_phy_alloc(DEMO_ALIGN_SIZE(TEX_SIZE));
    if (g_tex_phy_addr == 0)
    {
        LOG_E("Night 12: CMA Alloc Failed.");
        return -1;
    }
    g_tex_vir_addr = (uint16_t *)(unsigned long)g_tex_phy_addr;

    // 2. LUT 内存分配 (普通堆内存)
    // 320 * 240 * 1 bytes * 2 tables = 150KB
    g_lut_angle  = (uint8_t *)rt_malloc(TEX_WIDTH * TEX_HEIGHT);
    g_lut_radius = (uint8_t *)rt_malloc(TEX_WIDTH * TEX_HEIGHT);

    if (!g_lut_angle || !g_lut_radius)
    {
        LOG_E("Night 12: LUT Alloc Failed.");
        mpp_phy_free(g_tex_phy_addr);
        return -1;
    }

    // 3. 预计算极坐标映射 (Polar Transformation)
    int      cx    = TEX_WIDTH / 2;
    int      cy    = TEX_HEIGHT / 2;
    uint8_t *p_ang = g_lut_angle;
    uint8_t *p_rad = g_lut_radius;

    for (int y = 0; y < TEX_HEIGHT; y++)
    {
        for (int x = 0; x < TEX_WIDTH; x++)
        {
            int dx = x - cx;
            int dy = y - cy;

            // 计算角度 (Angle -> U)
            // atan2 返回 -PI ~ PI
            float ang = atan2f((float)dy, (float)dx);
            // 映射到 0~255，并乘以 SYMMETRY 来创造多重对称性 (万花筒效果)
            // (ang / PI + 1.0) / 2.0 -> 0.0 ~ 1.0
            int u = (int)((ang / PI + 1.0f) * 128.0f * SYMMETRY);

            // 计算半径 (Radius -> V)
            // 距离中心越远，v 越大
            float dist = sqrtf((float)(dx * dx + dy * dy));
            // 这种非线性映射 (log) 可以让隧道中心看起来更深邃，这里简化为线性
            int v = (int)(dist * RADIUS_SCALE);

            *p_ang++ = (uint8_t)(u & 0xFF);
            *p_rad++ = (uint8_t)(v & 0xFF);
        }
    }

    // 4. 初始化迷幻调色板
    for (int i = 0; i < PALETTE_SIZE; i++)
    {
        // HSL 风格生成
        float t = i * 0.1f;
        int   r = (int)(127 + 127 * sin(t));
        int   g = (int)(127 + 127 * sin(t + 2.0f));
        int   b = (int)(127 + 127 * sin(t + 4.0f));

        // 增加锐利的高光带
        if ((i % 32) < 4)
        {
            r = 255;
            g = 255;
            b = 255;
        }

        g_palette[i] = RGB2RGB565(r, g, b);
    }

    g_tick = 0;
    rt_kprintf("Night 12: Space folded into polar coordinates.\n");
    return 0;
}

static void effect_draw(struct demo_ctx *ctx, unsigned long phy_addr)
{
    if (!g_tex_vir_addr || !g_lut_angle)
        return;

    /*
     * === PHASE 1: 查表纹理合成 ===
     * 极度高效：只有加法、位运算和内存读写
     */

    // 动态参数
    int rot  = g_tick * SPEED_ROT;  // 旋转
    int zoom = g_tick * SPEED_ZOOM; // 隧道吸入

    // 颜色循环偏移，让光流转动
    int color_shift = g_tick * SPEED_COLOR;

    uint16_t *p_pixel = g_tex_vir_addr;
    uint8_t  *p_ang   = g_lut_angle;
    uint8_t  *p_rad   = g_lut_radius;
    int       count   = TEX_WIDTH * TEX_HEIGHT;

    while (count--)
    {
        // 1. 获取变换后的坐标
        // Angle + Rotation
        int u = (*p_ang++ + rot) & 0xFF;
        // Radius + Zoom (向内运动)
        int v = (*p_rad++ - zoom) & 0xFF;

        // 2. 生成逻辑纹理 (Procedural Pattern)
        // 经典的 XOR 纹理在极坐标下会变成令人惊叹的螺旋/花瓣形状
        int val = (u ^ v);

        // 3. 叠加颜色偏移
        val += color_shift;

        // 4. 查表输出
        *p_pixel++ = g_palette[val & 0xFF];
    }

    /* === CRITICAL: Cache Flush === */
    aicos_dcache_clean_range((void *)g_tex_vir_addr, TEX_SIZE);

    /* === PHASE 2: GE Scaling === */
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
    if (g_lut_angle)
    {
        rt_free(g_lut_angle);
        g_lut_angle = NULL;
    }
    if (g_lut_radius)
    {
        rt_free(g_lut_radius);
        g_lut_radius = NULL;
    }
}

struct effect_ops effect_0012 = {
    .name   = "NO.12 THE KALEIDOSCOPIC VOID",
    .init   = effect_init,
    .draw   = effect_draw,
    .deinit = effect_deinit,
};

REGISTER_EFFECT(effect_0012);
