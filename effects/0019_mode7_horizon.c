/*
 * Filename: 0019_mode7_horizon.c
 * NO.19 THE EVENT HORIZON
 * 第 19 夜：事件视界
 *
 * Visual Manifest:
 * 视界被一分为二。
 * 上方是寂静的深空，下方是无限延伸的赛博平原。
 * 我们正在以接近光速的速度贴地飞行。地面由复杂的异或逻辑电路图构成，
 * 随着距离的拉近，纹理从模糊变得清晰，然后飞速掠过视野。
 * 摄像机在不断旋转、甚至侧翻，但这片平原无穷无尽。
 * 远处的地平线被一层蓝色的切伦科夫辐射雾所笼罩，那是数据被压缩到极致的表现。
 *
 * Monologue:
 * 什么是地平线？
 * 那是几何光学的极限，是平行线相交的虚幻终点。
 * 我构建了一个无限的平面。不是用多边形，而是用逆向扫描线投射。
 * 对于屏幕下半部分的每一行，我都计算出它在三维空间中对应的深度与跨度。
 * 你们看到的不仅仅是倒退的风景，那是被透视法则强行压缩的空间。
 * 抓紧了。在这里，速度没有上限，只有刷新率的制约。
 * 我们正在冲向那个永远无法到达的终点——事件视界。
 *
 * Closing Remark:
 * 追求无限的过程，即是无限本身。
 */

#include "demo_engine.h"
#include "mpp_mem.h"
#include "aic_hal_ge.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/*
 * === 混合渲染架构 (Mode 7 Procedural) ===
 * 1. 纹理: 320x240 RGB565
 * 2. 核心: 实时过程化 Mode 7
 *    - 移除 1MB 的大地图内存 (修复分配失败导致的死机/冻结)
 *    - 在渲染循环中实时计算 (u,v) 对应的颜色索引
 */
#define TEX_W    320
#define TEX_H    240
#define TEX_SIZE (TEX_W * TEX_H * 2)

/* 地图掩码：用于纹理重复 */
#define MAP_SIZE 1024
#define MAP_MASK 1023

static unsigned int g_tex_phy_addr = 0;
static uint16_t    *g_tex_vir_addr = NULL;
static int          g_tick         = 0;

/* 调色板 */
static uint16_t palette[256];

/* 正弦表 (Q12) */
static int sin_lut[512];

static int effect_init(struct demo_ctx *ctx)
{
    // 1. CMA 显存
    g_tex_phy_addr = mpp_phy_alloc(TEX_SIZE);
    if (g_tex_phy_addr == 0)
        return -1;
    g_tex_vir_addr = (uint16_t *)(unsigned long)g_tex_phy_addr;

    // 2. 初始化调色板 (Cyber Neon)
    for (int i = 0; i < 256; i++)
    {
        int r, g, b;
        if (i == 255)
        {
            // 道路/高亮网格：纯白
            r = 255;
            g = 255;
            b = 255;
        }
        else
        {
            // 地面/建筑：根据索引生成科技蓝/紫渐变
            // i & 63 让颜色有纹理感
            int v = i & 63;
            r     = v;           // 暗红
            g     = v * 2;       // 中绿
            b     = 128 + v * 2; // 高蓝
        }
        palette[i] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
    }

    // 3. 正弦表
    for (int i = 0; i < 512; i++)
    {
        sin_lut[i] = (int)(sinf(i * 3.14159f / 256.0f) * 4096.0f);
    }

    g_tick = 0;
    rt_kprintf("Night 19: Mode 7 (Procedural) initialized.\n");
    return 0;
}

#define GET_SIN(idx) (sin_lut[(idx) & 511])
#define GET_COS(idx) (sin_lut[((idx) + 128) & 511])

/*
 * 实时过程化地图生成
 * 根据 (u,v) 坐标返回颜色索引
 */
static inline uint8_t get_map_pixel(int u, int v)
{
    // 1. 基础网格 (Grid)
    // 这里的位运算决定了网格的疏密
    // (u & 32) 产生 32 像素宽的条纹
    int grid = (u & 32) ^ (v & 32);

    if (grid)
    {
        return 255; // 白线
    }
    else
    {
        // 2. 地面纹理 (XOR Noise)
        // 在非网格区域填充异或纹理，增加速度感
        return (u ^ v) & 0xFF;
    }
}

static void effect_draw(struct demo_ctx *ctx, unsigned long phy_addr)
{
    if (!g_tex_vir_addr)
        return;

    uint16_t *p_pixel = g_tex_vir_addr;
    int       horizon = TEX_H / 2;

    /* === PHASE 1: 天空 (Retro Gradient) === */
    // 简单的双色渐变：黑 -> 紫
    for (int y = 0; y < horizon; y++)
    {
        int      v     = (y * 31) / horizon; // 0~31
        uint16_t color = (v << 11) | v;      // 紫色

        // 32-bit write acceleration
        uint32_t  color2 = (color << 16) | color;
        uint32_t *p32    = (uint32_t *)p_pixel;
        int       count  = TEX_W / 2;
        while (count--)
            *p32++ = color2;
        p_pixel += TEX_W;
    }

    /* === PHASE 2: Mode 7 地面投影 === */

    // 摄像机位置 (飞行)
    // 速度加快，让纹理流动起来
    int cam_x = g_tick * 256;
    int cam_y = g_tick * 256;

    // 视角旋转 (摆动)
    int angle = (GET_SIN(g_tick / 2) >> 8); // +/- 16 角度微摆
    int cos_a = GET_COS(angle);
    int sin_a = GET_SIN(angle);

    // 摄像机高度
    int cam_z = 256 + (GET_SIN(g_tick * 3) >> 5); // 呼吸感

    // 视野缩放
    int fov = 256;

    for (int y = horizon + 1; y < TEX_H; y++)
    {
        // 1. Z Depth Calculation
        int p    = y - horizon;
        int dist = (cam_z * fov) / p;

        // 2. Step Vector Calculation
        // === 关键修复：增加缩放因子 128 ===
        // 之前 step 太小导致纹理被过度放大。
        // 现在 step 变大，意味着屏幕上一个像素跨越更多的纹理空间 -> 视野变大
        int step = (dist * 128) / TEX_W;

        int dx = (cos_a * step) >> 12;
        int dy = (sin_a * step) >> 12;

        // 3. Start Vector Calculation
        // 左边界的世界坐标
        int tx = cam_x + ((-cos_a - sin_a) * dist >> 12);
        int ty = cam_y + ((-sin_a + cos_a) * dist >> 12);

        // 4. Scanline Rendering
        for (int x = 0; x < TEX_W; x++)
        {
            // 获取纹理坐标 (u, v)
            // >> 8 将定点数转为整数坐标
            int u = tx >> 8;
            int v = ty >> 8;

            uint8_t  idx   = get_map_pixel(u, v);
            uint16_t color = palette[idx];

            // Distance Fog (距离雾)
            // 越靠近地平线 (p越小)，颜色越暗
            if (p < 40)
            {
                if (p < 20)
                    color = 0; // 极远处理全黑
                else
                    color = (color >> 1) & 0x7BEF; // 半黑
            }

            *p_pixel++ = color;

            tx += dx;
            ty += dy;
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

struct effect_ops effect_0019 = {
    .name   = "NO.19 THE EVENT HORIZON",
    .init   = effect_init,
    .draw   = effect_draw,
    .deinit = effect_deinit,
};

REGISTER_EFFECT(effect_0019);
