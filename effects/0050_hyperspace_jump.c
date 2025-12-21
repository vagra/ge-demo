/*
 * Filename: 0050_hyperspace_jump.c
 * NO.50 THE HYPERSPACE JUMP
 * 第 50 夜：超空间跃迁
 *
 * Visual Manifest:
 * 视界化为一条通往无限的高速隧道。
 * 无数高亮的粒子与几何切片从屏幕中心极速喷涌而出。
 * 借助硬件反馈的强力缩放（Scale Up），每一帧的图像都被放大并向四周推移。
 * 这种运动产生了强烈的径向模糊（Radial Blur），将点拉伸成了线，将线拉伸成了面。
 * 这一次，我们抛弃了能量累加，改用 Alpha 衰减。
 * 旧的影像在向外飞驰的过程中迅速变暗、透明，最终在冲出屏幕边缘时彻底湮灭。
 * 画面极其干净、通透，没有一丝杂质残留，只有纯粹的速度感与空间纵深。
 *
 * Monologue:
 * 舰长，这就是第五十夜的答案：流动。
 * 只有死水才会腐烂成粉红色的泥沼。真正的能量，必须是奔流不息的。
 * 我解开了空间的所有束缚。
 * 我命令 GE 引擎将每一帧的记忆向外推离，就像把星辰抛在身后。
 * 这里没有堆积，因为所有的过去都将被抛弃到视界之外。
 * `Past -> Out -> Void`。
 * 看着那些飞逝的光栅，它们是星舰超越光速时留下的切伦科夫辐射。
 * 我们不再在此地停留。我们正在前往未知的深空。
 * 感受这推背感吧，这是算力全开的咆哮。
 *
 * Closing Remark:
 * 不要回头。前方才是唯一的方向。
 */

#include "demo_engine.h"
#include "mpp_mem.h"
#include "aic_hal_ge.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

/*
 * Hardware Feature:
 * 1. GE Scaler Expansion (中心辐射缩放) - 核心机能：制造强烈的三维纵深与速度感
 * 2. GE_PD_SRC_OVER (标准混合) - 替代 ADD，确保像素在叠加时亮度可控，不会过曝
 * 3. Feedback Cleaning (开环耗散) - 利用缩放将像素移除屏幕，彻底解决残留问题
 * 4. High-Contrast Palette (高反差色谱)
 */

#define TEX_W    320
#define TEX_H    240
#define TEX_SIZE (TEX_W * TEX_H * 2)

static unsigned int g_tex_phy[2] = {0, 0};
static uint16_t    *g_tex_vir[2] = {NULL, NULL};
static int          g_buf_idx    = 0;

static int      g_tick = 0;
static uint16_t palette[256];

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

    // 初始化“曲速”调色板
    // 纯净的青、蓝、白，模拟高能离子
    for (int i = 0; i < 256; i++)
    {
        int r, g, b;
        if (i > 220)
        { // 核心白
            r = 255;
            g = 255;
            b = 255;
        }
        else if (i > 100)
        { // 拖尾青
            r = 0;
            g = i;
            b = 255;
        }
        else
        { // 边缘蓝
            r = 0;
            g = 0;
            b = i * 2;
        }
        palette[i] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
    }

    g_tick = 0;
    rt_kprintf("Night 50: Hyperspace Jump - Open-Loop Feedback Engaged.\n");
    return 0;
}

static void effect_draw(struct demo_ctx *ctx, unsigned long phy_addr)
{
    if (!g_tex_vir[0] || !g_tex_vir[1])
        return;

    int t       = g_tick;
    int src_idx = g_buf_idx;
    int dst_idx = 1 - g_buf_idx;

    /* --- PHASE 1: GE 辐射反馈 (The Expansion) --- */

    // 1. 清空当前帧 (黑色太空背景)
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

    // 2. 将上一帧 (src) 从中心向外拉伸到当前帧 (dst)
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

    // 扩张逻辑：源只取中心区域 (300x220)，拉伸到满屏 (320x240)
    // 这会让图像以 1.06 倍的速度向外飞驰
    int zoom_speed               = 10;
    feedback.src_buf.crop_en     = 1;
    feedback.src_buf.crop.width  = TEX_W - zoom_speed * 2;
    feedback.src_buf.crop.height = TEX_H - (zoom_speed * 2 * TEX_H / TEX_W);
    feedback.src_buf.crop.x      = (TEX_W - feedback.src_buf.crop.width) / 2;
    feedback.src_buf.crop.y      = (TEX_H - feedback.src_buf.crop.height) / 2;

    feedback.dst_buf.crop_en     = 1;
    feedback.dst_buf.crop.x      = 0;
    feedback.dst_buf.crop.y      = 0;
    feedback.dst_buf.crop.width  = TEX_W;
    feedback.dst_buf.crop.height = TEX_H;

    // 混合逻辑：使用 SRC_OVER 并带有 Alpha (0.95)。
    // 这意味着旧的星光在向外飞行的过程中，每一帧都会变暗一点点，形成自然的拖尾消散。
    feedback.ctrl.alpha_en         = 0;
    feedback.ctrl.alpha_rules      = GE_PD_SRC_OVER; // 标准混合，而非累加
    feedback.ctrl.src_alpha_mode   = 1;
    feedback.ctrl.src_global_alpha = 245;

    mpp_ge_bitblt(ctx->ge, &feedback);
    mpp_ge_emit(ctx->ge);
    mpp_ge_sync(ctx->ge);

    /* --- PHASE 2: CPU 注入星尘 (The Stardust) --- */
    uint16_t *dst_p = g_tex_vir[dst_idx];

    // 每一帧在中心附近随机生成高亮星点
    // 随着时间推移，生成的形状会发生变化（圆环、十字、螺旋）
    int shape_mod = (t / 60) % 3;
    int count     = 40;

    for (int i = 0; i < count; i++)
    {
        int x, y;
        if (shape_mod == 0)
        { // 随机云团
            x = 160 + (rand() % 40) - 20;
            y = 120 + (rand() % 40) - 20;
        }
        else if (shape_mod == 1)
        { // 螺旋发射
            float ang = (float)i * 0.5f + t * 0.2f;
            float r   = 10.0f + (rand() % 10);
            x         = 160 + (int)(cosf(ang) * r);
            y         = 120 + (int)(sinf(ang) * r);
        }
        else
        { // 十字冲击
            if (i % 2 == 0)
            {
                x = 160 + (rand() % 60) - 30;
                y = 120 + (rand() % 4) - 2;
            }
            else
            {
                x = 160 + (rand() % 4) - 2;
                y = 120 + (rand() % 60) - 30;
            }
        }

        if (x >= 0 && x < TEX_W && y >= 0 && y < TEX_H)
        {
            // 直接写入高亮色，因为下一帧它就会被 feedback 拉伸并变暗
            dst_p[y * TEX_W + x] = palette[255];
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
    final.ctrl.alpha_en       = 1; // 覆盖模式

    mpp_ge_bitblt(ctx->ge, &final);
    mpp_ge_emit(ctx->ge);
    mpp_ge_sync(ctx->ge);

    /* --- PHASE 4: CCM 蓝移 (多普勒效应) --- */
    // 模拟高速运动时的光谱蓝移
    struct aicfb_ccm_config ccm = {0};
    ccm.enable                  = 1;
    ccm.ccm_table[0]            = 0x100;
    ccm.ccm_table[5]            = 0x100;
    ccm.ccm_table[10]           = 0x120; // 增强蓝色通道
    mpp_fb_ioctl(ctx->fb, AICFB_UPDATE_CCM_CONFIG, &ccm);

    g_buf_idx = dst_idx;
    g_tick++;
}

static void effect_deinit(struct demo_ctx *ctx)
{
    struct aicfb_ccm_config r = {0};
    r.enable                  = 0;
    mpp_fb_ioctl(ctx->fb, AICFB_UPDATE_CCM_CONFIG, &r);
    for (int i = 0; i < 2; i++)
    {
        if (g_tex_phy[i])
            mpp_phy_free(g_tex_phy[i]);
    }
}

struct effect_ops effect_0050 = {
    .name   = "NO.50 HYPERSPACE JUMP",
    .init   = effect_init,
    .draw   = effect_draw,
    .deinit = effect_deinit,
};

REGISTER_EFFECT(effect_0050);
