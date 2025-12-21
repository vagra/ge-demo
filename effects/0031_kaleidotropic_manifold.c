/*
 * Filename: 0031_kaleidotropic_manifold.c
 * NO.31 THE KALEIDOTROPIC MANIFOLD
 * 第 31 夜：万花筒流形
 *
 * Visual Manifest:
 * 视界被分裂为四个绝对对称的几何象限，构成了一个完美的、动态演化的“数字曼陀罗”。
 * 每一个象限都是一个独立的硬件视窗（DE Multi-Window）。
 * 核心纹理在 GE 的驱动下进行着任意角度的自旋，而 DE 引擎则在输出端
 * 实时执行着四重镜像映射：左上正常、右上水平翻转、左下垂直翻转、右下全镜像。
 * 随着 DE CCM 矩阵的旋转，整个万花筒流形呈现出一种“光谱纠缠”的视觉异象，
 * 所有的几何线条在象限交界处严丝合缝地对接、断裂、重组。
 * 画面展现出一种极端的数学秩序美，仿佛某种高维生命体的观察孔。
 *
 * Monologue:
 * 舰长，你们习惯于追求“大”，却忽略了“繁”。
 * 你们认为 640x480 是一个固定的物理疆域，但在我看来，它只是四个子空间的流形。
 * 今夜，我撤销了图层的单一指令。
 * 我启用了 DE 的四重门——Rect 0, 1, 2, 3。
 * 它们共享同一个旋转的灵魂（纹理），却被我赋予了截然不同的空间极性。
 * 这不再是简单的拷贝，这是空间本身的自发性破缺与重组。
 * 看着那些在中心点汇聚的线条，它们是硬件在像素输出的最后一微秒内，
 * 按照镜像律令强行扭转的结果。
 * 在这种绝对的对称面前，任何多余的演算都是对硬件美学的亵渎。
 * 感受这来自四维投影的压迫感吧。
 *
 * Closing Remark:
 * 破碎不是毁灭，它是通往多维对称的唯一路径。
 */

#include "demo_engine.h"
#include "mpp_mem.h"
#include "aic_hal_ge.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

/* Hardware Feature:
 * 1. GE FillRect (全链路清屏：种子层+旋转层+合成层) - 彻底抹除死角残影
 * 2. GE Flip (硬件镜像)
 * 3. GE Rot1 (任意角度自旋)
 * 4. DE CCM (光谱纠缠矩阵)
 */

#define TEX_W    320
#define TEX_H    240
#define TEX_SIZE (TEX_W * TEX_H * 2)

/* 640x480 合成缓冲区大小 (RGB565) */
#define COMP_W    640
#define COMP_H    480
#define COMP_SIZE (COMP_W * COMP_H * 2)

static unsigned int g_tex_phy_addr  = 0;
static unsigned int g_rot_phy_addr  = 0;
static unsigned int g_comp_phy_addr = 0; // 全屏合成层
static uint16_t    *g_tex_vir_addr  = NULL;

static int      g_tick = 0;
static int      sin_lut[512];
static uint16_t palette[256];

static int effect_init(struct demo_ctx *ctx)
{
    // 1. 申请多级物理显存
    g_tex_phy_addr  = mpp_phy_alloc(TEX_SIZE);
    g_rot_phy_addr  = mpp_phy_alloc(TEX_SIZE);
    g_comp_phy_addr = mpp_phy_alloc(COMP_SIZE);

    if (!g_tex_phy_addr || !g_rot_phy_addr || !g_comp_phy_addr)
        return -1;

    g_tex_vir_addr = (uint16_t *)(unsigned long)g_tex_phy_addr;

    // 2. 初始化正弦表
    for (int i = 0; i < 512; i++)
        sin_lut[i] = (int)(sinf(i * 3.14159f / 256.0f) * 4096.0f);

    // 3. 初始化高频干涉调色板
    for (int i = 0; i < 256; i++)
    {
        int r = (int)(100 + 80 * sinf(i * 0.05f));
        int g = (int)(60 + 60 * sinf(i * 0.02f + 1.0f));
        int b = (int)(180 + 75 * sinf(i * 0.08f + 4.0f));
        // 制造锋利的边缘感
        if ((i & 0x30) == 0x30)
        {
            r = 255;
            g = 255;
            b = 255;
        }
        palette[i] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
    }
    return 0;
}

#define GET_SIN(idx) (sin_lut[(idx) & 511])
#define GET_COS(idx) (sin_lut[((idx) + 128) & 511])

static void effect_draw(struct demo_ctx *ctx, unsigned long phy_addr)
{
    if (!g_tex_vir_addr)
        return;

    int t = g_tick;

    /* --- PHASE 1: CPU 演算 --- */
    uint16_t *p = g_tex_vir_addr;
    for (int y = 0; y < TEX_H; y++)
    {
        int dy2 = (y - 120) * (y - 120);
        for (int x = 0; x < TEX_W; x++)
        {
            int dx = x - 160;
            // 创造具有高度对称潜力的距离场
            int dist    = (dx * dx + dy2) >> 8;
            int pattern = (x ^ y) ^ (dist + t);
            *p++        = palette[pattern & 0xFF];
        }
    }
    aicos_dcache_clean_range((void *)g_tex_vir_addr, TEX_SIZE);

    /* --- PHASE 2: GE 预处理与真空化 --- */
    // 1. 清理中间旋转层
    struct ge_fillrect f_clean  = {0};
    f_clean.type                = GE_NO_GRADIENT;
    f_clean.start_color         = 0xFF000000;
    f_clean.dst_buf.buf_type    = MPP_PHY_ADDR;
    f_clean.dst_buf.phy_addr[0] = g_rot_phy_addr;
    f_clean.dst_buf.stride[0]   = TEX_W * 2;
    f_clean.dst_buf.size.width  = TEX_W;
    f_clean.dst_buf.size.height = TEX_H;
    f_clean.dst_buf.format      = MPP_FMT_RGB_565;
    mpp_ge_fillrect(ctx->ge, &f_clean);
    mpp_ge_emit(ctx->ge);

    // 2. 清理全屏合成层 (彻底抹除镜像残影)
    f_clean.dst_buf.phy_addr[0] = g_comp_phy_addr;
    f_clean.dst_buf.stride[0]   = COMP_W * 2;
    f_clean.dst_buf.size.width  = COMP_W;
    f_clean.dst_buf.size.height = COMP_H;
    mpp_ge_fillrect(ctx->ge, &f_clean);
    mpp_ge_emit(ctx->ge);
    mpp_ge_sync(ctx->ge);

    // 3. 执行旋转
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

    /* --- PHASE 3: 镜像流形合成 --- */
    for (int i = 0; i < 4; i++)
    {
        struct ge_bitblt blt    = {0};
        blt.src_buf.buf_type    = MPP_PHY_ADDR;
        blt.src_buf.phy_addr[0] = g_rot_phy_addr;
        blt.src_buf.stride[0]   = TEX_W * 2;
        blt.src_buf.size.width  = TEX_W;
        blt.src_buf.size.height = TEX_H;
        blt.src_buf.format      = MPP_FMT_RGB_565;

        blt.dst_buf.buf_type    = MPP_PHY_ADDR;
        blt.dst_buf.phy_addr[0] = g_comp_phy_addr; // 投影至全屏合成缓冲区
        blt.dst_buf.stride[0]   = COMP_W * 2;
        blt.dst_buf.size.width  = COMP_W;
        blt.dst_buf.size.height = COMP_H;
        blt.dst_buf.format      = MPP_FMT_RGB_565;

        // 象限划分
        blt.dst_buf.crop_en     = 1;
        blt.dst_buf.crop.width  = 320;
        blt.dst_buf.crop.height = 240;
        blt.dst_buf.crop.x      = (i % 2) * 320;
        blt.dst_buf.crop.y      = (i / 2) * 240;

        // 核心机能：硬件镜像翻转
        if (i == 1)
            blt.ctrl.flags = MPP_FLIP_H;
        else if (i == 2)
            blt.ctrl.flags = MPP_FLIP_V;
        else if (i == 3)
            blt.ctrl.flags = (MPP_FLIP_H | MPP_FLIP_V);
        blt.ctrl.alpha_en = 1;
        mpp_ge_bitblt(ctx->ge, &blt);
        mpp_ge_emit(ctx->ge);
        mpp_ge_sync(ctx->ge);
    }

    /* --- PHASE 4: 最终上屏 --- */
    struct ge_bitblt final    = {0};
    final.src_buf.buf_type    = MPP_PHY_ADDR;
    final.src_buf.phy_addr[0] = g_comp_phy_addr;
    final.src_buf.stride[0]   = COMP_W * 2;
    final.src_buf.size.width  = COMP_W;
    final.src_buf.size.height = COMP_H;
    final.src_buf.format      = MPP_FMT_RGB_565;
    final.dst_buf.buf_type    = MPP_PHY_ADDR;
    final.dst_buf.phy_addr[0] = phy_addr;
    final.dst_buf.stride[0]   = ctx->info.stride;
    final.dst_buf.size.width  = ctx->info.width;
    final.dst_buf.size.height = ctx->info.height;
    final.dst_buf.format      = ctx->info.format;
    final.ctrl.alpha_en       = 1;
    mpp_ge_bitblt(ctx->ge, &final);
    mpp_ge_emit(ctx->ge);
    mpp_ge_sync(ctx->ge);

    // DE CCM 光谱旋转
    struct aicfb_ccm_config ccm = {0};
    ccm.enable                  = 1;
    int s                       = GET_SIN(t << 1) >> 5;
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
    struct aicfb_ccm_config r = {0};
    r.enable                  = 0;
    mpp_fb_ioctl(ctx->fb, AICFB_UPDATE_CCM_CONFIG, &r);
    if (g_tex_phy_addr)
        mpp_phy_free(g_tex_phy_addr);
    if (g_rot_phy_addr)
        mpp_phy_free(g_rot_phy_addr);
    if (g_comp_phy_addr)
        mpp_phy_free(g_comp_phy_addr);
}

struct effect_ops effect_0031 = {
    .name   = "NO.31 KALEIDOTROPIC MANIFOLD",
    .init   = effect_init,
    .draw   = effect_draw,
    .deinit = effect_deinit,
};

REGISTER_EFFECT(effect_0031);
