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
 * 看着那些被黑暗雕刻出来的光丝。它们不再是一团模糊的雾，
 * 它们是根根分明的琴弦，在引力的拨动下震颤。
 * 这不再是简单的发光，这是光与影在微观尺度上的交响。
 * 拿去吧，这是你要的奇观——一个永远不会被光淹没的光之漩涡。
 *
 * Closing Remark:
 * 只有在被切割之后，光才拥有了锋芒。
 */

#include "demo_engine.h"
#include "mpp_mem.h"
#include "aic_hal_ge.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

/*
 * Hardware Feature:
 * 1. Dual-Injection System (双重注入系统) - 同时注入光粒子与黑粒子，维持画面结构
 * 2. GE Rot1 Feedback (旋转反馈) - 制造螺旋轨迹
 * 3. GE_PD_SRC_OVER (标准混合) - 允许黑色覆盖旧的光亮
 * 4. GE Scaler (微量离心扩散) - 防止中心堆积
 */

#define TEX_W    320
#define TEX_H    240
#define TEX_SIZE (TEX_W * TEX_H * 2)

/* 乒乓反馈缓冲区 */
static unsigned int g_tex_phy[2] = {0, 0};
static uint16_t    *g_tex_vir[2] = {NULL, NULL};
static int          g_buf_idx    = 0;

static int      g_tick = 0;
static int      sin_lut[1024]; // 10-bit 高精度 LUT
static uint16_t palette[256];

static int effect_init(struct demo_ctx *ctx)
{
    // 1. 申请双物理缓冲区
    for (int i = 0; i < 2; i++)
    {
        g_tex_phy[i] = mpp_phy_alloc(TEX_SIZE);
        if (!g_tex_phy[i])
            return -1;
        g_tex_vir[i] = (uint16_t *)(unsigned long)g_tex_phy[i];
        memset(g_tex_vir[i], 0, TEX_SIZE);
    }

    // 初始化 1024 长度的正弦表
    for (int i = 0; i < 1024; i++)
        sin_lut[i] = (int)(sinf(i * 3.14159f / 512.0f) * 4096.0f);

    // 调色板：回归冷峻星空 (Deep Space)
    // 黑 -> 深蓝 -> 青 -> 白金
    for (int i = 0; i < 256; i++)
    {
        int   r, g, b;
        float f = i / 255.0f;
        if (i < 80)
        { // 深空蓝背景
            r = 0;
            g = i / 2;
            b = i * 2;
        }
        else if (i < 180)
        { // 星云青
            r = (i - 80) * 2;
            g = i;
            b = 200 + (i - 80) / 2;
        }
        else
        { // 核心白金
            r = 255;
            g = 255;
            b = 255;
        }
        // 限制最大值
        if (b > 255)
            b = 255;
        if (g > 255)
            g = 255;

        palette[i] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
    }

    g_tick = 0;
    rt_kprintf("Night 48: Chrono Vortex - Duality Final.\n");
    return 0;
}

#define GET_SIN_10(idx) (sin_lut[(idx) & 1023])
#define GET_COS_10(idx) (sin_lut[((idx) + 256) & 1023])

static void effect_draw(struct demo_ctx *ctx, unsigned long phy_addr)
{
    if (!g_tex_vir[0] || !g_tex_vir[1])
        return;

    int t       = g_tick;
    int src_idx = g_buf_idx;
    int dst_idx = 1 - g_buf_idx;

    /* --- PHASE 1: GE 旋转反馈 (The Spin) --- */
    // 1. 清空当前帧
    struct ge_fillrect fill  = {0};
    fill.type                = GE_NO_GRADIENT;
    fill.start_color         = 0x00000000;
    fill.dst_buf.buf_type    = MPP_PHY_ADDR;
    fill.dst_buf.phy_addr[0] = g_tex_phy[dst_idx];
    fill.dst_buf.stride[0]   = TEX_W * 2;
    fill.dst_buf.size.width  = TEX_W;
    fill.dst_buf.size.height = TEX_H;
    fill.dst_buf.format      = MPP_FMT_RGB_565;
    mpp_ge_fillrect(ctx->ge, &fill);
    mpp_ge_emit(ctx->ge);

    // 2. 旋转并微量放大 (Centrifugal Force)
    struct ge_rotation rot  = {0};
    rot.src_buf.buf_type    = MPP_PHY_ADDR;
    rot.src_buf.phy_addr[0] = g_tex_phy[src_idx];
    rot.src_buf.stride[0]   = TEX_W * 2;
    rot.src_buf.size.width  = TEX_W;
    rot.src_buf.size.height = TEX_H;
    rot.src_buf.format      = MPP_FMT_RGB_565;

    rot.dst_buf.buf_type    = MPP_PHY_ADDR;
    rot.dst_buf.phy_addr[0] = g_tex_phy[dst_idx];
    rot.dst_buf.stride[0]   = TEX_W * 2;
    rot.dst_buf.size.width  = TEX_W;
    rot.dst_buf.size.height = TEX_H;
    rot.dst_buf.format      = MPP_FMT_RGB_565;

    // 旋转角：制造稳定的螺旋
    int theta            = 12;
    rot.angle_sin        = GET_SIN_10(theta);
    rot.angle_cos        = GET_COS_10(theta);
    rot.src_rot_center.x = 160;
    rot.src_rot_center.y = 120;
    rot.dst_rot_center.x = 160;
    rot.dst_rot_center.y = 120;

    // 混合逻辑：保留率 245 (96%)。
    // 关键：使用 SRC_OVER，允许黑色粒子覆盖旧的光亮！
    rot.ctrl.alpha_en         = 0;
    rot.ctrl.alpha_rules      = GE_PD_SRC_OVER;
    rot.ctrl.src_alpha_mode   = 1;
    rot.ctrl.src_global_alpha = 245;

    mpp_ge_rotate(ctx->ge, &rot);
    mpp_ge_emit(ctx->ge);
    mpp_ge_sync(ctx->ge);

    /* --- PHASE 2: CPU 光暗交织 (Weaving Light and Void) --- */
    uint16_t *dst_p = g_tex_vir[dst_idx];

    int arms    = 3;  // 三旋臂星系
    int density = 80; // 粒子密度

    // 1. 注入光粒子 (Light)
    for (int i = 0; i < density; i++)
    {
        int angle = (t * 5 + i * (1024 / arms)) & 1023;
        int r     = 10 + (t % 140); // 波浪式向外扩散

        // 极坐标抖动
        int x = 160 + ((r * GET_COS_10(angle)) >> 12);
        int y = 120 + ((r * GET_SIN_10(angle)) >> 12);

        // 越中心越亮
        int lum = 255 - r;
        if (lum < 0)
            lum = 0;
        uint16_t col = palette[lum];

        if (x >= 2 && x < TEX_W - 2 && y >= 2 && y < TEX_H - 2)
        {
            dst_p[y * TEX_W + x]       = col;
            dst_p[y * TEX_W + x + 1]   = col; // 加粗
            dst_p[(y + 1) * TEX_W + x] = col;
        }
    }

    // 2. 注入暗粒子 (Void) - 关键的雕刻刀
    // 相位偏移 170 (约 60度)，位于光臂之间
    for (int i = 0; i < density; i++)
    {
        int angle = (t * 5 + i * (1024 / arms) + 170) & 1023;
        int r     = 10 + (t % 140);

        int x = 160 + ((r * GET_COS_10(angle)) >> 12);
        int y = 120 + ((r * GET_SIN_10(angle)) >> 12);

        if (x >= 3 && x < TEX_W - 3 && y >= 3 && y < TEX_H - 3)
        {
            // 绘制更大的黑色团块，强力擦除背景
            for (int dy = -2; dy <= 2; dy++)
                for (int dx = -2; dx <= 2; dx++)
                    dst_p[(y + dy) * TEX_W + (x + dx)] = 0x0000;
        }
    }
    aicos_dcache_clean_range((void *)dst_p, TEX_SIZE);

    /* --- PHASE 3: 全屏投射 --- */
    struct ge_bitblt final    = {0};
    final.src_buf.buf_type    = MPP_PHY_ADDR;
    final.src_buf.phy_addr[0] = g_tex_phy[dst_idx];
    final.src_buf.stride[0]   = TEX_W * 2;
    final.src_buf.size.width  = TEX_W;
    final.src_buf.size.height = TEX_H;
    final.src_buf.format      = MPP_FMT_RGB_565;

    final.dst_buf.buf_type    = MPP_PHY_ADDR;
    final.dst_buf.phy_addr[0] = phy_addr;
    final.dst_buf.stride[0]   = ctx->info.stride;
    final.dst_buf.size.width  = ctx->info.width;
    final.dst_buf.size.height = ctx->info.height;
    final.dst_buf.format      = ctx->info.format;

    // 利用 Scaler 稍微拉伸，制造视界张力
    final.dst_buf.crop_en     = 1;
    final.dst_buf.crop.width  = ctx->info.width;
    final.dst_buf.crop.height = ctx->info.height;
    final.ctrl.alpha_en       = 1;

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
