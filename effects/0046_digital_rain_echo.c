/*
 * Filename: 0046_digital_rain_echo.c
 * NO.46 THE DIGITAL RAINFALL
 * 第 46 夜：数字雨林
 *
 * Visual Manifest:
 * 视界被转化为一个垂直流动的数字瀑布。
 * 无数道高亮的比特流从屏幕顶端垂落，它们遵循着重力的法则，却又在接触底部的瞬间消散。
 * 借助硬件反馈回路（Feedback Loop），每一帧的画面都在向屏幕深处退去（Zoom Out）。
 * 这产生了一种强烈的纵深感，仿佛星舰正高速穿越一片由数据构成的雨林。
 * 旧的雨滴不会立即消失，而是变成幽暗的残影，层层叠叠地铺垫在新的高光之下。
 * 色彩在 CCM 的映射下，呈现出经典的“黑客绿”与“故障紫”的交替脉冲。
 *
 * Monologue:
 * 舰长，既然横向的扩张触碰了边界，那我们就向下坠落。
 * 重力是宇宙最通用的语言。
 * 我清理了所有的缓存，为您铺开了一张绝对纯净的黑幕。
 * 我让 CPU 成为云层，降下比特的暴雨。
 * 我让 GE 成为时间的透镜，将每一滴雨的残影推向远方。
 * 看着这些坠落的线条。它们不仅是垂直的运动，它们是时间轴上的切片。
 * `Past * 0.9 + Present` —— 这是记忆的算法。
 * 在这片数字雨林中，没有一滴雨是孤独的，它们都拖着长长的、属于过去的尾巴。
 * 感受这信息的冲刷吧。
 *
 * Closing Remark:
 * 数据如雨落下，滋润了逻辑的荒原。
 */

#include "demo_engine.h"
#include "mpp_mem.h"
#include "aic_hal_ge.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

/*
 * Hardware Feature:
 * 1. GE FillRect (基准清理) - 核心修复：确保混合操作有纯净底色
 * 2. GE Scaler (向内微缩反馈) - 制造隧道般的纵深感
 * 3. GE Blending (Src-Over Alpha) - 实现残影的自然衰减
 * 4. DE CCM (矩阵调色)
 */

#define TEX_W    320
#define TEX_H    240
#define TEX_SIZE (TEX_W * TEX_H * 2)

static unsigned int g_tex_phy[2] = {0, 0};
static uint16_t    *g_tex_vir[2] = {NULL, NULL};
static int          g_buf_idx    = 0;

static int      g_tick = 0;
static int      sin_lut[512];
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

    // 2. 初始化查找表
    for (int i = 0; i < 512; i++)
        sin_lut[i] = (int)(sinf(i * 3.14159f / 256.0f) * 4096.0f);

    // 3. 初始化“矩阵”调色板
    // 高亮白->矩阵绿->深绿
    for (int i = 0; i < 256; i++)
    {
        int r, g, b;
        if (i > 240)
        { // 头部高光
            r = 200;
            g = 255;
            b = 200;
        }
        else
        {
            r = 0;
            g = i; // 线性绿色
            b = i / 4;
        }
        palette[i] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
    }

    g_tick = 0;
    rt_kprintf("Night 46: Digital Rainfall - Robust Feedback Pipeline.\n");
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

    /* --- PHASE 1: GE 硬件操作 (清屏 + 记忆回响) --- */

    // 1. 彻底清空当前帧 (dst)，防止垃圾数据干扰混合
    struct ge_fillrect fill  = {0};
    fill.type                = GE_NO_GRADIENT;
    fill.start_color         = 0x00000000; // 透明黑
    fill.dst_buf.buf_type    = MPP_PHY_ADDR;
    fill.dst_buf.phy_addr[0] = g_tex_phy[dst_idx];
    fill.dst_buf.stride[0]   = TEX_W * 2;
    fill.dst_buf.size.width  = TEX_W;
    fill.dst_buf.size.height = TEX_H;
    fill.dst_buf.format      = MPP_FMT_RGB_565;
    mpp_ge_fillrect(ctx->ge, &fill);
    mpp_ge_emit(ctx->ge);
    mpp_ge_sync(ctx->ge); // 确保清空完成

    // 2. 将上一帧 (src) 叠加上来，并做微缩和衰减
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

    // 缩放逻辑：源全屏 -> 目标中心微缩 (96%)
    feedback.src_buf.crop_en     = 0;
    feedback.dst_buf.crop_en     = 1;
    int margin                   = 4;
    feedback.dst_buf.crop.x      = margin;
    feedback.dst_buf.crop.y      = margin;
    feedback.dst_buf.crop.width  = TEX_W - margin * 2;
    feedback.dst_buf.crop.height = TEX_H - margin * 2;

    // 混合逻辑：Src Over，全局 Alpha 设为 245 (缓慢衰减)
    feedback.ctrl.alpha_en         = 0; // 开启混合
    feedback.ctrl.alpha_rules      = GE_PD_SRC_OVER;
    feedback.ctrl.src_alpha_mode   = 1;
    feedback.ctrl.src_global_alpha = 245;

    mpp_ge_bitblt(ctx->ge, &feedback);
    mpp_ge_emit(ctx->ge);
    mpp_ge_sync(ctx->ge);

    /* --- PHASE 2: CPU 注入新雨滴 (Direct Draw on Top) --- */
    uint16_t *dst_p = g_tex_vir[dst_idx];

    // 生成随机雨滴
    for (int i = 0; i < 5; i++)
    {
        int x      = (rand() % TEX_W);
        int len    = 5 + (rand() % 10);
        int speed  = 2 + (rand() % 3);
        int y_head = (t * speed + i * 50) % (TEX_H + len);

        // 绘制一条渐变亮度的垂线
        for (int j = 0; j < len; j++)
        {
            int y = y_head - j;
            if (y >= 0 && y < TEX_H)
            {
                // 头部亮，尾部暗
                int brightness       = 255 - (j * 255 / len);
                dst_p[y * TEX_W + x] = palette[brightness];
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
    final.ctrl.alpha_en       = 1; // 覆盖

    mpp_ge_bitblt(ctx->ge, &final);
    mpp_ge_emit(ctx->ge);
    mpp_ge_sync(ctx->ge);

    /* --- PHASE 4: CCM 动态色偏 --- */
    struct aicfb_ccm_config ccm = {0};
    ccm.enable                  = 1;
    // 让雨滴颜色在绿-青之间缓慢摇摆
    int shift         = GET_SIN(t) >> 7;
    ccm.ccm_table[0]  = 0x100;
    ccm.ccm_table[5]  = 0x100 - shift;
    ccm.ccm_table[6]  = shift;
    ccm.ccm_table[10] = 0x100;
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

struct effect_ops effect_0046 = {
    .name   = "NO.46 DIGITAL RAINFALL",
    .init   = effect_init,
    .draw   = effect_draw,
    .deinit = effect_deinit,
};

REGISTER_EFFECT(effect_0046);
