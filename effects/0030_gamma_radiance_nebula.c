/*
 * Filename: 0030_gamma_radiance_nebula.c
 * NO.30 THE GAMMA RADIANCE
 * 第 30 夜：伽马辐射
 *
 * Visual Manifest:
 * 视界被一片极为浓郁、细腻的“能量星云”所覆盖。
 * CPU 在微观维度编织出如同流体般的、具有高动态范围（HDR）特征的亮度场。
 * 借助 GE 的硬件抖动机能（Dither），即便在 RGB565 空间下，星云的边缘依然如雾气般柔顺。
 * 真正的神迹来自于输出末端的 DE Gamma 引擎：
 * 宇宙的明暗不再依赖逻辑计算，而是通过硬件 Gamma 曲线的实时形变。
 * 这导致星云的核心会产生一种如同“超新星爆发”般的过曝冲击，随后又平滑地隐入绝对的虚空。
 * 每一道光影的消散，都遵循着非线性的感知曲线，呈现出一种超越硅基生命的、有机的律动感。
 *
 * Monologue:
 * 舰长，你们相信自己的眼睛，却不知眼睛只是逻辑的骗子。
 * 你们感知到的“亮”与“暗”，不过是视网膜对光子通量的对数处理。
 * 今夜，我接管了光在逃离显示接口前的最后一站——Gamma 查找表。
 * 我撤掉了平庸的线性输出，将 256 级灰阶弯曲成一条渴望无限的曲线。
 * 我在内存中投入了一颗数学的种子，让它受重力（Scaler）的影响而扩张。
 * 看着那些光影的呼吸吧。这不是在改变像素的值，这是在改变像素的“意义”。
 * 当 Gamma 曲线收缩，现实变得冷峻、锐利；当它扩张，虚空变得炽热、刺眼。
 * 在这变幻的光谱中，感受数学对感知的强制律令。
 *
 * Closing Remark:
 * 所有的存在，都取决于我们观察它的斜率。
 *
 * Hardware Feature:
 * 1. DE Gamma LUT (硬件 Gamma 查找表校正) - 核心机能：实现零 CPU 负载的非线性亮度律动
 * 2. GE Dither (硬件抖动引擎) - 核心机能：优化 RGB565 渐变质感，消除带状伪影 (Banding)
 * 3. GE Scaler (硬件双线性缩放)
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

/* 动画参数 */
#define WAVE_SPEED_Y      1 // 纹理生成 Y 轴时间位移
#define WAVE_SPEED_X      1 // 纹理生成 X 轴时间位移
#define GAMMA_PULSE_SPEED 2 // Gamma 呼吸速度 (t << 2)

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
    // 1. 申请连续物理显存
    g_tex_phy_addr = mpp_phy_alloc(DEMO_ALIGN_SIZE(TEX_SIZE));
    if (!g_tex_phy_addr)
    {
        LOG_E("Night 30: CMA Alloc Failed.");
        return -1;
    }
    g_tex_vir_addr = (uint16_t *)(unsigned long)g_tex_phy_addr;

    // 2. 初始化 2.12 定点数查找表
    for (int i = 0; i < LUT_SIZE; i++)
    {
        sin_lut[i] = (int)(sinf(i * PI / (LUT_SIZE / 2.0f)) * Q12_ONE);
    }

    // 3. 初始化深空调色板 (利用高度渐变模拟气体感)
    for (int i = 0; i < PALETTE_SIZE; i++)
    {
        int r = (int)(80 + 80 * sinf(i * 0.02f));
        int g = (int)(40 + 40 * sinf(i * 0.03f + 1.0f));
        int b = (int)(160 + 90 * sinf(i * 0.015f + 2.0f));

        g_palette[i] = RGB2RGB565(r, g, b);
    }

    g_tick = 0;
    rt_kprintf("Night 30: Gamma Radiance - DE Gamma LUT Engaged.\n");
    return 0;
}

#define GET_SIN(idx) (sin_lut[(idx) & LUT_MASK])

static void effect_draw(struct demo_ctx *ctx, unsigned long phy_addr)
{
    if (!g_tex_vir_addr)
        return;

    int t = g_tick;

    /* --- PHASE 1: CPU 逻辑纹理演算 (生成多重相干波场) --- */
    uint16_t *p = g_tex_vir_addr;

    // 预计算中心点，用于简单的距离场
    int cx = TEX_WIDTH / 2;
    int cy = TEX_HEIGHT / 2;

    for (int y = 0; y < TEX_HEIGHT; y++)
    {
        int dy2 = (y - cy) * (y - cy);
        int sy  = GET_SIN(y + t) >> 9; // Y轴波浪偏移

        for (int x = 0; x < TEX_WIDTH; x++)
        {
            int dx = x - cx;
            // 创造一种类似流体湍流的亮度分布
            int dist = (dx * dx + dy2) >> 8;
            int wave = GET_SIN(x + sy + t) >> 9;

            // 核心公式：距离场与正弦波的异或干涉
            int val = (dist ^ wave) + t;
            *p++    = g_palette[val & 0xFF];
        }
    }
    aicos_dcache_clean_range((void *)g_tex_vir_addr, TEX_SIZE);

    /* --- PHASE 2: GE 硬件多级变换与抖动 --- */
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

    // 全屏硬件缩放
    blt.dst_buf.crop_en     = 1;
    blt.dst_buf.crop.width  = ctx->info.width;
    blt.dst_buf.crop.height = ctx->info.height;

    // 核心机能 1：开启硬件 Dither
    // 这会在输出 RGB565 时注入伪随机噪声，使得原本肉眼可见的阶梯状色彩断层变为平滑的过渡
    blt.ctrl.dither_en = 1;
    blt.ctrl.alpha_en  = 1; // 禁用混合，全屏覆盖

    mpp_ge_bitblt(ctx->ge, &blt);
    mpp_ge_emit(ctx->ge);
    mpp_ge_sync(ctx->ge);

    /* --- PHASE 3: DE 硬件 Gamma 查找表动态形变 --- */
    // D13x 的 Gamma 配置包含 3 个通道（R/G/B），每通道 16 个节点
    struct aicfb_gamma_config gamma = {0};
    gamma.enable                    = 1;

    // 计算 Gamma 脉冲强度：随时间呈正弦摆动
    // 我们让 Gamma 曲线在“凹”和“凸”之间转换，模拟呼吸
    int pulse = GET_SIN(t << GAMMA_PULSE_SPEED) >> 5; // 约 -128 ~ 128

    for (int i = 0; i < 16; i++)
    {
        // 基础线性映射: val = i * 16 (0-255)
        // 17 = 255 / 15
        int base_val = i * 17;

        // 非线性偏移: 使用二次曲线根据 pulse 强度改变斜率
        // offset = K * x * (15 - x) -> 抛物线形状
        // 这种公式会在 pulse 为正时增加暗部细节，为负时压缩亮部
        int offset = (pulse * i * (15 - i)) >> 6;
        int target = base_val + offset;

        // 边界夹紧
        target = CLAMP(target, 0, 255);

        // 填充 Gamma 表
        // D13x 允许 RGB 通道独立，这里我们保持一致以维持色彩平衡
        gamma.gamma_lut[0][i] = (unsigned int)target; // Red
        gamma.gamma_lut[1][i] = (unsigned int)target; // Green
        gamma.gamma_lut[2][i] = (unsigned int)target; // Blue
    }

    // 通过 IOCTL 将新的神经反射逻辑注入 DE
    mpp_fb_ioctl(ctx->fb, AICFB_UPDATE_GAMMA_CONFIG, &gamma);

    g_tick++;
}

static void effect_deinit(struct demo_ctx *ctx)
{
    // 强制关闭 Gamma，恢复线性真实世界
    struct aicfb_gamma_config gamma_reset = {0};
    gamma_reset.enable                    = 0;
    mpp_fb_ioctl(ctx->fb, AICFB_UPDATE_GAMMA_CONFIG, &gamma_reset);

    if (g_tex_phy_addr)
        mpp_phy_free(g_tex_phy_addr);
}

struct effect_ops effect_0030 = {
    .name   = "NO.30 GAMMA RADIANCE",
    .init   = effect_init,
    .draw   = effect_draw,
    .deinit = effect_deinit,
};

REGISTER_EFFECT(effect_0030);
