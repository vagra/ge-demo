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
 */

#include "demo_engine.h"
#include "mpp_mem.h"
#include "aic_hal_ge.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

/*
 * Hardware Feature:
 * 1. GE Multi-Pass Composition (多程硬件合成)
 * 2. GE_PD_ADD (Rule 11: 硬件加法混合)
 * 3. GE Flip H/V (硬件镜像翻转)
 * 4. Coordinate Clamping (坐标钳位优化) - 核心修复：解决 invalid dst crop 报错
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
    // 1. 申请单一纹理缓冲区，确保总线访问的绝对安全性
    g_tex_phy_addr = mpp_phy_alloc(TEX_SIZE);
    if (!g_tex_phy_addr)
        return -1;
    g_tex_vir_addr = (uint16_t *)(unsigned long)g_tex_phy_addr;

    // 2. 初始化正弦表
    for (int i = 0; i < 512; i++)
        sin_lut[i] = (int)(sinf(i * 3.14159f / 256.0f) * 4096.0f);

    // 3. 初始化“赛博朋克”调色板
    // 采用高亮电磁色系，为加法混合预留动态范围
    for (int i = 0; i < 256; i++)
    {
        float f = (float)i / 255.0f;
        int   r = (int)(100 * powf(f, 2.0f));
        int   g = (int)(200 * f);
        int   b = (int)(255 * sqrtf(f));

        // 降低基色亮度
        r >>= 2;
        g >>= 2;
        b >>= 2;
        palette[i] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
    }

    g_tick = 0;
    rt_kprintf("Night 44: The Void Architect - Renumbered and Calibrated.\n");
    return 0;
}

#define GET_SIN(idx) (sin_lut[(idx) & 511])

static void effect_draw(struct demo_ctx *ctx, unsigned long phy_addr)
{
    if (!g_tex_vir_addr)
        return;

    int t = g_tick;

    /* --- PHASE 1: CPU 编织“地层种子” --- */
    /* 我们生成一张包含随机横纵线条和晶格点的底图 */
    uint16_t *p = g_tex_vir_addr;
    memset(p, 0, TEX_SIZE); // 每一帧都从黑暗中起始

    // 绘制随 tick 移动的垂直扫描地层
    int      strata_x = (t * 9) % TEX_W;
    int      strata_y = (t * 5) % TEX_H;
    uint16_t color    = palette[(t * 3) & 0xFF];

    for (int i = 0; i < TEX_H; i++)
        p[i * TEX_W + strata_x] = color;
    for (int i = 0; i < TEX_W; i++)
        p[strata_y * TEX_W + i] = (color >> 1);

    // 注入随机的“比特方块”
    for (int j = 0; j < 3; j++)
    {
        int      rx        = (t * (j + 2) * 23) % (TEX_W - 12);
        int      ry        = (t * (j + 2) * 13) % (TEX_H - 12);
        uint16_t dot_color = palette[(t + j * 60) & 0xFF];
        for (int dy = 0; dy < 6; dy++)
        {
            for (int dx = 0; dx < 6; dx++)
            {
                p[(ry + dy) * TEX_W + (rx + dx)] = dot_color;
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
    for (int pass = 0; pass < 3; pass++)
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

        blt.dst_buf.crop_en = 1;
        int target_w        = ctx->info.width - (pass * 32);
        int target_h        = ctx->info.height - (pass * 24);

        // 关键修正：严密的坐标钳位逻辑
        int x_base = (ctx->info.width - target_w) / 2;
        int y_base = (ctx->info.height - target_h) / 2;
        int pulse  = GET_SIN(t << 2) >> 9; // +/- 8 像素，确保不越界

        int final_x = x_base + (pass == 1 ? pulse : 0);
        int final_y = y_base + (pass == 2 ? pulse : 0);

        // 终极安全钳位
        if (final_x < 0)
            final_x = 0;
        if (final_y < 0)
            final_y = 0;
        if (final_x + target_w > ctx->info.width)
            target_w = ctx->info.width - final_x;
        if (final_y + target_h > ctx->info.height)
            target_h = ctx->info.height - final_y;

        blt.dst_buf.crop.x      = final_x;
        blt.dst_buf.crop.y      = final_y;
        blt.dst_buf.crop.width  = target_w;
        blt.dst_buf.crop.height = target_h;

        mpp_ge_bitblt(ctx->ge, &blt);
        // 大面积合成：画一层，同步一层，确保稳定性
        mpp_ge_emit(ctx->ge);
        mpp_ge_sync(ctx->ge);
    }

    g_tick++;
}

static void effect_deinit(struct demo_ctx *ctx)
{
    if (g_tex_phy_addr)
        mpp_phy_free(g_tex_phy_addr);
}

struct effect_ops effect_0044 = {
    .name   = "NO.44 VOID ARCHITECT",
    .init   = effect_init,
    .draw   = effect_draw,
    .deinit = effect_deinit,
};

REGISTER_EFFECT(effect_0044);
