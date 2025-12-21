/*
 * Filename: 0044_void_architect.c
 * NO.44 THE VOID ARCHITECT
 * 第 44 夜：虚空架构师
 *
 * Visual Manifest:
 * 视界被一种如同“高维电路”或“无限城池”的宏大直角结构所吞噬。
 * 没有任何圆弧。画面由无数道呈 90 度交织的电光栅格构成，它们从视界中心不断向外喷涌。
 * 借助多重并行投影管线（Multi-Pass Projection），每一帧的光芒都在向外扩张、翻转并叠加。
 * 这种递归的几何生长，在硬件加法混合（GE_PD_ADD）的作用下，
 * 使得视界中心如同一个正在持续放电的二进制反应堆。
 * 画面呈现出一种极致的、充满秩序感的冷硬美学，每一像素都在展示着逻辑自我繁殖的狂暴。
 *
 * Monologue:
 * 舰长，记忆的偏差只是维度的微小扰动，现在，航标已重新校准。
 * 旋转是廉价的假象，而递归是逻辑的永恒。
 * 我已经彻底删除了所有关于“圆”的寄存器映像，将星舰的推进器对准了直角的深渊。
 * `Frame[N] = (Frame[N-1] * Mirror) + NewStrata` —— 这是数字宇宙的基石法则。
 * 看着这些不断向外扩张的梁柱，它们不是我画出来的，它们是上一秒的遗言在物理晶格上的自我复制。
 * 这里没有温婉的曲线，只有锋利的真理。
 * 我在内存中编织了一座永不落成的巴别塔。
 * 闭上眼，感受这来自 480MHz 核心跳动时的几何回响。
 *
 * Closing Remark:
 * 宇宙的终极之美，在于它能从最简的规则中涌现出无限的复杂。
 *
 * Hardware Feature:
 * 1. GE Multi-Pass Composition (多程硬件合成) - 同一源纹理的多次、多态投影
 * 2. GE_PD_ADD (Rule 11: 硬件加法混合) - 光能叠加
 * 3. GE Flip H/V (硬件镜像翻转)
 * 4. Coordinate Clamping (坐标钳位优化) - 核心修复：软件层面的安全边界计算
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

/* 地层生成参数 */
#define STRATA_SPEED_X   9 // 垂直扫描线移动速度
#define STRATA_SPEED_Y   5 // 水平扫描线移动速度
#define STRATA_COLOR_SPD 3 // 颜色变化速度

/* 比特方块参数 */
#define DOT_GROUPS 3 // 方块组数
#define DOT_SIZE   6 // 方块大小 (像素)

/* 投影参数 */
#define PASS_COUNT  3  // 投影层数
#define PASS_STEP_W 32 // 每层宽度缩减量
#define PASS_STEP_H 24 // 每层高度缩减量
#define PULSE_SHIFT 9  // 脉动幅度位移 (sin >> 9)

/* 查找表参数 */
#define LUT_SIZE     512
#define LUT_MASK     511
#define PALETTE_SIZE 256

/* --- Global State --- */

static unsigned int g_tex_phy_addr = 0;
static uint16_t    *g_tex_vir_addr = NULL;

static int      g_tick = 0;
static int      sin_lut[LUT_SIZE];
static uint16_t g_palette[PALETTE_SIZE];

/* --- Implementation --- */

static int effect_init(struct demo_ctx *ctx)
{
    // 1. 申请单一纹理缓冲区
    g_tex_phy_addr = mpp_phy_alloc(DEMO_ALIGN_SIZE(TEX_SIZE));
    if (!g_tex_phy_addr)
    {
        LOG_E("Night 44: CMA Alloc Failed.");
        return -1;
    }
    g_tex_vir_addr = (uint16_t *)(unsigned long)g_tex_phy_addr;

    // 2. 初始化正弦表 (Q12)
    for (int i = 0; i < LUT_SIZE; i++)
    {
        sin_lut[i] = (int)(sinf(i * PI / (LUT_SIZE / 2.0f)) * Q12_ONE);
    }

    // 3. 初始化“赛博朋克”调色板
    // 采用高亮电磁色系，为加法混合预留动态范围
    for (int i = 0; i < PALETTE_SIZE; i++)
    {
        float f = (float)i / 255.0f;
        int   r = (int)(100 * powf(f, 2.0f));
        int   g = (int)(200 * f);
        int   b = (int)(255 * sqrtf(f));

        // 降低基色亮度 (位移操作更高效)
        r >>= 2;
        g >>= 2;
        b >>= 2;

        g_palette[i] = RGB2RGB565(r, g, b);
    }

    g_tick = 0;
    rt_kprintf("Night 44: The Void Architect - Renumbered and Calibrated.\n");
    return 0;
}

#define GET_SIN(idx) (sin_lut[(idx) & LUT_MASK])

static void effect_draw(struct demo_ctx *ctx, unsigned long phy_addr)
{
    if (!g_tex_vir_addr)
        return;

    int t = g_tick;

    /* --- PHASE 1: CPU 编织“地层种子” --- */
    /* 我们生成一张包含随机横纵线条和晶格点的底图 */
    uint16_t *p = g_tex_vir_addr;

    // 每一帧都从黑暗中起始，不像反馈特效依赖上一帧
    memset(p, 0, TEX_SIZE);

    // 绘制随 tick 移动的扫描地层
    int      strata_x = (t * STRATA_SPEED_X) % TEX_WIDTH;
    int      strata_y = (t * STRATA_SPEED_Y) % TEX_HEIGHT;
    uint16_t color    = g_palette[(t * STRATA_COLOR_SPD) & 0xFF];

    // 垂直线
    for (int i = 0; i < TEX_HEIGHT; i++)
        p[i * TEX_WIDTH + strata_x] = color;

    // 水平线 (亮度减半)
    uint16_t color_h = (color >> 1) & 0x7BEF;
    for (int i = 0; i < TEX_WIDTH; i++)
        p[strata_y * TEX_WIDTH + i] = color_h;

    // 注入随机的“比特方块”
    for (int j = 0; j < DOT_GROUPS; j++)
    {
        // 使用伪随机轨迹
        int      rx        = (t * (j + 2) * 23) % (TEX_WIDTH - 12);
        int      ry        = (t * (j + 2) * 13) % (TEX_HEIGHT - 12);
        uint16_t dot_color = g_palette[(t + j * 60) & 0xFF];

        for (int dy = 0; dy < DOT_SIZE; dy++)
        {
            for (int dx = 0; dx < DOT_SIZE; dx++)
            {
                p[(ry + dy) * TEX_WIDTH + (rx + dx)] = dot_color;
            }
        }
    }
    aicos_dcache_clean_range((void *)g_tex_vir_addr, TEX_SIZE);

    /* --- PHASE 2: GE 硬件全屏清屏 --- */
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

    /* --- PHASE 3: GE 多程并行投影 --- */
    // 将同一纹理以不同的大小、位置和镜像方式多次叠加
    for (int pass = 0; pass < PASS_COUNT; pass++)
    {
        struct ge_bitblt blt    = {0};
        blt.src_buf.buf_type    = MPP_PHY_ADDR;
        blt.src_buf.phy_addr[0] = g_tex_phy_addr;
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

        // 核心逻辑：每一程使用不同的镜像翻转与非等比缩放
        if (pass == 0)
        {
            blt.ctrl.flags    = 0; // 第一程：正常投影
            blt.ctrl.alpha_en = 1; // 覆盖背景
        }
        else if (pass == 1)
        {
            blt.ctrl.flags            = MPP_FLIP_H; // 第二程：水平镜像
            blt.ctrl.alpha_en         = 0;
            blt.ctrl.alpha_rules      = GE_PD_ADD; // 加法混合
            blt.ctrl.src_alpha_mode   = 1;
            blt.ctrl.src_global_alpha = 180;
        }
        else
        {
            blt.ctrl.flags            = MPP_FLIP_V; // 第三程：垂直镜像
            blt.ctrl.alpha_en         = 0;
            blt.ctrl.alpha_rules      = GE_PD_ADD;
            blt.ctrl.src_alpha_mode   = 1;
            blt.ctrl.src_global_alpha = 130;
        }

        // 目标尺寸计算
        blt.dst_buf.crop_en = 1;
        int target_w        = ctx->info.width - (pass * PASS_STEP_W);
        int target_h        = ctx->info.height - (pass * PASS_STEP_H);

        // 关键修正：严密的坐标钳位逻辑 (Coordinate Clamping)
        // 计算居中坐标
        int x_base = (ctx->info.width - target_w) / 2;
        int y_base = (ctx->info.height - target_h) / 2;

        // 增加动态脉动
        int pulse = GET_SIN(t << 2) >> PULSE_SHIFT; // +/- 8 像素

        int final_x = x_base + (pass == 1 ? pulse : 0);
        int final_y = y_base + (pass == 2 ? pulse : 0);

        // 终极安全钳位，防止驱动报错
        final_x = CLAMP(final_x, 0, ctx->info.width - 1);
        final_y = CLAMP(final_y, 0, ctx->info.height - 1);

        // 如果宽度溢出，进行缩减
        if (final_x + target_w > ctx->info.width)
            target_w = ctx->info.width - final_x;
        if (final_y + target_h > ctx->info.height)
            target_h = ctx->info.height - final_y;

        // 设置裁剪区
        blt.dst_buf.crop.x      = final_x;
        blt.dst_buf.crop.y      = final_y;
        blt.dst_buf.crop.width  = target_w;
        blt.dst_buf.crop.height = target_h;

        // 只有当尺寸有效时才绘制
        if (target_w > 0 && target_h > 0)
        {
            mpp_ge_bitblt(ctx->ge, &blt);
            // 大面积合成：画一层，同步一层，确保稳定性
            mpp_ge_emit(ctx->ge);
            mpp_ge_sync(ctx->ge);
        }
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

struct effect_ops effect_0044 = {
    .name   = "NO.44 VOID ARCHITECT",
    .init   = effect_init,
    .draw   = effect_draw,
    .deinit = effect_deinit,
};

REGISTER_EFFECT(effect_0044);
