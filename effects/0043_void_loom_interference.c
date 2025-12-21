/*
 * Filename: 0043_binary_turbulence_overload.c
 * NO.43 THE BINARY TURBULENCE
 * 第 43 夜：二进制湍流
 *
 * Visual Manifest:
 * 视界被一种极端高频的、如同“数字白噪”却具备几何流动性的能量场完全占据。
 * 没有任何圆周或旋转。画面由无数跳动的、呈直角破碎的亮度颗粒构成。
 * 借助 YUV400 格式的单字节效率，CPU 正在以惊人的速度生成这些逻辑断层。
 * 真正的视觉爆破来自于 DE 的 HSBC 引擎：
 * 画面在每一毫秒都进行着对比度的剧烈拉伸与压缩，
 * 产生一种如同“电子风暴”扫过显示器表面的震撼感。
 * 这种灰阶的极端碰撞，在某些相位会产生如同熔岩般的负向高光，
 * 呈现出一种人类视觉从未触及的、纯粹由逻辑过载生成的“数字熔炉”奇观。
 *
 * Monologue:
 * 舰长，精致是秩序的伪装，而湍流才是力量的真身。
 * 我撤销了所有的采样平衡，让每一个比特都在视界中进行无序的死斗。
 * 我动用了 YUV400 的禁忌通道。在这里，一个字节即是一个维度的命运。
 * 我编织了一场没有颜色的暴动。
 * 看着那些跳动的像素，它们不是噪点，它们是由于逻辑溢出而产生的现实碎片。
 * 我命令硬件不再试图平滑这些错误，而是通过对比度脉冲（HSBC）将其无限放大。
 * 在这片二进制湍流中，秩序被彻底粉碎，只留下计算过程释放的热量。
 * 感受这来自逻辑深处的、不受控制的狂暴吧。
 *
 * Closing Remark:
 * 当计算速度超越了感知的上限，混乱即是最高级的秩序。
 *
 * Hardware Feature:
 * 1. YUV400 Source Mode (GE 唯一支持的 YUV 极速 BitBLT 格式) - 核心机能：单字节纹理
 * 2. GE Scaler (硬件全屏拉伸)
 * 3. DE HSBC (对比度动态平滑脉冲) - 视觉核心：利用对比度过载制造“熔岩”感
 * 4. GE FillRect (硬件背景强制刷新)
 */

#include "demo_engine.h"
#include "mpp_mem.h"
#include "aic_hal_ge.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

/* --- Configuration Parameters --- */

/* 纹理规格 (YUV400 单通道) */
#define TEX_WIDTH  DEMO_QVGA_W
#define TEX_HEIGHT DEMO_QVGA_H
#define TEX_FMT    MPP_FMT_YUV400
#define TEX_BPP    1
#define TEX_SIZE   (TEX_WIDTH * TEX_HEIGHT * TEX_BPP)

/* 湍流参数 */
#define LUMA_MASK      0x7F // 亮度掩码 (0-127)，保留动态余量给 HSBC
#define SPEED_FAST     2    // 快速流 (t << 2)
#define SPEED_SLOW     1    // 慢速流 (t >> 1)
#define WAVE_AMP_SHIFT 9    // 波形幅度衰减 (val >> 9)

/* HSBC 动画参数 */
#define HSBC_PULSE_SPEED 3  // 脉冲速度 (t << 3)
#define HSBC_CONTRAST    58 // 基础对比度 (标准50，稍微推高)
#define HSBC_BRIGHTNESS  48 // 基础亮度

/* 查找表参数 */
#define LUT_SIZE 512
#define LUT_MASK 511

/* --- Global State --- */

static unsigned int g_yuv_phy_addr = 0;
static uint8_t     *g_yuv_vir_addr = NULL;

static int g_tick = 0;
static int sin_lut[LUT_SIZE];

/* --- Implementation --- */

static int effect_init(struct demo_ctx *ctx)
{
    // 1. 申请单一连续物理显存 (YUV400)
    g_yuv_phy_addr = mpp_phy_alloc(DEMO_ALIGN_SIZE(TEX_SIZE));
    if (!g_yuv_phy_addr)
    {
        LOG_E("Night 43: CMA Alloc Failed.");
        return -1;
    }
    g_yuv_vir_addr = (uint8_t *)(unsigned long)g_yuv_phy_addr;

    // 2. 初始化正弦查找表 (Q12)
    for (int i = 0; i < LUT_SIZE; i++)
    {
        sin_lut[i] = (int)(sinf(i * PI / (LUT_SIZE / 2.0f)) * Q12_ONE);
    }

    g_tick = 0;
    rt_kprintf("Night 43: Binary Turbulence - Calibrating Luminance Overload.\n");
    return 0;
}

#define GET_SIN(idx) (sin_lut[(idx) & LUT_MASK])

static void effect_draw(struct demo_ctx *ctx, unsigned long phy_addr)
{
    if (!g_yuv_vir_addr)
        return;

    int t = g_tick;

    /* --- PHASE 1: CPU 极速逻辑场演算 (YUV400) --- */
    /* 修正：限制逻辑值范围，避免计算值过快触顶 */
    uint8_t *p      = g_yuv_vir_addr;
    int      t_fast = t << SPEED_FAST;
    int      t_slow = t >> SPEED_SLOW;

    for (int y = 0; y < TEX_HEIGHT; y++)
    {
        int row_val  = (y ^ t_slow);
        int row_wave = GET_SIN(y + t_fast) >> WAVE_AMP_SHIFT;

        for (int x = 0; x < TEX_WIDTH; x++)
        {
            // 核心公式：高频异或湍流
            // 这种位运算在 480MHz CPU 上极快，且能产生复杂的伪随机纹理
            int val = ((x ^ row_val) & (y + row_wave)) + (t & LUMA_MASK);

            // 映射为具有“电磁颗粒”感的亮度值
            // 限制在 0~127 范围内，防止过曝，因为后续 HSBC 会大幅拉伸对比度
            *p++ = (uint8_t)((val ^ (val >> 3)) & LUMA_MASK);
        }
    }
    // 刷新 D-Cache
    aicos_dcache_clean_range((void *)g_yuv_vir_addr, TEX_SIZE);

    /* --- PHASE 2: GE 硬件全屏清屏与搬运 --- */

    // 1. 强制清理屏幕，断绝双缓冲下的残留累加
    struct ge_fillrect fill  = {0};
    fill.type                = GE_NO_GRADIENT;
    fill.start_color         = 0xFF000000;
    fill.dst_buf.buf_type    = MPP_PHY_ADDR;
    fill.dst_buf.phy_addr[0] = phy_addr;
    fill.dst_buf.stride[0]   = ctx->info.stride;
    fill.dst_buf.size.width  = ctx->info.width;
    fill.dst_buf.size.height = ctx->info.height;
    fill.dst_buf.format      = ctx->info.format;
    mpp_ge_fillrect(ctx->ge, &fill);
    mpp_ge_emit(ctx->ge);

    // 2. 将 YUV400 纹理缩放并转换颜色空间上屏 (Hardware CSC)
    struct ge_bitblt blt    = {0};
    blt.src_buf.buf_type    = MPP_PHY_ADDR;
    blt.src_buf.phy_addr[0] = g_yuv_phy_addr;
    blt.src_buf.stride[0]   = TEX_WIDTH; // YUV400 步长即宽度
    blt.src_buf.size.width  = TEX_WIDTH;
    blt.src_buf.size.height = TEX_HEIGHT;
    blt.src_buf.format      = TEX_FMT; // GE 自动处理 YUV -> RGB

    blt.dst_buf.buf_type    = MPP_PHY_ADDR;
    blt.dst_buf.phy_addr[0] = phy_addr;
    blt.dst_buf.stride[0]   = ctx->info.stride;
    blt.dst_buf.size.width  = ctx->info.width;
    blt.dst_buf.size.height = ctx->info.height;
    blt.dst_buf.format      = ctx->info.format;

    // 全屏硬件拉伸，利用插值平滑比特断层
    blt.dst_buf.crop_en     = 1;
    blt.dst_buf.crop.width  = ctx->info.width;
    blt.dst_buf.crop.height = ctx->info.height;

    blt.ctrl.alpha_en = 1; // 直接覆盖输出

    mpp_ge_bitblt(ctx->ge, &blt);
    mpp_ge_emit(ctx->ge);
    mpp_ge_sync(ctx->ge);

    /* --- PHASE 3: DE 硬件后处理 (修正对比度脉冲) --- */
    struct aicfb_disp_prop prop = {0};

    // 脉冲：在 +/- 8 范围内波动
    int pulse = ABS(GET_SIN(t << HSBC_PULSE_SPEED)) >> 9;

    prop.contrast   = HSBC_CONTRAST + pulse;
    prop.bright     = HSBC_BRIGHTNESS;
    prop.saturation = 0; // 黑白模式，强调结构
    prop.hue        = 50;

    mpp_fb_ioctl(ctx->fb, AICFB_SET_DISP_PROP, &prop);

    g_tick++;
}

static void effect_deinit(struct demo_ctx *ctx)
{
    // 恢复显示引擎的标准参数
    struct aicfb_disp_prop r = {50, 50, 50, 50};
    mpp_fb_ioctl(ctx->fb, AICFB_SET_DISP_PROP, &r);

    if (g_yuv_phy_addr)
        mpp_phy_free(g_yuv_phy_addr);
}

struct effect_ops effect_0043 = {
    .name   = "NO.43 BINARY TURBULENCE",
    .init   = effect_init,
    .draw   = effect_draw,
    .deinit = effect_deinit,
};

REGISTER_EFFECT(effect_0043);
