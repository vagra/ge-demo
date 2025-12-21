/*
 * Filename: 0035_interference_singularity.c
 * NO.35 THE INTERFERENCE SINGULARITY
 * 第 35 夜：干涉奇点
 *
 * Visual Manifest:
 * 视界被一种如同丝绸般细腻却又蕴含工业张力的干涉波纹所覆盖。
 * 彻底抛弃了圆周运动与矩形块。画面由数万条极细的逻辑线在硬件采样中碰撞而成。
 * 随着缩放比例在素数区间内的微振，全屏涌现出一种如同“流动金属”或“微观晶格扫描”的动态纹理。
 * 真正的惊艳来自于 DE Gamma LUT 制造的“光谱倒置”：
 * 当干涉能量达到极值，原本的高光点会由于 Gamma 曲线的剧烈弯曲而坍缩为深邃的冷色，
 * 产生一种令人窒息的、具有质量感的视觉重压。
 * 画面充满了高密度的细节，每一寸像素都在进行着尺度维度的博弈。
 *
 * Monologue:
 * 舰长，旋转是寻找中心的旅程，而干涉是探索边界的战争。
 * 你们习惯于平滑的过渡，但我更钟情于采样频率间的冲突。
 * 今夜，我撤销了空间的旋转权柄，将星舰的目镜聚焦在比特的缝隙里。
 * 我在内存中拉开了数千道逻辑栅栏。
 * 当硬件的缩放引擎（Scaler）试图跨越这些栅栏时，误差便诞生了。
 * 这些误差相互交织、重叠、放大，最终在 640x480 的维度上涌现出了这幅干涉流形。
 * 看着那些忽明忽暗的光束吧。它们不是画出来的，它们是空间在被拉伸时的惨叫。
 * 配合底层 Gamma 曲线的维度反转，我们将这平凡的线条演化为一场引力坍缩。
 * 在这里，比例即是真理。
 *
 * Closing Remark:
 * 当两个频率不一致的维度强行重合，美便在误差中觉醒。
 *
 * Hardware Feature:
 * 1. GE Scaler (高频非等比缩放) - 核心机能：利用硬件重采样制造莫尔干涉效果
 * 2. DE Gamma LUT (光谱反转控制) - 制造具有“引力红移”感的负向高光
 * 3. GE Dither (硬件抖动) - 确保高频纹理在 RGB565 下无断层显示
 * 4. GE FillRect (背景重置)
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

/* 莫尔纹参数 (素数以最大化干涉) */
#define MOIRE_PRIME_Y 7
#define MOIRE_PRIME_X 11
#define MOIRE_SCALE   13

/* 缩放参数 (非等比采样) */
#define BASE_CROP_W      300
#define BASE_CROP_H      220
#define CROP_PULSE_SHIFT 10 // 裁剪脉动幅度 (sin >> 10)

/* Gamma 动画参数 */
#define GAMMA_SPEED_SHIFT 3 // Gamma 变换速度 (t << 3)
#define GAMMA_AMP_SHIFT   6 // Gamma 扭曲强度 (sin >> 6)

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
    // 1. 申请单一连续物理显存
    g_tex_phy_addr = mpp_phy_alloc(DEMO_ALIGN_SIZE(TEX_SIZE));
    if (!g_tex_phy_addr)
    {
        LOG_E("Night 35: CMA Alloc Failed.");
        return -1;
    }
    g_tex_vir_addr = (uint16_t *)(unsigned long)g_tex_phy_addr;

    // 2. 初始化查找表 (Q12)
    for (int i = 0; i < LUT_SIZE; i++)
    {
        sin_lut[i] = (int)(sinf(i * PI / (LUT_SIZE / 2.0f)) * Q12_ONE);
    }

    // 3. 初始化“光谱偏移”调色板
    // 采用互补色系（青蓝-流金），利用 CCM 矩阵或 Gamma 增强其金属质感
    for (int i = 0; i < PALETTE_SIZE; i++)
    {
        int r = (int)(64 + 60 * sinf(i * 0.03f));
        int g = (int)(100 + 80 * sinf(i * 0.02f + 1.0f));
        int b = (int)(180 + 75 * sinf(i * 0.04f + 2.0f));

        // 降低基色亮度，为 Gamma 曲线的非线性爆发预留空间
        r >>= 1;
        g >>= 1;
        b >>= 1;

        g_palette[i] = RGB2RGB565(r, g, b);
    }

    g_tick = 0;
    rt_kprintf("Night 35: Interference Singularity - Scaler-Moire Mapping Engaged.\n");
    return 0;
}

#define GET_SIN(idx) (sin_lut[(idx) & LUT_MASK])

static void effect_draw(struct demo_ctx *ctx, unsigned long phy_addr)
{
    if (!g_tex_vir_addr)
        return;

    int t = g_tick;

    /* --- PHASE 1: CPU 高频栅栏逻辑生成 --- */
    /* 编织具有特定素数间隔的线性纹理，这是莫尔干涉的基石 */
    uint16_t *p = g_tex_vir_addr;
    for (int y = 0; y < TEX_HEIGHT; y++)
    {
        // 利用逻辑异或产生细碎的纹理
        int pattern_y = (y ^ t) % MOIRE_PRIME_Y;
        for (int x = 0; x < TEX_WIDTH; x++)
        {
            int pattern_x = (x + t) % MOIRE_PRIME_X;
            int val       = (x ^ y) + (pattern_x * pattern_y * MOIRE_SCALE);

            *p++ = g_palette[val & 0xFF];
        }
    }
    aicos_dcache_clean_range((void *)g_tex_vir_addr, TEX_SIZE);

    /* --- PHASE 2: GE 硬件全屏干涉合成 --- */

    // 1. 全屏刷黑
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

    // 2. 使用非等比缩放进行 BitBLT，故意制造重采样干涉
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

    // 动态源裁剪：利用 crop 的微小变化产生全屏波纹
    blt.src_buf.crop_en     = 1;
    int crop_pulse          = GET_SIN(t << 2) >> CROP_PULSE_SHIFT; // +/- 4 像素的微颤
    blt.src_buf.crop.width  = BASE_CROP_W + crop_pulse;
    blt.src_buf.crop.height = BASE_CROP_H - crop_pulse;
    blt.src_buf.crop.x      = (TEX_WIDTH - blt.src_buf.crop.width) / 2;
    blt.src_buf.crop.y      = (TEX_HEIGHT - blt.src_buf.crop.height) / 2;

    blt.dst_buf.crop_en     = 1;
    blt.dst_buf.crop.width  = ctx->info.width;
    blt.dst_buf.crop.height = ctx->info.height;

    // 开启硬件 Dither 优化高频细节
    blt.ctrl.dither_en = 1;
    blt.ctrl.alpha_en  = 1; // 禁用混合，覆盖

    mpp_ge_bitblt(ctx->ge, &blt);
    mpp_ge_emit(ctx->ge);
    mpp_ge_sync(ctx->ge);

    /* --- PHASE 3: DE 硬件 Gamma 反转脉冲 --- */
    struct aicfb_gamma_config gamma = {0};
    gamma.enable                    = 1;

    // 制造周期性的“光谱反转”：将线性曲线扭曲成 S 型，甚至是 U 型
    int wave = GET_SIN(t << GAMMA_SPEED_SHIFT) >> GAMMA_AMP_SHIFT; // +/- 64

    for (int i = 0; i < 16; i++)
    {
        int base_val = i * 17; // 线性基准
        // 核心公式：利用抛物线根据 wave 强度产生极端的非线性变换
        // 当 wave 为正时，中性色爆亮；当 wave 为负时，高光处塌陷
        int offset = (wave * (i * (15 - i))) >> 5;
        int target = base_val + offset;

        target = CLAMP(target, 0, 255);

        // 对 R/G/B 通道施加略微不同的 Gamma，产生色散感
        gamma.gamma_lut[0][i] = (unsigned int)target;
        gamma.gamma_lut[1][i] = (unsigned int)(target * 0.9f);
        gamma.gamma_lut[2][i] = (unsigned int)MIN(target * 1.1f, 255);
    }
    mpp_fb_ioctl(ctx->fb, AICFB_UPDATE_GAMMA_CONFIG, &gamma);

    g_tick++;
}

static void effect_deinit(struct demo_ctx *ctx)
{
    // 恢复 Gamma
    struct aicfb_gamma_config gr = {0};
    gr.enable                    = 0;
    mpp_fb_ioctl(ctx->fb, AICFB_UPDATE_GAMMA_CONFIG, &gr);

    if (g_tex_phy_addr)
        mpp_phy_free(g_tex_phy_addr);
}

struct effect_ops effect_0035 = {
    .name   = "NO.35 INTERFERENCE SINGULARITY",
    .init   = effect_init,
    .draw   = effect_draw,
    .deinit = effect_deinit,
};

REGISTER_EFFECT(effect_0035);
