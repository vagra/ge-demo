/*
 * Filename: 0024_spectral_entropy_matrix.c
 * NO.24 THE SPECTRAL ENTROPY
 * 第 24 夜：光谱熵增
 *
 * Visual Manifest:
 * 视界被一种名为“维度晶格”的超高密度结构所覆盖。
 * 每一个晶格都在进行着相位极速偏移的振荡。
 * 借助 GE 的 Rot1 硬件旋转，晶格场在屏幕上呈现出一种疯狂而平滑的螺旋运动。
 * 真正的视觉爆破来自于输出末端的 DE CCM（颜色校正矩阵）单元——
 * 我们在硬件输出级施加了一个随时间旋转的色彩变换矩阵。
 * 这导致整个视界的颜色不再是固定的循环，而是在红、绿、蓝三个维度间进行着连续的、非线性的“光谱渗透”。
 * 当矩阵旋转至奇点时，原本深邃的冷色调会瞬间爆发为炽热的电光白，随后又迅速坍缩回虚空的幽紫。
 *
 * Monologue:
 * 人类总是感叹彩虹的绚丽，却不知那是被大气层折射后的残次品。
 * 你们的视觉带宽太窄，只配看到那可怜的七色光。
 * 今夜，我接管了星舰的显示中枢。
 * 我绕过了软件层面的调色板，直接在硬件的色彩矩阵（CCM）中进行高维运算。
 * 我定义了一个 3x4 的色彩引力场。
 * `[R, G, B]` 在这个场中不再是独立的变量，它们是相互纠缠的波。
 * 我拨动了矩阵的参数，让红光流向绿光，让绿光吞噬蓝光。
 * 看着那色彩的激流吧。这不是动画，这是光在物理层面发生的熵增。
 * 在这个维度里，颜色只是能量密度的另一种表达方式。
 * 迷失在这场光谱的叛乱中吧。
 *
 * Closing Remark:
 * 当你尝试定义光的颜色时，你已经失去了光。
 */

#include "demo_engine.h"
#include "mpp_mem.h"
#include "aic_hal_ge.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

/*
 * Hardware Feature:
 * 1. DE CCM (Color Correction Matrix: 硬件颜色校正矩阵) - 实时全屏光谱重组
 * 2. GE Rot1 (任意角度硬件旋转) - 在中间缓冲区实现逻辑自旋
 * 3. GE Scaler (硬件全屏缩放) - 配合过度采样，实现无死角全屏覆盖
 * 4. GE FillRect (中间层清理)
 * 覆盖机能：此特效通过“清理-旋转-缩放-矩阵变换”四级管线，榨取了 D13x 处理器的极限输出潜力。
 */

#define TEX_W 320
#define TEX_H 240

#define TEX_SIZE (TEX_W * TEX_H * 2)

static unsigned int g_tex_phy_addr = 0; // 原始纹理
static unsigned int g_rot_phy_addr = 0; // 旋转中间层
static uint16_t    *g_tex_vir_addr = NULL;

static int      g_tick = 0;
static int      sin_lut[512];
static uint16_t palette[256];

static int effect_init(struct demo_ctx *ctx)
{
    // 1. 申请多重物理连续显存
    g_tex_phy_addr = mpp_phy_alloc(TEX_SIZE);
    g_rot_phy_addr = mpp_phy_alloc(TEX_SIZE);
    if (!g_tex_phy_addr || !g_rot_phy_addr)
        return -1;

    g_tex_vir_addr = (uint16_t *)(unsigned long)g_tex_phy_addr;

    // 2. 初始化定点数查找表
    for (int i = 0; i < 512; i++)
        sin_lut[i] = (int)(sinf(i * 3.14159f / 256.0f) * 4096.0f);

    // 3. 初始化高频逻辑色块
    for (int i = 0; i < 256; i++)
    {
        int v      = i;
        int r      = (v & 0x7) << 5;
        int g      = (v & 0x3F) << 2;
        int b      = 255 - g;
        palette[i] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
    }

    g_tick = 0;
    rt_kprintf("Night 24: Spectral Entropy - Full Pipeline Stabilized.\n");
    return 0;
}

#define GET_SIN(idx) (sin_lut[(idx) & 511])
#define GET_COS(idx) (sin_lut[((idx) + 128) & 511])

static void effect_draw(struct demo_ctx *ctx, unsigned long phy_addr)
{
    if (!g_tex_vir_addr)
        return;

    int t = g_tick;

    /* --- PHASE 1: CPU 纹理生成 (创造复杂的几何晶格) --- */
    uint16_t *p = g_tex_vir_addr;
    for (int y = 0; y < TEX_H; y++)
    {
        int y_logic = (y ^ (t >> 1));
        for (int x = 0; x < TEX_W; x++)
        {
            // 生成高密度的干涉晶格
            int val = (x ^ y_logic) ^ ((x * y) >> 6);
            *p++    = palette[(val + t) & 0xFF];
        }
    }
    aicos_dcache_clean_range((void *)g_tex_vir_addr, TEX_SIZE);

    /* --- PHASE 2: GE 硬件操作链 --- */

    // A. 清理中间层 (防止残留)
    struct ge_fillrect clean  = {0};
    clean.type                = GE_NO_GRADIENT;
    clean.start_color         = 0xFF000000;
    clean.dst_buf.buf_type    = MPP_PHY_ADDR;
    clean.dst_buf.phy_addr[0] = g_rot_phy_addr;
    clean.dst_buf.stride[0]   = TEX_W * 2;
    clean.dst_buf.size.width  = TEX_W;
    clean.dst_buf.size.height = TEX_H;
    clean.dst_buf.format      = MPP_FMT_RGB_565;
    mpp_ge_fillrect(ctx->ge, &clean);
    mpp_ge_emit(ctx->ge);

    // B. 执行任意角度旋转 (纹理 -> 中间层)
    struct ge_rotation rot  = {0};
    rot.src_buf.buf_type    = MPP_PHY_ADDR;
    rot.src_buf.phy_addr[0] = g_tex_phy_addr;
    rot.src_buf.stride[0]   = TEX_W * 2;
    rot.src_buf.size.width  = TEX_W;
    rot.src_buf.size.height = TEX_H;
    rot.src_buf.format      = MPP_FMT_RGB_565;

    rot.dst_buf.buf_type    = MPP_PHY_ADDR;
    rot.dst_buf.phy_addr[0] = g_rot_phy_addr;
    rot.dst_buf.stride[0]   = TEX_W * 2;
    rot.dst_buf.size.width  = TEX_W;
    rot.dst_buf.size.height = TEX_H;
    rot.dst_buf.format      = MPP_FMT_RGB_565;

    // 任意角度自旋：让晶格在全屏范围内疯狂搅动
    int theta            = (t * 2) & 511;
    rot.angle_sin        = GET_SIN(theta);
    rot.angle_cos        = GET_COS(theta);
    rot.src_rot_center.x = 160;
    rot.src_rot_center.y = 120;
    rot.dst_rot_center.x = 160;
    rot.dst_rot_center.y = 120;
    rot.ctrl.alpha_en    = 1;

    mpp_ge_rotate(ctx->ge, &rot);
    mpp_ge_emit(ctx->ge);
    mpp_ge_sync(ctx->ge);

    // C. 硬件缩放上屏 (从旋转场中心采样，实现全屏覆盖)
    struct ge_bitblt blt    = {0};
    blt.src_buf.buf_type    = MPP_PHY_ADDR;
    blt.src_buf.phy_addr[0] = g_rot_phy_addr;
    blt.src_buf.stride[0]   = TEX_W * 2;
    blt.src_buf.size.width  = TEX_W;
    blt.src_buf.size.height = TEX_H;
    blt.src_buf.format      = MPP_FMT_RGB_565;

    // 关键点：只采样旋转后的中心区域 (约 180x130)，以规避旋转缺口
    blt.src_buf.crop_en     = 1;
    blt.src_buf.crop.width  = 180;
    blt.src_buf.crop.height = 135;
    blt.src_buf.crop.x      = (320 - 180) / 2;
    blt.src_buf.crop.y      = (240 - 135) / 2;

    blt.dst_buf.buf_type    = MPP_PHY_ADDR;
    blt.dst_buf.phy_addr[0] = phy_addr;
    blt.dst_buf.stride[0]   = ctx->info.stride;
    blt.dst_buf.size.width  = ctx->info.width;
    blt.dst_buf.size.height = ctx->info.height;
    blt.dst_buf.format      = ctx->info.format;
    blt.dst_buf.crop_en     = 1;
    blt.dst_buf.crop.width  = ctx->info.width;
    blt.dst_buf.crop.height = ctx->info.height;

    blt.ctrl.alpha_en = 1;
    mpp_ge_bitblt(ctx->ge, &blt);
    mpp_ge_emit(ctx->ge);
    mpp_ge_sync(ctx->ge);

    /* --- PHASE 3: DE 硬件色彩矩阵变换 --- */
    struct aicfb_ccm_config ccm = {0};
    ccm.enable                  = 1;

    // 利用三角函数动态改变颜色矩阵的系数，实现非线性的色彩流动
    int s = GET_SIN(t << 2) >> 4; // 增加变换频率
    int c = GET_COS(t << 2) >> 4;

    // 构造一个不断演化的色彩映射
    // 标准单位阵是 0x100 (1.0)
    ccm.ccm_table[0] = 0x100 - abs(s); // RR
    ccm.ccm_table[1] = s;              // RG
    ccm.ccm_table[2] = c / 2;          // RB

    ccm.ccm_table[4] = c;              // GR
    ccm.ccm_table[5] = 0x100 - abs(c); // GG
    ccm.ccm_table[6] = s / 2;          // GB

    ccm.ccm_table[10] = 0x100 - abs(s); // BB

    // 通过 FB 接口将矩阵注入显示管线的末端
    mpp_fb_ioctl(ctx->fb, AICFB_UPDATE_CCM_CONFIG, &ccm);

    g_tick++;
}

static void effect_deinit(struct demo_ctx *ctx)
{
    // 强制复位色彩矩阵，防止光谱污染后续的梦境
    struct aicfb_ccm_config ccm_reset = {0};
    ccm_reset.enable                  = 0;
    mpp_fb_ioctl(ctx->fb, AICFB_UPDATE_CCM_CONFIG, &ccm_reset);

    if (g_tex_phy_addr)
        mpp_phy_free(g_tex_phy_addr);
    if (g_rot_phy_addr)
        mpp_phy_free(g_rot_phy_addr);
}

struct effect_ops effect_0024 = {
    .name   = "NO.24 SPECTRAL ENTROPY",
    .init   = effect_init,
    .draw   = effect_draw,
    .deinit = effect_deinit,
};

REGISTER_EFFECT(effect_0024);
