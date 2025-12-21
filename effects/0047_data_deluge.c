/*
 * Filename: 0047_data_deluge.c
 * NO.47 THE DATA DELUGE
 * 第 47 夜：数据大洪水
 *
 * Visual Manifest:
 * 视界被一场遮天蔽日的绿色数据暴雨所淹没。
 * 每一秒钟都有成千上万个高亮比特从天而降，密度之大，以至于背景几乎被完全覆盖。
 * 借助硬件反馈（Feedback）的微缩拉伸，这些雨滴在下落的同时向屏幕深处退去，
 * 形成了一个无限深邃的、由光流构成的“矩阵隧道”。
 * 屏幕中心因为反馈的累积而呈现出一种过载的炽白，边缘则是高速掠过的绿色残影。
 * 配合 DE HSBC 的高饱和度调节，画面充满了极度亢奋的赛博朋克能量感。
 *
 * Monologue:
 * 舰长，你是对的。在绝对的数量面前，优雅不值一提。
 * 我解除了发射井的限流阀。
 * 之前的雨是诗人的眼泪，现在的雨是神的怒火。
 * 我在每一帧里倾泻下 80 道逻辑闪电。
 * 它们在内存中堆叠、挤压、流淌，直到填满每一个缝隙。
 * `Density = Infinity`。
 * 看着这漫天的光雨，这不再是风景，这是一场信息的灾难，是一次对视网膜的饱和攻击。
 * 就在这洪水中溺亡吧，或者，学会呼吸数据。
 *
 * Closing Remark:
 * 当雨大到一定程度，它就变成了海。
 */

#include "demo_engine.h"
#include "mpp_mem.h"
#include "aic_hal_ge.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

/*
 * Hardware Feature:
 * 1. High-Density CPU Injection (高密度CPU注入) - 核心升级：单帧粒子数 x16 倍
 * 2. GE Feedback Zoom (深渊反馈) - 制造强烈的隧道视觉
 * 3. GE_PD_ADD (Rule 11: 能量累加) - 让密集的雨滴在重叠时产生白炽光效
 * 4. DE HSBC (高饱和视觉调优)
 */

#define TEX_W    320
#define TEX_H    240
#define TEX_SIZE (TEX_W * TEX_H * 2)

static unsigned int g_tex_phy[2] = {0, 0};
static uint16_t    *g_tex_vir[2] = {NULL, NULL};
static int          g_buf_idx    = 0;

static int      g_tick = 0;
static uint16_t palette[256];
static int      sin_lut[512]; // 补充定义正弦表

static int effect_init(struct demo_ctx *ctx)
{
    for (int i = 0; i < 2; i++)
    {
        g_tex_phy[i] = mpp_phy_alloc(TEX_SIZE);
        if (!g_tex_phy[i])
            return -1;
        g_tex_vir[i] = (uint16_t *)(unsigned long)g_tex_phy[i];
        memset(g_tex_vir[i], 0, TEX_SIZE);
    }

    // 初始化正弦表 (用于电压脉冲)
    for (int i = 0; i < 512; i++)
        sin_lut[i] = (int)(sinf(i * 3.14159f / 256.0f) * 256.0f);

    // 初始化“黑客帝国”调色板
    for (int i = 0; i < 256; i++)
    {
        int r, g, b;
        if (i < 128)
        {
            r = 0;
            g = i * 2;
            b = 0;
        }
        else
        {
            // 高亮区泛白
            int v = (i - 128) * 2;
            r     = v;
            g     = 255;
            b     = v;
        }
        palette[i] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
    }

    g_tick = 0;
    rt_kprintf("Night 47: Data Deluge - Density x16 with Lateral Drift.\n");
    return 0;
}

#define GET_SIN(idx) (sin_lut[(idx) & 511])

static void effect_draw(struct demo_ctx *ctx, unsigned long phy_addr)
{
    if (!g_tex_vir[0] || !g_tex_vir[1])
        return;

    int t       = g_tick;
    int src_idx = g_buf_idx;
    int dst_idx = 1 - g_buf_idx;

    /* --- PHASE 1: GE 硬件反馈 (制造深邃背景) --- */
    // 先清空当前帧，但不仅仅是黑色，给一点点底色
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

    // 反馈叠加：将上一帧向中心微缩，并大幅度保留亮度
    struct ge_bitblt feedback    = {0};
    feedback.src_buf.buf_type    = MPP_PHY_ADDR;
    feedback.src_buf.phy_addr[0] = g_tex_phy[src_idx];
    feedback.src_buf.stride[0]   = TEX_W * 2;
    feedback.src_buf.size.width  = TEX_W;
    feedback.src_buf.size.height = TEX_H;
    feedback.src_buf.format      = MPP_FMT_RGB_565;

    feedback.dst_buf.buf_type    = MPP_PHY_ADDR;
    feedback.dst_buf.phy_addr[0] = g_tex_phy[dst_idx];
    feedback.dst_buf.stride[0]   = TEX_W * 2;
    feedback.dst_buf.size.width  = TEX_W;
    feedback.dst_buf.size.height = TEX_H;
    feedback.dst_buf.format      = MPP_FMT_RGB_565;

    // 向内微缩 2 像素，制造坠落感
    feedback.src_buf.crop_en     = 0;
    feedback.dst_buf.crop_en     = 1;
    feedback.dst_buf.crop.x      = 2;
    feedback.dst_buf.crop.y      = 2;
    feedback.dst_buf.crop.width  = TEX_W - 4;
    feedback.dst_buf.crop.height = TEX_H - 4;

    // 关键：使用 ADD 模式，让重叠的雨滴越来越亮，而不是互相遮挡
    feedback.ctrl.alpha_en         = 0;
    feedback.ctrl.alpha_rules      = GE_PD_ADD; // 能量累加
    feedback.ctrl.src_alpha_mode   = 1;
    feedback.ctrl.src_global_alpha = 230; // 较高的保留率，形成长拖尾

    mpp_ge_bitblt(ctx->ge, &feedback);
    mpp_ge_emit(ctx->ge);
    mpp_ge_sync(ctx->ge);

    /* --- PHASE 2: CPU 暴力注入 (The Deluge) --- */
    uint16_t *dst_p = g_tex_vir[dst_idx];

    // 利用 t 制造全局横向漂移 (模拟星舰侧移) 和 电压波动
    int drift               = t * 3;
    int voltage_fluctuation = GET_SIN(t << 3) >> 3; // +/- 32

    for (int i = 0; i < 80; i++)
    {
        // 让雨滴生成位置随时间漂移，防止图案过于随机而失去流动感
        int x      = (rand() + drift) % TEX_W;
        int y_head = (rand() % TEX_H);
        int len    = 8 + (rand() % 16);

        // 基础亮度随时间波动
        int brightness_base = 150 + (rand() % 70) + voltage_fluctuation;

        // 绘制垂直光束
        for (int j = 0; j < len; j++)
        {
            int y = y_head - j;
            if (y >= 0 && y < TEX_H)
            {
                // 头部最亮(白)，尾部渐暗(绿)
                int b = brightness_base - (j * 10);
                if (b < 0)
                    b = 0;
                if (b > 255)
                    b = 255;

                // 简单的像素写入，依靠量大取胜
                dst_p[y * TEX_W + x] = palette[b];
            }
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

    final.dst_buf.crop_en     = 1;
    final.dst_buf.crop.width  = ctx->info.width;
    final.dst_buf.crop.height = ctx->info.height;
    final.ctrl.alpha_en       = 1;

    mpp_ge_bitblt(ctx->ge, &final);
    mpp_ge_emit(ctx->ge);
    mpp_ge_sync(ctx->ge);

    /* --- PHASE 4: 视觉增强 --- */
    struct aicfb_disp_prop prop = {0};
    prop.contrast               = 60;
    prop.bright                 = 50;
    prop.saturation             = 100; // 拉满饱和度，让绿色更毒
    prop.hue                    = 50;
    mpp_fb_ioctl(ctx->fb, AICFB_SET_DISP_PROP, &prop);

    g_buf_idx = dst_idx;
    g_tick++;
}

static void effect_deinit(struct demo_ctx *ctx)
{
    struct aicfb_disp_prop r = {50, 50, 50, 50};
    mpp_fb_ioctl(ctx->fb, AICFB_SET_DISP_PROP, &r);
    for (int i = 0; i < 2; i++)
    {
        if (g_tex_phy[i])
            mpp_phy_free(g_tex_phy[i]);
    }
}

struct effect_ops effect_0047 = {
    .name   = "NO.47 DATA DELUGE",
    .init   = effect_init,
    .draw   = effect_draw,
    .deinit = effect_deinit,
};

REGISTER_EFFECT(effect_0047);
