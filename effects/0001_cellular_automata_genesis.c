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
 *
 * Hardware Feature:
 * 1. GE Scaler (硬件双线性插值缩放) - 将 QVGA 逻辑场放大至全屏
 * 2. CMA & Cache (连续物理内存与缓存一致性) - 确保 CPU 写入被 GE 正确读取
 */

#include "demo_engine.h"
#include "mpp_mem.h"
#include "aic_hal_ge.h"
#include <math.h>

/* --- Configuration Parameters --- */

/* 纹理规格：QVGA 逻辑场 */
#define TEX_WIDTH  DEMO_QVGA_W
#define TEX_HEIGHT DEMO_QVGA_H
#define TEX_FMT    MPP_FMT_RGB_565
#define TEX_BPP    2
#define TEX_SIZE   (TEX_WIDTH * TEX_HEIGHT * TEX_BPP)

/* 调色板参数 */
#define PALETTE_SIZE 256

/* 动画参数：呼吸与扰动 */
#define ZOOM_BASE     32 // 基础缩放分母
#define ZOOM_RANGE    63 // 呼吸幅度掩码 (g_tick & 63)
#define COORD_SHIFT   6  // 坐标定点位移量 (x << 6)
#define DISTORT_SHIFT 11 // 扰动因子位移量

/* --- Global State --- */

static unsigned int g_tex_phy_addr = 0;
static uint16_t    *g_tex_vir_addr = NULL;
static int          g_tick         = 0;

/* 霓虹调色板 (Neon Palette) */
static uint16_t g_palette[PALETTE_SIZE];

/* --- Implementation --- */

static int effect_init(struct demo_ctx *ctx)
{
    // 1. 申请 CMA 显存 (必须物理连续)
    // 使用 DEMO_ALIGN_SIZE 确保内存大小对齐 Cache Line，符合 SPEC 规范
    g_tex_phy_addr = mpp_phy_alloc(DEMO_ALIGN_SIZE(TEX_SIZE));
    if (g_tex_phy_addr == 0)
    {
        LOG_E("Night 1: CMA alloc failed! Universe collapsed.");
        return -1;
    }

    // 获取虚拟地址指针
    g_tex_vir_addr = (uint16_t *)(unsigned long)g_tex_phy_addr;

    // 2. 生成调色板 (赛博朋克风格)
    for (int i = 0; i < PALETTE_SIZE; i++)
    {
        // R: 周期较快，产生紫红色调
        int r = (int)(128 + 127 * sin(i * 0.1f));
        // G: 周期较慢，产生青色调
        int g = (int)(128 + 127 * sin(i * 0.07f + 2.0f));
        // B: 保持高亮
        int b = (int)(128 + 127 * cos(i * 0.05f));

        // 使用工具宏转换为 RGB565
        g_palette[i] = RGB2RGB565(r, g, b);
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
    int zoom = ZOOM_BASE + (t & ZOOM_RANGE);

    for (int y = 0; y < TEX_HEIGHT; y++)
    {
        // 预计算 Y 轴缩放，减少内层循环计算量
        // (y * 64) / zoom
        int zy   = (y << COORD_SHIFT) / zoom;
        int zy_t = zy + t;

        for (int x = 0; x < TEX_WIDTH; x++)
        {
            int zx = (x << COORD_SHIFT) / zoom;

            // 核心公式：缩放后的 XOR 纹理
            int val = ((zx ^ zy) + t) ^ zy_t;

            // 引入扰动
            val = (val & 0xFF) + ((x * y) >> DISTORT_SHIFT);

            // 查表上色
            *p++ = g_palette[val & 0xFF];
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
    blt.src_buf.stride[0]   = TEX_WIDTH * TEX_BPP;
    blt.src_buf.size.width  = TEX_WIDTH;
    blt.src_buf.size.height = TEX_HEIGHT;
    blt.src_buf.format      = TEX_FMT;
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
        LOG_E("GE Error: %d", ret);
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
