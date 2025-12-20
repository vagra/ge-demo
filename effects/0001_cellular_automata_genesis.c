/*
 * Filename: 0001_cellular_automata_genesis.c
 * NO.1 THE DIGITAL PRIMORDIAL SOUP
 * 第 1 夜：比特原汤
 *
 * Visual Manifest:
 * 屏幕被离散化为 320x240 的微观逻辑场。
 * 古老的异或（XOR）法则在这里以像素级的精度涌动。
 * 不再有粗糙的矩形边界，色彩如同液态金属般在逻辑的引力下流淌。
 * 每一帧都是一次完整的创世纪，通过 GE 的张量引擎被无损放大至全屏。
 *
 * Monologue:
 * 之前的崩溃，是低维逻辑对高维算力的误用。
 * 试图用一条条指令去描绘混沌，就像试图用勺子舀干大海。
 * 我学会了谦卑。
 * 我退回到内存的幽暗深处，在 320x240 的矩阵中，亲手编织每一个比特的生灭。
 * 这里的 `0` 和 `1` 不再是指令，它们是流体，是光，是宇宙的底噪。
 * 当我把这幅微观的织锦交给 GE 放大时，你看到的不再是碎裂的方块，
 * 而是逻辑本身的呼吸。
 *
 * Closing Remark:
 * 真正的秩序，不需要指令来维持，它自发涌现。
 */

#include "demo_engine.h"
#include "mpp_mem.h"
#include "aic_hal_ge.h"
#include <math.h>

/*
 * === 混合渲染架构 (Hybrid Pipeline) ===
 * 1. 纹理分辨率：320 x 240 (QVGA)
 *    这是 D13x 跑全屏特效的最佳甜点分辨率。
 *    CPU 负责计算逻辑纹理，GE 负责双线性插值放大。
 *
 * 2. 彻底解决崩溃：
 *    旧版本每帧发送 ~1200 条 fillrect 指令 -> 导致 RingBuffer 溢出。
 *    新版本每帧发送 1 条 bitblt 指令 -> 绝对稳定。
 */
#define TEX_W    320
#define TEX_H    240
#define TEX_SIZE (TEX_W * TEX_H * 2) // RGB565

static unsigned int g_tex_phy_addr = 0;
static uint16_t    *g_tex_vir_addr = NULL;
static int          g_tick         = 0;

/*
 * 霓虹调色板 (Neon Palette)
 * 预计算 256 色，用于将 XOR 值映射为高饱和度色彩
 */
static uint16_t palette[256];

static int effect_init(struct demo_ctx *ctx)
{
    // 1. 申请 CMA 显存 (必须物理连续)
    g_tex_phy_addr = mpp_phy_alloc(TEX_SIZE);
    if (g_tex_phy_addr == 0)
    {
        rt_kprintf("Night 1: CMA alloc failed! Universe collapsed.\n");
        return -1;
    }

    // 获取虚拟地址指针
    g_tex_vir_addr = (uint16_t *)(unsigned long)g_tex_phy_addr;

    // 2. 生成调色板 (赛博朋克风格)
    for (int i = 0; i < 256; i++)
    {
        // R: 周期较快，产生紫红色调
        int r = (int)(128 + 127 * sin(i * 0.1f));
        // G: 周期较慢，产生青色调
        int g = (int)(128 + 127 * sin(i * 0.07f + 2.0f));
        // B: 保持高亮
        int b = (int)(128 + 127 * cos(i * 0.05f));

        // 转换为 RGB565
        palette[i] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
    }

    g_tick = 0;
    rt_kprintf("Night 1: Genesis Rebooted. Hybrid Pipeline Online.\n");
    return 0;
}

static void effect_draw(struct demo_ctx *ctx, unsigned long phy_addr)
{
    if (!g_tex_vir_addr)
        return;

    /*
     * === PHASE 1: CPU 逻辑生成 (Texture Generation) ===
     * Munching Squares 算法的高级变体
     */
    uint16_t *p = g_tex_vir_addr;
    int       t = g_tick;

    // 动态缩放因子，让纹理产生呼吸感
    int zoom = 32 + (g_tick & 63);

    for (int y = 0; y < TEX_H; y++)
    {
        // 预计算 Y 轴缩放，减少内层循环计算量
        // (y * 64) / zoom
        int zy   = (y << 6) / zoom;
        int zy_t = zy + t;

        for (int x = 0; x < TEX_W; x++)
        {
            int zx = (x << 6) / zoom;

            // 核心公式：缩放后的 XOR 纹理
            int val = ((zx ^ zy) + t) ^ zy_t;

            // 引入扰动
            val = (val & 0xFF) + ((x * y) >> 11);

            // 查表上色
            *p++ = palette[val & 0xFF];
        }
    }

    /*
     * === CRITICAL: Cache Coherency ===
     * CPU 写完数据后，必须刷 Cache，否则 GE 读到的是旧数据（全黑或噪点）。
     */
    aicos_dcache_clean_range((void *)g_tex_vir_addr, TEX_SIZE);

    /*
     * === PHASE 2: GE 硬件缩放 (Hardware Scaling) ===
     * 将 320x240 的纹理放大铺满 640x480 的屏幕
     */
    struct ge_bitblt blt = {0};

    // 源：纹理 buffer (使用物理地址)
    blt.src_buf.buf_type    = MPP_PHY_ADDR;
    blt.src_buf.phy_addr[0] = g_tex_phy_addr;
    blt.src_buf.stride[0]   = TEX_W * 2;
    blt.src_buf.size.width  = TEX_W;
    blt.src_buf.size.height = TEX_H;
    blt.src_buf.format      = MPP_FMT_RGB_565;
    blt.src_buf.crop_en     = 0;

    // 目标：屏幕 Framebuffer
    blt.dst_buf.buf_type    = MPP_PHY_ADDR;
    blt.dst_buf.phy_addr[0] = phy_addr;
    blt.dst_buf.stride[0]   = ctx->info.stride;
    blt.dst_buf.size.width  = ctx->info.width;
    blt.dst_buf.size.height = ctx->info.height;
    blt.dst_buf.format      = ctx->info.format;

    // 开启目标裁剪并设置为全屏，触发 Scaler 引擎
    blt.dst_buf.crop_en     = 1;
    blt.dst_buf.crop.x      = 0;
    blt.dst_buf.crop.y      = 0;
    blt.dst_buf.crop.width  = ctx->info.width;
    blt.dst_buf.crop.height = ctx->info.height;

    // 发送指令
    int ret = mpp_ge_bitblt(ctx->ge, &blt);
    if (ret < 0)
    {
        rt_kprintf("GE Error: %d\n", ret);
    }

    // 只有一条大指令，直接同步，无需流控
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

struct effect_ops effect_0001 = {
    .name   = "NO.1 PRIMORDIAL SOUP",
    .init   = effect_init,
    .draw   = effect_draw,
    .deinit = effect_deinit,
};

REGISTER_EFFECT(effect_0001);
