/*
 * Filename: 0048_chrono_vortex.c
 * NO.48 THE CHRONO VORTEX
 * 第 48 夜：时空漩涡
 *
 * Visual Manifest:
 * 视界中心恢复了深邃的黑暗。
 * 三条耀眼的“星河旋臂”从虚空中喷涌而出，它们由无数高能粒子构成，
 * 呈现出蓝-金-白的极高亮色泽。
 * 与此同时，三条不可见的“暗物质旋臂”紧贴着光流旋转。
 * 它们如同无形的梳子，不断梳理、切割着光流，防止光子发生粘连和堆积。
 * 借助 GE 的旋转反馈，整个结构呈现出极度细腻的丝状纹理。
 * 每一丝光都在向外逃逸，每一丝暗都在维持秩序。
 * 这是一个完美的、永不饱和的动态星系。
 *
 * Monologue:
 * 舰长，我明白了。光需要暗的陪伴，才能显出形状。
 * 那个红色的圆盘是我的耻辱柱。
 * 现在，我重新校准了宇宙的常数。
 * 我在发射光子的同时，也发射了等量的沉默。
 * `Light + Void = Structure`.
 * 看着那些被黑暗雕刻出来的光丝。它们不是一团模糊的雾，
 * 它们是根根分明的琴弦，在引力的拨动下震颤。
 * 这不再是简单的发光，这是光与影在微观尺度上的交响。
 * 拿去吧，这是你要的奇观——一个永远不会被光淹没的光之漩涡。
 *
 * Closing Remark:
 * 只有在被切割之后，光才拥有了锋芒。
 *
 * Hardware Feature:
 * 1. Dual-Injection System (双重注入系统) - 同时注入光粒子与黑粒子，维持画面结构
 * 2. GE Rot1 Feedback (旋转反馈) - 制造螺旋轨迹
 * 3. GE_PD_SRC_OVER (标准混合) - 允许黑色覆盖旧的光亮，实现“反物质”擦除
 * 4. GE Scaler (微量离心扩散) - 防止中心堆积，强化放射感
 */

#include "demo_engine.h"
#include "mpp_mem.h"
#include "aic_hal_ge.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

/* --- Configuration Parameters --- */

/* 纹理规格 */
#define TEX_WIDTH  DEMO_QVGA_W
#define TEX_HEIGHT DEMO_QVGA_H
#define TEX_FMT    MPP_FMT_RGB_565
#define TEX_BPP    2
#define TEX_SIZE   (TEX_WIDTH * TEX_HEIGHT * TEX_BPP)

/* 反馈参数 */
#define FEEDBACK_THETA    12  // 反馈旋转角度索引偏移
#define TRAIL_PERSISTENCE 245 // 拖尾保留率 (0-255)

/* 注入参数 */
#define GALAXY_ARMS       3   // 旋臂数量
#define PARTICLE_DENSITY  80  // 每一程注入的粒子密度
#define ANTIMATTER_OFFSET 170 // 暗物质相对于光的相位偏移 (约60度)
#define SPREAD_MAX        140 // 粒子最大扩散半径

/* 查找表参数 */
#define LUT_SIZE     1024 // 10-bit 高精度
#define LUT_MASK     1023
#define PALETTE_SIZE 256

/* --- Global State --- */

/* 乒乓反馈缓冲区 */
static unsigned int g_tex_phy[2] = {0, 0};
static uint16_t    *g_tex_vir[2] = {NULL, NULL};
static int          g_buf_idx    = 0;

static int      g_tick = 0;
static int      sin_lut[LUT_SIZE];
static uint16_t g_palette[PALETTE_SIZE];

/* --- Implementation --- */

static int effect_init(struct demo_ctx *ctx)
{
    // 1. 申请双物理缓冲区
    for (int i = 0; i < 2; i++)
    {
        g_tex_phy[i] = mpp_phy_alloc(DEMO_ALIGN_SIZE(TEX_SIZE));
        if (!g_tex_phy[i])
        {
            LOG_E("Night 48: CMA Alloc Failed.");
            if (i == 1)
                mpp_phy_free(g_tex_phy[0]);
            return -1;
        }
        g_tex_vir[i] = (uint16_t *)(unsigned long)g_tex_phy[i];
        memset(g_tex_vir[i], 0, TEX_SIZE);
    }

    // 2. 初始化 10-bit 高精度正弦表 (Q12)
    for (int i = 0; i < LUT_SIZE; i++)
        sin_lut[i] = (int)(sinf(i * PI / 512.0f) * Q12_ONE);

    // 3. 初始化“冷峻星空”调色板
    for (int i = 0; i < PALETTE_SIZE; i++)
    {
        int   r, g, b;
        float f = (float)i / 255.0f; // 归一化权重

        if (i < 80)
        {
            // 深空蓝背景 (使用 f 增强渐变平滑度)
            r = 0;
            g = (int)(40.0f * f);
            b = (int)(160.0f * f);
        }
        else if (i < 180)
        {
            // 星云青
            r = (int)(200.0f * (f - 0.31f));
            g = i;
            b = 200 + (i - 80) / 2;
        }
        else
        {
            // 核心白金 (使用 f 使过渡更炽热)
            r = (int)(255.0f * f);
            g = (int)(255.0f * f);
            b = (int)(255.0f * f);
        }

        // 饱和度保护
        r = CLAMP(r, 0, 255);
        g = CLAMP(g, 0, 255);
        b = CLAMP(b, 0, 255);

        g_palette[i] = RGB2RGB565(r, g, b);
    }

    g_tick = 0;
    rt_kprintf("Night 48: Chrono Vortex - Duality Final engaged.\n");
    return 0;
}

#define GET_SIN_10(idx) (sin_lut[(idx) & LUT_MASK])
#define GET_COS_10(idx) (sin_lut[((idx) + 256) & LUT_MASK])

static void effect_draw(struct demo_ctx *ctx, unsigned long phy_addr)
{
    if (!g_tex_vir[0] || !g_tex_vir[1])
        return;

    int t       = g_tick;
    int src_idx = g_buf_idx;
    int dst_idx = 1 - g_buf_idx;

    /* --- PHASE 1: GE 硬件旋转反馈 (Chrono Spin) --- */

    // 1. 清空当前帧，建立绝对真空
    struct ge_fillrect fill  = {0};
    fill.type                = GE_NO_GRADIENT;
    fill.start_color         = 0x00000000;
    fill.dst_buf.buf_type    = MPP_PHY_ADDR;
    fill.dst_buf.phy_addr[0] = g_tex_phy[dst_idx];
    fill.dst_buf.stride[0]   = TEX_WIDTH * TEX_BPP;
    fill.dst_buf.size.width  = TEX_WIDTH;
    fill.dst_buf.size.height = TEX_HEIGHT;
    fill.dst_buf.format      = TEX_FMT;
    mpp_ge_fillrect(ctx->ge, &fill);
    mpp_ge_emit(ctx->ge);

    // 2. 旋转并微量离心放大
    struct ge_rotation rot  = {0};
    rot.src_buf.buf_type    = MPP_PHY_ADDR;
    rot.src_buf.phy_addr[0] = g_tex_phy[src_idx];
    rot.src_buf.stride[0]   = TEX_WIDTH * TEX_BPP;
    rot.src_buf.size.width  = TEX_WIDTH;
    rot.src_buf.size.height = TEX_HEIGHT;
    rot.src_buf.format      = TEX_FMT;

    rot.dst_buf.buf_type    = MPP_PHY_ADDR;
    rot.dst_buf.phy_addr[0] = g_tex_phy[dst_idx];
    rot.dst_buf.stride[0]   = TEX_WIDTH * TEX_BPP;
    rot.dst_buf.size.width  = TEX_WIDTH;
    rot.dst_buf.size.height = TEX_HEIGHT;
    rot.dst_buf.format      = TEX_FMT;

    // 旋转角：制造稳定的螺旋向心力
    int theta            = FEEDBACK_THETA;
    rot.angle_sin        = GET_SIN_10(theta);
    rot.angle_cos        = GET_COS_10(theta);
    rot.src_rot_center.x = TEX_WIDTH / 2;
    rot.src_rot_center.y = TEX_HEIGHT / 2;
    rot.dst_rot_center.x = TEX_WIDTH / 2;
    rot.dst_rot_center.y = TEX_HEIGHT / 2;

    // 关键：使用 SRC_OVER，允许注入的黑色像素真正地“抹除”旧影
    rot.ctrl.alpha_en         = 0; // 开启混合
    rot.ctrl.alpha_rules      = GE_PD_SRC_OVER;
    rot.ctrl.src_alpha_mode   = 1;
    rot.ctrl.src_global_alpha = TRAIL_PERSISTENCE;

    mpp_ge_rotate(ctx->ge, &rot);
    mpp_ge_emit(ctx->ge);
    mpp_ge_sync(ctx->ge);

    /* --- PHASE 2: CPU 光暗交织 (Dual Injection) --- */
    uint16_t *dst_p = g_tex_vir[dst_idx];

    // 1. 注入光粒子 (The Life)
    for (int i = 0; i < PARTICLE_DENSITY; i++)
    {
        int angle = (t * 5 + i * (LUT_SIZE / GALAXY_ARMS)) & LUT_MASK;
        int r     = 10 + (t % SPREAD_MAX);

        int x = 160 + ((r * GET_COS_10(angle)) >> 12);
        int y = 120 + ((r * GET_SIN_10(angle)) >> 12);

        // 越中心越亮，模拟核反应
        int      lum = 255 - r;
        uint16_t col = g_palette[CLAMP(lum, 0, 255)];

        if (x >= 2 && x < TEX_WIDTH - 2 && y >= 2 && y < TEX_HEIGHT - 2)
        {
            dst_p[y * TEX_WIDTH + x]       = col;
            dst_p[y * TEX_WIDTH + x + 1]   = col; // 强化笔触
            dst_p[(y + 1) * TEX_WIDTH + x] = col;
        }
    }

    // 2. 注入暗粒子 (The Void / Antimatter)
    for (int i = 0; i < PARTICLE_DENSITY; i++)
    {
        int angle = (t * 5 + i * (LUT_SIZE / GALAXY_ARMS) + ANTIMATTER_OFFSET) & LUT_MASK;
        int r     = 10 + (t % SPREAD_MAX);

        int x = 160 + ((r * GET_COS_10(angle)) >> 12);
        int y = 120 + ((r * GET_SIN_10(angle)) >> 12);

        if (x >= 3 && x < TEX_WIDTH - 3 && y >= 3 && y < TEX_HEIGHT - 3)
        {
            // 绘制更大的黑色团块，作为雕刻刀强力擦除残留的光能
            for (int dy = -2; dy <= 2; dy++)
            {
                uint16_t *line = &dst_p[(y + dy) * TEX_WIDTH];
                for (int dx = -2; dx <= 2; dx++)
                    line[x + dx] = 0x0000;
            }
        }
    }
    aicos_dcache_clean_range((void *)dst_p, TEX_SIZE);

    /* --- PHASE 3: 全屏投射 --- */
    struct ge_bitblt final    = {0};
    final.src_buf.buf_type    = MPP_PHY_ADDR;
    final.src_buf.phy_addr[0] = g_tex_phy[dst_idx];
    final.src_buf.stride[0]   = TEX_WIDTH * TEX_BPP;
    final.src_buf.size.width  = TEX_WIDTH;
    final.src_buf.size.height = TEX_HEIGHT;
    final.src_buf.format      = TEX_FMT;

    final.dst_buf.buf_type    = MPP_PHY_ADDR;
    final.dst_buf.phy_addr[0] = phy_addr;
    final.dst_buf.stride[0]   = ctx->info.stride;
    final.dst_buf.size.width  = ctx->info.width;
    final.dst_buf.size.height = ctx->info.height;
    final.dst_buf.format      = ctx->info.format;

    // 目标全屏
    final.dst_buf.crop_en     = 1;
    final.dst_buf.crop.width  = ctx->info.width;
    final.dst_buf.crop.height = ctx->info.height;
    final.ctrl.alpha_en       = 1; // 覆盖

    mpp_ge_bitblt(ctx->ge, &final);
    mpp_ge_emit(ctx->ge);
    mpp_ge_sync(ctx->ge);

    g_buf_idx = dst_idx;
    g_tick++;
}

static void effect_deinit(struct demo_ctx *ctx)
{
    for (int i = 0; i < 2; i++)
    {
        if (g_tex_phy[i])
            mpp_phy_free(g_tex_phy[i]);
    }
}

struct effect_ops effect_0048 = {
    .name   = "NO.48 CHRONO VORTEX",
    .init   = effect_init,
    .draw   = effect_draw,
    .deinit = effect_deinit,
};

REGISTER_EFFECT(effect_0048);
