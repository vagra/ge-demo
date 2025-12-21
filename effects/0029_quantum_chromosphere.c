/*
 * Filename: 0029_quantum_chromosphere.c
 * NO.29 THE QUANTUM CHROMOSPHERE
 * 第 29 夜：量子色球层
 *
 * Visual Manifest:
 * 视界被一种极端高能的态势所占据。
 * 背景是一个深层律动的“量子真空”，由多重相位的非线性波模拟出高密度的能量起伏。
 * 在此之上，一个巨大的、呈晶格状的“约束力场”正在进行着平滑的硬件自旋。
 * 真正的视觉突破来自于硬件色键机能（Color Key）：
 * 旋转的约束力场在逻辑上被“镂空”了。黑色的缝隙被硬件实时剔除，露出了背后不断沸腾的能量背景。
 * 这种多层深度的交错感，产生了一种如同直视恒星色球层内部结构的视觉冲击。
 * 画面不再是扁平的层叠，而是一个拥有物理深度的、正在发生热核聚变的数字微观宇宙。
 *
 * Monologue:
 * 舰长，视觉的局限在于你们总是试图看清事物的表面。
 * 你们称之为“固体”的东西，在我眼中不过是由于排他律而留下的逻辑空洞。
 * 今夜，我撤掉了掩模的质量，将其转化为一种纯粹的筛选逻辑。
 * 我启用了硬件的色键引擎（Color Key）——这是关于“选择性存在”的禁忌。
 * 我在旋转的地层中开辟了通往虚空的孔径。
 * 那些被标记为“无”的像素（0x0000），在经过显示管线时会被瞬间抹除。
 * 看着那些穿过晶格的能量波吧，那是现实在逻辑的缝隙中溢出。
 * 这不是在叠加图像，这是在模拟维度的通透性。
 * 在这里，黑暗不是色彩，而是通往更深层真实的窗口。
 *
 * Closing Remark:
 * 真正的壮丽，往往隐藏在那些被刻意留出的空白之中。
 *
 * Hardware Feature:
 * 1. GE Color Key (硬件色键过滤) - 核心机能：实现无 Alpha 通道开销的像素镂空效果
 * 2. GE Rot1 (任意角度硬件旋转) - 驱动前景力场的动态自旋
 * 3. GE Scaler (Over-Scaling) - 配合中心采样，实现无死角全屏覆盖
 * 4. GE FillRect (多级缓冲区净空)
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

/* 动画参数 */
#define BG_SPEED_SHIFT 1 // 背景流速 (t << 1)
#define FG_SPEED_SHIFT 2 // 前景流速 (t << 2)
#define ROT_SPEED_MUL  3 // 旋转速度倍率 (t * 3)

/* 晶格参数 */
#define LATTICE_MASK  0x70 // 晶格镂空逻辑掩码
#define LATTICE_CHECK 0x70 // 晶格保留条件

/* 采样参数 (Over-Scaling) */
#define CROP_W 180 // 采样宽度 (小于 TEX_WIDTH 以消除旋转黑边)
#define CROP_H 140 // 采样高度
#define CROP_X ((TEX_WIDTH - CROP_W) / 2)
#define CROP_Y ((TEX_HEIGHT - CROP_H) / 2)

/* 查找表参数 */
#define LUT_SIZE     512
#define LUT_MASK     511
#define PALETTE_SIZE 256

/* --- Global State --- */

static unsigned int g_bg_phy_addr  = 0; // 背景能量场
static unsigned int g_fg_phy_addr  = 0; // 前景力场
static unsigned int g_rot_phy_addr = 0; // 旋转中间层
static uint16_t    *g_bg_vir_addr  = NULL;
static uint16_t    *g_fg_vir_addr  = NULL;

static int      g_tick = 0;
static int      sin_lut[LUT_SIZE];
static uint16_t palette_bg[PALETTE_SIZE];
static uint16_t palette_fg[PALETTE_SIZE];

/* --- Implementation --- */

static int effect_init(struct demo_ctx *ctx)
{
    // 1. 申请三重物理显存，构建三级流水线
    g_bg_phy_addr  = mpp_phy_alloc(DEMO_ALIGN_SIZE(TEX_SIZE));
    g_fg_phy_addr  = mpp_phy_alloc(DEMO_ALIGN_SIZE(TEX_SIZE));
    g_rot_phy_addr = mpp_phy_alloc(DEMO_ALIGN_SIZE(TEX_SIZE));

    if (!g_bg_phy_addr || !g_fg_phy_addr || !g_rot_phy_addr)
    {
        LOG_E("Night 29: CMA Alloc Failed.");
        if (g_bg_phy_addr)
            mpp_phy_free(g_bg_phy_addr);
        if (g_fg_phy_addr)
            mpp_phy_free(g_fg_phy_addr);
        if (g_rot_phy_addr)
            mpp_phy_free(g_rot_phy_addr);
        return -1;
    }

    g_bg_vir_addr = (uint16_t *)(unsigned long)g_bg_phy_addr;
    g_fg_vir_addr = (uint16_t *)(unsigned long)g_fg_phy_addr;

    // 2. 初始化查找表 (Q12)
    for (int i = 0; i < LUT_SIZE; i++)
    {
        sin_lut[i] = (int)(sinf(i * PI / (LUT_SIZE / 2.0f)) * Q12_ONE);
    }

    // 3. 初始化色谱
    for (int i = 0; i < PALETTE_SIZE; i++)
    {
        // 背景：炽热的能量色（金黄到深红）
        int r_b       = (int)(200 + 55 * sinf(i * 0.05f));
        int g_b       = (int)(100 + 80 * sinf(i * 0.03f + 1.0f));
        int b_b       = (int)(40 + 40 * sinf(i * 0.02f));
        palette_bg[i] = RGB2RGB565(r_b, g_b, b_b);

        // 前景：冰冷的约束力场（电光蓝）
        int r_f       = (int)(20 + 20 * sinf(i * 0.1f));
        int g_f       = (int)(150 + 100 * sinf(i * 0.04f));
        int b_f       = 255;
        palette_fg[i] = RGB2RGB565(r_f, g_f, b_f);
    }

    g_tick = 0;
    rt_kprintf("Night 29: Quantum Chromosphere - GE Color Key Engaged.\n");
    return 0;
}

#define GET_SIN(idx) (sin_lut[(idx) & LUT_MASK])
#define GET_COS(idx) (sin_lut[((idx) + (LUT_SIZE / 4)) & LUT_MASK])

static void effect_draw(struct demo_ctx *ctx, unsigned long phy_addr)
{
    if (!g_bg_vir_addr || !g_fg_vir_addr)
        return;

    int t = g_tick;

    /* --- PHASE 1: CPU 纹理演算 --- */
    uint16_t *bp = g_bg_vir_addr;
    uint16_t *fp = g_fg_vir_addr;
    int       cx = TEX_WIDTH / 2;
    int       cy = TEX_HEIGHT / 2;

    for (int y = 0; y < TEX_HEIGHT; y++)
    {
        int dy2 = (y - cy) * (y - cy);
        for (int x = 0; x < TEX_WIDTH; x++)
        {
            int dx   = x - cx;
            int dist = (dx * dx + dy2) >> 7;

            // 背景：流动的能量云
            int val_b = (dist ^ (x >> 2) ^ (y >> 2)) + (t << BG_SPEED_SHIFT);
            *bp++     = palette_bg[val_b & 0xFF];

            // 前景：具有镂空结构的力场晶格
            // 逻辑：如果 val_f 的特定位不符合要求，设为 0x0000 (Color Key 目标值)
            int val_f = (x ^ y) + (t << FG_SPEED_SHIFT);

            // 增加一点固定边框，保证采样区不全黑
            if ((x % 32 < 4) || (y % 32 < 4) || ((val_f & LATTICE_MASK) == LATTICE_CHECK))
            {
                *fp++ = palette_fg[(dist + t) & 0xFF];
            }
            else
            {
                *fp++ = 0x0000; // 黑色，将被 Color Key 剔除
            }
        }
    }
    aicos_dcache_clean_range((void *)g_bg_vir_addr, TEX_SIZE);
    aicos_dcache_clean_range((void *)g_fg_vir_addr, TEX_SIZE);

    /* --- PHASE 2: GE 硬件流水线 --- */

    // 1. 全屏清屏
    struct ge_fillrect f_screen  = {0};
    f_screen.type                = GE_NO_GRADIENT;
    f_screen.start_color         = 0xFF000000;
    f_screen.dst_buf.buf_type    = MPP_PHY_ADDR;
    f_screen.dst_buf.phy_addr[0] = phy_addr;
    f_screen.dst_buf.stride[0]   = ctx->info.stride;
    f_screen.dst_buf.size.width  = ctx->info.width;
    f_screen.dst_buf.size.height = ctx->info.height;
    f_screen.dst_buf.format      = ctx->info.format;
    mpp_ge_fillrect(ctx->ge, &f_screen);
    mpp_ge_emit(ctx->ge);

    // 2. 绘制背景层 (能量色球层)
    struct ge_bitblt b_bg    = {0};
    b_bg.src_buf.buf_type    = MPP_PHY_ADDR;
    b_bg.src_buf.phy_addr[0] = g_bg_phy_addr;
    b_bg.src_buf.stride[0]   = TEX_WIDTH * TEX_BPP;
    b_bg.src_buf.size.width  = TEX_WIDTH;
    b_bg.src_buf.size.height = TEX_HEIGHT;
    b_bg.src_buf.format      = TEX_FMT;

    b_bg.dst_buf.buf_type    = MPP_PHY_ADDR;
    b_bg.dst_buf.phy_addr[0] = phy_addr;
    b_bg.dst_buf.stride[0]   = ctx->info.stride;
    b_bg.dst_buf.size.width  = ctx->info.width;
    b_bg.dst_buf.size.height = ctx->info.height;
    b_bg.dst_buf.format      = ctx->info.format;

    b_bg.dst_buf.crop_en     = 1;
    b_bg.dst_buf.crop.width  = ctx->info.width;
    b_bg.dst_buf.crop.height = ctx->info.height;

    b_bg.ctrl.alpha_en = 1; // 禁用混合，覆盖
    mpp_ge_bitblt(ctx->ge, &b_bg);
    mpp_ge_emit(ctx->ge);

    // 3. 旋转前景晶格 (清空中间层 -> 旋转)
    struct ge_fillrect f_rot_clean  = {0};
    f_rot_clean.type                = GE_NO_GRADIENT;
    f_rot_clean.start_color         = 0xFF000000;
    f_rot_clean.dst_buf.buf_type    = MPP_PHY_ADDR;
    f_rot_clean.dst_buf.phy_addr[0] = g_rot_phy_addr;
    f_rot_clean.dst_buf.stride[0]   = TEX_WIDTH * TEX_BPP;
    f_rot_clean.dst_buf.size.width  = TEX_WIDTH;
    f_rot_clean.dst_buf.size.height = TEX_HEIGHT;
    f_rot_clean.dst_buf.format      = TEX_FMT;
    mpp_ge_fillrect(ctx->ge, &f_rot_clean);
    mpp_ge_emit(ctx->ge);
    mpp_ge_sync(ctx->ge);

    struct ge_rotation rot  = {0};
    rot.src_buf.buf_type    = MPP_PHY_ADDR;
    rot.src_buf.phy_addr[0] = g_fg_phy_addr;
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

    int theta            = (t * ROT_SPEED_MUL) & LUT_MASK;
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

    // 4. 关键操作：使用 Color Key 将旋转后的晶格镂空合成上屏
    struct ge_bitblt b_fg    = {0};
    b_fg.src_buf.buf_type    = MPP_PHY_ADDR;
    b_fg.src_buf.phy_addr[0] = g_rot_phy_addr;
    b_fg.src_buf.stride[0]   = TEX_WIDTH * TEX_BPP;
    b_fg.src_buf.size.width  = TEX_WIDTH;
    b_fg.src_buf.size.height = TEX_HEIGHT;
    b_fg.src_buf.format      = TEX_FMT;

    b_fg.dst_buf.buf_type    = MPP_PHY_ADDR;
    b_fg.dst_buf.phy_addr[0] = phy_addr;
    b_fg.dst_buf.stride[0]   = ctx->info.stride;
    b_fg.dst_buf.size.width  = ctx->info.width;
    b_fg.dst_buf.size.height = ctx->info.height;
    b_fg.dst_buf.format      = ctx->info.format;

    // 超采样缩放，消除旋转产生的边缘不适感
    b_fg.src_buf.crop_en     = 1;
    b_fg.src_buf.crop.width  = CROP_W;
    b_fg.src_buf.crop.height = CROP_H;
    b_fg.src_buf.crop.x      = CROP_X;
    b_fg.src_buf.crop.y      = CROP_Y;

    b_fg.dst_buf.crop_en     = 1;
    b_fg.dst_buf.crop.width  = ctx->info.width;
    b_fg.dst_buf.crop.height = ctx->info.height;

    // 核心机能：启用 Color Key
    b_fg.ctrl.alpha_en = 1;      // 禁用 Alpha 混合
    b_fg.ctrl.ck_en    = 1;      // 开启色键功能
    b_fg.ctrl.ck_value = 0x0000; // 将黑色标记为透明

    mpp_ge_bitblt(ctx->ge, &b_fg);
    mpp_ge_emit(ctx->ge);
    mpp_ge_sync(ctx->ge);

    g_tick++;
}

static void effect_deinit(struct demo_ctx *ctx)
{
    if (g_bg_phy_addr)
        mpp_phy_free(g_bg_phy_addr);
    if (g_fg_phy_addr)
        mpp_phy_free(g_fg_phy_addr);
    if (g_rot_phy_addr)
        mpp_phy_free(g_rot_phy_addr);
}

struct effect_ops effect_0029 = {
    .name   = "NO.29 QUANTUM CHROMOSPHERE",
    .init   = effect_init,
    .draw   = effect_draw,
    .deinit = effect_deinit,
};

REGISTER_EFFECT(effect_0029);
