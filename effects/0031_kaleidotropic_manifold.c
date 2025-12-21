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
 *
 * Hardware Feature:
 * 1. GE Multi-Stage Composition (多级合成) - 源->旋转->合成->上屏
 * 2. GE Flip H/V (硬件镜像) - 构建四象限对称曼陀罗
 * 3. GE Rot1 (任意角度自旋)
 * 4. DE CCM (光谱纠缠矩阵) - 实时色彩空间旋转
 */

#include "demo_engine.h"
#include "mpp_mem.h"
#include "aic_hal_ge.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

/* --- Configuration Parameters --- */

/* 纹理规格 (源与旋转层) */
#define TEX_WIDTH  DEMO_QVGA_W
#define TEX_HEIGHT DEMO_QVGA_H
#define TEX_FMT    MPP_FMT_RGB_565
#define TEX_BPP    2
#define TEX_SIZE   (TEX_WIDTH * TEX_HEIGHT * TEX_BPP)

/* 合成缓冲区规格 (全屏) */
#define COMP_WIDTH  DEMO_SCREEN_WIDTH
#define COMP_HEIGHT DEMO_SCREEN_HEIGHT
#define COMP_SIZE   (COMP_WIDTH * COMP_HEIGHT * TEX_BPP)

/* 动画参数 */
#define DIST_SHIFT      8 // 距离场缩放 (dist >> 8)
#define ROT_SPEED_SHIFT 1 // 旋转速度 (t << 1)
#define CCM_SPEED_SHIFT 1 // 色彩矩阵旋转速度 (t << 1)

/* 查找表参数 */
#define LUT_SIZE     512
#define LUT_MASK     511
#define PALETTE_SIZE 256

/* --- Global State --- */

static unsigned int g_tex_phy_addr  = 0; // CPU源
static unsigned int g_rot_phy_addr  = 0; // 旋转后
static unsigned int g_comp_phy_addr = 0; // 全屏合成层
static uint16_t    *g_tex_vir_addr  = NULL;

static int      g_tick = 0;
static int      sin_lut[LUT_SIZE];
static uint16_t g_palette[PALETTE_SIZE];

/* --- Implementation --- */

static int effect_init(struct demo_ctx *ctx)
{
    // 1. 申请多级物理显存
    g_tex_phy_addr  = mpp_phy_alloc(DEMO_ALIGN_SIZE(TEX_SIZE));
    g_rot_phy_addr  = mpp_phy_alloc(DEMO_ALIGN_SIZE(TEX_SIZE));
    g_comp_phy_addr = mpp_phy_alloc(DEMO_ALIGN_SIZE(COMP_SIZE));

    if (!g_tex_phy_addr || !g_rot_phy_addr || !g_comp_phy_addr)
    {
        LOG_E("Night 31: CMA Alloc Failed.");
        if (g_tex_phy_addr)
            mpp_phy_free(g_tex_phy_addr);
        if (g_rot_phy_addr)
            mpp_phy_free(g_rot_phy_addr);
        if (g_comp_phy_addr)
            mpp_phy_free(g_comp_phy_addr);
        return -1;
    }

    g_tex_vir_addr = (uint16_t *)(unsigned long)g_tex_phy_addr;

    // 2. 初始化正弦表 (Q12)
    for (int i = 0; i < LUT_SIZE; i++)
    {
        sin_lut[i] = (int)(sinf(i * PI / (LUT_SIZE / 2.0f)) * Q12_ONE);
    }

    // 3. 初始化高频干涉调色板
    for (int i = 0; i < PALETTE_SIZE; i++)
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
        g_palette[i] = RGB2RGB565(r, g, b);
    }

    g_tick = 0;
    return 0;
}

#define GET_SIN(idx) (sin_lut[(idx) & LUT_MASK])
#define GET_COS(idx) (sin_lut[((idx) + (LUT_SIZE / 4)) & LUT_MASK])

static void effect_draw(struct demo_ctx *ctx, unsigned long phy_addr)
{
    if (!g_tex_vir_addr)
        return;

    int t = g_tick;

    /* --- PHASE 1: CPU 演算 (Texture Generation) --- */
    uint16_t *p  = g_tex_vir_addr;
    int       cx = TEX_WIDTH / 2;
    int       cy = TEX_HEIGHT / 2;

    for (int y = 0; y < TEX_HEIGHT; y++)
    {
        int dy2 = (y - cy) * (y - cy);
        for (int x = 0; x < TEX_WIDTH; x++)
        {
            int dx = x - cx;
            // 创造具有高度对称潜力的距离场
            int dist    = (dx * dx + dy2) >> DIST_SHIFT;
            int pattern = (x ^ y) ^ (dist + t);
            *p++        = g_palette[pattern & 0xFF];
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
    f_clean.dst_buf.stride[0]   = TEX_WIDTH * TEX_BPP;
    f_clean.dst_buf.size.width  = TEX_WIDTH;
    f_clean.dst_buf.size.height = TEX_HEIGHT;
    f_clean.dst_buf.format      = TEX_FMT;
    mpp_ge_fillrect(ctx->ge, &f_clean);
    mpp_ge_emit(ctx->ge);

    // 2. 清理全屏合成层 (彻底抹除镜像残影)
    f_clean.dst_buf.phy_addr[0] = g_comp_phy_addr;
    f_clean.dst_buf.stride[0]   = COMP_WIDTH * TEX_BPP;
    f_clean.dst_buf.size.width  = COMP_WIDTH;
    f_clean.dst_buf.size.height = COMP_HEIGHT;
    mpp_ge_fillrect(ctx->ge, &f_clean);
    mpp_ge_emit(ctx->ge);
    mpp_ge_sync(ctx->ge); // 确保清理完成

    // 3. 执行旋转 (Src -> Rot)
    struct ge_rotation rot  = {0};
    rot.src_buf.buf_type    = MPP_PHY_ADDR;
    rot.src_buf.phy_addr[0] = g_tex_phy_addr;
    rot.src_buf.stride[0]   = TEX_WIDTH * TEX_BPP;
    rot.src_buf.size.width  = TEX_WIDTH;
    rot.src_buf.size.height = TEX_HEIGHT;
    rot.src_buf.format      = TEX_FMT;

    rot.dst_buf.buf_type    = MPP_PHY_ADDR;
    rot.dst_buf.phy_addr[0] = g_rot_phy_addr;
    rot.dst_buf.stride[0]   = TEX_WIDTH * TEX_BPP;
    rot.dst_buf.size.width  = TEX_WIDTH;
    rot.dst_buf.size.height = TEX_HEIGHT;
    rot.dst_buf.format      = TEX_FMT;

    int theta            = (t << ROT_SPEED_SHIFT) & LUT_MASK;
    rot.angle_sin        = GET_SIN(theta);
    rot.angle_cos        = GET_COS(theta);
    rot.src_rot_center.x = TEX_WIDTH / 2;
    rot.src_rot_center.y = TEX_HEIGHT / 2;
    rot.dst_rot_center.x = TEX_WIDTH / 2;
    rot.dst_rot_center.y = TEX_HEIGHT / 2;
    rot.ctrl.alpha_en    = 1;

    mpp_ge_rotate(ctx->ge, &rot);
    mpp_ge_emit(ctx->ge);
    mpp_ge_sync(ctx->ge);

    /* --- PHASE 3: 镜像流形合成 (Four Quadrants) --- */
    // 将旋转后的纹理以不同镜像方式投射到合成缓冲区的四个象限
    for (int i = 0; i < 4; i++)
    {
        struct ge_bitblt blt    = {0};
        blt.src_buf.buf_type    = MPP_PHY_ADDR;
        blt.src_buf.phy_addr[0] = g_rot_phy_addr;
        blt.src_buf.stride[0]   = TEX_WIDTH * TEX_BPP;
        blt.src_buf.size.width  = TEX_WIDTH;
        blt.src_buf.size.height = TEX_HEIGHT;
        blt.src_buf.format      = TEX_FMT;

        blt.dst_buf.buf_type    = MPP_PHY_ADDR;
        blt.dst_buf.phy_addr[0] = g_comp_phy_addr; // 投影至全屏合成缓冲区
        blt.dst_buf.stride[0]   = COMP_WIDTH * TEX_BPP;
        blt.dst_buf.size.width  = COMP_WIDTH;
        blt.dst_buf.size.height = COMP_HEIGHT;
        blt.dst_buf.format      = TEX_FMT;

        // 象限划分：Target 320x240 @ (0,0), (320,0), (0,240), (320,240)
        // 注意：这里我们不做缩放，只是搬运
        blt.dst_buf.crop_en     = 1;
        blt.dst_buf.crop.width  = TEX_WIDTH;
        blt.dst_buf.crop.height = TEX_HEIGHT;
        blt.dst_buf.crop.x      = (i % 2) * TEX_WIDTH;
        blt.dst_buf.crop.y      = (i / 2) * TEX_HEIGHT;

        // 核心机能：硬件镜像翻转
        // 0: Top-Left (Normal)
        // 1: Top-Right (Flip H)
        // 2: Bottom-Left (Flip V)
        // 3: Bottom-Right (Flip H | V)
        if (i == 1)
            blt.ctrl.flags = MPP_FLIP_H;
        else if (i == 2)
            blt.ctrl.flags = MPP_FLIP_V;
        else if (i == 3)
            blt.ctrl.flags = (MPP_FLIP_H | MPP_FLIP_V);
        else
            blt.ctrl.flags = 0;

        blt.ctrl.alpha_en = 1; // 覆盖
        mpp_ge_bitblt(ctx->ge, &blt);
        mpp_ge_emit(ctx->ge);
    }
    // 象限绘制完毕后，同步一帧
    mpp_ge_sync(ctx->ge);

    /* --- PHASE 4: 最终上屏 --- */
    struct ge_bitblt final    = {0};
    final.src_buf.buf_type    = MPP_PHY_ADDR;
    final.src_buf.phy_addr[0] = g_comp_phy_addr;
    final.src_buf.stride[0]   = COMP_WIDTH * TEX_BPP;
    final.src_buf.size.width  = COMP_WIDTH;
    final.src_buf.size.height = COMP_HEIGHT;
    final.src_buf.format      = TEX_FMT;

    final.dst_buf.buf_type    = MPP_PHY_ADDR;
    final.dst_buf.phy_addr[0] = phy_addr;
    final.dst_buf.stride[0]   = ctx->info.stride;
    final.dst_buf.size.width  = ctx->info.width;
    final.dst_buf.size.height = ctx->info.height;
    final.dst_buf.format      = ctx->info.format;

    // 如果屏幕不是 640x480，这里会触发缩放
    final.dst_buf.crop_en     = 1;
    final.dst_buf.crop.width  = ctx->info.width;
    final.dst_buf.crop.height = ctx->info.height;

    final.ctrl.alpha_en = 1;
    mpp_ge_bitblt(ctx->ge, &final);
    mpp_ge_emit(ctx->ge);
    mpp_ge_sync(ctx->ge);

    /* --- PHASE 5: DE CCM 光谱旋转 --- */
    struct aicfb_ccm_config ccm = {0};
    ccm.enable                  = 1;
    int s                       = GET_SIN(t << CCM_SPEED_SHIFT) >> 5;

    // 简单的色彩旋转矩阵
    ccm.ccm_table[0]  = 0x100 - abs(s);
    ccm.ccm_table[1]  = s;
    ccm.ccm_table[5]  = 0x100 - abs(s);
    ccm.ccm_table[6]  = s;
    ccm.ccm_table[10] = 0x100;

    mpp_fb_ioctl(ctx->fb, AICFB_UPDATE_CCM_CONFIG, &ccm);

    g_tick++;
}

static void effect_deinit(struct demo_ctx *ctx)
{
    // 复位 CCM
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
