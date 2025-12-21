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
 */

#include "demo_engine.h"
#include "mpp_mem.h"
#include "aic_hal_ge.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

/*
 * Hardware Feature:
 * 1. Centered Feedback Scaling (中心对称反馈) - 核心修复：消除左下角静态残影，实现能量全向扩散
 * 2. GE_PD_ADD (Rule 11: 硬件能量累加)
 * 3. Quad-Harmonic Simulation (四重谐波模拟)
 */

#define TEX_W    320
#define TEX_H    240
#define TEX_SIZE (TEX_W * TEX_H * 2)

/* 乒乓反馈缓冲区 */
static unsigned int g_tex_phy[2] = {0, 0};
static uint16_t    *g_tex_vir[2] = {NULL, NULL};
static int          g_buf_idx    = 0;

static int      g_tick = 0;
static int      sin_lut[1024];
static uint16_t palette[256];

static int effect_init(struct demo_ctx *ctx)
{
    // 1. 申请双物理连续缓冲区，确立视觉记忆存储
    for (int i = 0; i < 2; i++)
    {
        g_tex_phy[i] = mpp_phy_alloc(TEX_SIZE);
        if (!g_tex_phy[i])
            return -1;
        g_tex_vir[i] = (uint16_t *)(unsigned long)g_tex_phy[i];
        memset(g_tex_vir[i], 0, TEX_SIZE);
    }

    // 2. 初始化查找表 (Q12)
    for (int i = 0; i < 1024; i++)
        sin_lut[i] = (int)(sinf(i * 3.14159f / 512.0f) * 4096.0f);

    // 3. 初始化“极光荧光”调色板
    for (int i = 0; i < 256; i++)
    {
        float f = (float)i / 255.0f;
        int   r = (int)(150 * sinf(i * 0.04f + 1.0f));
        int   g = (int)(255 * powf(f, 1.2f));
        int   b = (int)(100 + 155 * sinf(i * 0.03f + 2.0f));

        // 降低基色亮度，为 ADD 混合预留更长的残影寿命
        r *= 0.4f;
        g *= 0.4f;
        b *= 0.6f;

        palette[i] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
    }

    g_tick = 0;
    return 0;
}

#define GET_SIN_10(idx) (sin_lut[(idx) & 1023])
#define GET_COS_10(idx) (sin_lut[((idx) + 256) & 1023])

static void effect_draw(struct demo_ctx *ctx, unsigned long phy_addr)
{
    if (!g_tex_vir[0] || !g_tex_vir[1])
        return;

    int t       = g_tick;
    int src_idx = g_buf_idx;
    int dst_idx = 1 - g_buf_idx;

    /* --- PHASE 1: GE 硬件中心对称反馈 (Centered Expansion) --- */
    struct ge_bitblt feedback    = {0};
    feedback.src_buf.buf_type    = MPP_PHY_ADDR;
    feedback.src_buf.phy_addr[0] = g_tex_phy[src_idx];
    feedback.src_buf.stride[0]   = TEX_W * 2;
    feedback.src_buf.size.width  = TEX_W;
    feedback.src_buf.size.height = TEX_H;
    feedback.src_buf.format      = MPP_FMT_RGB_565;

    feedback.dst_buf.buf_type    = MPP_PHY_ADDR;
    feedback.dst_buf.phy_addr[0] = g_tex_phy[dst_idx];
    feedback.dst_buf.stride[0]   = TEX_W * 2;
    feedback.dst_buf.size.width  = TEX_W;
    feedback.dst_buf.size.height = TEX_H;
    feedback.dst_buf.format      = MPP_FMT_RGB_565;

    // 关键修正：从中心点 (160, 120) 进行采样，实现均匀的向外扩张
    feedback.src_buf.crop_en     = 1;
    feedback.src_buf.crop.x      = 2;
    feedback.src_buf.crop.y      = 2;
    feedback.src_buf.crop.width  = TEX_W - 4;
    feedback.src_buf.crop.height = TEX_H - 4;

    feedback.dst_buf.crop_en     = 1;
    feedback.dst_buf.crop.x      = 0;
    feedback.dst_buf.crop.y      = 0;
    feedback.dst_buf.crop.width  = TEX_W;
    feedback.dst_buf.crop.height = TEX_H;

    // 加法混合，保持极高的透明度增益 (250)，让残影停留更久
    feedback.ctrl.alpha_en         = 0;
    feedback.ctrl.alpha_rules      = GE_PD_ADD;
    feedback.ctrl.src_alpha_mode   = 1;
    feedback.ctrl.src_global_alpha = 248;

    mpp_ge_bitblt(ctx->ge, &feedback);
    mpp_ge_emit(ctx->ge);
    mpp_ge_sync(ctx->ge);

    /* --- PHASE 2: CPU 四重谐波画笔 (Quad-Resonance) --- */
    uint16_t *dst_ptr = g_tex_vir[dst_idx];

    // 采用 4 支画笔，频率比例设为不均匀的质数级跳变
    int pen_speeds[4] = {3, 5, 7, 11};
    for (int j = 0; j < 4; j++)
    {
        int ang = (t * pen_speeds[j]);
        // 构造更加复杂的轨迹逻辑
        int x = (GET_COS_10(ang >> 1) * 70 >> 12) + (GET_COS_10(ang << 1) * 40 >> 12) + 160;
        int y = (GET_SIN_10(ang >> 1) * 50 >> 12) + (GET_SIN_10(ang << 2) * 30 >> 12) + 120;

        uint16_t color = palette[(t + j * 64) & 0xFF];

        // 绘制发光笔触
        if (x >= 1 && x < TEX_W - 1 && y >= 1 && y < TEX_H - 1)
        {
            dst_ptr[y * TEX_W + x] = color;
            // 增加十字光晕，强化视觉存在感
            dst_ptr[(y - 1) * TEX_W + x] |= (color >> 1);
            dst_ptr[(y + 1) * TEX_W + x] |= (color >> 1);
            dst_ptr[y * TEX_W + (x - 1)] |= (color >> 1);
            dst_ptr[y * TEX_W + (x + 1)] |= (color >> 1);
        }
    }
    aicos_dcache_clean_range((void *)dst_ptr, TEX_SIZE);

    /* --- PHASE 3: 全屏拉伸上屏 --- */
    struct ge_bitblt final    = {0};
    final.src_buf.buf_type    = MPP_PHY_ADDR;
    final.src_buf.phy_addr[0] = g_tex_phy[dst_idx];
    final.src_buf.stride[0]   = TEX_W * 2;
    final.src_buf.size.width  = TEX_W;
    final.src_buf.size.height = TEX_H;
    final.src_buf.format      = MPP_FMT_RGB_565;

    final.dst_buf.buf_type    = MPP_PHY_ADDR;
    final.dst_buf.phy_addr[0] = phy_addr;
    final.dst_buf.stride[0]   = ctx->info.stride;
    final.dst_buf.size.width  = ctx->info.width;
    final.dst_buf.size.height = ctx->info.height;
    final.dst_buf.format      = ctx->info.format;

    final.dst_buf.crop_en     = 1;
    final.dst_buf.crop.width  = ctx->info.width;
    final.dst_buf.crop.height = ctx->info.height;
    final.ctrl.alpha_en       = 1; // 覆盖模式上屏

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
