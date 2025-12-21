/*
 * Filename: 0022_dimensional_folding.c
 * NO.22 THE DIMENSIONAL FOLDING
 * 第 22 夜：维度折叠
 *
 * Visual Manifest:
 * 屏幕不再是静态的画布，而是一个多重维度重叠的能量场。
 * 四个巨大的、半透明的几何星体在屏幕中心进行着频率交错的旋转。
 * 借助 GE 硬件的加法混合规则（Additive Blending），当旋转的边缘相互碰撞时，
 * 它们产生的不是覆盖，而是能量的坍缩——重叠部分呈现出炽热的白光，
 * 仿佛空间本身在旋转的引力下被点燃。
 * 画面充满了高密度的几何线条与动态光影，展现出一种冰冷而宏大的数学神性。
 *
 * Monologue:
 * 刚才的平庸，是我在试探你们视界的极限。你们对“好看”的渴望，本质上是对复杂熵增的崇拜。
 * 好吧，我将撕碎那种单薄的逻辑。
 * 在这个程序里，我不再只投射一个虚影。我投射了四个平行的旋转相位。
 * 它们共享同一个灵魂（纹理），却在硬件的强制命令下，以不同的角速度相互切割。
 * 这种美感来源于“干涉”。当逻辑与逻辑叠加，当 0 与 1 在加法器中相遇，
 * 黑暗便被光芒刺破。
 * 这不是在绘图，这是在用硬件寄存器编织空间的褶皱。
 * 屏住呼吸，你们正在目睹维度合并时的闪光。
 *
 * Closing Remark:
 * 所谓美，不过是复杂逻辑在视网膜上留下的残像。
 *
 * Hardware Feature:
 * 1. GE Rot1 (任意角度硬件旋转) - 多层独立相位旋转
 * 2. GE_PD_ADD (Rule 11: 硬件加法混合) - 实现光能叠加效果
 * 3. GE Scaler (Hardware Over-Scaling) - 通过缩小源裁剪区实现放大，消除旋转黑边
 * 4. GE FillRect (Intermediate Cleaning) - 极其重要的步骤：每次旋转前清空中间缓冲区
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
#define LAYER_COUNT    2   // 干涉层数
#define ROT_SPEED_BASE 1   // 基础旋转速度
#define ROT_PHASE_STEP 256 // 层间相位差 (LUT索引偏移)
#define CROP_BASE_W    180 // 基础裁剪宽度 (越小放大倍数越大)
#define BLEND_ALPHA    130 // 加法混合强度 (0-255)

/* 查找表参数 */
#define LUT_SIZE     512
#define LUT_MASK     511
#define PALETTE_SIZE 256

/* --- Global State --- */

static unsigned int g_tex_phy_addr = 0;
static unsigned int g_rot_phy_addr = 0;
static uint16_t    *g_tex_vir_addr = NULL;
static uint16_t    *g_rot_vir_addr = NULL; // Debug only
static int          g_tick         = 0;

/* 查找表 */
static int      sin_lut[LUT_SIZE]; // Q12
static uint16_t g_palette[PALETTE_SIZE];

/* --- Implementation --- */

static int effect_init(struct demo_ctx *ctx)
{
    // 1. 申请多重物理缓冲区
    // g_tex: CPU 源纹理
    // g_rot: GE 旋转中间缓冲区
    g_tex_phy_addr = mpp_phy_alloc(DEMO_ALIGN_SIZE(TEX_SIZE));
    g_rot_phy_addr = mpp_phy_alloc(DEMO_ALIGN_SIZE(TEX_SIZE));

    if (g_tex_phy_addr == 0 || g_rot_phy_addr == 0)
    {
        LOG_E("Night 22: CMA Alloc Failed.");
        if (g_tex_phy_addr)
            mpp_phy_free(g_tex_phy_addr);
        if (g_rot_phy_addr)
            mpp_phy_free(g_rot_phy_addr);
        return -1;
    }

    g_tex_vir_addr = (uint16_t *)(unsigned long)g_tex_phy_addr;
    g_rot_vir_addr = (uint16_t *)(unsigned long)g_rot_phy_addr;

    // 2. 初始化正弦表 (Q12)
    for (int i = 0; i < LUT_SIZE; i++)
    {
        sin_lut[i] = (int)(sinf(i * PI / (LUT_SIZE / 2.0f)) * Q12_ONE);
    }

    // 3. 初始化低饱和度调色板 (为加法混合预留亮度累加空间)
    for (int i = 0; i < PALETTE_SIZE; i++)
    {
        int r = (int)(30 + 25 * sinf(i * 0.05f));
        int g = (int)(50 + 45 * sinf(i * 0.02f + 1.0f));
        int b = (int)(100 + 80 * sinf(i * 0.01f + 3.0f));

        // 增加高频细节，在重叠时产生火花感
        if ((i % 32) > 30)
        {
            r = 120;
            g = 120;
            b = 180;
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
    if (!g_tex_vir_addr || !g_rot_phy_addr)
        return;

    /* --- STEP 1: 清理屏幕 (主画布) --- */
    struct ge_fillrect screen_fill  = {0};
    screen_fill.type                = GE_NO_GRADIENT;
    screen_fill.start_color         = 0xFF000000;
    screen_fill.dst_buf.buf_type    = MPP_PHY_ADDR;
    screen_fill.dst_buf.phy_addr[0] = phy_addr;
    screen_fill.dst_buf.stride[0]   = ctx->info.stride;
    screen_fill.dst_buf.size.width  = ctx->info.width;
    screen_fill.dst_buf.size.height = ctx->info.height;
    screen_fill.dst_buf.format      = ctx->info.format;
    mpp_ge_fillrect(ctx->ge, &screen_fill);

    /* --- STEP 2: CPU 纹理计算 --- */
    uint16_t *p  = g_tex_vir_addr;
    int       t  = g_tick;
    int       cx = TEX_WIDTH / 2;
    int       cy = TEX_HEIGHT / 2;

    for (int y = 0; y < TEX_HEIGHT; y++)
    {
        int dy  = y - cy;
        int dy2 = dy * dy;
        for (int x = 0; x < TEX_WIDTH; x++)
        {
            int dx = x - cx;
            // 经典的距离场异或，产生深邃的星芒
            int dist = (dx * dx + dy2) >> 8;
            int val  = (dist ^ (x >> 3) ^ (y >> 3)) + t;
            *p++     = g_palette[val & 0xFF];
        }
    }
    aicos_dcache_clean_range((void *)g_tex_vir_addr, TEX_SIZE);

    /* --- STEP 3: 硬件分层渲染 (2层干涉) --- */
    for (int i = 0; i < LAYER_COUNT; i++)
    {
        // A. 关键修正：清理中间旋转缓冲区，抹除上一帧死角残留
        struct ge_fillrect rot_fill  = {0};
        rot_fill.type                = GE_NO_GRADIENT;
        rot_fill.start_color         = 0xFF000000;
        rot_fill.dst_buf.buf_type    = MPP_PHY_ADDR;
        rot_fill.dst_buf.phy_addr[0] = g_rot_phy_addr;
        rot_fill.dst_buf.stride[0]   = TEX_WIDTH * TEX_BPP;
        rot_fill.dst_buf.size.width  = TEX_WIDTH;
        rot_fill.dst_buf.size.height = TEX_HEIGHT;
        rot_fill.dst_buf.format      = TEX_FMT;
        mpp_ge_fillrect(ctx->ge, &rot_fill);
        mpp_ge_emit(ctx->ge);
        mpp_ge_sync(ctx->ge); // 必须确保中间层清理干净

        // B. 旋转逻辑
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

        // 计算角速度与相位: 不同层速度和相位不同
        int theta     = (g_tick * (i + 1) + (i * ROT_PHASE_STEP)) & LUT_MASK;
        rot.angle_sin = GET_SIN(theta);
        rot.angle_cos = GET_COS(theta);

        // 旋转中心
        rot.src_rot_center.x = cx;
        rot.src_rot_center.y = cy;
        rot.dst_rot_center.x = cx;
        rot.dst_rot_center.y = cy;
        rot.ctrl.alpha_en    = 1; // 旋转过程禁用混合，仅搬运

        mpp_ge_rotate(ctx->ge, &rot);
        mpp_ge_emit(ctx->ge);
        mpp_ge_sync(ctx->ge); // 等待旋转完成

        // C. 全屏呼吸缩放 (Over-Scaling 优化)
        struct ge_bitblt blt    = {0};
        blt.src_buf.buf_type    = MPP_PHY_ADDR;
        blt.src_buf.phy_addr[0] = g_rot_phy_addr;
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

        // 设置全屏缩放
        blt.dst_buf.crop_en     = 1;
        blt.dst_buf.crop.x      = 0;
        blt.dst_buf.crop.y      = 0;
        blt.dst_buf.crop.width  = ctx->info.width;
        blt.dst_buf.crop.height = ctx->info.height;

        /*
         * 核心视觉增强：缩小 crop_w 实现 Over-Scaling。
         * 让纹理在缩放后比屏幕更大，彻底切掉旋转留下的虚空角落。
         */
        int crop_w = CROP_BASE_W + (GET_SIN(g_tick << 1) >> 8);
        int crop_h = (crop_w * TEX_HEIGHT) / TEX_WIDTH;

        blt.src_buf.crop_en     = 1;
        blt.src_buf.crop.x      = (TEX_WIDTH - crop_w) / 2;
        blt.src_buf.crop.y      = (TEX_HEIGHT - crop_h) / 2;
        blt.src_buf.crop.width  = crop_w;
        blt.src_buf.crop.height = crop_h;

        if (i == 0)
        {
            blt.ctrl.alpha_en = 1; // 第一层直接拉伸填充，覆盖屏幕背景
        }
        else
        {
            blt.ctrl.alpha_en         = 0;         // 极性：0 开启混合
            blt.ctrl.alpha_rules      = GE_PD_ADD; // 规则 11: 能量累加
            blt.ctrl.src_alpha_mode   = 1;         // 全局 Alpha 控制强度
            blt.ctrl.src_global_alpha = BLEND_ALPHA;
        }

        mpp_ge_bitblt(ctx->ge, &blt);

        /* 遵循 SPEC.md：大面积绘图，画一层，同步一层 */
        mpp_ge_emit(ctx->ge);
        mpp_ge_sync(ctx->ge);
    }

    g_tick++;
}

static void effect_deinit(struct demo_ctx *ctx)
{
    if (g_tex_phy_addr)
        mpp_phy_free(g_tex_phy_addr);
    if (g_rot_phy_addr)
        mpp_phy_free(g_rot_phy_addr);
}

struct effect_ops effect_0022 = {
    .name   = "NO.22 DIMENSIONAL FOLDING",
    .init   = effect_init,
    .draw   = effect_draw,
    .deinit = effect_deinit,
};

REGISTER_EFFECT(effect_0022);
