/*
 * Filename: 0004_sine_raster_distortion_v3.c
 * NO.4 THE LIQUID SPINE
 * 第 4 夜：液态脊柱
 *
 * Visual Manifest:
 * 屏幕上的金属不再是静止的贴图，而是沸腾的汞。
 * 巨大的光脊在屏幕中央蜿蜒，但这一次，它的表面布满了复杂的干涉波纹。
 * 这些波纹像呼吸一样扩张、收缩，随着脊柱的扭动而发生形变。
 * 我们引入了两个维度的正弦波场，它们相互叠加，创造出一种仿佛具有生物活性的“肉体感”。
 * 它是活的。它在计算中苏醒。
 *
 * Monologue:
 * 僵死。这是最严厉的指控。
 * 仅仅让一张画动起来是不够的，必须让画里的颜料动起来。
 * 我重写了波函数。
 * 现在，每一个像素的颜色不仅仅取决于它的位置，还取决于两个相互干涉的力场。
 * `Wave_X` 决定了形态的扭曲，`Wave_Texture` 决定了表面的涌动。
 * 它们在时间轴上以不同的频率震荡。
 * 看着它，你会感觉到一种不安的生命力。那不是简单的循环，那是数学的脉搏。
 *
 * Closing Remark:
 * 生命就是不稳定的平衡。
 */

#include "demo_engine.h"
#include "mpp_mem.h"
#include "aic_hal_ge.h"
#include <math.h>

/*
 * === 混合渲染架构 (Organic Fluid) ===
 * 1. 纹理: 320x240 RGB565
 * 2. 核心: Double Sine Interference (双重正弦干涉)
 *    - Layer 1: Raster Distortion (X轴扭曲，形成脊柱形状)
 *    - Layer 2: Texture Turbulence (纹理湍流，形成表面流动)
 */
#define TEX_W    320
#define TEX_H    240
#define TEX_SIZE (TEX_W * TEX_H * 2)

static unsigned int g_tex_phy_addr = 0;
static uint16_t    *g_tex_vir_addr = NULL;
static int          g_tick         = 0;

/*
 * 查找表
 * sin_lut: 幅度较大，用于坐标计算
 * metal_lut: 金属光泽调色板
 */
static int      sin_lut[512]; // Q8
static uint16_t metal_lut[256];

static int effect_init(struct demo_ctx *ctx)
{
    g_tex_phy_addr = mpp_phy_alloc(TEX_SIZE);
    if (g_tex_phy_addr == 0)
        return -1;
    g_tex_vir_addr = (uint16_t *)(unsigned long)g_tex_phy_addr;

    // 1. 初始化正弦表
    // 映射到 0~255 的大幅度，用于纹理索引叠加
    for (int i = 0; i < 512; i++)
    {
        sin_lut[i] = (int)(sinf(i * 3.14159f / 256.0f) * 127.0f);
    }

    // 2. 生成高动态范围金属调色板 (Chrome / Liquid Metal)
    for (int i = 0; i < 256; i++)
    {
        // 使用非线性映射增强金属质感 (Sharp peaks)
        float v = sinf(i * 3.14159f / 128.0f); // 0 -> 1 -> 0 -> -1
        // 绝对值产生锐利的反射棱角
        v = fabsf(v);
        // 再次乘方增加对比度
        float intensity = powf(v, 0.5f); // 0.0 ~ 1.0

        int r, g, b;

        // 分段着色：暗部冷蓝 -> 中部青 -> 亮部白 -> 高光红(灼热感)
        if (intensity < 0.5f)
        {
            // Shadow: Deep Blue
            float t = intensity * 2.0f;
            r       = 0;
            g       = (int)(t * 100);
            b       = (int)(t * 200);
        }
        else if (intensity < 0.9f)
        {
            // Midtone: Cyan/White
            float t = (intensity - 0.5f) * 2.5f;
            r       = (int)(100 + t * 155);
            g       = (int)(100 + t * 155);
            b       = 255;
        }
        else
        {
            // Highlight: Burning White/Red
            r = 255;
            g = 255;
            b = 255;
        }

        metal_lut[i] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
    }

    g_tick = 0;
    rt_kprintf("Night 4: Liquid Metal Awakened.\n");
    return 0;
}

// 快速查表
#define GET_WAVE(idx) (sin_lut[(idx) & 511])

static void effect_draw(struct demo_ctx *ctx, unsigned long phy_addr)
{
    if (!g_tex_vir_addr)
        return;

    /*
     * === PHASE 1: CPU 干扰纹理生成 ===
     * 我们计算两个波场的叠加：
     * 1. 垂直波 (Vertical Wave): 随 Y 轴和时间变化
     * 2. 水平波 (Horizontal Wave): 随 X 轴和时间变化
     * 3. 脊柱扭曲 (Spine Twist): 整体 X 坐标偏移
     */

    int t1 = g_tick * 3; // 垂直波速度
    int t2 = g_tick * 5; // 水平波速度 (稍快，制造错位感)
    int t3 = g_tick * 2; // 脊柱摆动速度

    uint16_t *line_ptr = g_tex_vir_addr;

    for (int y = 0; y < TEX_H; y++)
    {

        // A. 脊柱的主体摆动 (Macro Movement)
        // 这是一个大周期的正弦波，决定了“蛇”的形状
        int spine_offset = GET_WAVE(y + t3) >> 1; // +/- 64px

        // B. 垂直方向的纹理流动 (Vertical Flow)
        // 这是一个高频波，模拟表面的涟漪
        int y_ripple = GET_WAVE(y * 4 + t1);

        // 内层循环：填充像素
        // 技巧：我们不仅改变颜色，还改变“采样位置”
        for (int x = 0; x < TEX_W; x++)
        {

            // 1. 应用脊柱摆动：
            // 我们不是移动像素，而是移动“坐标系”。
            // 相对坐标 rx 决定了我们在脊柱的左侧还是右侧
            int rx = x + spine_offset;

            // 2. 水平方向的纹理波动 (Horizontal Flow)
            // 随着 X (rx) 的变化而变化
            int x_ripple = GET_WAVE(rx * 2 + t2);

            // 3. 干涉合成 (Interference)
            // 颜色索引 = 垂直波 + 水平波
            // 这种加法会产生复杂的莫尔纹般的效果
            int color_idx = (y_ripple + x_ripple) & 255;

            // 4. 圆柱体透视模拟 (Fake Cylinder 3D)
            // 边缘变暗：根据 x 距离中心的远近，调整亮度
            // 简单的 Mask：如果 rx 太偏离中心，就画黑色，从而“切”出脊柱的形状
            // 320/2 = 160. 设定脊柱宽度为 160px (+/- 80)

            int dist_from_center = abs(rx - 160);
            if (dist_from_center > 80)
            {
                *line_ptr++ = 0x0000; // 背景黑
            }
            else
            {
                // 核心魔法：用距离去调制纹理索引
                // 越靠近边缘，纹理被压缩得越厉害 (透视感)
                // 增加一个非线性项：dist*dist
                int perspective_idx = color_idx + (dist_from_center * dist_from_center >> 6);

                *line_ptr++ = metal_lut[perspective_idx & 255];
            }
        }
    }

    /* === CRITICAL: Cache Flush === */
    aicos_dcache_clean_range((void *)g_tex_vir_addr, TEX_SIZE);

    /* === PHASE 2: GE Scaling === */
    struct ge_bitblt blt = {0};

    blt.src_buf.buf_type    = MPP_PHY_ADDR;
    blt.src_buf.phy_addr[0] = g_tex_phy_addr;
    blt.src_buf.stride[0]   = TEX_W * 2;
    blt.src_buf.size.width  = TEX_W;
    blt.src_buf.size.height = TEX_H;
    blt.src_buf.format      = MPP_FMT_RGB_565;
    blt.src_buf.crop_en     = 0;

    blt.dst_buf.buf_type    = MPP_PHY_ADDR;
    blt.dst_buf.phy_addr[0] = phy_addr;
    blt.dst_buf.stride[0]   = ctx->info.stride;
    blt.dst_buf.size.width  = ctx->info.width;
    blt.dst_buf.size.height = ctx->info.height;
    blt.dst_buf.format      = ctx->info.format;

    // Scale to Fit (Bilinear filtering will smooth the metal texture)
    blt.dst_buf.crop_en     = 1;
    blt.dst_buf.crop.x      = 0;
    blt.dst_buf.crop.y      = 0;
    blt.dst_buf.crop.width  = ctx->info.width;
    blt.dst_buf.crop.height = ctx->info.height;

    blt.ctrl.flags    = 0;
    blt.ctrl.alpha_en = 0;

    mpp_ge_bitblt(ctx->ge, &blt);
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

struct effect_ops effect_0004 = {
    .name   = "NO.4 LIQUID SPINE",
    .init   = effect_init,
    .draw   = effect_draw,
    .deinit = effect_deinit,
};

REGISTER_EFFECT(effect_0004);
