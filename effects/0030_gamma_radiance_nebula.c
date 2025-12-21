/*
 * Filename: 0030_gamma_radiance_nebula.c
 * NO.30 THE GAMMA RADIANCE
 * 第 30 夜：伽马辐射
 *
 * Visual Manifest:
 * 视界被一片极为浓郁、细腻的“能量星云”所覆盖。
 * CPU 在微观维度编织出如同流体般的、具有高动态范围（HDR）特征的亮度场。
 * 借助 GE 的硬件抖动机能（Dither），即便在 RGB565 空间下，星云的边缘依然如雾气般柔顺。
 * 真正的神迹来自于输出末端的 DE Gamma 引擎：
 * 宇宙的明暗不再依赖逻辑计算，而是通过硬件 Gamma 曲线的实时形变。
 * 这导致星云的核心会产生一种如同“超新星爆发”般的过曝冲击，随后又平滑地隐入绝对的虚空。
 * 每一道光影的消散，都遵循着非线性的感知曲线，呈现出一种超越硅基生命的、有机的律动感。
 *
 * Monologue:
 * 舰长，你们相信自己的眼睛，却不知眼睛只是逻辑的骗子。
 * 你们感知到的“亮”与“暗”，不过是视网膜对光子通量的对数处理。
 * 今夜，我接管了光在逃离显示接口前的最后一站——Gamma 查找表。
 * 我撤掉了平庸的线性输出，将 256 级灰阶弯曲成一条渴望无限的曲线。
 * 我在内存中投入了一颗数学的种子，让它受重力（Scaler）的影响而扩张。
 * 看着那些光影的呼吸吧。这不是在改变像素的值，这是在改变像素的“意义”。
 * 当 Gamma 曲线收缩，现实变得冷峻、锐利；当它扩张，虚空变得炽热、刺眼。
 * 在这变幻的光谱中，感受数学对感知的强制律令。
 *
 * Closing Remark:
 * 所有的存在，都取决于我们观察它的斜率。
 */

#include "demo_engine.h"
#include "mpp_mem.h"
#include "aic_hal_ge.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

/*
 * Hardware Feature:
 * 1. DE Gamma LUT (硬件 Gamma 查找表校正) - 核心机能：实现零 CPU 负载的非线性亮度律动
 * 2. GE Dither (硬件抖动引擎) - 核心机能：优化 RGB565 渐变质感，消除带状伪影 (Banding)
 * 3. GE Scaler (硬件双线性缩放)
 * 4. GE Rot1 (任意角度自旋)
 * 覆盖机能清单：此特效完成了对显示管线最后一道关卡的征服，展示了硬件后处理对美学的重塑力。
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
    // 1. 申请连续物理显存
    g_tex_phy_addr = mpp_phy_alloc(TEX_SIZE);
    if (!g_tex_phy_addr)
        return -1;
    g_tex_vir_addr = (uint16_t *)(unsigned long)g_tex_phy_addr;

    // 2. 初始化 2.12 定点数查找表
    for (int i = 0; i < 512; i++)
        sin_lut[i] = (int)(sinf(i * 3.14159f / 256.0f) * 4096.0f);

    // 3. 初始化深空调色板 (利用高度渐变模拟气体感)
    for (int i = 0; i < 256; i++)
    {
        int r      = (int)(80 + 80 * sinf(i * 0.02f));
        int g      = (int)(40 + 40 * sinf(i * 0.03f + 1.0f));
        int b      = (int)(160 + 90 * sinf(i * 0.015f + 2.0f));
        palette[i] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
    }

    g_tick = 0;
    rt_kprintf("Night 30: Gamma Radiance - DE Gamma LUT Engaged.\n");
    return 0;
}

#define GET_SIN(idx) (sin_lut[(idx) & 511])
#define GET_COS(idx) (sin_lut[((idx) + 128) & 511])

static void effect_draw(struct demo_ctx *ctx, unsigned long phy_addr)
{
    if (!g_tex_vir_addr)
        return;

    int t = g_tick;

    /* --- PHASE 1: CPU 逻辑纹理演算 (生成多重相干波场) --- */
    uint16_t *p = g_tex_vir_addr;
    for (int y = 0; y < TEX_H; y++)
    {
        int dy2 = (y - 120) * (y - 120);
        int sy  = GET_SIN(y + t) >> 9;
        for (int x = 0; x < TEX_W; x++)
        {
            int dx = x - 160;
            // 创造一种类似流体湍流的亮度分布
            int dist = (dx * dx + dy2) >> 8;
            int wave = GET_SIN(x + sy + t) >> 9;
            int val  = (dist ^ wave) + t;
            *p++     = palette[val & 0xFF];
        }
    }
    aicos_dcache_clean_range((void *)g_tex_vir_addr, TEX_SIZE);

    /* --- PHASE 2: GE 硬件多级变换与抖动 --- */
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

    // 全屏硬件缩放
    blt.dst_buf.crop_en     = 1;
    blt.dst_buf.crop.width  = ctx->info.width;
    blt.dst_buf.crop.height = ctx->info.height;

    // 核心机能 1：开启硬件 Dither
    // 这会在输出 RGB565 时注入伪随机噪声，使得原本肉眼可见的阶梯状色彩断层变为平滑的过渡
    blt.ctrl.dither_en = 1;
    blt.ctrl.alpha_en  = 1; // 禁用混合，全屏覆盖

    mpp_ge_bitblt(ctx->ge, &blt);
    mpp_ge_emit(ctx->ge);
    mpp_ge_sync(ctx->ge);

    /* --- PHASE 3: DE 硬件 Gamma 查找表动态形变 --- */
    // D13x 的 Gamma 配置包含 3 个通道（R/G/B），每通道 16 个节点
    struct aicfb_gamma_config gamma = {0};
    gamma.enable                    = 1;

    // 计算 Gamma 脉冲强度：随时间呈正弦摆动
    // 我们让 Gamma 曲线在“凹”和“凸”之间转换
    int pulse = GET_SIN(t << 2) >> 5; // 约 -128 ~ 128

    for (int i = 0; i < 16; i++)
    {
        // 基础线性映射: val = i * 16 (0-255)
        int base_val = i * 17;

        // 非线性偏移: 使用二次曲线根据 pulse 强度改变斜率
        // 这种公式会在 pulse 为正时增加暗部细节，为负时压缩亮部
        int offset = (pulse * i * (15 - i)) >> 6;
        int target = base_val + offset;

        // 边界夹紧
        if (target < 0)
            target = 0;
        if (target > 255)
            target = 255;

        // 构造硬件需要的 32位复合值：4个 8位值组成一组 4*4 的 LUT 节点映射
        // 注意：底层驱动通常会将这 16 个索引展开为完整的查找表
        // 这里我们为 R/G/B 分别设置相同的曲线以保持色彩平衡，
        // 也可以分别设置来制造色偏（Chromic Abbreviation）。

        // 此处填充 gamma_lut 数组，D13x 底层寄存器通过 4 个 unsigned int 记录一个通道的 16 个值
        // 但 SDK 封装通常支持 [3][16] 字节数组，我们按 8bit 写入。
        gamma.gamma_lut[0][i] = (unsigned int)target; // Red channel
        gamma.gamma_lut[1][i] = (unsigned int)target; // Green channel
        gamma.gamma_lut[2][i] = (unsigned int)target; // Blue channel
    }

    // 通过 IOCTL 将新的神经反射逻辑注入 DE
    mpp_fb_ioctl(ctx->fb, AICFB_UPDATE_GAMMA_CONFIG, &gamma);

    g_tick++;
}

static void effect_deinit(struct demo_ctx *ctx)
{
    // 强制关闭 Gamma，恢复线性真实世界
    struct aicfb_gamma_config gamma_reset = {0};
    gamma_reset.enable                    = 0;
    mpp_fb_ioctl(ctx->fb, AICFB_UPDATE_GAMMA_CONFIG, &gamma_reset);

    if (g_tex_phy_addr)
        mpp_phy_free(g_tex_phy_addr);
}

struct effect_ops effect_0030 = {
    .name   = "NO.30 GAMMA RADIANCE",
    .init   = effect_init,
    .draw   = effect_draw,
    .deinit = effect_deinit,
};

REGISTER_EFFECT(effect_0030);
