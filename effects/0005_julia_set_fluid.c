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
 */

#include "demo_engine.h"
#include "mpp_mem.h"    // 必须包含：用于分配物理连续内存(CMA)
#include "aic_hal_ge.h" // 必须包含：用于 Cache 操作宏
#include <math.h>

/*
 * === 混合渲染架构 (Hybrid Pipeline) ===
 * 1. 纹理分辨率：320 x 240 (QVGA)
 *    像素量仅为全屏 (640x480) 的 1/4。
 *    CPU 负担减轻 75%，可以进行更复杂的浮点/定点运算。
 *
 * 2. 像素格式：RGB565
 *    源数据使用 RGB565，带宽比 RGB888 节省 33%。
 *    GE 会在 BitBLT 时自动将 RGB565 转换为屏幕的 RGB888。
 */
#define TEX_W    320
#define TEX_H    240
#define TEX_SIZE (TEX_W * TEX_H * 2)

/*
 * 物理地址句柄。
 * GE 只能访问物理地址。mpp_phy_alloc 返回的就是物理地址。
 */
static unsigned int g_tex_phy_addr = 0;
static uint16_t    *g_tex_vir_addr = NULL;
static int          g_tick         = 0;

/* 正弦查找表 (Q12定点数, 4096=1.0) */
static int sin_lut[512];

static int effect_init(struct demo_ctx *ctx)
{
    /*
     * 1. 申请 CMA (Continuous Memory Allocator) 内存
     * 这是 GE 能够正常工作的关键。普通的 malloc 内存可能不连续或不可被 DMA 访问。
     */
    g_tex_phy_addr = mpp_phy_alloc(TEX_SIZE);
    if (g_tex_phy_addr == 0)
    {
        rt_kprintf("Night 5: Critical Error - CMA Alloc Failed!\n");
        return -1;
    }

    /* 在 D13x Flat 内存模型中，物理地址可以直接转为虚拟指针使用 */
    g_tex_vir_addr = (uint16_t *)(unsigned long)g_tex_phy_addr;

    /* 2. 初始化数学表 */
    for (int i = 0; i < 512; i++)
    {
        sin_lut[i] = (int)(sinf(i * 3.14159f / 256.0f) * 4096.0f);
    }

    g_tick = 0;
    rt_kprintf("Night 5: Hybrid Pipeline Ready. TexAddr: 0x%08x\n", g_tex_phy_addr);
    return 0;
}

#define GET_SIN(idx) (sin_lut[(idx) & 511])
#define GET_COS(idx) (sin_lut[((idx) + 128) & 511])

/*
 * 快速颜色映射
 * 将迭代次数 (0~31) 映射为热烈的 RGB565 火焰色
 */
static inline uint16_t map_color_fire(int iter)
{
    if (iter >= 31)
        return 0x0000; // 黑色核心

    int i = iter * 8; // 扩展范围
    int r, g, b;

    // 生成从深红 -> 橙色 -> 黄色 -> 白色的渐变
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

    // RGB888 -> RGB565
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
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

    // 动态参数 C，随时间画圆
    int c_re = GET_COS(g_tick) * 3 / 4;
    int c_im = GET_SIN(g_tick * 2) * 3 / 4;

    // 缩放系数 (呼吸效果)
    int zoom = 3000 + (GET_SIN(g_tick / 2) >> 2); // Q12

    // 预计算中心偏移
    int       offset_x = TEX_W / 2;
    int       offset_y = TEX_H / 2;
    uint16_t *p_pixel  = g_tex_vir_addr;

    // 遍历纹理像素
    for (int y = 0; y < TEX_H; y++)
    {
        for (int x = 0; x < TEX_W; x++)
        {
            // 将屏幕坐标映射到复平面 (-1.5 ~ 1.5)
            // z_re = (x - w/2) / (w/3) / zoom
            int z_re = ((x - offset_x) * 3 * 4096) / TEX_W * 4096 / zoom;
            int z_im = ((y - offset_y) * 3 * 4096) / TEX_H * 4096 / zoom;

            int i;
            // 迭代 16 次 (平衡画质与帧率)
            for (i = 0; i < 16; i++)
            {
                int z_re2 = (z_re * z_re) >> 12;
                int z_im2 = (z_im * z_im) >> 12;

                // 如果模大于 4 (也就是 re2 + im2 > 4.0)，则逃逸
                if (z_re2 + z_im2 > (4 << 12))
                    break;

                // Z = Z^2 + C
                // 新的虚部 = 2 * z_re * z_im + c_im
                int new_re = z_re2 - z_im2 + c_re;
                int new_im = ((z_re * z_im) >> 11) + c_im; // >>11 等价于 *2 >>12

                z_re = new_re;
                z_im = new_im;
            }

            // 写入像素
            *p_pixel++ = map_color_fire(i * 2);
        }
    }

    /*
     * === CRITICAL: Cache Coherency ===
     * CPU 刚刚写完了数据，它们现在还在 D-Cache 里。
     * GE 是通过 DMA 读内存的，DMA 不经过 Cache。
     * 必须强制将 Cache 数据刷入 DRAM。
     */
    aicos_dcache_clean_range((void *)g_tex_vir_addr, TEX_SIZE);

    /*
     * === PHASE 2: GE 硬件缩放 (Stretch Blit) ===
     * 将 320x240 的纹理放大铺满 640x480 的屏幕
     */
    struct ge_bitblt blt = {0};

    // 源：Texture
    blt.src_buf.buf_type    = MPP_PHY_ADDR;
    blt.src_buf.phy_addr[0] = g_tex_phy_addr; // 物理地址
    blt.src_buf.stride[0]   = TEX_W * 2;      // RGB565 stride
    blt.src_buf.size.width  = TEX_W;
    blt.src_buf.size.height = TEX_H;
    blt.src_buf.format      = MPP_FMT_RGB_565;
    blt.src_buf.crop_en     = 0; // 源不裁剪，全图使用

    // 目标：Screen Framebuffer
    blt.dst_buf.buf_type    = MPP_PHY_ADDR;
    blt.dst_buf.phy_addr[0] = phy_addr;
    blt.dst_buf.stride[0]   = ctx->info.stride;
    blt.dst_buf.size.width  = ctx->info.width;
    blt.dst_buf.size.height = ctx->info.height;
    blt.dst_buf.format      = ctx->info.format; // 保持屏幕原始格式 (RGB888)

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
        // 如果这里报错，通常是因为前面的 mpp_ge_sync 没处理好，或者物理地址非法
        rt_kprintf("GE Error: %d. Check mem align!\n", ret);
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
