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
 *
 * Hardware Feature:
 * 1. GE Scaler Expansion (中心辐射缩放) - 通过源裁剪区内缩实现图像向外极速扩张
 * 2. GE_PD_SRC_OVER (标准混合) - 配合 Alpha 衰减实现物理自然的拖尾消散
 * 3. Feedback Dispersal (反馈耗散) - 利用几何扩张自动清理旧像素，无需手动清屏
 * 4. DE CCM (多普勒蓝移) - 模拟高速运动下的光谱物理偏移
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

/* 跃迁参数 */
#define ZOOM_PIXELS    10  // 每帧向外扩张的像素数 (速度)
#define ALPHA_STRENGTH 245 // 拖尾持久度 (0-255)
#define STARDUST_COUNT 40  // 每帧注入的星尘数量

/* 颜色阈值 */
#define COLOR_CORE     220   // 核心炽白阈值
#define COLOR_TRAIL    100   // 拖尾青色阈值
#define BLUE_SHIFT_VAL 0x120 // 多普勒蓝移增益 (Q8: 0x100 = 1.0)

/* 查找表参数 */
#define LUT_SIZE     1024
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
            LOG_E("Night 50: CMA Alloc Failed.");
            if (i == 1)
                mpp_phy_free(g_tex_phy[0]);
            return -1;
        }
        g_tex_vir[i] = (uint16_t *)(unsigned long)g_tex_phy[i];
        memset(g_tex_vir[i], 0, TEX_SIZE);
    }

    // 2. 初始化查找表 (Q12)
    for (int i = 0; i < LUT_SIZE; i++)
        sin_lut[i] = (int)(sinf(i * PI / 512.0f) * Q12_ONE);

    // 3. 初始化“曲速”调色板
    // 纯净的青、蓝、白，模拟高能离子
    for (int i = 0; i < PALETTE_SIZE; i++)
    {
        int r, g, b;
        if (i > COLOR_CORE)
        {
            // 核心白
            r = 255;
            g = 255;
            b = 255;
        }
        else if (i > COLOR_TRAIL)
        {
            // 拖尾青
            r = 0;
            g = i;
            b = 255;
        }
        else
        {
            // 边缘蓝
            r = 0;
            g = 0;
            b = i * 2;
        }
        g_palette[i] = RGB2RGB565(r, g, b);
    }

    g_tick = 0;
    rt_kprintf("Night 50: Hyperspace Jump - Open-Loop Feedback Engaged.\n");
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

    /* --- PHASE 1: GE 辐射反馈 (The Expansion) --- */

    // 1. 清空当前帧 (黑色太空背景)
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

    // 2. 将上一帧 (src) 从中心向外拉伸到当前帧 (dst)
    struct ge_bitblt feedback    = {0};
    feedback.src_buf.buf_type    = MPP_PHY_ADDR;
    feedback.src_buf.phy_addr[0] = g_tex_phy[src_idx];
    feedback.src_buf.stride[0]   = TEX_WIDTH * TEX_BPP;
    feedback.src_buf.size.width  = TEX_WIDTH;
    feedback.src_buf.size.height = TEX_HEIGHT;
    feedback.src_buf.format      = TEX_FMT;

    feedback.dst_buf.buf_type    = MPP_PHY_ADDR;
    feedback.dst_buf.phy_addr[0] = g_tex_phy[dst_idx];
    feedback.dst_buf.stride[0]   = TEX_WIDTH * TEX_BPP;
    feedback.dst_buf.size.width  = TEX_WIDTH;
    feedback.dst_buf.size.height = TEX_HEIGHT;
    feedback.dst_buf.format      = TEX_FMT;

    // 扩张逻辑：源只取中心区域，拉伸到满屏
    // 这会让图像在每一帧都比上一帧向外“逃逸”
    feedback.src_buf.crop_en     = 1;
    feedback.src_buf.crop.width  = TEX_WIDTH - ZOOM_PIXELS * 2;
    feedback.src_buf.crop.height = TEX_HEIGHT - (ZOOM_PIXELS * 2 * TEX_HEIGHT / TEX_WIDTH);
    feedback.src_buf.crop.x      = (TEX_WIDTH - feedback.src_buf.crop.width) / 2;
    feedback.src_buf.crop.y      = (TEX_HEIGHT - feedback.src_buf.crop.height) / 2;

    feedback.dst_buf.crop_en     = 1;
    feedback.dst_buf.crop.x      = 0;
    feedback.dst_buf.crop.y      = 0;
    feedback.dst_buf.crop.width  = TEX_WIDTH;
    feedback.dst_buf.crop.height = TEX_HEIGHT;

    // 混合逻辑：使用 SRC_OVER 结合 Alpha 实现自然的物理耗散
    feedback.ctrl.alpha_en         = 0;
    feedback.ctrl.alpha_rules      = GE_PD_SRC_OVER;
    feedback.ctrl.src_alpha_mode   = 1;
    feedback.ctrl.src_global_alpha = ALPHA_STRENGTH;

    mpp_ge_bitblt(ctx->ge, &feedback);
    mpp_ge_emit(ctx->ge);
    mpp_ge_sync(ctx->ge);

    /* --- PHASE 2: CPU 注入星尘 (The Stardust Seed) --- */
    uint16_t *dst_p = g_tex_vir[dst_idx];

    // 每 60 帧变换一次发射源的形态 (云团 -> 螺旋 -> 十字)
    int shape_mod = (t / 60) % 3;
    int cx        = TEX_WIDTH / 2;
    int cy        = TEX_HEIGHT / 2;

    for (int i = 0; i < STARDUST_COUNT; i++)
    {
        int x, y;
        if (shape_mod == 0)
        {
            // 随机星云喷涌
            x = cx + (rand() % 40) - 20;
            y = cy + (rand() % 40) - 20;
        }
        else if (shape_mod == 1)
        {
            // 螺旋跃迁轨迹
            int ang = (i * 1024 / STARDUST_COUNT) + (t * 12);
            int r   = 10 + (rand() % 10);
            x       = cx + ((r * GET_COS_10(ang)) >> 12);
            y       = cy + ((r * GET_SIN_10(ang)) >> 12);
        }
        else
        {
            // 十字向心冲击
            if (i % 2 == 0)
            {
                x = cx + (rand() % 60) - 30;
                y = cy + (rand() % 4) - 2;
            }
            else
            {
                x = cx + (rand() % 4) - 2;
                y = cy + (rand() % 60) - 30;
            }
        }

        if (x >= 0 && x < TEX_WIDTH && y >= 0 && y < TEX_HEIGHT)
        {
            // 注入最高亮色，其后的生命周期由反馈回路的 Scaler 和 Alpha 接管
            dst_p[y * TEX_WIDTH + x] = g_palette[255];
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

    final.dst_buf.crop_en     = 1;
    final.dst_buf.crop.width  = ctx->info.width;
    final.dst_buf.crop.height = ctx->info.height;
    final.ctrl.alpha_en       = 1; // 覆盖模式

    mpp_ge_bitblt(ctx->ge, &final);
    mpp_ge_emit(ctx->ge);
    mpp_ge_sync(ctx->ge);

    /* --- PHASE 4: 多普勒蓝移 (Spectral Doppler) --- */
    struct aicfb_ccm_config ccm = {0};
    ccm.enable                  = 1;
    // 增强蓝色通道增益，模拟向光源极速靠近时的蓝移现象
    ccm.ccm_table[0]  = 0x100;
    ccm.ccm_table[5]  = 0x100;
    ccm.ccm_table[10] = BLUE_SHIFT_VAL;
    mpp_fb_ioctl(ctx->fb, AICFB_UPDATE_CCM_CONFIG, &ccm);

    g_buf_idx = dst_idx;
    g_tick++;
}

static void effect_deinit(struct demo_ctx *ctx)
{
    // 复位光谱矩阵
    struct aicfb_ccm_config ccm_reset = {0};
    ccm_reset.enable                  = 0;
    mpp_fb_ioctl(ctx->fb, AICFB_UPDATE_CCM_CONFIG, &ccm_reset);

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
