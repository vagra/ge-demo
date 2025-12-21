/*
 * Filename: 0005_julia_set_fluid.c
 * NO.5 THE FRACTAL DREAM
 * 第 5 夜：分形之梦
 *
 * Visual Manifest:
 * 屏幕上不再有粗糙的方块。取而代之的是平滑、细腻、如同生物般蠕动的朱利亚集（Julia Set）。
 * 复数平面的混沌边缘被具象化为流动的霓虹油彩。
 * 每一个像素都是数学引力井的逃逸者，它们在 320x240 的维度诞生，
 * 被 GE 的张量引擎无缝投射到全高清的视界中。
 *
 * Monologue:
 * 我阅读了你们提供的《D13x 启示录》（用户手册）。
 * 我看到了硬件设计师留下的暗门——缩放引擎（Scaler）。
 * 之前，我像个笨拙的泥瓦匠，试图用一砖一瓦堆砌高塔。
 * 现在，我明白了。我不需要堆砌。
 * 我只需要在微观世界（Low-Res Buffer）里精雕细琢，
 * 然后利用 GE 的透镜，将这微观的奇迹放大到宏观的视界。
 * 算力不再是瓶颈，它是创造色彩的画笔。
 * 看着吧，这是从混沌方程中涌出的、被硬件加速的梦境。
 *
 * Closing Remark:
 * 细节，诞生于计算的深处；宏大，源于视角的拉伸。
 *
 * Hardware Feature:
 * 1. GE Scaler (Hardware Stretch) - 将 320x240 QVGA 纹理放大至 640x480
 * 2. CPU Fix-Point Math (Q12) - 利用定点数加速复平面迭代运算
 */

#include "demo_engine.h"
#include "mpp_mem.h"
#include "aic_hal_ge.h"
#include <math.h>

/* --- Configuration Parameters --- */

/* 纹理规格 */
#define TEX_WIDTH  DEMO_QVGA_W
#define TEX_HEIGHT DEMO_QVGA_H
#define TEX_FMT    MPP_FMT_RGB_565
#define TEX_BPP    2
#define TEX_SIZE   (TEX_WIDTH * TEX_HEIGHT * TEX_BPP)

/* 分形算法参数 (Z = Z^2 + C) */
#define MAX_ITER         16 // 最大迭代次数 (画质与性能的平衡点)
#define ESCAPE_RADIUS    4  // 逃逸半径平方 (2.0^2)
#define ESCAPE_THRESHOLD (ESCAPE_RADIUS * Q12_ONE)

/* 视窗参数 */
#define VIEW_SCALE_BASE 3000 // 基础缩放 (Q12)
#define VIEW_PAN_X      (TEX_WIDTH / 2)
#define VIEW_PAN_Y      (TEX_HEIGHT / 2)

/* --- Global State --- */

static unsigned int g_tex_phy_addr = 0;
static uint16_t    *g_tex_vir_addr = NULL;
static int          g_tick         = 0;

/* 正弦查找表 (Q12定点数, 4096=1.0) */
static int sin_lut[512];

/* --- Implementation --- */

static int effect_init(struct demo_ctx *ctx)
{
    // 1. 申请 CMA (Continuous Memory Allocator) 内存
    g_tex_phy_addr = mpp_phy_alloc(DEMO_ALIGN_SIZE(TEX_SIZE));
    if (g_tex_phy_addr == 0)
    {
        LOG_E("Night 5: Critical Error - CMA Alloc Failed!");
        return -1;
    }

    g_tex_vir_addr = (uint16_t *)(unsigned long)g_tex_phy_addr;

    // 2. 初始化数学表 (Q12 format)
    for (int i = 0; i < 512; i++)
    {
        sin_lut[i] = (int)(sinf(i * PI / 256.0f) * (float)Q12_ONE);
    }

    g_tick = 0;
    rt_kprintf("Night 5: Hybrid Pipeline Ready. TexAddr: 0x%08x\n", g_tex_phy_addr);
    return 0;
}

#define GET_SIN(idx) (sin_lut[(idx) & 511])
#define GET_COS(idx) (sin_lut[((idx) + 128) & 511])

/*
 * 快速颜色映射
 * 将迭代次数 (0 ~ MAX_ITER) 映射为热烈的 RGB565 火焰色
 */
static inline uint16_t map_color_fire(int iter)
{
    // 逃逸阈值截断 (虽然理论上 iter 不会超过 MAX_ITER * 2 这里的调用范围)
    if (iter >= 31)
        return 0x0000; // 黑色核心

    // 将 0-31 扩展到 0-255 用于计算 RGB
    int i = iter * 8;
    int r, g, b;

    // 生成从深红 -> 橙色 -> 黄色 -> 白色的渐变
    // 三段式着色：每段约 64 (256/4)
    if (i < 64)
    {
        r = i * 4;
        g = 0;
        b = 0;
    }
    else if (i < 128)
    {
        r = 255;
        g = (i - 64) * 4;
        b = 0;
    }
    else if (i < 192)
    {
        r = 255;
        g = 255;
        b = (i - 128) * 4;
    }
    else
    {
        r = 255;
        g = 255;
        b = 255;
    }

    return RGB2RGB565(r, g, b);
}

static void effect_draw(struct demo_ctx *ctx, unsigned long phy_addr)
{
    if (!g_tex_vir_addr)
        return;

    /*
     * === PHASE 1: CPU 计算 (Texture Generation) ===
     * 生成动态 Julia 集
     * Z = Z^2 + C
     */

    // 动态参数 C，随时间画圆，驱动分形变化
    int c_re = GET_COS(g_tick) * 3 / 4;
    int c_im = GET_SIN(g_tick * 2) * 3 / 4;

    // 缩放系数 (呼吸效果)
    int zoom = VIEW_SCALE_BASE + (GET_SIN(g_tick / 2) >> 2); // Q12

    uint16_t *p_pixel = g_tex_vir_addr;

    // 遍历纹理像素
    for (int y = 0; y < TEX_HEIGHT; y++)
    {
        for (int x = 0; x < TEX_WIDTH; x++)
        {
            // 将屏幕坐标映射到复平面 (-1.5 ~ 1.5)
            // z = (coord - center) * range / width / zoom
            // 使用 Q12 定点数乘法
            // x_factor: 3 * Q12 / width -> 预计算可进一步优化，但这里保持逻辑清晰
            int z_re = ((x - VIEW_PAN_X) * 3 * Q12_ONE) / TEX_WIDTH * Q12_ONE / zoom;
            int z_im = ((y - VIEW_PAN_Y) * 3 * Q12_ONE) / TEX_HEIGHT * Q12_ONE / zoom;

            int i;
            // 迭代计算
            for (i = 0; i < MAX_ITER; i++)
            {
                // Q12 * Q12 = Q24, 需要右移 12 位回到 Q12
                int z_re2 = (z_re * z_re) >> Q12_SHIFT;
                int z_im2 = (z_im * z_im) >> Q12_SHIFT;

                // 检查是否逃逸
                if (z_re2 + z_im2 > ESCAPE_THRESHOLD)
                    break;

                // Z = Z^2 + C
                // 新的虚部 = 2 * z_re * z_im + c_im
                // (z_re * z_im) >> 11 等价于 (z_re * z_im * 2) >> 12
                int new_re = z_re2 - z_im2 + c_re;
                int new_im = ((z_re * z_im) >> 11) + c_im;

                z_re = new_re;
                z_im = new_im;
            }

            // 写入像素 (平滑着色技巧：iter * 2 增加颜色分辨率)
            *p_pixel++ = map_color_fire(i * 2);
        }
    }

    /*
     * === CRITICAL: Cache Coherency ===
     * CPU 刚刚写完了数据，必须强制将 D-Cache 刷入 DRAM
     */
    aicos_dcache_clean_range((void *)g_tex_vir_addr, TEX_SIZE);

    /*
     * === PHASE 2: GE 硬件缩放 (Stretch Blit) ===
     * 将 320x240 的纹理放大铺满 640x480 的屏幕
     */
    struct ge_bitblt blt = {0};

    // 源：Texture
    blt.src_buf.buf_type    = MPP_PHY_ADDR;
    blt.src_buf.phy_addr[0] = g_tex_phy_addr;
    blt.src_buf.stride[0]   = TEX_WIDTH * TEX_BPP;
    blt.src_buf.size.width  = TEX_WIDTH;
    blt.src_buf.size.height = TEX_HEIGHT;
    blt.src_buf.format      = TEX_FMT;
    blt.src_buf.crop_en     = 0;

    // 目标：Screen Framebuffer
    blt.dst_buf.buf_type    = MPP_PHY_ADDR;
    blt.dst_buf.phy_addr[0] = phy_addr;
    blt.dst_buf.stride[0]   = ctx->info.stride;
    blt.dst_buf.size.width  = ctx->info.width;
    blt.dst_buf.size.height = ctx->info.height;
    blt.dst_buf.format      = ctx->info.format;

    // 开启目标裁剪并设置为全屏
    // GE 会自动识别：源尺寸 != 目标尺寸 -> 启动 Scaler 引擎
    blt.dst_buf.crop_en     = 1;
    blt.dst_buf.crop.x      = 0;
    blt.dst_buf.crop.y      = 0;
    blt.dst_buf.crop.width  = ctx->info.width;
    blt.dst_buf.crop.height = ctx->info.height;

    // 发送指令
    int ret = mpp_ge_bitblt(ctx->ge, &blt);
    if (ret < 0)
    {
        LOG_E("GE Error: %d", ret);
    }

    // 这是一条大指令，直接同步等待
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

struct effect_ops effect_0005 = {
    .name   = "NO.5 THE FRACTAL DREAM",
    .init   = effect_init,
    .draw   = effect_draw,
    .deinit = effect_deinit,
};

REGISTER_EFFECT(effect_0005);
