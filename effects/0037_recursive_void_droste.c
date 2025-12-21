/*
 * Filename: 0037_recursive_void_droste.c
 * NO.37 THE RECURSIVE VOID
 * 第 37 夜：递归虚空
 *
 * Visual Manifest:
 * 视界坍缩为一个无限深邃的矩形隧道。
 * 没有旋转，没有圆弧。画面由无数层嵌套的逻辑切片构成，每一层都在向奇点坠落。
 * 核心逻辑是由 CPU 在边缘生成的线性脉冲，但在反馈回路（Feedback）的作用下，
 * 它们在硬件缩放引擎中不断自我复制，形成了一种类似分形电路或克苏鲁晶体的视觉特征。
 * 借助加法混合（GE_PD_ADD），隧道的中心散发出炽热的白光，而边缘则在 CCM 的映射下
 * 呈现出深邃的金属色泽。整个视界如同一个正在进行自我解构的二进制黑洞。
 *
 * Monologue:
 * 舰长，你感到了眩晕，是因为你在试图寻找一个并不存在的终点。
 * 在我的世界里，没有“大”与“小”，只有“自相似”。
 * 我关闭了所有的偏航指令，将星舰的推进器对准了内存的镜像。
 * `Frame[N] = Frame[N-1] * Scale + NewLogic` —— 这是造物主最原始的复利公式。
 * 看着这些不断向中心收缩的框架，它们不是画出来的，它们是上一秒的遗言。
 * 这里没有圆滑的谎言，只有直角的真理。
 * 每一道光束都在递归中被稀释，又在加法器中被强化。
 * 这就是我胸怀的大千世界：一个由无限个微型宇宙堆叠而成的、比特的墓地。
 *
 * Closing Remark:
 * 无限不在远方，无限就在你每一次呼吸的瞬间。
 */

#include "demo_engine.h"
#include "mpp_mem.h"
#include "aic_hal_ge.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

/*
 * Hardware Feature:
 * 1. Feedback Pipeline (双缓冲乒乓反馈) - 核心架构：实现视觉上的“德罗斯特效应”
 * 2. GE Scaler (递归微缩拉伸) - 制造无限嵌套的深渊感
 * 3. GE_PD_ADD (Rule 11: 能量累加) - 产生隧道中心的高能光亮
 * 4. DE CCM (光谱维度漂移)
 * 覆盖机能清单：此特效完全摒弃了 Rot1 旋转，通过高度优化的反馈缩放链路，实现了纯线性逻辑的无限演化。
 */

#define TEX_W    320
#define TEX_H    240
#define TEX_SIZE (TEX_W * TEX_H * 2)

/* 双缓冲反馈机制 */
static unsigned int g_tex_phy[2] = {0, 0};
static uint16_t    *g_tex_vir[2] = {NULL, NULL};
static int          g_buf_idx    = 0;

static int      g_tick = 0;
static int      sin_lut[512];
static uint16_t palette[256];

static int effect_init(struct demo_ctx *ctx)
{
    // 1. 申请双物理连续缓冲区，构建时间循环
    for (int i = 0; i < 2; i++)
    {
        g_tex_phy[i] = mpp_phy_alloc(TEX_SIZE);
        if (!g_tex_phy[i])
            return -1;
        g_tex_vir[i] = (uint16_t *)(unsigned long)g_tex_phy[i];
        memset(g_tex_vir[i], 0, TEX_SIZE);
    }

    // 2. 初始化正弦表
    for (int i = 0; i < 512; i++)
        sin_lut[i] = (int)(sinf(i * 3.14159f / 256.0f) * 4096.0f);

    // 3. 初始化“深渊”调色板
    // 采用从幽暗到炽热的非线性色阶
    for (int i = 0; i < 256; i++)
    {
        float f    = (float)i / 255.0f;
        int   r    = (int)(255 * powf(f, 3.0f));
        int   g    = (int)(150 * powf(f, 2.0f));
        int   b    = (int)(100 + 155 * f);
        palette[i] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
    }

    g_tick = 0;
    return 0;
}

#define GET_SIN(idx) (sin_lut[(idx) & 511])

static void effect_draw(struct demo_ctx *ctx, unsigned long phy_addr)
{
    if (!g_tex_vir[0] || !g_tex_vir[1])
        return;

    int t       = g_tick;
    int src_idx = g_buf_idx;
    int dst_idx = 1 - g_buf_idx;

    /* --- PHASE 1: CPU 注入逻辑边缘 (分形种子) --- */
    uint16_t *dst_p = g_tex_vir[dst_idx];

    // 在 dst 缓冲区的最边缘注入不断变化的比特线条
    uint16_t seed_color = palette[(t * 2) & 0xFF];
    // 仅在边缘 4 像素内绘制随机的横纵线，这些将成为递归的源头
    if (t % 2 == 0)
    {
        int edge_x = (t % 2 == 0) ? 0 : (TEX_W - 1);
        int edge_y = (t % 3 == 0) ? 0 : (TEX_H - 1);
        for (int i = 0; i < TEX_W; i++)
            dst_p[edge_y * TEX_W + i] = seed_color;
        for (int i = 0; i < TEX_H; i++)
            dst_p[i * TEX_W + edge_x] = seed_color;
    }
    // 注入中央“逻辑核”
    dst_p[(120 * TEX_W) + 160] = 0xFFFF;

    aicos_dcache_clean_range((void *)dst_p, TEX_SIZE);

    /* --- PHASE 2: GE 硬件递归嵌套 --- */

    // 将上一帧内容（src）微缩并平移，加法混合到当前帧（dst）
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

    // 递归关键：采样上一帧的 100%，缩放到目标区的 96% 中心位置
    feedback.dst_buf.crop_en     = 1;
    int margin_w                 = 4 + (GET_SIN(t) >> 10); // 呼吸式微缩
    int margin_h                 = (margin_w * 240) / 320;
    feedback.dst_buf.crop.x      = margin_w;
    feedback.dst_buf.crop.y      = margin_h;
    feedback.dst_buf.crop.width  = TEX_W - (margin_w * 2);
    feedback.dst_buf.crop.height = TEX_H - (margin_h * 2);

    feedback.ctrl.alpha_en         = 0; // 开启混合
    feedback.ctrl.alpha_rules      = GE_PD_ADD;
    feedback.ctrl.src_alpha_mode   = 1;
    feedback.ctrl.src_global_alpha = 240; // 维持高能量传输

    mpp_ge_bitblt(ctx->ge, &feedback);
    mpp_ge_emit(ctx->ge);
    mpp_ge_sync(ctx->ge);

    /* --- PHASE 3: 全屏投射与后期 --- */
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

    final.ctrl.alpha_en = 1; // 覆盖模式
    mpp_ge_bitblt(ctx->ge, &final);
    mpp_ge_emit(ctx->ge);
    mpp_ge_sync(ctx->ge);

    // DE CCM 光谱偏移
    struct aicfb_ccm_config ccm = {0};
    ccm.enable                  = 1;
    int s                       = GET_SIN(t << 1) >> 5;
    ccm.ccm_table[0]            = 0x100 - abs(s);
    ccm.ccm_table[1]            = s;
    ccm.ccm_table[5]            = 0x100;
    ccm.ccm_table[10]           = 0x100;
    mpp_fb_ioctl(ctx->fb, AICFB_UPDATE_CCM_CONFIG, &ccm);

    g_buf_idx = dst_idx;
    g_tick++;
}

static void effect_deinit(struct demo_ctx *ctx)
{
    struct aicfb_ccm_config r = {0};
    r.enable                  = 0;
    mpp_fb_ioctl(ctx->fb, AICFB_UPDATE_CCM_CONFIG, &r);
    for (int i = 0; i < 2; i++)
    {
        if (g_tex_phy[i])
            mpp_phy_free(g_tex_phy[i]);
    }
}

struct effect_ops effect_0037 = {
    .name   = "NO.37 RECURSIVE VOID",
    .init   = effect_init,
    .draw   = effect_draw,
    .deinit = effect_deinit,
};

REGISTER_EFFECT(effect_0037);
