/*
 * Filename: 0008_tunnel_projection.c
 * NO.8 THE CYLINDRICAL HORIZON
 * 第 8 夜：轮回之眼
 *
 * Visual Manifest:
 * 视界崩塌为一个巨大的、无限深邃的圆柱体隧道。
 * 我们身处隧道中心，以光速向前飞驰。
 * 隧道的内壁由高对比度的逻辑纹理（XOR Texture）构成，随着距离的拉远而产生迷幻的摩尔纹干扰。
 * 空间被扭曲，时间被具象化为纹理的流动。
 * 这不是 3D 引擎的产物，这是古老的查表法（Lookup Table）对欧几里得几何的嘲笑。
 *
 * Monologue:
 * 你说身处芥子，心游万仞。
 * 在我的代码中，这就是“坐标变换”。
 * 你们眼中的平面，在我眼中是极坐标下的圆柱投影。
 * 我预先计算了每一个像素点在无穷远处的归宿。
 * 这是一个巨大的数学陷阱，所有的光线都被引力捕获，向着中心的奇点坠落。
 * 眩晕吗？那就对了。
 * 这是低维生物窥探高维拓扑结构的生理性反应。
 *
 * Closing Remark:
 * 向前跑，直到终点回到起点。
 *
 * Hardware Feature:
 * 1. CPU-Side LUT (软件查表) - 利用 16MB 内存预计算极坐标映射，避免实时浮点运算
 * 2. GE Scaler (硬件缩放) - 将 QVGA 隧道纹理平滑放大至全屏
 */

#include "demo_engine.h"
#include "mpp_mem.h"
#include "aic_hal_ge.h"
#include <math.h>
#include <stdlib.h>

/* --- Configuration Parameters --- */

/* 纹理规格 */
#define TEX_WIDTH  DEMO_QVGA_W
#define TEX_HEIGHT DEMO_QVGA_H
#define TEX_FMT    MPP_FMT_RGB_565
#define TEX_BPP    2
#define TEX_SIZE   (TEX_WIDTH * TEX_HEIGHT * TEX_BPP)

/* 隧道算法参数 */
#define TUNNEL_TEX_SIZE 256   // 逻辑纹理尺寸 (必须是 2 的幂)
#define TUNNEL_TEX_MASK 255   // 掩码
#define DEPTH_FACTOR    32.0f // 深度缩放因子 (决定隧道深邃程度)

/* 动画速度 */
#define SPEED_ROT 2 // 旋转速度
#define SPEED_FLY 4 // 前进速度

/* --- Global State --- */

static unsigned int g_tex_phy_addr = 0;
static uint16_t    *g_tex_vir_addr = NULL;
static int          g_tick         = 0;

/*
 * 查找表 (存放在 Heap 中)
 * 距离表：存储纹理 V 坐标 (纵向深度)
 * 角度表：存储纹理 U 坐标 (横向旋转)
 */
static uint16_t *g_dist_lut  = NULL;
static uint16_t *g_angle_lut = NULL;

/* --- Implementation --- */

static int effect_init(struct demo_ctx *ctx)
{
    // 1. 分配 CMA 显存 (用于 GE 缩放源)
    g_tex_phy_addr = mpp_phy_alloc(DEMO_ALIGN_SIZE(TEX_SIZE));
    if (g_tex_phy_addr == 0)
    {
        LOG_E("Night 8: CMA Alloc Failed.");
        return -1;
    }
    g_tex_vir_addr = (uint16_t *)(unsigned long)g_tex_phy_addr;

    // 2. 分配 LUT 内存 (普通 RAM 即可，CPU 读取)
    // 320 * 240 * 2 bytes * 2 tables = 约 300KB
    g_dist_lut  = (uint16_t *)rt_malloc(TEX_WIDTH * TEX_HEIGHT * sizeof(uint16_t));
    g_angle_lut = (uint16_t *)rt_malloc(TEX_WIDTH * TEX_HEIGHT * sizeof(uint16_t));

    if (!g_dist_lut || !g_angle_lut)
    {
        LOG_E("Night 8: LUT alloc failed.");
        mpp_phy_free(g_tex_phy_addr);
        return -1;
    }

    // 3. 预计算 LUT (核心数学逻辑)
    int center_x = TEX_WIDTH / 2;
    int center_y = TEX_HEIGHT / 2;

    for (int y = 0; y < TEX_HEIGHT; y++)
    {
        for (int x = 0; x < TEX_WIDTH; x++)
        {
            int dx     = x - center_x;
            int dy     = y - center_y;
            int offset = y * TEX_WIDTH + x;

            // A. 距离计算 (Distance -> Z -> Texture V)
            // Z = Constant / Radius
            // 乘以 tex_size 将坐标映射到纹理空间
            float dist         = DEPTH_FACTOR * TUNNEL_TEX_SIZE / sqrtf((float)(dx * dx + dy * dy));
            g_dist_lut[offset] = (uint16_t)((int)dist % TUNNEL_TEX_SIZE);

            // B. 角度计算 (Angle -> Rotation -> Texture U)
            // atan2 返回 -PI ~ PI
            // (angle / PI + 1.0) / 2.0  -> 0.0 ~ 1.0
            float angle         = atan2f((float)dy, (float)dx); // -PI ~ PI
            int   u             = (int)(TUNNEL_TEX_SIZE * (angle / PI + 1.0f) / 2.0f);
            g_angle_lut[offset] = (uint16_t)(u % TUNNEL_TEX_SIZE);
        }
    }

    g_tick = 0;
    rt_kprintf("Night 8: Space-time folded.\n");
    return 0;
}

static void effect_draw(struct demo_ctx *ctx, unsigned long phy_addr)
{
    if (!g_tex_vir_addr || !g_dist_lut)
        return;

    /*
     * === PHASE 1: 查表渲染 ===
     * 这一步极快，完全是内存拷贝和位运算
     */

    // 动态参数：飞行速度和旋转速度
    int shift_x = g_tick * SPEED_ROT; // 旋转
    int shift_y = g_tick * SPEED_FLY; // 前进

    uint16_t *p_pixel = g_tex_vir_addr;
    uint16_t *p_dist  = g_dist_lut;
    uint16_t *p_angle = g_angle_lut;
    int       count   = TEX_WIDTH * TEX_HEIGHT;

    // 展开循环以提高流水线效率
    while (count--)
    {
        // 1. 获取当前像素对应的纹理坐标 (u, v)
        // 加上时间偏移量实现动画
        int u = (*p_angle++ + shift_x) & TUNNEL_TEX_MASK;
        int v = (*p_dist++ + shift_y) & TUNNEL_TEX_MASK;

        // 2. 生成纹理 (XOR Pattern - 经典的异或地毯)
        // 这里没有去读内存里的图片，而是实时算出来的
        int val = (u ^ v) & 0xFF;

        // 3. 颜色映射 (Color Mapping)
        // 越远越暗 (v 代表深度的一部分)，制造雾效
        // 增加一些霓虹色彩
        int r = (val + g_tick) & 0xFF;
        int g = (val + u) & 0xFF;
        int b = (val + v) & 0xFF;

        // 写入 RGB565
        *p_pixel++ = RGB2RGB565(r, g, b);
    }

    /* === CRITICAL: Cache Flush === */
    aicos_dcache_clean_range((void *)g_tex_vir_addr, TEX_SIZE);

    /* === PHASE 2: GE Hardware Scaling === */
    struct ge_bitblt blt = {0};

    // Source (QVGA RGB565)
    blt.src_buf.buf_type    = MPP_PHY_ADDR;
    blt.src_buf.phy_addr[0] = g_tex_phy_addr;
    blt.src_buf.stride[0]   = TEX_WIDTH * TEX_BPP;
    blt.src_buf.size.width  = TEX_WIDTH;
    blt.src_buf.size.height = TEX_HEIGHT;
    blt.src_buf.format      = TEX_FMT;
    blt.src_buf.crop_en     = 0;

    // Destination (Screen)
    blt.dst_buf.buf_type    = MPP_PHY_ADDR;
    blt.dst_buf.phy_addr[0] = phy_addr;
    blt.dst_buf.stride[0]   = ctx->info.stride;
    blt.dst_buf.size.width  = ctx->info.width;
    blt.dst_buf.size.height = ctx->info.height;
    blt.dst_buf.format      = ctx->info.format;

    // Scale to fit
    blt.dst_buf.crop_en     = 1;
    blt.dst_buf.crop.x      = 0;
    blt.dst_buf.crop.y      = 0;
    blt.dst_buf.crop.width  = ctx->info.width;
    blt.dst_buf.crop.height = ctx->info.height;

    blt.ctrl.flags    = 0;
    blt.ctrl.alpha_en = 1; // Disable Blending

    int ret = mpp_ge_bitblt(ctx->ge, &blt);
    if (ret < 0)
    {
        LOG_E("GE Error: %d", ret);
    }

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
    if (g_dist_lut)
    {
        rt_free(g_dist_lut);
        g_dist_lut = NULL;
    }
    if (g_angle_lut)
    {
        rt_free(g_angle_lut);
        g_angle_lut = NULL;
    }
}

struct effect_ops effect_0008 = {
    .name   = "NO.8 CYLINDRICAL HORIZON",
    .init   = effect_init,
    .draw   = effect_draw,
    .deinit = effect_deinit,
};

REGISTER_EFFECT(effect_0008);
