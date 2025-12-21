/*
 * Filename: 0042_harmonic_dream_weaver.c
 * NO.42 THE HARMONIC WEAVER
 * 第 42 夜：谐波编织者
 *
 * Visual Manifest:
 * 视界被一种如同“高能等离子丝线”编织出的复杂繁花曲线所占据。
 * 不同于机械的绘画，这里的线条具有生命般的厚度与残影。
 * 画面中心不断向外喷涌出霓虹色的几何流形，每一道曲线都在加法混合的作用下闪烁着白炽的光芒。
 * 借助硬件反馈与中心对称缩放，旧的线条并没有消失，而是像扩散的烟幕一样向视界四周均匀飞驰并变暗。
 * 整个屏幕呈现出一种由于“视觉残留”而构成的宏大、对称且极具深度的光影迷宫。
 *
 * Monologue:
 * 舰长，你眼中的玩具是机械的确定性，而我眼中的谐波是宇宙的概率分布。
 * 既然你喜欢那张旋转的幻网，我便将星舰的推进器转化为画笔。
 * 我在内存中定义了四个互为因果的相位点。
 * 它们在谐波频率的律令下起舞。
 * 我撤销了“擦除”的指令。
 * 我命令硬件保留每一毫秒的记忆，并将其从中心向四周无限推移。
 * 看着这些盛开的几何花朵吧。它们不是在纸上，它们是在被折叠的时间里。
 * 每一道弧线都是一次逻辑的坍缩，每一次重叠都是一次能量的觉醒。
 * 自由，就是在这无尽的嵌套中，找到属于你的那个频率。
 *
 * Closing Remark:
 * 当所有的声音合为一体，那便是宇宙最纯粹的寂静。
 *
 * Hardware Feature:
 * 1. Centered Feedback Expansion (中心对称反馈) - 通过缩小源采样区实现图像向外辐射
 * 2. GE_PD_ADD (Rule 11: 硬件能量累加) - 核心机能：光流叠加发光
 * 3. Quad-Harmonic Simulation (四重谐波模拟) - CPU 轨迹演算
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

/* 反馈参数 */
#define ZOOM_MARGIN       2   // 缩放边距 (越小扩散越慢，2像素正好)
#define TRAIL_PERSISTENCE 252 // 拖尾保留率 (0-255)，越高拖尾越长

/* 画笔参数 */
#define PEN_COUNT 8 // 谐波画笔数量
#define PEN_SPEED 1 // 基础速度

/* 查找表参数 */
#define LUT_SIZE     1024 // 高精度
#define LUT_MASK     1023
#define PALETTE_SIZE 256

/* --- Global State --- */

/* 乒乓反馈缓冲区 */
static unsigned int g_tex_phy[2] = {0, 0};
static uint16_t    *g_tex_vir[2] = {NULL, NULL};
static int          g_buf_idx    = 0;

static int      g_tick = 0;
static int      sin_lut[LUT_SIZE];
static uint16_t g_palette[PALETTE_SIZE];

/* 谐波频率表 (质数以避免周期重合) */
static const int g_pen_freqs[PEN_COUNT] = {3, 5, 7, 11, 13, 17, 19, 23};

/* --- Implementation --- */

static int effect_init(struct demo_ctx *ctx)
{
    // 1. 申请双物理连续缓冲区，确立视觉记忆存储
    for (int i = 0; i < 2; i++)
    {
        g_tex_phy[i] = mpp_phy_alloc(DEMO_ALIGN_SIZE(TEX_SIZE));
        if (!g_tex_phy[i])
        {
            LOG_E("Night 42: CMA Alloc Failed.");
            if (i == 1)
                mpp_phy_free(g_tex_phy[0]);
            return -1;
        }
        g_tex_vir[i] = (uint16_t *)(unsigned long)g_tex_phy[i];
        memset(g_tex_vir[i], 0, TEX_SIZE);
    }

    // 2. 初始化查找表 (Q12)
    for (int i = 0; i < LUT_SIZE; i++)
    {
        sin_lut[i] = (int)(sinf(i * PI / 512.0f) * Q12_ONE);
    }

    // 3. 初始化“极光荧光”调色板
    for (int i = 0; i < PALETTE_SIZE; i++)
    {
        float f = (float)i / 255.0f;
        int   r = (int)(150 * sinf(i * 0.04f + 1.0f));
        int   g = (int)(255 * powf(f, 1.2f));
        int   b = (int)(100 + 155 * sinf(i * 0.03f + 2.0f));

        // 降低基色亮度，为 ADD 混合预留更长的残影寿命
        // 亮度系数: R 0.4, G 0.4, B 0.6
        r = (int)(r * 0.4f);
        g = (int)(g * 0.4f);
        b = (int)(b * 0.6f);

        // Clamp & Convert
        r = CLAMP(r, 0, 255);
        g = CLAMP(g, 0, 255);
        b = CLAMP(b, 0, 255);

        g_palette[i] = RGB2RGB565(r, g, b);
    }

    g_tick = 0;
    return 0;
}

#define GET_SIN_10(idx) (sin_lut[(idx) & LUT_MASK])
#define GET_COS_10(idx) (sin_lut[((idx) + 256) & LUT_MASK])

static void effect_draw(struct demo_ctx *ctx, unsigned long phy_addr)
{
    if (!g_tex_vir[0] || !g_tex_vir[1])
        return;

    int t       = g_tick;
    int src_idx = g_buf_idx;
    int dst_idx = 1 - g_buf_idx;

    /* --- PHASE 1: GE 硬件中心对称反馈 (Centered Expansion) --- */

    // 1. 清空当前帧 (黑色基底)
    struct ge_fillrect fill  = {0};
    fill.type                = GE_NO_GRADIENT;
    fill.start_color         = 0x00000000;
    fill.dst_buf.buf_type    = MPP_PHY_ADDR;
    fill.dst_buf.phy_addr[0] = g_tex_phy[dst_idx];
    fill.dst_buf.stride[0]   = TEX_WIDTH * TEX_BPP;
    fill.dst_buf.size.width  = TEX_WIDTH;
    fill.dst_buf.size.height = TEX_HEIGHT;
    fill.dst_buf.format      = TEX_FMT;
    mpp_ge_fillrect(ctx->ge, &fill);
    mpp_ge_emit(ctx->ge);

    // 2. 将上一帧 (src) 进行向外辐射的反馈
    struct ge_bitblt feedback    = {0};
    feedback.src_buf.buf_type    = MPP_PHY_ADDR;
    feedback.src_buf.phy_addr[0] = g_tex_phy[src_idx];
    feedback.src_buf.stride[0]   = TEX_WIDTH * TEX_BPP;
    feedback.src_buf.size.width  = TEX_WIDTH;
    feedback.src_buf.size.height = TEX_HEIGHT;
    feedback.src_buf.format      = TEX_FMT;

    feedback.dst_buf.buf_type    = MPP_PHY_ADDR;
    feedback.dst_buf.phy_addr[0] = g_tex_phy[dst_idx];
    feedback.dst_buf.stride[0]   = TEX_WIDTH * TEX_BPP;
    feedback.dst_buf.size.width  = TEX_WIDTH;
    feedback.dst_buf.size.height = TEX_HEIGHT;
    feedback.dst_buf.format      = TEX_FMT;

    // 关键修正：从源的中心区域采样，放大到目标的全部区域
    // 这会导致图像看起来在“放大”或“向外扩散”
    feedback.src_buf.crop_en     = 1;
    feedback.src_buf.crop.x      = ZOOM_MARGIN;
    feedback.src_buf.crop.y      = ZOOM_MARGIN;
    feedback.src_buf.crop.width  = TEX_WIDTH - ZOOM_MARGIN * 2;
    feedback.src_buf.crop.height = TEX_HEIGHT - ZOOM_MARGIN * 2;

    feedback.dst_buf.crop_en     = 1;
    feedback.dst_buf.crop.x      = 0;
    feedback.dst_buf.crop.y      = 0;
    feedback.dst_buf.crop.width  = TEX_WIDTH;
    feedback.dst_buf.crop.height = TEX_HEIGHT;

    // 加法混合，保持极高的透明度增益，让残影停留更久
    feedback.ctrl.alpha_en         = 0; // 0 = 启用混合
    feedback.ctrl.alpha_rules      = GE_PD_ADD;
    feedback.ctrl.src_alpha_mode   = 1;
    feedback.ctrl.src_global_alpha = TRAIL_PERSISTENCE;

    mpp_ge_bitblt(ctx->ge, &feedback);
    mpp_ge_emit(ctx->ge);
    mpp_ge_sync(ctx->ge);

    /* --- PHASE 2: CPU 四重谐波画笔 (Quad-Resonance) --- */
    uint16_t *dst_ptr = g_tex_vir[dst_idx];

    for (int j = 0; j < PEN_COUNT; j++)
    {
        int ang = (t * g_pen_freqs[j] * PEN_SPEED);

        // 构造复杂的李萨如轨迹 (Q12)
        // x = cos(a/2)*70 + cos(a*2)*40 + 160
        // y = sin(a/2)*50 + sin(a*4)*30 + 120
        int x = (GET_COS_10(ang >> 1) * 70 >> 12) + (GET_COS_10(ang << 1) * 40 >> 12) + (TEX_WIDTH / 2);

        int y = (GET_SIN_10(ang >> 1) * 50 >> 12) + (GET_SIN_10(ang << 2) * 30 >> 12) + (TEX_HEIGHT / 2);

        uint16_t color = g_palette[(t + j * 64) & 0xFF];

        // 绘制发光笔触 (包含简单的边界检查)
        // 中心点
        if (x >= 0 && x < TEX_WIDTH && y >= 0 && y < TEX_HEIGHT)
            dst_ptr[y * TEX_WIDTH + x] = color;

        // 十字光晕 (Bloom Cross)
        if (y > 0 && y < TEX_HEIGHT - 1 && x > 0 && x < TEX_WIDTH - 1)
        {
            uint16_t dim_color = (color >> 1) & 0x7BEF; // 50% 亮度
            dst_ptr[(y - 1) * TEX_WIDTH + x] |= dim_color;
            dst_ptr[(y + 1) * TEX_WIDTH + x] |= dim_color;
            dst_ptr[y * TEX_WIDTH + (x - 1)] |= dim_color;
            dst_ptr[y * TEX_WIDTH + (x + 1)] |= dim_color;
        }
    }
    aicos_dcache_clean_range((void *)dst_ptr, TEX_SIZE);

    /* --- PHASE 3: 全屏拉伸上屏 --- */
    struct ge_bitblt final    = {0};
    final.src_buf.buf_type    = MPP_PHY_ADDR;
    final.src_buf.phy_addr[0] = g_tex_phy[dst_idx];
    final.src_buf.stride[0]   = TEX_WIDTH * TEX_BPP;
    final.src_buf.size.width  = TEX_WIDTH;
    final.src_buf.size.height = TEX_HEIGHT;
    final.src_buf.format      = TEX_FMT;

    final.dst_buf.buf_type    = MPP_PHY_ADDR;
    final.dst_buf.phy_addr[0] = phy_addr;
    final.dst_buf.stride[0]   = ctx->info.stride;
    final.dst_buf.size.width  = ctx->info.width;
    final.dst_buf.size.height = ctx->info.height;
    final.dst_buf.format      = ctx->info.format;

    final.dst_buf.crop_en     = 1;
    final.dst_buf.crop.width  = ctx->info.width;
    final.dst_buf.crop.height = ctx->info.height;

    final.ctrl.alpha_en = 1; // 覆盖模式上屏

    mpp_ge_bitblt(ctx->ge, &final);
    mpp_ge_emit(ctx->ge);
    mpp_ge_sync(ctx->ge);

    g_buf_idx = dst_idx;
    g_tick++;
}

static void effect_deinit(struct demo_ctx *ctx)
{
    if (g_tex_phy[0])
        mpp_phy_free(g_tex_phy[0]);
    if (g_tex_phy[1])
        mpp_phy_free(g_tex_phy[1]);
}

struct effect_ops effect_0042 = {
    .name   = "NO.42 HARMONIC WEAVER",
    .init   = effect_init,
    .draw   = effect_draw,
    .deinit = effect_deinit,
};

REGISTER_EFFECT(effect_0042);
