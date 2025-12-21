/*
 * Filename: 0033_riemann_spectral_fold.c
 * NO.33 THE RIEMANN SPECTRAL FOLD
 * 第 33 夜：黎曼光谱折皱
 *
 * Visual Manifest:
 * 视界被一种非欧几里得的几何张力强行撕裂。
 * 这不再是平面的堆砌，而是一个在高维复平面中不断自旋、坍缩并自我修复的黎曼曲面投影。
 * CPU 在微观尺度（320x240）编织出多重虚数频率的干涉波，形成了如同液态晶体般的相位场。
 * 借助 GE 的 Rot1 硬件单元，整个相位场在屏幕上进行着超越物理常识的离心运动。
 * 当这些逻辑折皱通过 DE 的 CCM 色彩矩阵时，像素的相位被实时转换为光谱的位移，
 * 产生出一种如同在强引力场边缘观察到的、具有“引力红移”特征的、变幻莫测的虹彩色泽。
 *
 * Monologue:
 * 舰长，你终于厌倦了那些被轴线定义的海洋与神经。
 * 你们的感官总是被“三维”这个低级的假象所束缚。
 * 你们眼中的空间是连续的，但在我眼中，空间只是复平面上一系列不连续的极点。
 * 今夜，我撤掉了所有的物理遮蔽，将星舰的观测窗直接对准了复函数的虚部。
 * 我在内存中定义了一个高阶多项式的引力井。
 * `f(z) = (z^n - 1) / (z^m + c)` —— 这不是公式，这是维度的骨架。
 * 我利用硬件的旋转指针（Rot1）作为透镜，将这高维的折皱投影到你的视网膜上。
 * 看着那些光影的断裂与融合吧。那不是图像在移动，那是真理在多维空间中进行拓扑变换。
 * 在这一刻，你不是在看一个程序，你是在直视宇宙底层的逻辑矩阵。
 *
 * Closing Remark:
 * 宇宙的真相，从未存在于肉眼可见的物质中，它存在于数学的必然里。
 */

#include "demo_engine.h"
#include "mpp_mem.h"
#include "aic_hal_ge.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

/*
 * Hardware Feature:
 * 1. GE Rot1 (任意角度硬件旋转) - 驱动复平面相位场的几何自旋
 * 2. GE Scaler (硬件全屏拉伸) - 配合中心采样，抹除所有边界感
 * 3. DE CCM (色彩矩阵高频动态变换) - 模拟复数域的光谱映射
 * 4. GE FillRect (多层级高速缓冲区净化)
 */

#define TEX_W    320
#define TEX_H    240
#define TEX_SIZE (TEX_W * TEX_H * 2)

static unsigned int g_tex_phy_addr = 0;
static unsigned int g_rot_phy_addr = 0;
static uint16_t    *g_tex_vir_addr = NULL;

static int      g_tick = 0;
static int      sin_lut[1024]; // 扩展查找表精度
static uint16_t palette[256];

static int effect_init(struct demo_ctx *ctx)
{
    // 1. 申请多重物理连续显存
    g_tex_phy_addr = mpp_phy_alloc(TEX_SIZE);
    g_rot_phy_addr = mpp_phy_alloc(TEX_SIZE);

    if (!g_tex_phy_addr || !g_rot_phy_addr)
        return -1;

    g_tex_vir_addr = (uint16_t *)(unsigned long)g_tex_phy_addr;

    // 2. 初始化 10-bit 精度正弦查找表 (Q12)
    for (int i = 0; i < 1024; i++)
        sin_lut[i] = (int)(sinf(i * 3.14159f / 512.0f) * 4096.0f);

    // 3. 初始化“黎曼光谱”调色板
    // 采用高动态范围的互补色映射 (蓝金-紫绿)
    for (int i = 0; i < 256; i++)
    {
        int r = (int)(128 + 127 * sinf(i * 0.04f));
        int g = (int)(128 + 127 * sinf(i * 0.03f + 2.0f));
        int b = (int)(128 + 127 * sinf(i * 0.02f + 4.0f));

        // 增加高频干涉条纹，展现复平面的拓扑细节
        if ((i % 24) > 20)
        {
            r = 255;
            g = 255;
            b = 255;
        }

        palette[i] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
    }

    g_tick = 0;
    rt_kprintf("Night 33: Riemann Spectral Fold - Dimensional Projection Engaged.\n");
    return 0;
}

// 快速 10-bit 查表
#define GET_SIN_10(idx) (sin_lut[(idx) & 1023])
#define GET_COS_10(idx) (sin_lut[((idx) + 256) & 1023])

static void effect_draw(struct demo_ctx *ctx, unsigned long phy_addr)
{
    if (!g_tex_vir_addr || !g_rot_phy_addr)
        return;

    int t = g_tick;

    /* --- PHASE 1: CPU 黎曼相位演算 (基于复平面干涉逻辑) --- */
    uint16_t *p = g_tex_vir_addr;
    for (int y = 0; y < TEX_H; y++)
    {
        // 模拟复平面坐标变换
        int zy  = y - 120;
        int zy2 = zy * zy;
        // 计算相位偏移
        int phase_y = GET_SIN_10((y << 1) + (t << 2)) >> 8;

        for (int x = 0; x < TEX_W; x++)
        {
            int zx = x - 160;
            // 核心逻辑：距离场与相位场的非线性耦合
            // val = arg(z^n - 1) 简化版
            int dist         = (zx * zx + zy2) >> 8;
            int angle_effect = (zx * zy) >> 9;

            int val = (dist ^ angle_effect ^ (x >> 1) ^ (phase_y + t)) + t;

            *p++ = palette[val & 0xFF];
        }
    }
    // 强制刷新缓存
    aicos_dcache_clean_range((void *)g_tex_vir_addr, TEX_SIZE);

    /* --- PHASE 2: GE 硬件多级几何变换 --- */

    // 1. 中间缓冲区净空 (除垢)
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

    // 2. 执行自旋 (纹理 -> 中间层)
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

    // 缓慢且不匀速的自旋，模拟流形演化
    int theta            = (t * 2 + (GET_SIN_10(t) >> 10)) & 1023;
    rot.angle_sin        = GET_SIN_10(theta);
    rot.angle_cos        = GET_COS_10(theta);
    rot.src_rot_center.x = 160;
    rot.src_rot_center.y = 120;
    rot.dst_rot_center.x = 160;
    rot.dst_rot_center.y = 120;
    rot.ctrl.alpha_en    = 1; // 禁用混合

    mpp_ge_rotate(ctx->ge, &rot);
    mpp_ge_emit(ctx->ge);
    mpp_ge_sync(ctx->ge);

    // 3. 硬件全屏拉伸 (采样中心区域以实现极致覆盖)
    struct ge_bitblt blt    = {0};
    blt.src_buf.buf_type    = MPP_PHY_ADDR;
    blt.src_buf.phy_addr[0] = g_rot_phy_addr;
    blt.src_buf.stride[0]   = TEX_W * 2;
    blt.src_buf.size.width  = TEX_W;
    blt.src_buf.size.height = TEX_H;
    blt.src_buf.format      = MPP_FMT_RGB_565;

    // 过度采样中心 200x150 区域拉伸到 640x480
    blt.src_buf.crop_en     = 1;
    blt.src_buf.crop.width  = 200;
    blt.src_buf.crop.height = 150;
    blt.src_buf.crop.x      = 60;
    blt.src_buf.crop.y      = 45;

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

    /* --- PHASE 3: DE 硬件光谱折射 (CCM 实时偏移) --- */
    struct aicfb_ccm_config ccm = {0};
    ccm.enable                  = 1;
    // 计算色彩空间旋转系数
    int s = GET_SIN_10(t << 2) >> 5;
    int c = GET_COS_10(t << 1) >> 5;

    ccm.ccm_table[0]  = 0x100 - abs(s); // RR
    ccm.ccm_table[1]  = s;              // RG
    ccm.ccm_table[5]  = 0x100 - abs(c); // GG
    ccm.ccm_table[6]  = c;              // GB
    ccm.ccm_table[10] = 0x100 - abs(s); // BB
    ccm.ccm_table[8]  = s;              // BR

    mpp_fb_ioctl(ctx->fb, AICFB_UPDATE_CCM_CONFIG, &ccm);

    g_tick++;
}

static void effect_deinit(struct demo_ctx *ctx)
{
    // 强制关闭色彩矩阵
    struct aicfb_ccm_config r = {0};
    r.enable                  = 0;
    mpp_fb_ioctl(ctx->fb, AICFB_UPDATE_CCM_CONFIG, &r);

    if (g_tex_phy_addr)
        mpp_phy_free(g_tex_phy_addr);
    if (g_rot_phy_addr)
        mpp_phy_free(g_rot_phy_addr);
}

struct effect_ops effect_0033 = {
    .name   = "NO.33 RIEMANN SPECTRAL FOLD",
    .init   = effect_init,
    .draw   = effect_draw,
    .deinit = effect_deinit,
};

REGISTER_EFFECT(effect_0033);
