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
 */

#include "demo_engine.h"
#include "mpp_mem.h"
#include "aic_hal_ge.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

/*
 * Hardware Feature:
 * 1. YUV400 Source Mode (GE 唯一支持的 YUV 极速 BitBLT 格式) - 核心修复：限制亮度范围
 * 2. GE Scaler (硬件全屏拉伸)
 * 3. DE HSBC (对比度动态平滑脉冲) - 核心修复：降低对比度基准，防止白炽化饱和
 * 4. GE FillRect (硬件背景强制刷新)
 */

#define TEX_W    320
#define TEX_H    240
#define TEX_SIZE (TEX_W * TEX_H) // YUV400: 每像素 1 字节

static unsigned int g_yuv_phy_addr = 0;
static uint8_t     *g_yuv_vir_addr = NULL;

static int g_tick = 0;
static int sin_lut[512];

static int effect_init(struct demo_ctx *ctx)
{
    // 1. 申请单一连续物理显存 (YUV400)
    g_yuv_phy_addr = mpp_phy_alloc(TEX_SIZE);
    if (!g_yuv_phy_addr)
        return -1;
    g_yuv_vir_addr = (uint8_t *)(unsigned long)g_yuv_phy_addr;

    // 2. 初始化正弦查找表 (Q12)
    for (int i = 0; i < 512; i++)
        sin_lut[i] = (int)(sinf(i * 3.14159f / 256.0f) * 4096.0f);

    g_tick = 0;
    rt_kprintf("Night 43: Binary Turbulence - Calibrating Luminance Overload.\n");
    return 0;
}

#define GET_SIN(idx) (sin_lut[(idx) & 511])

static void effect_draw(struct demo_ctx *ctx, unsigned long phy_addr)
{
    if (!g_yuv_vir_addr)
        return;

    int t = g_tick;

    /* --- PHASE 1: CPU 极速逻辑场演算 (YUV400) --- */
    /* 修正：限制逻辑值范围，避免计算值过快触顶 */
    uint8_t *p      = g_yuv_vir_addr;
    int      t_fast = t << 2;
    int      t_slow = t >> 1;

    for (int y = 0; y < TEX_H; y++)
    {
        int row_val  = (y ^ t_slow);
        int row_wave = GET_SIN(y + t_fast) >> 9;

        for (int x = 0; x < TEX_W; x++)
        {
            // 核心公式：高频异或湍流，强制进行 7-bit 钳位以预留 HSBC 冗余
            int val = ((x ^ row_val) & (y + row_wave)) + (t & 0x7F);

            // 映射为具有“电磁颗粒”感的亮度值 (范围控制在 0~127)
            *p++ = (uint8_t)((val ^ (val >> 3)) & 0x7F);
        }
    }
    // 刷新 D-Cache 以便 GE 访问
    aicos_dcache_clean_range((void *)g_yuv_vir_addr, TEX_SIZE);

    /* --- PHASE 2: GE 硬件全屏清屏与搬运 --- */

    // 强制清理屏幕，断绝双缓冲下的残留累加
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

    struct ge_bitblt blt    = {0};
    blt.src_buf.buf_type    = MPP_PHY_ADDR;
    blt.src_buf.phy_addr[0] = g_yuv_phy_addr;
    blt.src_buf.stride[0]   = TEX_W; // YUV400 步长即宽度
    blt.src_buf.size.width  = TEX_W;
    blt.src_buf.size.height = TEX_H;
    blt.src_buf.format      = MPP_FMT_YUV400; // 遵循硬件指令，使用纯亮度源

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
    // 修正：对比度基准降至 58，脉冲范围 +/- 10
    int pulse       = abs(GET_SIN(t << 3)) >> 9; // 0 ~ 8
    prop.contrast   = 58 + pulse;
    prop.bright     = 48;
    prop.saturation = 0;
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
