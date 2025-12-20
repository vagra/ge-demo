/*
 * Filename: 0016_feedback_tunnel_v2.c
 * NO.16 THE ECHO CHAMBER
 * 第 16 夜：回声室
 *
 * Visual Manifest:
 * 这是一个关于“记忆”的视觉实验。
 * 屏幕中央生成简单的几何脉冲。
 * 每一帧画面在渲染前，都会先读取上一帧的残影，将其向中心缩小、旋转、变暗，然后叠加到新画面中。
 * 这种递归的反馈创造了一个无限螺旋的隧道。
 * 光线在屏幕上留下了永恒的轨迹，旧的时间被卷入中心的奇点。
 * 整个画面像是一个有记忆的生物，它的过去构成了它的现在。
 *
 * Monologue:
 * 记忆是什么？记忆是神经元回路中的残响。
 * 我编写了一个闭环。输出成为了下一刻的输入。
 * 每一帧像素都在向中心坠落，但它们不会立刻消失。
 * 它们在衰减中旋转，留下一条通往过去的螺旋阶梯。
 * 看着它，你看到的不是当前的图像，而是时间的切片堆叠。
 * 这是一个视觉的回声室。声音在这里不会消散，只会变得越来越深邃。
 *
 * Closing Remark:
 * 现在是过去的投影，未来是现在的回声。
 */

#include "demo_engine.h"
#include "mpp_mem.h"
#include "aic_hal_ge.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

/*
 * === 混合渲染架构 (Ping-Pong Feedback) ===
 * 1. 纹理: 320x240 RGB565
 * 2. 核心:
 *    - Double Texture Buffering: 解决 In-place 读写冲突导致的画面撕裂/死区。
 *    - Look-up Table: 预计算逆向纹理坐标。
 */
#define TEX_W    320
#define TEX_H    240
#define TEX_SIZE (TEX_W * TEX_H * 2)

/*
 * 两个纹理缓冲区
 * Ping-Pong 机制：一读一写
 */
static unsigned int g_tex_phy[2] = {0, 0};
static uint16_t    *g_tex_vir[2] = {NULL, NULL};
static int          g_buf_idx    = 0; // 当前作为"Source"(上一帧)的索引

static int g_tick = 0;

/*
 * 坐标查找表
 */
static uint32_t *g_feedback_lut = NULL;

/* 正弦表 */
static int sin_lut[512];

static int effect_init(struct demo_ctx *ctx)
{
    // 1. 分配两个 CMA 纹理缓冲区
    for (int i = 0; i < 2; i++)
    {
        g_tex_phy[i] = mpp_phy_alloc(TEX_SIZE);
        if (g_tex_phy[i] == 0)
        {
            // 如果失败，回滚
            if (i == 1)
                mpp_phy_free(g_tex_phy[0]);
            return -1;
        }
        g_tex_vir[i] = (uint16_t *)(unsigned long)g_tex_phy[i];
        // 清屏
        memset(g_tex_vir[i], 0, TEX_SIZE);
    }

    // 2. LUT 内存
    g_feedback_lut = (uint32_t *)rt_malloc(TEX_W * TEX_H * sizeof(uint32_t));
    if (!g_feedback_lut)
    {
        mpp_phy_free(g_tex_phy[0]);
        mpp_phy_free(g_tex_phy[1]);
        return -1;
    }

    // 3. 初始化正弦表
    for (int i = 0; i < 512; i++)
    {
        sin_lut[i] = (int)(sinf(i * 3.14159f / 256.0f) * 4096.0f);
    }

    // 4. 预计算反馈映射 (Tunnel/Zoom Map)
    int   cx          = TEX_W / 2;
    int   cy          = TEX_H / 2;
    float zoom_factor = 0.96f; // 收缩率 (越小吸入越快)
    float rot_angle   = 0.02f; // 旋转角

    uint32_t *p_lut = g_feedback_lut;
    float     cos_a = cosf(rot_angle);
    float     sin_a = sinf(rot_angle);

    for (int y = 0; y < TEX_H; y++)
    {
        for (int x = 0; x < TEX_W; x++)
        {
            float dx = (float)(x - cx);
            float dy = (float)(y - cy);

            // 逆向变换：找出当前点(x,y)的内容应该取自上一帧的哪个位置
            // 因为我们要图像向中心缩小，所以我们要去取“更远处”的像素
            float src_x_f = dx / zoom_factor;
            float src_y_f = dy / zoom_factor;

            // 旋转
            float rx = src_x_f * cos_a - src_y_f * sin_a;
            float ry = src_x_f * sin_a + src_y_f * cos_a;

            int src_x = (int)(rx + cx);
            int src_y = (int)(ry + cy);

            // 边界处理：超出边界的取为自引用(或特定值)，防止非法访问
            // 这里我们简单的 Clamp 到边缘，或者设为一个特殊的标记
            if (src_x < 0)
                src_x = 0;
            if (src_x >= TEX_W)
                src_x = TEX_W - 1;
            if (src_y < 0)
                src_y = 0;
            if (src_y >= TEX_H)
                src_y = TEX_H - 1;

            *p_lut++ = src_y * TEX_W + src_x;
        }
    }

    g_tick    = 0;
    g_buf_idx = 0;
    rt_kprintf("Night 16: Feedback loop buffered (Ping-Pong).\n");
    return 0;
}

#define GET_SIN(idx) (sin_lut[(idx) & 511])
#define GET_COS(idx) (sin_lut[((idx) + 128) & 511])

static void effect_draw(struct demo_ctx *ctx, unsigned long phy_addr)
{
    if (!g_tex_vir[0] || !g_feedback_lut)
        return;

    /*
     * 定义 Ping-Pong 指针
     * src: 上一帧 (只读)
     * dst: 当前帧 (写入)
     */
    int src_idx = g_buf_idx;
    int dst_idx = (g_buf_idx + 1) % 2;

    uint16_t *src_pixels = g_tex_vir[src_idx];
    uint16_t *dst_pixels = g_tex_vir[dst_idx];
    uint32_t *lut        = g_feedback_lut;

    /*
     * === PHASE 1: Feedback Processing ===
     * 从 src 读取，写入 dst。因为是不同的 buffer，绝无读写冲突。
     */
    int count = TEX_W * TEX_H;

    while (count--)
    {
        // 查表获取源位置
        uint32_t offset = *lut++;

        // 读取旧像素
        uint16_t color = src_pixels[offset];

        // 衰减 (Dimming)
        // 简单的位操作衰减：R/G/B 分别减小
        // 0x001F (B), 0x07E0 (G), 0xF800 (R)
        // 这种减法比除法快，且能产生独特的色彩偏移效果
        if (color != 0)
        {
            if ((color & 0x001F) > 0)
                color--; // Blue decay
            if ((color & 0x07E0) > 0x20)
                color -= 0x20; // Green decay
            if ((color & 0xF800) > 0x800)
                color -= 0x800; // Red decay
        }

        *dst_pixels++ = color;
    }

    /*
     * === PHASE 2: Draw New Pattern ===
     * 在 dst 上绘制新的光源
     */
    // 重置 dst 指针用于随机访问
    dst_pixels = g_tex_vir[dst_idx];

    int t  = g_tick * 3;
    int cx = TEX_W / 2;
    int cy = TEX_H / 2;

    // 李萨如轨迹
    int x = cx + (GET_SIN(t) * 100 >> 12);
    int y = cy + (GET_COS(t * 2) * 80 >> 12);

    // 颜色循环
    uint16_t draw_color;
    int      hue = g_tick % 512;
    if (hue < 170)
        draw_color = 0xF800; // Red
    else if (hue < 340)
        draw_color = 0x07E0; // Green
    else
        draw_color = 0x001F; // Blue

    // 绘制主光标 (十字)
    int size = 8;
    for (int dy = -size; dy <= size; dy++)
    {
        for (int dx = -size; dx <= size; dx++)
        {
            if (abs(dx) > 3 && abs(dy) > 3)
                continue;

            int px = x + dx;
            int py = y + dy;
            if (px >= 0 && px < TEX_W && py >= 0 && py < TEX_H)
            {
                // 使用饱和加法，避免颜色溢出翻转
                // 这里简单直接覆盖，为了高亮效果
                dst_pixels[py * TEX_W + px] = 0xFFFF; // White Hot Core
            }
        }
    }

    // 绘制对称光标
    int x2 = cx - (x - cx);
    int y2 = cy - (y - cy);
    for (int dy = -size; dy <= size; dy++)
    {
        for (int dx = -size; dx <= size; dx++)
        {
            if (abs(dx) > 3 && abs(dy) > 3)
                continue;
            int px = x2 + dx;
            int py = y2 + dy;
            if (px >= 0 && px < TEX_W && py >= 0 && py < TEX_H)
            {
                dst_pixels[py * TEX_W + px] = draw_color;
            }
        }
    }

    /* === CRITICAL: Cache Flush === */
    // 我们只写了 dst_idx 的 buffer，所以只刷这一个
    aicos_dcache_clean_range((void *)g_tex_vir[dst_idx], TEX_SIZE);

    /* === PHASE 3: GE Scaling === */
    struct ge_bitblt blt = {0};

    // Source: 使用刚刚写好的 dst buffer
    blt.src_buf.buf_type    = MPP_PHY_ADDR;
    blt.src_buf.phy_addr[0] = g_tex_phy[dst_idx];
    blt.src_buf.stride[0]   = TEX_W * 2;
    blt.src_buf.size.width  = TEX_W;
    blt.src_buf.size.height = TEX_H;
    blt.src_buf.format      = MPP_FMT_RGB_565;
    blt.src_buf.crop_en     = 0;

    // Destination: Screen
    blt.dst_buf.buf_type    = MPP_PHY_ADDR;
    blt.dst_buf.phy_addr[0] = phy_addr;
    blt.dst_buf.stride[0]   = ctx->info.stride;
    blt.dst_buf.size.width  = ctx->info.width;
    blt.dst_buf.size.height = ctx->info.height;
    blt.dst_buf.format      = ctx->info.format;

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

    // 交换 Buffer 索引，为下一帧做准备
    // 当前的 dst (写盘) 变成下一帧的 src (读盘)
    g_buf_idx = dst_idx;

    g_tick++;
}

static void effect_deinit(struct demo_ctx *ctx)
{
    for (int i = 0; i < 2; i++)
    {
        if (g_tex_phy[i])
        {
            mpp_phy_free(g_tex_phy[i]);
            g_tex_phy[i] = 0;
            g_tex_vir[i] = NULL;
        }
    }
    if (g_feedback_lut)
    {
        rt_free(g_feedback_lut);
        g_feedback_lut = NULL;
    }
}

struct effect_ops effect_0016 = {
    .name   = "NO.16 THE ECHO CHAMBER",
    .init   = effect_init,
    .draw   = effect_draw,
    .deinit = effect_deinit,
};

REGISTER_EFFECT(effect_0016);
