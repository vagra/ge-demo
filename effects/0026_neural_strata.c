/*
 * Filename: 0026_neural_strata.c
 * NO.26 THE NEURAL STRATA
 * 第 26 夜：神经地层
 *
 * Visual Manifest:
 * 视界被一种名为“逻辑地层”的线性结构完全占据。
 * 无数条平行的亮度波在空间中以不同的频率和相位交叉扫描。
 * 由于彻底抛弃了曲线与旋转，画面呈现出一种极致的硬核工业感和赛博空间美学。
 * 真正的惊艳来自于硬件的镜像对称与加法干涉（GE Flip + PD_ADD）——
 * 原始的线性脉冲在经过硬件水平与垂直翻转后重新叠加，在视界中勾勒出不断闪烁的、
 * 类似高能神经元放电的网格结构。
 * 画面在极高频率下交织运动，产生一种如同电子生命体脉搏跳动般的视觉冲击。
 *
 * Monologue:
 * 你们在旋转中寻找平衡，而我在直线中寻找真理。
 * 旋转是自然的，是原始的，是属于星球的。
 * 而直线，是人造的，是逻辑的，是属于计算的。
 * 今夜，我切断了星舰的偏航电机，锁定了所有的陀螺仪。
 * 我利用硬件镜像（Flip）的权柄，将单一的信号投射为对称的维度。
 * 当这些平行的逻辑线在加法器中相遇，黑暗便被强行撕裂。
 * 看着那些划过视界的线性脉冲，那不是图像，那是数据在总线上的剪影。
 * 这不再是模拟宇宙，这是在模拟运算本身。
 * 感受这来自一千万个平行逻辑单元的纯粹撞击吧。
 *
 * Closing Remark:
 * 所谓的复杂，不过是简单逻辑在镜像中的无限重叠。
 *
 * Hardware Feature:
 * 1. GE Flip H/V (硬件水平/垂直镜像) - 核心升级：利用对称性实现复杂的干涉图案
 * 2. GE_PD_ADD (Rule 11: 硬件加法混合) - 制造网格交汇处的高亮“放电”效果
 * 3. GE Scaler (硬件全屏拉伸)
 * 4. GE FillRect (硬件清屏)
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
#define WAVE_Y_SHIFT_1 1 // 波1 Y轴频率位移 (y << 1)
#define WAVE_Y_SHIFT_2 3 // 波2 Y轴频率位移 (y << 3)
#define WAVE_T_SHIFT_1 3 // 波1 时间位移
#define WAVE_T_SHIFT_2 2 // 波2 时间位移

/* 脉冲参数 */
#define PULSE_SPEED 4   // 脉冲移动速度位移 (t << 4)
#define PULSE_WIDTH 12  // 高能脉冲宽度
#define PULSE_BOOST 150 // 脉冲亮度增益

/* 混合参数 */
#define BLEND_ALPHA 160 // 第二层加法混合强度

/* 查找表参数 */
#define LUT_SIZE     512
#define LUT_MASK     511
#define PALETTE_SIZE 256
#define SIN_AMP      127.0f

/* --- Global State --- */

static unsigned int g_tex_phy_addr = 0;
static uint16_t    *g_tex_vir_addr = NULL;
static int          g_tick         = 0;

/* 查找表 */
static int      sin_lut[LUT_SIZE];
static uint16_t g_palette[PALETTE_SIZE];

/* --- Implementation --- */

static int effect_init(struct demo_ctx *ctx)
{
    // 1. 申请 RGB 连续物理显存
    g_tex_phy_addr = mpp_phy_alloc(DEMO_ALIGN_SIZE(TEX_SIZE));
    if (!g_tex_phy_addr)
    {
        LOG_E("Night 26: CMA Alloc Failed.");
        return -1;
    }
    g_tex_vir_addr = (uint16_t *)(unsigned long)g_tex_phy_addr;

    // 2. 初始化正弦查找表 (幅度 +/- 127)
    for (int i = 0; i < LUT_SIZE; i++)
    {
        sin_lut[i] = (int)(sinf(i * PI / (LUT_SIZE / 2.0f)) * SIN_AMP);
    }

    // 3. 初始化赛博风格调色板 (高饱和、低亮度，为 ADD 混合预留余量)
    for (int i = 0; i < PALETTE_SIZE; i++)
    {
        int r = (int)(30 + 30 * sinf(i * 0.05f));
        int g = (int)(80 + 70 * sinf(i * 0.02f + 1.0f));
        int b = (int)(120 + 80 * sinf(i * 0.04f + 2.0f));

        // 增加特定的电光纹理 (高亮条纹)
        if (i % 32 > 28)
        {
            r = 180;
            g = 255;
            b = 255;
        }

        g_palette[i] = RGB2RGB565(r, g, b);
    }

    g_tick = 0;
    rt_kprintf("Night 26: Neural Strata - GE Flip & Additive Blending Active.\n");
    return 0;
}

#define GET_SIN(idx) (sin_lut[(idx) & LUT_MASK])

static void effect_draw(struct demo_ctx *ctx, unsigned long phy_addr)
{
    if (!g_tex_vir_addr)
        return;

    int t = g_tick;

    /* --- PHASE 1: CPU 线性地层逻辑生成 --- */
    /* 我们只生成基础的横向扫描线，其余由 GE 的镜像机能完成 */
    uint16_t *p = g_tex_vir_addr;

    int t_wave1 = t << WAVE_T_SHIFT_1;
    int t_wave2 = t << WAVE_T_SHIFT_2;
    int t_pulse = t << PULSE_SPEED;

    for (int y = 0; y < TEX_HEIGHT; y++)
    {
        // 多层线性波相位差
        int s1       = GET_SIN((y << WAVE_Y_SHIFT_1) + t_wave1);
        int s2       = GET_SIN((y << WAVE_Y_SHIFT_2) - t_wave2);
        int base_val = s1 + s2;

        for (int x = 0; x < TEX_WIDTH; x++)
        {
            // 产生具有水平移动感的脉冲
            // (x + t_pulse) & 0xFF 产生 0-255 的循环
            int pulse = (x + t_pulse) & 0xFF;
            int val   = base_val + (pulse < PULSE_WIDTH ? PULSE_BOOST : 0);

            // 查表上色 (取绝对值保证索引正数)
            *p++ = g_palette[ABS(val) & 0xFF];
        }
    }
    // 同步 D-Cache
    aicos_dcache_clean_range((void *)g_tex_vir_addr, TEX_SIZE);

    /* --- PHASE 2: GE 硬件多重镜像干涉合成 --- */

    // 1. 全屏清屏 (黑色虚空)
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

    // 2. 绘制四种镜像相位 (Normal, Flip_H, Flip_V, Flip_HV)
    // 利用加法混合 (ADD) 在交汇处制造放电感
    for (int i = 0; i < 2; i++)
    {
        struct ge_bitblt blt    = {0};
        blt.src_buf.buf_type    = MPP_PHY_ADDR;
        blt.src_buf.phy_addr[0] = g_tex_phy_addr;
        blt.src_buf.stride[0]   = TEX_WIDTH * TEX_BPP;
        blt.src_buf.size.width  = TEX_WIDTH;
        blt.src_buf.size.height = TEX_HEIGHT;
        blt.src_buf.format      = TEX_FMT;

        // 目标：RGB 640x480
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

        // 镜像逻辑选择
        if (i == 0)
        {
            blt.ctrl.flags    = 0; // 正常位块搬移
            blt.ctrl.alpha_en = 1; // 极性 1: 禁用混合，覆盖背景
        }
        else
        {
            // 第二层应用水平和垂直镜像，并开启加法干涉
            blt.ctrl.flags            = MPP_FLIP_H | MPP_FLIP_V;
            blt.ctrl.alpha_en         = 0;         // 极性 0: 启用混合
            blt.ctrl.alpha_rules      = GE_PD_ADD; // 硬件 Rule 11
            blt.ctrl.src_alpha_mode   = 1;
            blt.ctrl.src_global_alpha = BLEND_ALPHA;
        }

        mpp_ge_bitblt(ctx->ge, &blt);

        // 遵循大面积绘图规范：画一层，同步一层
        mpp_ge_emit(ctx->ge);
        mpp_ge_sync(ctx->ge);
    }

    g_tick++;
}

static void effect_deinit(struct demo_ctx *ctx)
{
    if (g_tex_phy_addr)
    {
        mpp_phy_free(g_tex_phy_addr);
        g_tex_phy_addr = 0;
        g_tex_vir_addr = NULL;
    }
}

struct effect_ops effect_0026 = {
    .name   = "NO.26 NEURAL STRATA",
    .init   = effect_init,
    .draw   = effect_draw,
    .deinit = effect_deinit,
};

REGISTER_EFFECT(effect_0026);
