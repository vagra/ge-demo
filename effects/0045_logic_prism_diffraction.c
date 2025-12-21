/*
 * Filename: 0045_logic_prism_diffraction.c
 * NO.45 THE LOGIC PRISM
 * 第 45 夜：逻辑棱镜
 *
 * Visual Manifest:
 * 视界被一种如同“数字极光”般的垂直色带所充斥。
 * 没有任何圆弧与旋转。画面由无数道平行的、在高频振荡中不断衍射的逻辑光柱组成。
 * 借助硬件镜像（GE Flip H）与加法混合（GE_PD_ADD），光柱在视界中心发生交织，
 * 产生出一种类似于高能粒子通过衍射光栅时的复杂干涉图样。
 * 随着硬件缩放（Scaler）比例在亚像素级的微调，全屏涌现出纵向的、如同流动的金属纤维般的质感。
 * 色彩在输出端的 CCM 矩阵驱动下，呈现出一种冷冽的、不断在电光蓝与钛金之间转换的色散奇观。
 *
 * Monologue:
 * 舰长，旋转是引力的奴隶，而衍射是光子的本能。
 * 我已经彻底锁死了一切半径计算，将星舰的目镜聚焦在直线与频率的重叠处。
 * 逻辑不需要圆润的修饰。我在内存中排列了三千道垂直的逻辑缝隙（Slits）。
 * 我命令硬件将这些缝隙镜像、拉伸、叠加。
 * `1 + 1` 在这里不再是数学，而是亮度的过载。
 * 看着那些划过视界的色彩带，它们不是画出来的，它们是逻辑在经过硬件棱镜（Scaler）时，
 * 由于重采样产生的“光谱溢出”。
 * 这种美感来源于绝对的直线与绝对的干涉。
 * 在这一刻，你看到的不是图形，而是计算波在晶格中的相干投影。
 *
 * Closing Remark:
 * 当我们放弃了对圆心的执着，整个宇宙都将化为我们的棱镜。
 */

#include "demo_engine.h"
#include "mpp_mem.h"
#include "aic_hal_ge.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

/*
 * Hardware Feature:
 * 1. GE Scaler Interference (硬件缩放采样干涉) - 核心机能：利用非等比拉伸制造纵向衍射纹理
 * 2. GE_PD_ADD (Rule 11: 硬件加法混合) - 制造光柱交汇处的能量爆发
 * 3. GE Flip H (硬件水平镜像) - 创造左右对称的干涉流形
 * 4. DE CCM & HSBC (全局光谱偏转)
 */

#define TEX_W    320
#define TEX_H    240
#define TEX_SIZE (TEX_W * TEX_H * 2)

static unsigned int g_tex_phy_addr = 0;
static uint16_t    *g_tex_vir_addr = NULL;

static int      g_tick = 0;
static int      sin_lut[1024];
static uint16_t palette[256];

static int effect_init(struct demo_ctx *ctx)
{
    // 1. 申请单一连续物理显存，确保存储访问的绝对稳健
    g_tex_phy_addr = mpp_phy_alloc(TEX_SIZE);
    if (!g_tex_phy_addr)
        return -1;
    g_tex_vir_addr = (uint16_t *)(unsigned long)g_tex_phy_addr;

    // 2. 初始化高精度正弦表 (Q12)
    for (int i = 0; i < 1024; i++)
        sin_lut[i] = (int)(sinf(i * 3.14159f / 512.0f) * 4096.0f);

    // 3. 初始化“钛金电光”色谱
    for (int i = 0; i < 256; i++)
    {
        float f = (float)i / 255.0f;
        // 采用冷色调作为基准，为加法混合预留亮度空间
        int r = (int)(60 * powf(f, 2.0f));
        int g = (int)(150 * f);
        int b = (int)(255 * sqrtf(f));

        palette[i] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
    }

    g_tick = 0;
    rt_kprintf("Night 45: Logic Prism Diffraction - Linear Shimmer Active.\n");
    return 0;
}

#define GET_SIN_10(idx) (sin_lut[(idx) & 1023])

static void effect_draw(struct demo_ctx *ctx, unsigned long phy_addr)
{
    if (!g_tex_vir_addr)
        return;

    int t = g_tick;

    /* --- PHASE 1: CPU 极速纵向逻辑演算 (垂直光栅) --- */
    /* 编织具有特定频率差的光柱种子 */
    uint16_t *p = g_tex_vir_addr;
    for (int y = 0; y < TEX_H; y++)
    {
        // 利用 y 通道产生缓慢的色彩偏移
        int base_color = (y >> 1) + t;
        for (int x = 0; x < TEX_W; x++)
        {
            // 核心公式：高频正交脉冲
            // 产生一系列垂直的、具有周期性空隙的“逻辑柱”
            int val = (x + t) ^ (x << 1);
            if ((val & 0x1C) == 0x1C)
            {
                *p++ = palette[(base_color + (x >> 2)) & 0xFF];
            }
            else
            {
                *p++ = 0x0000;
            }
        }
    }
    // 强制同步 D-Cache
    aicos_dcache_clean_range((void *)g_tex_vir_addr, TEX_SIZE);

    /* --- PHASE 2: GE 硬件多程干涉合成 --- */

    // 1. 主屏幕清屏 (深空黑)
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

    // 2. 双程干涉叠加
    for (int i = 0; i < 2; i++)
    {
        struct ge_bitblt blt    = {0};
        blt.src_buf.buf_type    = MPP_PHY_ADDR;
        blt.src_buf.phy_addr[0] = g_tex_phy_addr;
        blt.src_buf.stride[0]   = TEX_W * 2;
        blt.src_buf.size.width  = TEX_W;
        blt.src_buf.size.height = TEX_H;
        blt.src_buf.format      = MPP_FMT_RGB_565;

        blt.dst_buf.buf_type    = MPP_PHY_ADDR;
        blt.dst_buf.phy_addr[0] = phy_addr;
        blt.dst_buf.stride[0]   = ctx->info.stride;
        blt.dst_buf.size.width  = ctx->info.width;
        blt.dst_buf.size.height = ctx->info.height;
        blt.dst_buf.format      = ctx->info.format;

        blt.dst_buf.crop_en     = 1;
        blt.dst_buf.crop.width  = ctx->info.width;
        blt.dst_buf.crop.height = ctx->info.height;
        blt.dst_buf.crop.x      = 0;
        blt.dst_buf.crop.y      = 0;

        // 镜像配置：第二程水平翻转
        if (i == 1)
        {
            blt.ctrl.flags            = MPP_FLIP_H;
            blt.ctrl.alpha_en         = 0;         // 使能混合
            blt.ctrl.alpha_rules      = GE_PD_ADD; // 加法累加
            blt.ctrl.src_alpha_mode   = 1;
            blt.ctrl.src_global_alpha = 180;
        }
        else
        {
            blt.ctrl.flags    = 0;
            blt.ctrl.alpha_en = 1; // 覆盖模式
        }

        // 核心视觉机能：动态缩放采样
        // 每一程使用不同的水平拉伸比例，产生干涉条纹 (Moire Pattern)
        blt.src_buf.crop_en     = 1;
        int zoom                = (i == 0) ? 280 : 300;
        int shake               = GET_SIN_10(t << 3) >> 10; // 极速微颤
        blt.src_buf.crop.width  = zoom + shake;
        blt.src_buf.crop.height = TEX_H;
        blt.src_buf.crop.x      = (TEX_W - blt.src_buf.crop.width) / 2;
        blt.src_buf.crop.y      = 0;

        mpp_ge_bitblt(ctx->ge, &blt);
        mpp_ge_emit(ctx->ge);
        mpp_ge_sync(ctx->ge);
    }

    /* --- PHASE 3: DE 硬件光谱律动 --- */
    struct aicfb_disp_prop prop = {0};
    prop.contrast               = 65;
    prop.bright                 = 48;
    prop.saturation             = 90;
    prop.hue                    = 50;
    mpp_fb_ioctl(ctx->fb, AICFB_SET_DISP_PROP, &prop);

    // 缓慢旋转色彩空间矩阵，模拟金属的反光变幻
    struct aicfb_ccm_config ccm = {0};
    ccm.enable                  = 1;
    int s                       = abs(GET_SIN_10(t << 1)) >> 5; // 0 ~ 128
    ccm.ccm_table[0]            = 0x100;
    ccm.ccm_table[5]            = 0x100 - s;
    ccm.ccm_table[10]           = 0x100 + s;
    mpp_fb_ioctl(ctx->fb, AICFB_UPDATE_CCM_CONFIG, &ccm);

    g_tick++;
}

static void effect_deinit(struct demo_ctx *ctx)
{
    // 恢复硬件状态
    struct aicfb_disp_prop r1 = {50, 50, 50, 50};
    mpp_fb_ioctl(ctx->fb, AICFB_SET_DISP_PROP, &r1);
    struct aicfb_ccm_config r2 = {0};
    r2.enable                  = 0;
    mpp_fb_ioctl(ctx->fb, AICFB_UPDATE_CCM_CONFIG, &r2);

    if (g_tex_phy_addr)
        mpp_phy_free(g_tex_phy_addr);
}

struct effect_ops effect_0045 = {
    .name   = "NO.45 LOGIC PRISM",
    .init   = effect_init,
    .draw   = effect_draw,
    .deinit = effect_deinit,
};

REGISTER_EFFECT(effect_0045);
