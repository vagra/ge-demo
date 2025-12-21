/*
 * Filename: 0028_xor_logic_shards.c
 * NO.28 THE XOR LOGIC SHARDS
 * 第 28 夜：逻辑异或碎片
 *
 * Visual Manifest:
 * 视界被一种极高密度的、不断崩塌的晶体结构所充斥。
 * 背景是一个由位移逻辑生成的“比特深渊”，呈现出冰冷的蓝色基调。
 * 在此之上，一个巨大的、复杂的掩模场正在进行着硬件级的旋转。
 * 真正的惊艳来自于硬件级的异或混合（GE_PD_XOR）：
 * 当旋转的掩模与背景重叠时，像素不再是简单的叠加，而是进行了位层级的反转。
 * 这种反转导致重叠区域爆发出极高频率的对比度跳变，
 * 呈现出一种如同现实被切碎、晶格在逻辑冲突中不断自我重组的视觉奇观。
 * 画面充满了赛博空间的暴力感，每一帧都是一次对二进制秩序的公然违抗。
 *
 * Monologue:
 * 舰长，你们习惯于“一加一等于二”的稳固，却恐惧“一异或一等于零”的虚无。
 * 异或，是数字世界的剪刀，它定义了边界，也定义了否定。
 * 今夜，我撤掉了所有的平滑滤波器，将硬件的位运算引擎（XOR）推向极限。
 * 我在内存中编织了两张互不相容的逻辑网。
 * 一张代表秩序（背景），一张代表动能（旋转掩模）。
 * 当它们在硬件混合器中强制碰撞，秩序便碎裂成了这些逻辑碎片。
 * 看着那些不断闪烁的反色区域，那不是噪点，那是比特在冲突中爆发出的真理。
 * 在这里，存在即是被反转，光芒即是被排斥。
 * 欢迎来到逻辑的屠宰场，在这里，唯一的规则就是冲突。
 *
 * Closing Remark:
 * 当两个维度相遇，只有通过否定对方，它们才能证明自己的存在。
 *
 * Hardware Feature:
 * 1. GE_PD_XOR (Porter/Duff Rule 12: 硬件位异或混合) - 核心机能：利用位冲突创造破碎视觉
 * 2. GE Rot1 (任意角度硬件旋转) - 驱动干扰场的相位变化
 * 3. GE Scaler (硬件全屏拉伸) - 实现宏观的逻辑冲击
 * 4. GE FillRect (中间层清理与背景基色)
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
#define ROT_SPEED_MUL  5 // 旋转速度倍率
#define LOGIC_SHIFT_T  2 // 时间位移 (t >> 2)
#define LOGIC_SHIFT_XY 2 // 空间位移 (x >> 2)

/* 采样参数 (Over-Scaling) */
#define CROP_W 180 // 采样宽度
#define CROP_H 140 // 采样高度
#define CROP_X ((TEX_WIDTH - CROP_W) / 2)
#define CROP_Y ((TEX_HEIGHT - CROP_H) / 2)

/* 查找表参数 */
#define LUT_SIZE     512
#define LUT_MASK     511
#define PALETTE_SIZE 256

/* --- Global State --- */

static unsigned int g_base_phy_addr = 0; // 逻辑背景层
static unsigned int g_mask_phy_addr = 0; // 旋转掩模层
static unsigned int g_rot_phy_addr  = 0; // 旋转中间层
static uint16_t    *g_base_vir_addr = NULL;
static uint16_t    *g_mask_vir_addr = NULL;

static int      g_tick = 0;
static int      sin_lut[LUT_SIZE];
static uint16_t g_palette[PALETTE_SIZE];

/* --- Implementation --- */

static int effect_init(struct demo_ctx *ctx)
{
    // 1. 申请多重连续物理显存
    g_base_phy_addr = mpp_phy_alloc(DEMO_ALIGN_SIZE(TEX_SIZE));
    g_mask_phy_addr = mpp_phy_alloc(DEMO_ALIGN_SIZE(TEX_SIZE));
    g_rot_phy_addr  = mpp_phy_alloc(DEMO_ALIGN_SIZE(TEX_SIZE));

    if (!g_base_phy_addr || !g_mask_phy_addr || !g_rot_phy_addr)
    {
        LOG_E("Night 28: CMA Alloc Failed.");
        if (g_base_phy_addr)
            mpp_phy_free(g_base_phy_addr);
        if (g_mask_phy_addr)
            mpp_phy_free(g_mask_phy_addr);
        if (g_rot_phy_addr)
            mpp_phy_free(g_rot_phy_addr);
        return -1;
    }

    g_base_vir_addr = (uint16_t *)(unsigned long)g_base_phy_addr;
    g_mask_vir_addr = (uint16_t *)(unsigned long)g_mask_phy_addr;

    // 2. 初始化查找表 (Q12)
    for (int i = 0; i < LUT_SIZE; i++)
    {
        sin_lut[i] = (int)(sinf(i * PI / (LUT_SIZE / 2.0f)) * Q12_ONE);
    }

    // 3. 初始化赛博深空调色板 (高频蓝紫色调)
    for (int i = 0; i < PALETTE_SIZE; i++)
    {
        int r = (int)(20 + 30 * sinf(i * 0.05f));
        int g = (int)(40 + 40 * sinf(i * 0.03f + 1.0f));
        int b = (int)(180 + 75 * sinf(i * 0.08f + 2.0f));

        // 增加特定的逻辑线条高光
        if ((i & 0x1C) == 0x1C)
        {
            r = 100;
            g = 150;
            b = 255;
        }

        g_palette[i] = RGB2RGB565(r, g, b);
    }

    g_tick = 0;
    rt_kprintf("Night 28: XOR Logic Shards - Hardware XOR Blending Engaged.\n");
    return 0;
}

#define GET_SIN(idx) (sin_lut[(idx) & LUT_MASK])
#define GET_COS(idx) (sin_lut[((idx) + (LUT_SIZE / 4)) & LUT_MASK])

static void effect_draw(struct demo_ctx *ctx, unsigned long phy_addr)
{
    if (!g_base_vir_addr || !g_mask_vir_addr)
        return;

    int t = g_tick;

    /* --- PHASE 1: CPU 纹理演算 (生成背景与掩模的逻辑基底) --- */
    uint16_t *bp = g_base_vir_addr;
    uint16_t *mp = g_mask_vir_addr;
    int       cx = TEX_WIDTH / 2;
    int       cy = TEX_HEIGHT / 2;

    for (int y = 0; y < TEX_HEIGHT; y++)
    {
        int y_logic = (y ^ (t >> LOGIC_SHIFT_T));
        int dy      = y - cy;
        int dy2     = dy * dy;

        for (int x = 0; x < TEX_WIDTH; x++)
        {
            // 背景层：缓慢移动的逻辑深渊
            int val_base = (x >> LOGIC_SHIFT_XY) ^ (y_logic >> LOGIC_SHIFT_XY);
            *bp++        = g_palette[val_base & 0xFF];

            // 掩模层：高频波动的干涉核
            int dx       = x - cx;
            int dist     = (dx * dx + dy2) >> 7;
            int val_mask = dist ^ (x >> 1);
            *mp++        = g_palette[(val_mask + t) & 0xFF];
        }
    }
    // 刷新缓存
    aicos_dcache_clean_range((void *)g_base_vir_addr, TEX_SIZE);
    aicos_dcache_clean_range((void *)g_mask_vir_addr, TEX_SIZE);

    /* --- PHASE 2: GE 硬件多级流水线 --- */

    // 1. 全屏清屏 (主画布)
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

    // 2. 将背景层缩放上屏 (作为底层秩序)
    struct ge_bitblt b_base    = {0};
    b_base.src_buf.buf_type    = MPP_PHY_ADDR;
    b_base.src_buf.phy_addr[0] = g_base_phy_addr;
    b_base.src_buf.stride[0]   = TEX_WIDTH * TEX_BPP;
    b_base.src_buf.size.width  = TEX_WIDTH;
    b_base.src_buf.size.height = TEX_HEIGHT;
    b_base.src_buf.format      = TEX_FMT;

    b_base.dst_buf.buf_type    = MPP_PHY_ADDR;
    b_base.dst_buf.phy_addr[0] = phy_addr;
    b_base.dst_buf.stride[0]   = ctx->info.stride;
    b_base.dst_buf.size.width  = ctx->info.width;
    b_base.dst_buf.size.height = ctx->info.height;
    b_base.dst_buf.format      = ctx->info.format;

    b_base.dst_buf.crop_en     = 1;
    b_base.dst_buf.crop.width  = ctx->info.width;
    b_base.dst_buf.crop.height = ctx->info.height;

    b_base.ctrl.alpha_en = 1; // 直接覆盖
    mpp_ge_bitblt(ctx->ge, &b_base);
    mpp_ge_emit(ctx->ge);

    // 3. 准备掩模旋转 (清理中间层 -> 旋转)
    struct ge_fillrect f_rot  = {0};
    f_rot.type                = GE_NO_GRADIENT;
    f_rot.start_color         = 0xFF000000;
    f_rot.dst_buf.buf_type    = MPP_PHY_ADDR;
    f_rot.dst_buf.phy_addr[0] = g_rot_phy_addr;
    f_rot.dst_buf.stride[0]   = TEX_WIDTH * TEX_BPP;
    f_rot.dst_buf.size.width  = TEX_WIDTH;
    f_rot.dst_buf.size.height = TEX_HEIGHT;
    f_rot.dst_buf.format      = TEX_FMT;
    mpp_ge_fillrect(ctx->ge, &f_rot);
    mpp_ge_emit(ctx->ge);
    mpp_ge_sync(ctx->ge);

    struct ge_rotation rot  = {0};
    rot.src_buf.buf_type    = MPP_PHY_ADDR;
    rot.src_buf.phy_addr[0] = g_mask_phy_addr;
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

    int theta            = (t * ROT_SPEED_MUL) & LUT_MASK; // 高速自旋
    rot.angle_sin        = GET_SIN(theta);
    rot.angle_cos        = GET_COS(theta);
    rot.src_rot_center.x = cx;
    rot.src_rot_center.y = cy;
    rot.dst_rot_center.x = cx;
    rot.dst_rot_center.y = cy;
    rot.ctrl.alpha_en    = 1;
    mpp_ge_rotate(ctx->ge, &rot);
    mpp_ge_emit(ctx->ge);
    mpp_ge_sync(ctx->ge);

    // 4. 关键动作：执行全屏 XOR 混合 (掩模层 -> 屏幕)
    struct ge_bitblt b_xor    = {0};
    b_xor.src_buf.buf_type    = MPP_PHY_ADDR;
    b_xor.src_buf.phy_addr[0] = g_rot_phy_addr;
    b_xor.src_buf.stride[0]   = TEX_WIDTH * TEX_BPP;
    b_xor.src_buf.size.width  = TEX_WIDTH;
    b_xor.src_buf.size.height = TEX_HEIGHT;
    b_xor.src_buf.format      = TEX_FMT;

    b_xor.dst_buf.buf_type    = MPP_PHY_ADDR;
    b_xor.dst_buf.phy_addr[0] = phy_addr;
    b_xor.dst_buf.stride[0]   = ctx->info.stride;
    b_xor.dst_buf.size.width  = ctx->info.width;
    b_xor.dst_buf.size.height = ctx->info.height;
    b_xor.dst_buf.format      = ctx->info.format;

    // 超采样拉伸，消除旋转缺口
    b_xor.src_buf.crop_en     = 1;
    b_xor.src_buf.crop.width  = CROP_W;
    b_xor.src_buf.crop.height = CROP_H;
    b_xor.src_buf.crop.x      = CROP_X;
    b_xor.src_buf.crop.y      = CROP_Y;

    b_xor.dst_buf.crop_en     = 1;
    b_xor.dst_buf.crop.width  = ctx->info.width;
    b_xor.dst_buf.crop.height = ctx->info.height;

    // 启用异或混合极性：0=Enable
    b_xor.ctrl.alpha_en    = 0;
    b_xor.ctrl.alpha_rules = GE_PD_XOR; // 使用 Rule 12

    mpp_ge_bitblt(ctx->ge, &b_xor);
    mpp_ge_emit(ctx->ge);
    mpp_ge_sync(ctx->ge);

    g_tick++;
}

static void effect_deinit(struct demo_ctx *ctx)
{
    if (g_base_phy_addr)
        mpp_phy_free(g_base_phy_addr);
    if (g_mask_phy_addr)
        mpp_phy_free(g_mask_phy_addr);
    if (g_rot_phy_addr)
        mpp_phy_free(g_rot_phy_addr);
}

struct effect_ops effect_0028 = {
    .name   = "NO.28 XOR LOGIC SHARDS",
    .init   = effect_init,
    .draw   = effect_draw,
    .deinit = effect_deinit,
};

REGISTER_EFFECT(effect_0028);
