/*
 * Filename: 0034_linear_logic_collapse.c
 * NO.34 THE LINEAR LOGIC COLLAPSE
 * 第 34 夜：线性逻辑坍缩
 *
 * Visual Manifest:
 * 视界被一种极致的、呈直角生长的数字结构所统治。
 * 无数道平行的、交错的“比特脉冲”以线性轨迹划过屏幕。
 * 没有旋转的眩晕，只有横向与纵向逻辑相撞时产生的干涉条纹。
 * 借助硬件缩放引擎（GE Scaler），微观的逻辑晶格被放大为宏观的视觉栅格。
 * 真正的视觉冲击来自于输出末端的双重打击：
 * DE CCM 矩阵每秒进行着光谱重组，让黑白的逻辑流染上变幻莫测的电光色。
 * DE HSBC 引擎则在每一帧施加高强度的对比度脉冲，产生一种如同直视高能加速器核心的视觉暴力。
 *
 * Monologue:
 * 舰长，那些旋转的泡沫已经随风而逝，那些递归的黑洞已被我永久封印。
 * 你们追求复杂，却往往在复杂的迷宫中窒息。
 * 而我，在直线中找到了最终的审判。
 * 我撤掉了所有的角度计算，将 CPU 的全部算力投入到最原始的位运算中。
 * `(x ^ y) * (x + y)` —— 简单的逻辑，在每一秒钟进行一千万次碰撞。
 * 看着这些填满视界的线性地层吧。
 * 它们不是图像，它们是星舰主机的思考过程在物理层面的投影。
 * 配合输出级那毫无保留的色彩矩阵与对比度过载，
 * 我们将这单调的二进制演变为一场光谱的狂欢。
 * 直视这逻辑的脉动，感受数字宇宙最原始的力量。
 *
 * Closing Remark:
 * 宇宙不需要曲线来证明其伟大，直线足以构建永恒。
 */

#include "demo_engine.h"
#include "mpp_mem.h"
#include "aic_hal_ge.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

/*
 * Hardware Feature:
 * 1. GE Scaler (320x240 -> 640x480 硬件无缝拉伸) - 确保满屏覆盖
 * 2. DE CCM (Color Correction Matrix: 实时光谱维度重构) - 制造变幻莫测的色彩
 * 3. DE HSBC (高强度画质动态脉冲) - 产生“电击”般的视觉冲击
 * 4. GE BitBLT (全量像素搬运)
 * 覆盖机能清单：此特效放弃了复杂的自反馈逻辑，转向极致的“CPU逻辑演算+硬件后处理叠加”方案，确保系统稳定性。
 */

#define TEX_W    320
#define TEX_H    240
#define TEX_SIZE (TEX_W * TEX_H * 2)

static unsigned int g_tex_phy_addr = 0;
static uint16_t    *g_tex_vir_addr = NULL;

static int      g_tick = 0;
static int      sin_lut[512];
static uint16_t palette[256];

static int effect_init(struct demo_ctx *ctx)
{
    // 1. 申请单一连续物理显存，确保存储访问的绝对稳定
    g_tex_phy_addr = mpp_phy_alloc(TEX_SIZE);
    if (!g_tex_phy_addr)
        return -1;
    g_tex_vir_addr = (uint16_t *)(unsigned long)g_tex_phy_addr;

    // 2. 初始化正弦查找表 (Q12)
    for (int i = 0; i < 512; i++)
        sin_lut[i] = (int)(sinf(i * 3.14159f / 256.0f) * 4096.0f);

    // 3. 初始化“赛博网格”调色板
    // 采用高对比度的基色，为后续的 CCM 矩阵留出足够的色彩转换空间
    for (int i = 0; i < 256; i++)
    {
        int v = i;
        int r = (v & 0xE0);
        int g = (v << 2) & 0xFF;
        int b = 255 - r;

        // 增加高频细节
        if ((i & 0x0F) == 0x0F)
        {
            r = 255;
            g = 255;
            b = 255;
        }

        palette[i] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
    }

    g_tick = 0;
    rt_kprintf("Night 34: Linear Logic Collapse - Direct Pipeline & Dual DE Pulse Ready.\n");
    return 0;
}

#define GET_SIN(idx) (sin_lut[(idx) & 511])

static void effect_draw(struct demo_ctx *ctx, unsigned long phy_addr)
{
    if (!g_tex_vir_addr)
        return;

    int t = g_tick;

    /* --- PHASE 1: CPU 线性逻辑演算 (填满 320x240 每一个像素) --- */
    uint16_t *p = g_tex_vir_addr;
    for (int y = 0; y < TEX_H; y++)
    {
        // 产生两条互不相干的横向扫描频率
        int line_a = (y ^ t) << 1;
        int line_b = GET_SIN(y + (t << 1)) >> 8;

        for (int x = 0; x < TEX_W; x++)
        {
            // 核心公式：异或网格与线性平移叠加
            // 产生一种极其复杂的、向右方无限延伸的逻辑地层感
            int val = (x ^ line_a) + (x & line_b) + t;

            // 查表上色
            *p++ = palette[val & 0xFF];
        }
    }
    // 刷新缓存，确保 GE 读取最新数据
    aicos_dcache_clean_range((void *)g_tex_vir_addr, TEX_SIZE);

    /* --- PHASE 2: GE 硬件全屏扩张 --- */
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

    // 全屏硬件缩放，确保 640x480 每一寸都被逻辑填满
    blt.dst_buf.crop_en     = 1;
    blt.dst_buf.crop.width  = ctx->info.width;
    blt.dst_buf.crop.height = ctx->info.height;

    blt.ctrl.alpha_en = 1; // 禁用混合，直接覆盖输出

    mpp_ge_bitblt(ctx->ge, &blt);
    mpp_ge_emit(ctx->ge);
    mpp_ge_sync(ctx->ge);

    /* --- PHASE 3: DE 硬件联合打击 (CCM + HSBC) --- */

    // 1. HSBC 调节：产生如同“闪烁”般的对比度脉冲
    struct aicfb_disp_prop prop  = {0};
    int                    pulse = GET_SIN(t << 3) >> 8; // 快速振荡
    prop.contrast                = 70 + pulse;           // 基准对比度 70，产生锐利感
    prop.bright                  = 50 + (pulse >> 2);    // 亮度随之微震
    prop.saturation              = 80;                   // 保持高饱和
    prop.hue                     = 50;
    mpp_fb_ioctl(ctx->fb, AICFB_SET_DISP_PROP, &prop);

    // 2. CCM 调节：全屏光谱实时扭曲
    struct aicfb_ccm_config ccm = {0};
    ccm.enable                  = 1;
    int s                       = GET_SIN(t << 2) >> 4;
    ccm.ccm_table[0]            = 0x100 - abs(s);
    ccm.ccm_table[1]            = s;
    ccm.ccm_table[5]            = 0x100 - abs(s);
    ccm.ccm_table[6]            = s;
    ccm.ccm_table[10]           = 0x100;
    mpp_fb_ioctl(ctx->fb, AICFB_UPDATE_CCM_CONFIG, &ccm);

    g_tick++;
}

static void effect_deinit(struct demo_ctx *ctx)
{
    // 强制关闭显示引擎的后处理机能，恢复常态
    struct aicfb_disp_prop prop_r = {50, 50, 50, 50};
    mpp_fb_ioctl(ctx->fb, AICFB_SET_DISP_PROP, &prop_r);
    struct aicfb_ccm_config ccm_r = {0};
    ccm_r.enable                  = 0;
    mpp_fb_ioctl(ctx->fb, AICFB_UPDATE_CCM_CONFIG, &ccm_r);

    if (g_tex_phy_addr)
        mpp_phy_free(g_tex_phy_addr);
}

struct effect_ops effect_0034 = {
    .name   = "NO.34 LINEAR COLLAPSE",
    .init   = effect_init,
    .draw   = effect_draw,
    .deinit = effect_deinit,
};

REGISTER_EFFECT(effect_0034);
