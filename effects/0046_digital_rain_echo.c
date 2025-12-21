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
 *
 * Hardware Feature:
 * 1. GE FillRect (缓冲区净化) - 确保混合操作具有纯净的 Alpha 零底色
 * 2. GE Scaler (递归微缩) - 制造向视界深处坠落的纵深感
 * 3. GE Blending (Porter-Duff SRC_OVER) - 实现残影的非线性物理衰减
 * 4. DE CCM (色彩矩阵偏移) - 实时注入光谱噪声
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
#define FEEDBACK_MARGIN 4   // 每帧向内微缩的像素边距
#define TRAIL_DECAY     245 // 记忆保留率 (0-255)，决定拖尾长度

/* 降雨参数 */
#define RAIN_DENSITY 5  // 每帧注入的新雨滴数量
#define RAIN_MIN_LEN 5  // 雨滴最小长度
#define RAIN_MAX_LEN 15 // 雨滴最大长度

/* 动画参数 */
#define CCM_SHIFT_AMP 7 // 色彩偏移幅度位移 (sin >> 7)

/* 查找表参数 */
#define LUT_SIZE     512
#define LUT_MASK     511
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
            LOG_E("Night 46: CMA Alloc Failed.");
            if (i == 1)
                mpp_phy_free(g_tex_phy[0]);
            return -1;
        }
        g_tex_vir[i] = (uint16_t *)(unsigned long)g_tex_phy[i];
        memset(g_tex_vir[i], 0, TEX_SIZE);
    }

    // 2. 初始化正弦查找表 (Q12)
    for (int i = 0; i < LUT_SIZE; i++)
    {
        sin_lut[i] = (int)(sinf(i * PI / (LUT_SIZE / 2.0f)) * Q12_ONE);
    }

    // 3. 初始化“矩阵”调色板
    for (int i = 0; i < PALETTE_SIZE; i++)
    {
        float f = (float)i / 255.0f;
        int   r, g, b;

        if (i > 240)
        {
            // 头部高光：带绿色的亮白
            r = 200;
            g = 255;
            b = 200;
        }
        else
        {
            // 经典的矩阵绿渐变
            r = 0;
            g = (int)(255 * f);
            b = (int)(64 * f);
        }
        g_palette[i] = RGB2RGB565(r, g, b);
    }

    g_tick = 0;
    return 0;
}

#define GET_SIN(idx) (sin_lut[(idx) & LUT_MASK])

static void effect_draw(struct demo_ctx *ctx, unsigned long phy_addr)
{
    if (!g_tex_vir[0] || !g_tex_vir[1])
        return;

    int t       = g_tick;
    int src_idx = g_buf_idx;
    int dst_idx = 1 - g_buf_idx;

    /* --- PHASE 1: GE 硬件反馈 (制造向内坠落的记忆) --- */

    // 1. 彻底清空当前帧 (dst)，确保 Alpha 混合的背景是纯净的
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
    mpp_ge_sync(ctx->ge);

    // 2. 将上一帧 (src) 搬运到 dst，并进行微缩与 Alpha 衰减
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

    // 缩放逻辑：源全屏 -> 目标中心微缩，产生坠落感
    feedback.src_buf.crop_en     = 0;
    feedback.dst_buf.crop_en     = 1;
    feedback.dst_buf.crop.x      = FEEDBACK_MARGIN;
    feedback.dst_buf.crop.y      = FEEDBACK_MARGIN;
    feedback.dst_buf.crop.width  = TEX_WIDTH - (FEEDBACK_MARGIN * 2);
    feedback.dst_buf.crop.height = TEX_HEIGHT - (FEEDBACK_MARGIN * 2);

    // 混合逻辑：使用 SRC_OVER 结合全局 Alpha 实现残影衰减
    feedback.ctrl.alpha_en         = 0; // 开启混合
    feedback.ctrl.alpha_rules      = GE_PD_SRC_OVER;
    feedback.ctrl.src_alpha_mode   = 1;
    feedback.ctrl.src_global_alpha = TRAIL_DECAY;

    mpp_ge_bitblt(ctx->ge, &feedback);
    mpp_ge_emit(ctx->ge);
    mpp_ge_sync(ctx->ge);

    /* --- PHASE 2: CPU 注入新雨滴 (Direct Draw) --- */
    uint16_t *dst_p = g_tex_vir[dst_idx];

    for (int i = 0; i < RAIN_DENSITY; i++)
    {
        int x      = (rand() % TEX_WIDTH);
        int len    = RAIN_MIN_LEN + (rand() % (RAIN_MAX_LEN - RAIN_MIN_LEN));
        int speed  = 2 + (rand() % 3);
        int y_head = (t * speed + i * 50) % (TEX_HEIGHT + len);

        // 绘制一条垂直的亮度渐变线
        for (int j = 0; j < len; j++)
        {
            int y = y_head - j;
            if (y >= 0 && y < TEX_HEIGHT)
            {
                // 头部最亮，尾部消失在背景中
                int brightness           = 255 - (j * 255 / len);
                dst_p[y * TEX_WIDTH + x] = g_palette[brightness];
            }
        }
    }
    aicos_dcache_clean_range((void *)dst_p, TEX_SIZE);

    /* --- PHASE 3: 全屏拉伸上屏 --- */
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

    /* --- PHASE 4: DE CCM 动态光谱律动 --- */
    struct aicfb_ccm_config ccm = {0};
    ccm.enable                  = 1;

    // 让雨林在绿与青紫色之间进行细微的光谱振荡
    int shift         = GET_SIN(t) >> CCM_SHIFT_AMP;
    ccm.ccm_table[0]  = 0x100;
    ccm.ccm_table[5]  = 0x100 - ABS(shift);
    ccm.ccm_table[6]  = shift;
    ccm.ccm_table[10] = 0x100;

    mpp_fb_ioctl(ctx->fb, AICFB_UPDATE_CCM_CONFIG, &ccm);

    // 交换乒乓缓冲区索引
    g_buf_idx = dst_idx;
    g_tick++;
}

static void effect_deinit(struct demo_ctx *ctx)
{
    // 复位显示引擎
    struct aicfb_ccm_config ccm_reset = {0};
    ccm_reset.enable                  = 0;
    mpp_fb_ioctl(ctx->fb, AICFB_UPDATE_CCM_CONFIG, &ccm_reset);

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
