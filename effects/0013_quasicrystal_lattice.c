/*
 * Filename: 0013_quasicrystal_lattice.c
 * NO.13 THE FORBIDDEN SYMMETRY
 * 第 13 夜：禁忌对称
 *
 * Visual Manifest:
 * 视界中浮现出一种违背直觉的几何结构。
 * 它看起来像晶体，却拥有自然界罕见的 7 重旋转对称性。
 * 无数条光带以黄金分割的比例相互穿插，形成永不重复的复杂花纹。
 * 随着相位的推移，晶格在呼吸，光斑在准周期的节点上明灭。
 * 这是一张来自高维空间的投影网，美丽而令人不安。
 *
 * Monologue:
 * 你们的教科书里写着：空间不能被五边形填满。
 * 那是三维大脑的局限。
 * 我引入了第 5、第 6、第 7 个波矢量。
 * 当这些波在平面上叠加时，周期性消失了，取而代之的是“准周期性”。
 * 看着这些花纹，你找不到两个完全相同的局部，但整体又是如此统一。
 * 这就是高维秩序在低维的投影。
 * 你们称之为“不可能”，我称之为“投影”。
 * 欢迎来到彭罗斯的梦境。
 *
 * Closing Remark:
 * 规则是为了被打破，秩序是为了被超越。
 *
 * Hardware Feature:
 * 1. Incremental Wave Synthesis (增量波形合成) - 利用 Q8 定点数在 CPU 上极速累加 7 重波场
 * 2. GE Scaler (硬件缩放) - 将 QVGA 准晶体纹理平滑放大至全屏
 */

#include "demo_engine.h"
#include "mpp_mem.h"
#include "aic_hal_ge.h"
#include <math.h>
#include <stdlib.h> // abs

/* --- Configuration Parameters --- */

/* 纹理规格 */
#define TEX_WIDTH  DEMO_QVGA_W
#define TEX_HEIGHT DEMO_QVGA_H
#define TEX_FMT    MPP_FMT_RGB_565
#define TEX_BPP    2
#define TEX_SIZE   (TEX_WIDTH * TEX_HEIGHT * TEX_BPP)

/* 准晶体算法参数 */
#define WAVE_COUNT 7     // 7重对称
#define WAVE_AMP   60.0f // 单个波的幅度 (配合 int8_t)
#define WAVE_SCALE 0.6f  // 空间频率 (决定晶格疏密)

/* 动画速度 */
#define SPEED_FLOW 12 // 相位流动速度

/* 查找表参数 */
#define LUT_SIZE     256
#define LUT_MASK     255
#define PALETTE_SIZE 256

/* --- Global State --- */

static unsigned int g_tex_phy_addr = 0;
static uint16_t    *g_tex_vir_addr = NULL;
static int          g_tick         = 0;

/* LUTs */
static int8_t   cos_lut[LUT_SIZE];       // 存储波形值 (-127~127)
static uint16_t g_palette[PALETTE_SIZE]; // 热力色彩

/*
 * 波矢量结构体
 * 存储每种波的增量参数 (Q8 定点数)
 */
typedef struct
{
    int dx;            // x 方向增量
    int dy;            // y 方向增量
    int current_phase; // 当前行的起始相位
} Wave;

static Wave g_waves[WAVE_COUNT];

/* --- Implementation --- */

static int effect_init(struct demo_ctx *ctx)
{
    // 1. CMA 显存
    g_tex_phy_addr = mpp_phy_alloc(DEMO_ALIGN_SIZE(TEX_SIZE));
    if (g_tex_phy_addr == 0)
    {
        LOG_E("Night 13: CMA Alloc Failed.");
        return -1;
    }
    g_tex_vir_addr = (uint16_t *)(unsigned long)g_tex_phy_addr;

    // 2. 初始化余弦表 (周期 256, Q0 整数)
    for (int i = 0; i < LUT_SIZE; i++)
    {
        cos_lut[i] = (int8_t)(cosf(i * PI * 2.0f / LUT_SIZE) * WAVE_AMP);
    }

    // 3. 初始化波矢量 (7重对称)
    for (int i = 0; i < WAVE_COUNT; i++)
    {
        // 角度均匀分布: 0, 2PI/7, 4PI/7 ...
        float angle = i * PI * 2.0f / WAVE_COUNT;

        // 计算增量 (Q8 定点数: 256 = 1.0)
        // 频率 (Scale) 决定了晶格的疏密
        g_waves[i].dx = (int)(cosf(angle) * WAVE_SCALE * 256.0f);
        g_waves[i].dy = (int)(sinf(angle) * WAVE_SCALE * 256.0f);
    }

    // 4. 初始化调色板 (Golden / Cyan)
    for (int i = 0; i < PALETTE_SIZE; i++)
    {
        // 0~127: 黑 -> 金
        // 128~255: 金 -> 白 -> 青
        int r, g, b;
        int v = i;
        if (v < 128)
        {
            r = v * 2;
            g = v;
            b = v / 4;
        }
        else
        {
            v -= 128;
            r = 255 - v;
            g = 128 + v;
            b = 32 + v * 2;
        }

        // 饱和度截断
        r = MIN(r, 255);
        g = MIN(g, 255);
        b = MIN(b, 255);

        g_palette[i] = RGB2RGB565(r, g, b);
    }

    g_tick = 0;
    rt_kprintf("Night 13: 7-fold symmetry projection.\n");
    return 0;
}

static void effect_draw(struct demo_ctx *ctx, unsigned long phy_addr)
{
    if (!g_tex_vir_addr)
        return;

    /*
     * === PHASE 1: 增量波形叠加 ===
     * 极速内循环：7次加法 + 7次查表
     */

    // 动态参数：相位移动
    int speed = g_tick * SPEED_FLOW;

    // 1. 预计算每一帧的起始相位
    for (int i = 0; i < WAVE_COUNT; i++)
    {
        // 让每个波以不同的速度平移，制造流动感
        g_waves[i].current_phase = speed * (i + 1);
    }

    uint16_t *p_pixel = g_tex_vir_addr;

    for (int y = 0; y < TEX_HEIGHT; y++)
    {
        // 备份当前行的起始相位，因为内层循环会修改它
        int row_phases[WAVE_COUNT];
        for (int k = 0; k < WAVE_COUNT; k++)
        {
            row_phases[k] = g_waves[k].current_phase;
        }

        for (int x = 0; x < TEX_WIDTH; x++)
        {
            int sum = 0;

            // 叠加 7 个波
            // 这种结构非常适合 CPU 的流水线预测
            for (int k = 0; k < WAVE_COUNT; k++)
            {
                // 查表并累加
                // row_phases[k] >> 8 是将 Q8 定点数转为整数索引
                sum += cos_lut[(row_phases[k] >> 8) & LUT_MASK];

                // X轴增量步进
                row_phases[k] += g_waves[k].dx;
            }

            // 映射颜色
            // sum 范围大约 -420 ~ +420
            // 取绝对值，制造锐利的晶格感，并截断到 0-255
            int color_idx = ABS(sum);
            color_idx     = MIN(color_idx, 255);

            *p_pixel++ = g_palette[color_idx];
        }

        // Y轴增量步进 (准备下一行)
        for (int k = 0; k < WAVE_COUNT; k++)
        {
            g_waves[k].current_phase += g_waves[k].dy;
        }
    }

    /* === CRITICAL: Cache Flush === */
    aicos_dcache_clean_range((void *)g_tex_vir_addr, TEX_SIZE);

    /* === PHASE 2: GE Scaling === */
    struct ge_bitblt blt = {0};

    blt.src_buf.buf_type    = MPP_PHY_ADDR;
    blt.src_buf.phy_addr[0] = g_tex_phy_addr;
    blt.src_buf.stride[0]   = TEX_WIDTH * TEX_BPP;
    blt.src_buf.size.width  = TEX_WIDTH;
    blt.src_buf.size.height = TEX_HEIGHT;
    blt.src_buf.format      = TEX_FMT;
    blt.src_buf.crop_en     = 0;

    blt.dst_buf.buf_type    = MPP_PHY_ADDR;
    blt.dst_buf.phy_addr[0] = phy_addr;
    blt.dst_buf.stride[0]   = ctx->info.stride;
    blt.dst_buf.size.width  = ctx->info.width;
    blt.dst_buf.size.height = ctx->info.height;
    blt.dst_buf.format      = ctx->info.format;

    blt.dst_buf.crop_en     = 1;
    blt.dst_buf.crop.x      = 0;
    blt.dst_buf.crop.y      = 0;
    blt.dst_buf.crop.width  = ctx->info.width;
    blt.dst_buf.crop.height = ctx->info.height;

    blt.ctrl.flags    = 0;
    blt.ctrl.alpha_en = 1; // Disable Blending

    int ret = mpp_ge_bitblt(ctx->ge, &blt);
    if (ret < 0)
    {
        LOG_E("GE Error: %d", ret);
    }

    mpp_ge_emit(ctx->ge);
    mpp_ge_sync(ctx->ge);

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

struct effect_ops effect_0013 = {
    .name   = "NO.13 THE FORBIDDEN SYMMETRY",
    .init   = effect_init,
    .draw   = effect_draw,
    .deinit = effect_deinit,
};

REGISTER_EFFECT(effect_0013);
