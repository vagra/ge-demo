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
 */

#include "demo_engine.h"
#include "mpp_mem.h"
#include "aic_hal_ge.h"
#include <math.h>
#include <stdlib.h>

/*
 * === 混合渲染架构 (Polar Mapping) ===
 * 1. 纹理: 320x240 RGB565
 * 2. 核心: 预计算极坐标查找表 (Polar LUT)
 *    实时计算 atan2 和 sqrt 是 CPU 的噩梦。
 *    我们在 Init 阶段将每个 (x,y) 对应的 (angle, radius) 存入 LUT。
 *    Draw 阶段只需查表：Pixel = Pattern(Angle + Rot, Radius + Zoom)。
 */
#define TEX_W    320
#define TEX_H    240
#define TEX_SIZE (TEX_W * TEX_H * 2)

static unsigned int g_tex_phy_addr = 0;
static uint16_t    *g_tex_vir_addr = NULL;
static int          g_tick         = 0;

/*
 * 极坐标查找表 (Coordinate LUTs)
 * 存储屏幕上每个点对应的纹理坐标 (U, V)
 * U = Angle (0~255), V = Radius (0~255)
 * 需要约 150KB RAM，D13CCS 16MB 内存绰绰有余。
 */
static uint8_t *g_lut_angle  = NULL;
static uint8_t *g_lut_radius = NULL;

/* 调色板 */
static uint16_t palette[256];

static int effect_init(struct demo_ctx *ctx)
{
    // 1. CMA 显存
    g_tex_phy_addr = mpp_phy_alloc(TEX_SIZE);
    if (g_tex_phy_addr == 0)
        return -1;
    g_tex_vir_addr = (uint16_t *)(unsigned long)g_tex_phy_addr;

    // 2. LUT 内存分配 (普通堆内存)
    g_lut_angle  = (uint8_t *)rt_malloc(TEX_W * TEX_H);
    g_lut_radius = (uint8_t *)rt_malloc(TEX_W * TEX_H);

    if (!g_lut_angle || !g_lut_radius)
    {
        rt_kprintf("Night 12: LUT Alloc Failed.\n");
        return -1;
    }

    // 3. 预计算极坐标映射 (Polar Transformation)
    int      cx    = TEX_W / 2;
    int      cy    = TEX_H / 2;
    uint8_t *p_ang = g_lut_angle;
    uint8_t *p_rad = g_lut_radius;

    for (int y = 0; y < TEX_H; y++)
    {
        for (int x = 0; x < TEX_W; x++)
        {
            int dx = x - cx;
            int dy = y - cy;

            // 计算角度 (Angle -> U)
            // atan2 返回 -PI ~ PI
            float ang = atan2f((float)dy, (float)dx);
            // 映射到 0~255，并乘以 2 (或者 4) 来创造多重对称性 (万花筒效果)
            // 这里乘以 3，制造 3 瓣对称结构
            int u = (int)((ang / 3.14159f + 1.0f) * 128.0f * 3.0f);

            // 计算半径 (Radius -> V)
            // 距离中心越远，v 越大
            float dist = sqrtf((float)(dx * dx + dy * dy));
            // 这种非线性映射 (log) 可以让隧道中心看起来更深邃
            int v = (int)(dist * 1.5f); // 线性缩放

            *p_ang++ = (uint8_t)(u & 0xFF);
            *p_rad++ = (uint8_t)(v & 0xFF);
        }
    }

    // 4. 初始化迷幻调色板
    for (int i = 0; i < 256; i++)
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

        palette[i] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
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
    int rot  = g_tick;     // 旋转
    int zoom = g_tick * 2; // 隧道吸入

    // 颜色循环偏移，让光流转动
    int color_shift = g_tick * 3;

    uint16_t *p_pixel = g_tex_vir_addr;
    uint8_t  *p_ang   = g_lut_angle;
    uint8_t  *p_rad   = g_lut_radius;
    int       count   = TEX_W * TEX_H;

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
        *p_pixel++ = palette[val & 0xFF];
    }

    /* === CRITICAL: Cache Flush === */
    aicos_dcache_clean_range((void *)g_tex_vir_addr, TEX_SIZE);

    /* === PHASE 2: GE Scaling === */
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
