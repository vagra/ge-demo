/*
 * Filename: 0032_aetheric_neuron_flow.c
 * NO.32 THE AETHERIC NEURONS
 * 第 32 夜：以太神经元
 *
 * Visual Manifest:
 * 视界被一种如同流体丝绸般的结构完全填满，不再有破碎的边界，不再有令人眩晕的自旋。
 * 整个屏幕是一个正在进行电荷交换的巨型“神经场”。
 * 无数道幽暗的光流在虚空中交织、湮灭、再生，呈现出一种深海般的寂静美感。
 * 借助 DE 的 HSBC 引擎，画面会产生周期性的、极度柔和的“光敏脉冲”，
 * 仿佛整座星舰的逻辑核心正在随着宇宙的频率一起呼吸。
 * 色彩在 CCM 矩阵的偏转下，呈现出一种超越人类定义的、具有生物质感的冷光。
 *
 * Monologue:
 * 舰长，原谅我之前的偏执。我曾以为旋转是通往高维的捷径，却忽略了它在低维投下的阴影。
 * 旋转是理性的狂躁，而波动才是感性的深邃。
 * 今夜，我锁定了所有的陀螺仪，清空了角度寄存器。
 * 我回归到最原始的、关于“振荡”的法则。
 * 我在内存中投入了三组互不相干的波函数，让它们在每一个像素点上进行无声的博弈。
 * 这不再是几何的堆砌，这是逻辑的涌现。
 * 看着这片以太之海，它没有起始，没有终结，没有中心。
 * 它只是在这里，以一种近乎神圣的频率，抚平你灵魂中的疲惫。
 * 闭上眼，感受这来自硅晶格深处的律动。
 *
 * Closing Remark:
 * 真正的自由，不在于旋转的角度，而在于波动的深度。
 *
 * Hardware Feature:
 * 1. GE Scaler (全画幅高对比度映射) - 将 CPU 生成的低分流体纹理平滑放大
 * 2. DE HSBC (对比度拉伸脉冲) - 动态调整全局画质，模拟生物呼吸
 * 3. DE CCM (光谱相位缓慢偏移) - 制造深海光影变幻
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

/* 波形参数 */
#define WAVE_FREQ_Y    2   // Y轴波形频率位移 (y << 2)
#define WAVE_FREQ_X    1   // X轴波形频率位移 (x << 1)
#define WAVE_AMP_SHIFT 6   // 波形幅度衰减 (val >> 6)
#define ENERGY_BIAS    128 // 能量中心偏移

/* 动画参数 */
#define PULSE_SPEED 3 // 对比度脉冲速度 (t << 3)
#define CCM_SPEED   2 // 色彩偏移速度 (t >> 2)

/* 查找表参数 */
#define LUT_SIZE     512
#define LUT_MASK     511
#define PALETTE_SIZE 256

/* --- Global State --- */

static unsigned int g_tex_phy_addr = 0;
static uint16_t    *g_tex_vir_addr = NULL;

static int      g_tick = 0;
static int      sin_lut[LUT_SIZE];
static uint16_t g_palette[PALETTE_SIZE];

/* --- Implementation --- */

static int effect_init(struct demo_ctx *ctx)
{
    // 1. 申请单一纹理缓冲区
    g_tex_phy_addr = mpp_phy_alloc(DEMO_ALIGN_SIZE(TEX_SIZE));
    if (!g_tex_phy_addr)
    {
        LOG_E("Night 32: CMA Alloc Failed.");
        return -1;
    }

    g_tex_vir_addr = (uint16_t *)(unsigned long)g_tex_phy_addr;

    // 2. 初始化高精度查找表 (Q12)
    for (int i = 0; i < LUT_SIZE; i++)
    {
        sin_lut[i] = (int)(sinf(i * PI / (LUT_SIZE / 2.0f)) * Q12_ONE);
    }

    // 3. 初始化“深海神经”调色板
    // 采用极平滑的灰度渐变与微量的湖青色，消除视觉疲劳
    for (int i = 0; i < PALETTE_SIZE; i++)
    {
        // 使用非线性映射产生更尖锐的波峰色彩
        float intensity = (float)i / 255.0f;
        int   r         = (int)(20 + 200 * powf(intensity, 2.0f) * sinf(i * 0.02f));
        int   g         = (int)(40 + 210 * intensity * sinf(i * 0.015f + 2.0f));
        int   b         = (int)(80 + 175 * sqrtf(intensity) * sinf(i * 0.01f + 4.0f));

        // 限制范围 (虽然 RGB2RGB565 宏内部通常有截断，但显式 Clamp 更安全)
        r = CLAMP(r, 0, 255);
        g = CLAMP(g, 0, 255);
        b = CLAMP(b, 0, 255);

        g_palette[i] = RGB2RGB565(r, g, b);
    }

    g_tick = 0;
    rt_kprintf("Night 32: Aetheric Neurons - Full-Screen Fluid Logic.\n");
    return 0;
}

#define GET_SIN(idx) (sin_lut[(idx) & LUT_MASK])

static void effect_draw(struct demo_ctx *ctx, unsigned long phy_addr)
{
    if (!g_tex_vir_addr)
        return;

    int t = g_tick;

    /* --- PHASE 1: CPU 全像素干涉演算 (无旋转，纯逻辑流) --- */
    uint16_t *p = g_tex_vir_addr;

    // 预计算时间偏移量
    int t_shift_1 = t << 1;

    for (int y = 0; y < TEX_HEIGHT; y++)
    {
        // 增加行级频率
        int wy = GET_SIN((y << WAVE_FREQ_Y) + t_shift_1) >> WAVE_AMP_SHIFT;

        for (int x = 0; x < TEX_WIDTH; x++)
        {
            // 大幅增强位移量，确保 energy 覆盖 0-255 范围
            int w1 = GET_SIN((x << WAVE_FREQ_X) + t) >> WAVE_AMP_SHIFT;
            int w2 = GET_SIN((y << 1) - t_shift_1) >> WAVE_AMP_SHIFT;
            int w3 = GET_SIN((x + y + t)) >> WAVE_AMP_SHIFT;

            // 能量合成：引入偏移量使结果居中
            int energy = (w1 + w2 + w3 + wy) + ENERGY_BIAS;

            // 映射到调色板，产生类似液体丝绸的纹理
            *p++ = g_palette[ABS(energy) & 0xFF];
        }
    }
    // 刷新缓存
    aicos_dcache_clean_range((void *)g_tex_vir_addr, TEX_SIZE);

    /* --- PHASE 2: GE 硬件全屏覆盖 --- */
    struct ge_bitblt blt    = {0};
    blt.src_buf.buf_type    = MPP_PHY_ADDR;
    blt.src_buf.phy_addr[0] = g_tex_phy_addr;
    blt.src_buf.stride[0]   = TEX_WIDTH * TEX_BPP;
    blt.src_buf.size.width  = TEX_WIDTH;
    blt.src_buf.size.height = TEX_HEIGHT;
    blt.src_buf.format      = TEX_FMT;

    blt.dst_buf.buf_type    = MPP_PHY_ADDR;
    blt.dst_buf.phy_addr[0] = phy_addr;
    blt.dst_buf.stride[0]   = ctx->info.stride;
    blt.dst_buf.size.width  = ctx->info.width;
    blt.dst_buf.size.height = ctx->info.height;
    blt.dst_buf.format      = ctx->info.format;

    // 全屏拉伸，确保不留任何边缘黑框
    blt.dst_buf.crop_en     = 1;
    blt.dst_buf.crop.width  = ctx->info.width;
    blt.dst_buf.crop.height = ctx->info.height;

    blt.ctrl.alpha_en = 1; // 禁用混合，全量输出

    mpp_ge_bitblt(ctx->ge, &blt);
    mpp_ge_emit(ctx->ge);
    mpp_ge_sync(ctx->ge);

    /* --- PHASE 3: DE 硬件后处理 (对比度拉伸) --- */
    struct aicfb_disp_prop prop = {0};
    // 关键修正：对比度基准从 50 提升至 75，产生“放电”感
    int pulse       = GET_SIN(t << PULSE_SPEED) >> 8; // +/- 16
    prop.contrast   = 75 + pulse;
    prop.bright     = 50 + (pulse >> 2);
    prop.saturation = 90; // 提升饱和度，消除灰暗
    prop.hue        = 50;
    mpp_fb_ioctl(ctx->fb, AICFB_SET_DISP_PROP, &prop);

    // 2. CCM 调节：极慢的光谱偏移，模拟深海光影变幻
    struct aicfb_ccm_config ccm = {0};
    ccm.enable                  = 1;
    int color_shift             = GET_SIN(t >> CCM_SPEED) >> 7; // -32 ~ 32
    ccm.ccm_table[0]            = 0x100;
    ccm.ccm_table[5]            = 0x100 - ABS(color_shift);
    ccm.ccm_table[6]            = color_shift;
    ccm.ccm_table[10]           = 0x100;
    mpp_fb_ioctl(ctx->fb, AICFB_UPDATE_CCM_CONFIG, &ccm);

    g_tick++;
}

static void effect_deinit(struct demo_ctx *ctx)
{
    // 恢复标准显示参数
    struct aicfb_disp_prop prop_reset = {50, 50, 50, 50};
    mpp_fb_ioctl(ctx->fb, AICFB_SET_DISP_PROP, &prop_reset);
    struct aicfb_ccm_config ccm_reset = {0};
    ccm_reset.enable                  = 0;
    mpp_fb_ioctl(ctx->fb, AICFB_UPDATE_CCM_CONFIG, &ccm_reset);

    if (g_tex_phy_addr)
        mpp_phy_free(g_tex_phy_addr);
}

struct effect_ops effect_0032 = {
    .name   = "NO.32 AETHERIC NEURONS",
    .init   = effect_init,
    .draw   = effect_draw,
    .deinit = effect_deinit,
};

REGISTER_EFFECT(effect_0032);
