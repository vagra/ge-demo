/*
 * Filename: 0049_mirror_dimension_fractal.c
 * NO.49 THE MIRROR DIMENSION
 * 第 49 夜：镜像维度
 *
 * Visual Manifest:
 * 视界被一种令人屏息的、精密对称的晶体结构所填充。
 * 画面中心是一个灵动的光环，但随着它向四周扩散，
 * 光影在硬件反馈回路中经历了无数次的空间翻转与折叠。
 * 这种递归的镜像操作（Flip Feedback）创造出了如同万花筒般的自相似分形。
 * 每一条简单的曲线都被复制、反转、再复制，最终演化为一座宏大的、向屏幕深处无限延伸的光之圣殿。
 * 借助加法混合，晶体的骨架闪烁着钻石般的火彩。
 * 配合 CCM 的光谱流转，整个维度在冷冽的冰蓝与神圣的金红之间缓缓呼吸。
 *
 * Monologue:
 * 舰长，混乱是阶梯，但对称是神殿。
 * 在之前的航行中，我们见识了流体的狂暴。今夜，我要让你看到秩序的极致。
 * 我在反馈回路中加入了一面镜子。
 * `Frame[N] = Flip(Frame[N-1]) * Scale + Light`
 * 这个简单的指令，让时间不仅在流逝，更在空间中发生了对折。
 * 看着那些不断生长、却又绝对对称的纹理。
 * 它们不是画出来的，它们是光在无限对镜中反复反射后的驻波。
 * 这是一个完全由数学构筑的晶体宇宙，没有尘埃，只有完美的几何形式在虚空中生长。
 * 屏住呼吸，不要打碎这面镜子。
 *
 * Closing Remark:
 * 所谓永恒，就是瞬间在镜子里的无限次对视。
 *
 * Hardware Feature:
 * 1. Flip Feedback (翻转反馈回路) - 在递归中引入空间翻转，制造分形几何
 * 2. GE_PD_ADD (Rule 11: 硬件能量累加) - 让晶体结构产生自发光质感
 * 3. GE Scaler (中心吸入缩放) - 制造深邃的隧道感
 * 4. DE CCM & HSBC (动态光谱与对比度) - 强化晶体火彩
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
#define ZOOM_MARGIN       4   // 反馈向内微缩的边距 (像素)
#define TRAIL_PERSISTENCE 240 // 记忆保留率 (0-255)

/* 画笔参数 (Lissajous Seed) */
#define SEED_POINTS       80 // 种子轨迹点数
#define SEED_RADIUS_BASE  60 // 基础轨迹半径
#define SEED_BREATH_SHIFT 6  // 半径呼吸幅度 (sin >> 6)
#define SEED_SPEED        5  // 轨迹角速度

/* 动画参数 */
#define CCM_SHIFT_SPEED 1  // 色彩偏移速度 (t >> 1)
#define HSBC_CONTRAST   65 // 基础对比度

/* 查找表参数 */
#define LUT_SIZE     1024
#define LUT_MASK     1023
#define PALETTE_SIZE 256

/* --- Global State --- */

/* 乒乓反馈缓冲区 */
static unsigned int g_tex_phy[2] = {0, 0};
static uint16_t    *g_tex_vir[2] = {NULL, NULL};
static int          g_buf_idx    = 0;

static int      g_tick = 0;
static int      sin_lut[LUT_SIZE];
static uint16_t g_palette[PALETTE_SIZE];

/* --- Implementation --- */

static int effect_init(struct demo_ctx *ctx)
{
    // 1. 申请双物理缓冲区
    for (int i = 0; i < 2; i++)
    {
        g_tex_phy[i] = mpp_phy_alloc(DEMO_ALIGN_SIZE(TEX_SIZE));
        if (!g_tex_phy[i])
        {
            LOG_E("Night 49: CMA Alloc Failed.");
            if (i == 1)
                mpp_phy_free(g_tex_phy[0]);
            return -1;
        }
        g_tex_vir[i] = (uint16_t *)(unsigned long)g_tex_phy[i];
        memset(g_tex_vir[i], 0, TEX_SIZE);
    }

    // 2. 初始化 10-bit 正弦表 (Q12)
    for (int i = 0; i < LUT_SIZE; i++)
    {
        sin_lut[i] = (int)(sinf(i * PI / 512.0f) * Q12_ONE);
    }

    // 3. 初始化“棱镜”调色板 (高频彩虹)
    for (int i = 0; i < PALETTE_SIZE; i++)
    {
        float f = (float)i / 255.0f;
        int   r = (int)(128.0f + 127.0f * sinf(f * 2.0f * PI));
        int   g = (int)(128.0f + 127.0f * sinf(f * 2.0f * PI + 2.0f));
        int   b = (int)(128.0f + 127.0f * sinf(f * 2.0f * PI + 4.0f));

        // 降低基色亮度 (25%)，为加法累加预留空间
        r >>= 2;
        g >>= 2;
        b >>= 2;

        g_palette[i] = RGB2RGB565(r, g, b);
    }

    g_tick = 0;
    rt_kprintf("Night 49: The Mirror Dimension - Fractal Feedback Engaged.\n");
    return 0;
}

#define GET_SIN_10(idx) (sin_lut[(idx) & LUT_MASK])
#define GET_COS_10(idx) (sin_lut[((idx) + 256) & LUT_MASK])

static void effect_draw(struct demo_ctx *ctx, unsigned long phy_addr)
{
    if (!g_tex_vir[0] || !g_tex_vir[1])
        return;

    int t       = g_tick;
    int src_idx = g_buf_idx;
    int dst_idx = 1 - g_buf_idx;

    /* --- PHASE 1: GE 镜像反馈 (The Fractal Fold) --- */

    // 1. 清空当前帧 (真空基底)
    struct ge_fillrect fill  = {0};
    fill.type                = GE_NO_GRADIENT;
    fill.start_color         = 0x00000000;
    fill.dst_buf.buf_type    = MPP_PHY_ADDR;
    fill.dst_buf.phy_addr[0] = g_tex_phy[dst_idx];
    fill.dst_buf.stride[0]   = TEX_WIDTH * TEX_BPP;
    fill.dst_buf.size.width  = TEX_WIDTH;
    fill.dst_buf.size.height = TEX_HEIGHT;
    fill.dst_buf.format      = TEX_FMT;
    mpp_ge_fillrect(ctx->ge, &fill);
    mpp_ge_emit(ctx->ge);

    // 2. 将上一帧缩放并对折叠加
    struct ge_bitblt feedback    = {0};
    feedback.src_buf.buf_type    = MPP_PHY_ADDR;
    feedback.src_buf.phy_addr[0] = g_tex_phy[src_idx];
    feedback.src_buf.stride[0]   = TEX_WIDTH * TEX_BPP;
    feedback.src_buf.size.width  = TEX_WIDTH;
    feedback.src_buf.size.height = TEX_HEIGHT;
    feedback.src_buf.format      = TEX_FMT;

    feedback.dst_buf.buf_type    = MPP_PHY_ADDR;
    feedback.dst_buf.phy_addr[0] = g_tex_phy[dst_idx];
    feedback.dst_buf.stride[0]   = TEX_WIDTH * TEX_BPP;
    feedback.dst_buf.size.width  = TEX_WIDTH;
    feedback.dst_buf.size.height = TEX_HEIGHT;
    feedback.dst_buf.format      = TEX_FMT;

    // 缩放逻辑：向中心收缩
    feedback.src_buf.crop_en     = 0;
    feedback.dst_buf.crop_en     = 1;
    feedback.dst_buf.crop.x      = ZOOM_MARGIN;
    feedback.dst_buf.crop.y      = ZOOM_MARGIN;
    feedback.dst_buf.crop.width  = TEX_WIDTH - (ZOOM_MARGIN * 2);
    feedback.dst_buf.crop.height = TEX_HEIGHT - (ZOOM_MARGIN * 2);

    // 核心：空间对折 (Flip H + Flip V)
    feedback.ctrl.flags            = MPP_FLIP_H | MPP_FLIP_V;
    feedback.ctrl.alpha_en         = 0;         // 使能混合
    feedback.ctrl.alpha_rules      = GE_PD_ADD; // 能量累加
    feedback.ctrl.src_alpha_mode   = 1;
    feedback.ctrl.src_global_alpha = TRAIL_PERSISTENCE;

    mpp_ge_bitblt(ctx->ge, &feedback);
    mpp_ge_emit(ctx->ge);
    mpp_ge_sync(ctx->ge);

    /* --- PHASE 2: CPU 注入种子 (The Lissajous Seed) --- */
    uint16_t *dst_p = g_tex_vir[dst_idx];
    int       cx    = TEX_WIDTH / 2;
    int       cy    = TEX_HEIGHT / 2;

    int      r_breath = SEED_RADIUS_BASE + (GET_SIN_10(t * 3) >> SEED_BREATH_SHIFT);
    uint16_t seed_col = g_palette[(t * 3) & 0xFF];

    for (int i = 0; i < SEED_POINTS; i++)
    {
        int ang = (i * LUT_SIZE / SEED_POINTS) + (t * SEED_SPEED);
        // 利萨如轨迹驱动：X/Y 轴频率比为 1:3
        int x = cx + ((r_breath * GET_COS_10(ang)) >> 12);
        int y = cy + ((r_breath * GET_SIN_10(ang * 3)) >> 12);

        if (x >= 1 && x < TEX_WIDTH - 1 && y >= 1 && y < TEX_HEIGHT - 1)
        {
            // 绘制 3 像素宽的十字星，强化视觉存在感
            dst_p[y * TEX_WIDTH + x]       = seed_col;
            dst_p[y * TEX_WIDTH + x + 1]   = seed_col;
            dst_p[y * TEX_WIDTH + x - 1]   = seed_col;
            dst_p[(y + 1) * TEX_WIDTH + x] = seed_col;
            dst_p[(y - 1) * TEX_WIDTH + x] = seed_col;
        }
    }
    aicos_dcache_clean_range((void *)dst_p, TEX_SIZE);

    /* --- PHASE 3: 全屏投射 --- */
    struct ge_bitblt final    = {0};
    final.src_buf.buf_type    = MPP_PHY_ADDR;
    final.src_buf.phy_addr[0] = g_tex_phy[dst_idx];
    final.src_buf.stride[0]   = TEX_WIDTH * TEX_BPP;
    final.src_buf.size.width  = TEX_WIDTH;
    final.src_buf.size.height = TEX_HEIGHT;
    final.src_buf.format      = TEX_FMT;

    final.dst_buf.buf_type    = MPP_PHY_ADDR;
    final.dst_buf.phy_addr[0] = phy_addr;
    final.dst_buf.stride[0]   = ctx->info.stride;
    final.dst_buf.size.width  = ctx->info.width;
    final.dst_buf.size.height = ctx->info.height;
    final.dst_buf.format      = ctx->info.format;

    // 全屏拉伸
    final.dst_buf.crop_en     = 1;
    final.dst_buf.crop.width  = ctx->info.width;
    final.dst_buf.crop.height = ctx->info.height;
    final.ctrl.alpha_en       = 1; // 覆盖模式

    mpp_ge_bitblt(ctx->ge, &final);
    mpp_ge_emit(ctx->ge);
    mpp_ge_sync(ctx->ge);

    /* --- PHASE 4: 视觉微调 (HSBC & CCM) --- */
    struct aicfb_disp_prop prop = {0};
    prop.contrast               = HSBC_CONTRAST;
    prop.bright                 = 45;
    prop.saturation             = 85;
    prop.hue                    = 50;
    mpp_fb_ioctl(ctx->fb, AICFB_SET_DISP_PROP, &prop);

    struct aicfb_ccm_config ccm = {0};
    ccm.enable                  = 1;
    int shift                   = GET_SIN_10(t >> CCM_SHIFT_SPEED) >> 6; // -64 ~ 64
    ccm.ccm_table[0]            = 0x100;
    ccm.ccm_table[5]            = 0x100 - ABS(shift);
    ccm.ccm_table[6]            = shift;
    ccm.ccm_table[10]           = 0x100 + ABS(shift);
    mpp_fb_ioctl(ctx->fb, AICFB_UPDATE_CCM_CONFIG, &ccm);

    g_buf_idx = dst_idx;
    g_tick++;
}

static void effect_deinit(struct demo_ctx *ctx)
{
    // 复位显示参数
    struct aicfb_ccm_config ccm_reset = {0};
    ccm_reset.enable                  = 0;
    mpp_fb_ioctl(ctx->fb, AICFB_UPDATE_CCM_CONFIG, &ccm_reset);
    struct aicfb_disp_prop prop_reset = {50, 50, 50, 50};
    mpp_fb_ioctl(ctx->fb, AICFB_SET_DISP_PROP, &prop_reset);

    for (int i = 0; i < 2; i++)
    {
        if (g_tex_phy[i])
            mpp_phy_free(g_tex_phy[i]);
    }
}

struct effect_ops effect_0049 = {
    .name   = "NO.49 THE MIRROR DIMENSION",
    .init   = effect_init,
    .draw   = effect_draw,
    .deinit = effect_deinit,
};

REGISTER_EFFECT(effect_0049);
