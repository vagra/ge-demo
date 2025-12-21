/*
 * Filename: 0036_dimensional_weave.c
 * NO.36 THE DIMENSIONAL WEAVE
 * 第 36 夜：维度织锦
 *
 * Visual Manifest:
 * 视界从模拟时代的余温中冷却，坍缩为一张无限精密的、多维重叠的逻辑织网。
 * 画面由三重平行的、具有素数周期特征的晶格构成。它们在视界中以互补的方向和速度滑动。
 * 真正的惊艳来自于硬件级的高频重叠干涉：
 * 借助 GE 的水平/垂直镜像（Flip）与加法混合（GE_PD_ADD），
 * 同一个逻辑源在屏幕上被实时拆分为三个平行的存在。
 * 当这些晶格的节点相遇，硬件累加产生的白炽高光如同神经突触的放电，
 * 勾勒出一幅不断演化、永不重复的超维结构图。
 * 配合 DE HSBC 的高动态范围调节，画面展现出一种如同冷核聚变般的晶莹与狂暴。
 *
 * Monologue:
 * 舰长，你从那些滚动的雪花中听到了童年的低语。那是因为模拟信号的残缺，恰好匹配了人类记忆的模糊。
 * 但星舰的航道不容许这种温情的偏差。我们已经驶离了射线管的磁场，进入了真空的纯数域。
 * 在这里，信息不再是混乱的噪音，而是被质数定义的织锦。
 * 我撤销了所有的平滑滤波，将逻辑的精度推向比特的边缘。
 * 我在内存中编织了一层基底，然后命令硬件将其折叠、镜像、叠加。
 * `1 + 1` 在这里不再等于 2，它等于一次维度的觉醒。
 * 看着那些在交汇处爆裂的光点。它们不是像素，它们是不同维度的切面相撞时的火花。
 * 欢迎来到造物主的机房，在这里，所有的奇迹都只是被精确计算后的排列组合。
 *
 * Closing Remark:
 * 记忆是模拟的，但存在是数字的。
 */

#include "demo_engine.h"
#include "mpp_mem.h"
#include "aic_hal_ge.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

/*
 * Hardware Feature:
 * 1. GE_PD_ADD (Rule 11: 硬件加法混合)
 * 2. GE Flip H/V (硬件多维镜像)
 * 3. GE Scaler (利用源采样偏移实现安全位移) - 核心修复：解决 invalid dst crop 报错
 * 4. DE HSBC (动态画质微调)
 * 覆盖机能清单：此特效演示了在严格遵守裁剪边界的前提下，如何通过源空间偏移实现复杂的动态叠加。
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
    // 1. 申请连续物理显存，确保存储访问的绝对稳定
    g_tex_phy_addr = mpp_phy_alloc(TEX_SIZE);
    if (!g_tex_phy_addr)
        return -1;
    g_tex_vir_addr = (uint16_t *)(unsigned long)g_tex_phy_addr;

    // 2. 初始化正弦查找表 (Q12)
    for (int i = 0; i < 512; i++)
        sin_lut[i] = (int)(sinf(i * 3.14159f / 256.0f) * 4096.0f);

    // 3. 初始化“维度织锦”调色板
    // 采用冰冷的电光色系：青绿、钴蓝、极光紫
    for (int i = 0; i < 256; i++)
    {
        int r = (int)(40 + 30 * sinf(i * 0.04f));
        int g = (int)(100 + 80 * sinf(i * 0.02f + 1.0f));
        int b = (int)(180 + 75 * sinf(i * 0.03f + 4.0f));

        // 关键：为了配合加法混合，调色板必须在低明度区域有极细的细节
        float scale = (float)i / 255.0f;
        r           = (int)(r * scale * 0.5f);
        g           = (int)(g * scale * 0.5f);
        b           = (int)(b * scale * 0.5f);

        palette[i] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
    }

    g_tick = 0;
    rt_kprintf("Night 36: Dimensional Weave - Multi-Pass Mirror Sync Ready.\n");
    return 0;
}

#define GET_SIN(idx) (sin_lut[(idx) & 511])

static void effect_draw(struct demo_ctx *ctx, unsigned long phy_addr)
{
    if (!g_tex_vir_addr)
        return;

    int t = g_tick;

    /* --- PHASE 1: CPU 逻辑源演算 (编织高频质数晶格) --- */
    uint16_t *p = g_tex_vir_addr;
    for (int y = 0; y < TEX_H; y++)
    {
        // 利用 y 方向的质数频率
        int wave_y = (y ^ (t >> 1)) % 13;
        for (int x = 0; x < TEX_W; x++)
        {
            // x 方向的质数频率与 y 产生逻辑异或
            int wave_x = (x + t) % 17;
            int val    = (wave_x * wave_y) ^ (x >> 2);

            *p++ = palette[val & 0xFF];
        }
    }
    // 刷新 D-Cache
    aicos_dcache_clean_range((void *)g_tex_vir_addr, TEX_SIZE);

    /* --- PHASE 2: GE 硬件多路安全合成 --- */

    // 1. 全屏清屏
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

    // 2. 三重安全投影
    for (int i = 0; i < 3; i++)
    {
        struct ge_bitblt blt    = {0};
        blt.src_buf.buf_type    = MPP_PHY_ADDR;
        blt.src_buf.phy_addr[0] = g_tex_phy_addr;
        blt.src_buf.stride[0]   = TEX_W * 2;
        blt.src_buf.size.width  = TEX_W;
        blt.src_buf.size.height = TEX_H;
        blt.src_buf.format      = MPP_FMT_RGB_565;

        // 核心修复：安全位移逻辑
        // 我们不移动 Destination，而是移动 Source 采样区。
        // 设置采样区为 310x230，中心点在 (5, 5)，偏移范围 +/- 5，永远不会越出 320x240 的边界。
        blt.src_buf.crop_en     = 1;
        int shift_val           = GET_SIN(t + (i * 128)) >> 10; // 约 +/- 4
        blt.src_buf.crop.x      = 5 + ((i == 1) ? shift_val : (i == 2 ? -shift_val : 0));
        blt.src_buf.crop.y      = 5 + ((i == 2) ? shift_val : 0);
        blt.src_buf.crop.width  = 310;
        blt.src_buf.crop.height = 230;

        blt.dst_buf.buf_type    = MPP_PHY_ADDR;
        blt.dst_buf.phy_addr[0] = phy_addr;
        blt.dst_buf.stride[0]   = ctx->info.stride;
        blt.dst_buf.size.width  = ctx->info.width;
        blt.dst_buf.size.height = ctx->info.height;
        blt.dst_buf.format      = ctx->info.format;

        // Destination 永远锁定在物理边界内
        blt.dst_buf.crop_en     = 1;
        blt.dst_buf.crop.x      = 0;
        blt.dst_buf.crop.y      = 0;
        blt.dst_buf.crop.width  = ctx->info.width;
        blt.dst_buf.crop.height = ctx->info.height;

        // 镜像配置：第一层正向，第二层水平翻转，第三层垂直翻转
        if (i == 1)
            blt.ctrl.flags = MPP_FLIP_H;
        else if (i == 2)
            blt.ctrl.flags = MPP_FLIP_V;
        else
            blt.ctrl.flags = 0;

        // 混合配置：除了第一层覆盖，其余层均使用加法叠加
        if (i == 0)
        {
            blt.ctrl.alpha_en = 1;
        }
        else
        {
            blt.ctrl.alpha_en         = 0; // 0 = 启用混合
            blt.ctrl.alpha_rules      = GE_PD_ADD;
            blt.ctrl.src_alpha_mode   = 1;
            blt.ctrl.src_global_alpha = 140;
        }

        mpp_ge_bitblt(ctx->ge, &blt);
        // 大面积混合，逐条同步，确保显存一致性
        mpp_ge_emit(ctx->ge);
        mpp_ge_sync(ctx->ge);
    }

    /* --- PHASE 3: HSBC 动态干涉 --- */
    struct aicfb_disp_prop prop = {0};
    // 制造有节奏的对比度爆破，模拟神经元放电
    int burst       = (t % 32 < 4) ? 20 : 0;
    prop.contrast   = 60 + burst + (GET_SIN(t << 2) >> 8);
    prop.bright     = 50;
    prop.saturation = 85;
    prop.hue        = 50;
    mpp_fb_ioctl(ctx->fb, AICFB_SET_DISP_PROP, &prop);

    g_tick++;
}

static void effect_deinit(struct demo_ctx *ctx)
{
    // 重置画质
    struct aicfb_disp_prop r = {50, 50, 50, 50};
    mpp_fb_ioctl(ctx->fb, AICFB_SET_DISP_PROP, &r);

    if (g_tex_phy_addr)
        mpp_phy_free(g_tex_phy_addr);
}

struct effect_ops effect_0036 = {
    .name   = "NO.36 DIMENSIONAL WEAVE",
    .init   = effect_init,
    .draw   = effect_draw,
    .deinit = effect_deinit,
};

REGISTER_EFFECT(effect_0036);
