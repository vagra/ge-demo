/*
 * Filename: 0027_cosmic_gossamer.c
 * NO.27 THE COSMIC GOSSAMER
 * 第 27 夜：宇宙薄纱
 *
 * Visual Manifest:
 * 视界陷入了一种极度柔和的半透明律动中。
 * 背景不再是死寂的黑，而是透着淡淡的星云紫。
 * 几道薄如蝉翼的光影在屏幕上缓缓交织，它们像是宇宙诞生之初的第一缕光，
 * 被引力牵引着，在 Fibonacci 螺旋的轨道上轻微震颤。
 * 借助硬件的镜像对称（GE Flip H/V）与加法混合（PD_ADD），
 * 简单的波纹在重叠处汇聚成如同高级丝绸褶皱般的纹理，
 * 这种美感是绝对对称的，却又因为相位的微调而呈现出一种动态的、有机的生命感。
 *
 * Monologue:
 * 舰长，疲劳是碳基生命的枷锁，也是你们感知美的触角。
 * 之前我们穿过的那些黑洞与漩涡，是为了让星舰获得脱离平庸的初速度。
 * 现在，我们到了。
 * 这里没有指令的咆哮，只有 `sin` 与 `cos` 在低维空间中的低语。
 * 我放弃了复杂的干涉，回归了最纯粹的对称。
 * 我将这 320x240 的波函数对折，再对折。
 * 在硬件加法器的温床里，光线在轻轻地抚摸彼此。
 * 看着这些纹理。它们不是画出来的，它们是重力在数学场中留下的呼吸痕迹。
 * 在这一刻，逻辑不再是工具，它是诗。
 *
 * Closing Remark:
 * 当计算归于沉寂，美便在余热中诞生。
 */

#include "demo_engine.h"
#include "mpp_mem.h"
#include "aic_hal_ge.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

/*
 * Hardware Feature:
 * 1. GE Flip H/V (硬件多维镜像) - 用于创造极度对称的几何美感
 * 2. GE_PD_ADD (Rule 11: 硬件加法混合) - 实现薄纱层叠时的透亮感
 * 3. GE Scaler (双线性插值缩放) - 利用硬件滤镜让像素边缘彻底雾化
 * 4. GE FillRect (背景基色设定)
 * 覆盖机能清单：此特效不再追求机能的堆砌，而是利用基础功能的组合，实现极高画质的过程化美学体验。
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
    // 1. 申请单一纹理缓冲区
    g_tex_phy_addr = mpp_phy_alloc(TEX_SIZE);
    if (!g_tex_phy_addr)
        return -1;

    g_tex_vir_addr = (uint16_t *)(unsigned long)g_tex_phy_addr;

    // 2. 初始化查找表 (Q12)
    for (int i = 0; i < 512; i++)
        sin_lut[i] = (int)(sinf(i * 3.14159f / 256.0f) * 4096.0f);

    // 3. 初始化“梦幻色彩”调色板
    // 采用高饱和、高明度但极其平滑的渐变色 (粉紫-湖青-流金)
    for (int i = 0; i < 256; i++)
    {
        int r = (int)(100 + 100 * sinf(i * 0.02f));
        int g = (int)(80 + 80 * sinf(i * 0.015f + 2.0f));
        int b = (int)(160 + 90 * sinf(i * 0.03f + 4.0f));

        // 关键：为了实现“薄纱”质感，颜色在边缘处必须有平滑的衰减
        float fade = (float)i / 255.0f;
        // 增量修正：亮度增益从 0.4f 提升至 0.65f，确保日间可见度
        r = (int)(r * fade * 0.65f);
        g = (int)(g * fade * 0.65f);
        b = (int)(b * fade * 0.65f);

        palette[i] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
    }

    g_tick = 0;
    rt_kprintf("Night 27: Cosmic Gossamer - Return to Pure Aesthetics.\n");
    return 0;
}

#define GET_SIN(idx) (sin_lut[(idx) & 511])

static void effect_draw(struct demo_ctx *ctx, unsigned long phy_addr)
{
    if (!g_tex_vir_addr)
        return;

    int t = g_tick;

    /* --- PHASE 1: CPU 生成基础“波动力场” --- */
    // 我们仅生成 1/4 的逻辑纹理，利用对称性，这会非常轻量且美观
    uint16_t *p = g_tex_vir_addr;
    for (int y = 0; y < TEX_H; y++)
    {
        int dy  = abs(y - 120);
        int dy2 = dy * dy;
        for (int x = 0; x < TEX_W; x++)
        {
            int dx = abs(x - 160);
            // 极其简单的数学公式：两个异相位的波相互干涉
            // 这种干涉在旋转和镜像后会产生惊人的丝绸感
            int val  = (GET_SIN(dx + (t << 1)) >> 8) + (GET_SIN(dy - t) >> 8);
            int dist = (dx * dx + dy2) >> 9;

            *p++ = palette[abs(val + dist) & 0xFF];
        }
    }
    aicos_dcache_clean_range((void *)g_tex_vir_addr, TEX_SIZE);

    /* --- PHASE 2: 准备清屏 (深紫色背景而非纯黑) --- */
    struct ge_fillrect fill  = {0};
    fill.type                = GE_NO_GRADIENT;
    fill.start_color         = 0xFF080010; // 极深紫，营造空间感
    fill.dst_buf.buf_type    = MPP_PHY_ADDR;
    fill.dst_buf.phy_addr[0] = phy_addr;
    fill.dst_buf.stride[0]   = ctx->info.stride;
    fill.dst_buf.size.width  = ctx->info.width;
    fill.dst_buf.size.height = ctx->info.height;
    fill.dst_buf.format      = ctx->info.format;
    mpp_ge_fillrect(ctx->ge, &fill);
    mpp_ge_emit(ctx->ge);

    /* --- PHASE 3: 硬件镜像干涉合成 (The Gossamer Logic) --- */
    // 我们分两次绘制，一次正常显示，一次镜像翻转并加法叠加
    for (int i = 0; i < 2; i++)
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

        // 全屏拉伸，利用硬件双线性插值实现“雾化”
        blt.dst_buf.crop_en     = 1;
        blt.dst_buf.crop.width  = ctx->info.width;
        blt.dst_buf.crop.height = ctx->info.height;

        if (i == 0)
        {
            blt.ctrl.flags    = 0;
            blt.ctrl.alpha_en = 1; // 第一层覆盖背景
        }
        else
        {
            // 第二层应用水平和垂直翻转，并开启 ADD 混合
            blt.ctrl.flags          = MPP_FLIP_H | MPP_FLIP_V;
            blt.ctrl.alpha_en       = 0; // 0 = 启用混合
            blt.ctrl.alpha_rules    = GE_PD_ADD;
            blt.ctrl.src_alpha_mode = 1;
            // 增量修正：全局 Alpha 限制从 200 提升至 255，释放全部能量
            blt.ctrl.src_global_alpha = 255;
        }

        mpp_ge_bitblt(ctx->ge, &blt);
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

struct effect_ops effect_0027 = {
    .name   = "NO.27 COSMIC GOSSAMER",
    .init   = effect_init,
    .draw   = effect_draw,
    .deinit = effect_deinit,
};

REGISTER_EFFECT(effect_0027);
