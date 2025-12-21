/*
 * Filename: 0016_feedback_tunnel.c
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
 *
 * Hardware Feature:
 * 1. Ping-Pong Buffering (双缓冲反馈) - 解决自读写竞争产生的画面撕裂
 * 2. CPU-Side Lookup Table (LUT) - 预计算反向映射坐标，实现扭曲反馈
 * 3. GE Scaler (硬件缩放) - 将低分反馈纹理放大至全屏
 */

#include "demo_engine.h"
#include "mpp_mem.h"
#include "aic_hal_ge.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

/* --- Configuration Parameters --- */

/* 纹理规格 */
#define TEX_WIDTH  DEMO_QVGA_W
#define TEX_HEIGHT DEMO_QVGA_H
#define TEX_FMT    MPP_FMT_RGB_565
#define TEX_BPP    2
#define TEX_SIZE   (TEX_WIDTH * TEX_HEIGHT * TEX_BPP)

/* 反馈参数 */
#define ZOOM_FACTOR 0.96f // 缩放衰减率 (<1.0 向内吸入)
#define ROT_ANGLE   0.02f // 旋转角度 (弧度)

/* 颜色衰减参数 (RGB565) */
#define DECAY_MASK_R 0xF800
#define DECAY_STEP_R 0x0800
#define DECAY_MASK_G 0x07E0
#define DECAY_STEP_G 0x0020
#define DECAY_MASK_B 0x001F
#define DECAY_STEP_B 0x0001

/* 动画参数 */
#define CURSOR_SIZE 8   // 光标半径
#define COLOR_CYCLE 512 // 颜色循环周期
#define SPEED_LISA  3   // 李萨如运动速度

/* --- Global State --- */

/* 乒乓缓冲区 */
static unsigned int g_tex_phy[2] = {0, 0};
static uint16_t    *g_tex_vir[2] = {NULL, NULL};
static int          g_buf_idx    = 0; // 当前作为"Source"(上一帧)的索引

static int g_tick = 0;

/* 坐标查找表 (反向映射) */
static uint32_t *g_feedback_lut = NULL;

/* 正弦表 (Q12) */
static int sin_lut[512];

/* --- Implementation --- */

static int effect_init(struct demo_ctx *ctx)
{
    // 1. 分配两个 CMA 纹理缓冲区
    for (int i = 0; i < 2; i++)
    {
        g_tex_phy[i] = mpp_phy_alloc(DEMO_ALIGN_SIZE(TEX_SIZE));
        if (g_tex_phy[i] == 0)
        {
            if (i == 1)
                mpp_phy_free(g_tex_phy[0]);
            LOG_E("Night 16: CMA Alloc Failed.");
            return -1;
        }
        g_tex_vir[i] = (uint16_t *)(unsigned long)g_tex_phy[i];
        memset(g_tex_vir[i], 0, TEX_SIZE);
    }

    // 2. LUT 内存
    g_feedback_lut = (uint32_t *)rt_malloc(TEX_WIDTH * TEX_HEIGHT * sizeof(uint32_t));
    if (!g_feedback_lut)
    {
        LOG_E("Night 16: LUT Alloc Failed.");
        mpp_phy_free(g_tex_phy[0]);
        mpp_phy_free(g_tex_phy[1]);
        return -1;
    }

    // 3. 初始化正弦表
    for (int i = 0; i < 512; i++)
    {
        sin_lut[i] = (int)(sinf(i * PI / 256.0f) * Q12_ONE);
    }

    // 4. 预计算反馈映射 (Tunnel/Zoom Map)
    int       cx    = TEX_WIDTH / 2;
    int       cy    = TEX_HEIGHT / 2;
    float     cos_a = cosf(ROT_ANGLE);
    float     sin_a = sinf(ROT_ANGLE);
    uint32_t *p_lut = g_feedback_lut;

    for (int y = 0; y < TEX_HEIGHT; y++)
    {
        for (int x = 0; x < TEX_WIDTH; x++)
        {
            float dx = (float)(x - cx);
            float dy = (float)(y - cy);

            // 逆向变换：找出当前点(x,y)的内容应该取自上一帧的哪个位置
            // 因为我们要图像向中心缩小，所以我们要去取“更远处”的像素
            float src_x_f = dx / ZOOM_FACTOR;
            float src_y_f = dy / ZOOM_FACTOR;

            // 旋转
            float rx = src_x_f * cos_a - src_y_f * sin_a;
            float ry = src_x_f * sin_a + src_y_f * cos_a;

            int src_x = (int)(rx + cx);
            int src_y = (int)(ry + cy);

            // 边界处理：Clamp 到边缘
            src_x = CLAMP(src_x, 0, TEX_WIDTH - 1);
            src_y = CLAMP(src_y, 0, TEX_HEIGHT - 1);

            *p_lut++ = src_y * TEX_WIDTH + src_x;
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
     * 从 src 读取，写入 dst。
     */
    int count = TEX_WIDTH * TEX_HEIGHT;

    while (count--)
    {
        // 查表获取源位置
        uint32_t offset = *lut++;

        // 读取旧像素
        uint16_t color = src_pixels[offset];

        // 衰减 (Dimming)
        // 使用掩码判断和减法，比浮点乘法快得多
        if (color != 0)
        {
            if ((color & DECAY_MASK_B) > 0)
                color -= DECAY_STEP_B; // Blue decay
            if ((color & DECAY_MASK_G) > DECAY_STEP_G)
                color -= DECAY_STEP_G; // Green decay
            if ((color & DECAY_MASK_R) > DECAY_STEP_R)
                color -= DECAY_STEP_R; // Red decay
        }

        *dst_pixels++ = color;
    }

    /*
     * === PHASE 2: Draw New Pattern ===
     * 在 dst 上绘制新的光源
     */
    int t  = g_tick * SPEED_LISA;
    int cx = TEX_WIDTH / 2;
    int cy = TEX_HEIGHT / 2;

    // 李萨如轨迹
    int x = cx + ((GET_SIN(t) * 100) >> Q12_SHIFT);
    int y = cy + ((GET_COS(t * 2) * 80) >> Q12_SHIFT);

    // 颜色循环
    uint16_t draw_color;
    int      hue = g_tick % COLOR_CYCLE;

    // 三色循环
    if (hue < (COLOR_CYCLE / 3))
        draw_color = RGB2RGB565(255, 0, 0); // Red
    else if (hue < (COLOR_CYCLE * 2 / 3))
        draw_color = RGB2RGB565(0, 255, 0); // Green
    else
        draw_color = RGB2RGB565(0, 0, 255); // Blue

    // 绘制主光标 (Box Drawing)
    int size = CURSOR_SIZE;
    for (int dy = -size; dy <= size; dy++)
    {
        for (int dx = -size; dx <= size; dx++)
        {
            if (abs(dx) > 3 && abs(dy) > 3)
                continue; // 十字形状

            int px = x + dx;
            int py = y + dy;

            if (px >= 0 && px < TEX_WIDTH && py >= 0 && py < TEX_HEIGHT)
            {
                // 饱和加法：这里简单覆盖为高亮白
                g_tex_vir[dst_idx][py * TEX_WIDTH + px] = 0xFFFF;
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

            if (px >= 0 && px < TEX_WIDTH && py >= 0 && py < TEX_HEIGHT)
            {
                g_tex_vir[dst_idx][py * TEX_WIDTH + px] = draw_color;
            }
        }
    }

    /* === CRITICAL: Cache Flush === */
    aicos_dcache_clean_range((void *)g_tex_vir[dst_idx], TEX_SIZE);

    /* === PHASE 3: GE Scaling === */
    struct ge_bitblt blt = {0};

    // Source: dst buffer
    blt.src_buf.buf_type    = MPP_PHY_ADDR;
    blt.src_buf.phy_addr[0] = g_tex_phy[dst_idx];
    blt.src_buf.stride[0]   = TEX_WIDTH * TEX_BPP;
    blt.src_buf.size.width  = TEX_WIDTH;
    blt.src_buf.size.height = TEX_HEIGHT;
    blt.src_buf.format      = TEX_FMT;
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
    blt.ctrl.alpha_en = 1; // Disable Blending

    mpp_ge_bitblt(ctx->ge, &blt);
    mpp_ge_emit(ctx->ge);
    mpp_ge_sync(ctx->ge);

    // 交换 Buffer 索引
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
