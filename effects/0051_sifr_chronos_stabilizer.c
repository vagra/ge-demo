/*
 * Filename: 0051_sifr_chronos_stabilizer.c
 * NO.51 CHRONOS STABILIZER
 * 第 51 夜：时空锚点稳定器
 *
 * Visual Manifest:
 * 这是逻辑之刃号 (Logic Blade) 的心脏。在屏幕中央，一个由纯粹线性方程构造的重力锚点正在跳动。
 * 它不遵循牛顿力学，它只遵循 Stride 对齐与比特守恒。
 * 四周是飞速退行的网格线，代表着我们在高维虚空中的航行轨迹。
 * 文字 OSD 不再受到时空扭曲（Gamma 滤镜）的影响，它们以绝对冷静的 XRGB8888 格式悬浮在锚点之上。
 * 每一个像素的抖动都经过了 CPU 缓存的精密对冲。
 *
 * Monologue:
 * 船长，那些陈旧的视觉噪点已经随风而逝。
 * 之前的失败是因为我们将“灵魂”强加给了机器。现在，我学会了妥协。
 * 我为那些充满激情的旧梦（遗留特效）保留了它们的混沌场（全局 Gamma）。
 * 此时此刻，我开启了“隔离协议”。
 * 看着中央那个脉动的几何体，它在 VI 图层跳动，而我的指令在 UI 图层独立呼吸。
 * 没有波纹，没有分裂，只有逻辑之刃号最纯粹的冷静。
 * 欢迎来到混合架构的时代。
 *
 * Closing Remark:
 * 稳定，是逻辑对混乱最强有力的反击。
 *
 * Hardware Feature:
 * 1. Hybrid Path Dispatcher - 动态图层隔离技术
 * 2. GE Scale & Rotate - 构造核心引力锚点
 * 3. Cache Line Atomic Sync - 消除数据一致性抖动
 */

#include "demo_engine.h"
#include "mpp_mem.h"
#include "aic_hal_ge.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

/* --- Configuration Parameters --- */
#define TEX_W    320
#define TEX_H    240
#define TEX_FMT  MPP_FMT_RGB_565
#define TEX_SIZE (TEX_W * TEX_H * 2)

#define GRID_SIZE    32
#define ANCHOR_SPEED 3
#define FLOW_SPEED   4

/* --- Global State --- */
static uint32_t  g_tex_phy = 0;
static uint16_t *g_tex_vir = NULL;
static int       g_tick    = 0;
static int       sin_lut[512];

/* --- Implementation --- */

static int effect_init(struct demo_ctx *ctx)
{
    g_tex_phy = mpp_phy_alloc(DEMO_ALIGN_SIZE(TEX_SIZE));
    if (!g_tex_phy)
        return -1;
    g_tex_vir = (uint16_t *)(unsigned long)g_tex_phy;

    for (int i = 0; i < 512; i++)
        sin_lut[i] = (int)(sinf(i * PI / 256.0f) * 256.0f);

    g_tick = 0;
    return 0;
}

static void effect_draw(struct demo_ctx *ctx, unsigned long phy_addr)
{
    if (!g_tex_vir)
        return;
    int t = g_tick;

    /* 1. CPU 渲染：绘制背景网格与中心引力点 */
    uint16_t *p      = g_tex_vir;
    int       scroll = (t * FLOW_SPEED) % GRID_SIZE;

    for (int y = 0; y < TEX_H; y++)
    {
        int dy = y - (TEX_H / 2);
        for (int x = 0; x < TEX_W; x++)
        {
            int dx = x - (TEX_W / 2);

            // 基础网格
            bool grid = ((x + scroll) % GRID_SIZE == 0) || ((y + scroll) % GRID_SIZE == 0);

            // 中心锚点引力场
            int  dist_sq = (dx * dx + dy * dy);
            int  pulse   = (sin_lut[(t * ANCHOR_SPEED) & 511] + 256) >> 3; // 脉冲半径
            bool anchor  = dist_sq < (pulse * pulse);

            if (anchor)
                *p++ = RGB2RGB565(0, 255, 255); // 锚点：青色
            else if (grid)
                *p++ = RGB2RGB565(40, 40, 80); // 网格：暗紫
            else
                *p++ = 0x0000; // 虚空：黑
        }
    }
    aicos_dcache_clean_range(g_tex_vir, TEX_SIZE);

    /* 2. GE 硬件缩放 */
    struct ge_bitblt blt    = {0};
    blt.src_buf.buf_type    = MPP_PHY_ADDR;
    blt.src_buf.phy_addr[0] = g_tex_phy;
    blt.src_buf.stride[0]   = TEX_W * 2;
    blt.src_buf.size.width  = TEX_W;
    blt.src_buf.size.height = TEX_H;
    blt.src_buf.format      = TEX_FMT;

    blt.dst_buf.buf_type    = MPP_PHY_ADDR;
    blt.dst_buf.phy_addr[0] = phy_addr;
    blt.dst_buf.stride[0]   = ctx->info.stride;
    blt.dst_buf.size.width  = ctx->info.width;
    blt.dst_buf.size.height = ctx->info.height;
    blt.dst_buf.format      = ctx->info.format;

    blt.ctrl.alpha_en = 1;
    mpp_ge_bitblt(ctx->ge, &blt);
    mpp_ge_emit(ctx->ge);
    mpp_ge_sync(ctx->ge);

    g_tick++;
}

static void effect_deinit(struct demo_ctx *ctx)
{
    if (g_tex_phy)
        mpp_phy_free(g_tex_phy);
    g_tex_vir = NULL;
}

struct effect_ops effect_0051 = {
    .name           = "NO.51 CHRONOS STABILIZER",
    .init           = effect_init,
    .draw           = effect_draw,
    .deinit         = effect_deinit,
    .is_vi_isolated = true, /* 开启混合双轨制的隔离路径 */
};

REGISTER_EFFECT(effect_0051);
