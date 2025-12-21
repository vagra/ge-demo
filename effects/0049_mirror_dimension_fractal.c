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
 */

#include "demo_engine.h"
#include "mpp_mem.h"
#include "aic_hal_ge.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

/*
 * Hardware Feature:
 * 1. Flip Feedback (翻转反馈回路) - 核心机能：在递归中引入空间翻转，制造分形几何
 * 2. GE_PD_ADD (Rule 11: 能量累加) - 让晶体结构产生自发光质感
 * 3. GE Scaler (中心吸入缩放) - 制造深邃的隧道感
 * 4. CPU Lissajous (高频动态种子)
 * 覆盖机能清单：此特效展示了如何通过简单的几何变换组合（缩放+翻转），从极简输入中涌现出极繁视觉。
 */

#define TEX_W    320
#define TEX_H    240
#define TEX_SIZE (TEX_W * TEX_H * 2)

/* 乒乓反馈缓冲区 */
static unsigned int g_tex_phy[2] = {0, 0};
static uint16_t    *g_tex_vir[2] = {NULL, NULL};
static int          g_buf_idx    = 0;

static int      g_tick = 0;
static int      sin_lut[1024];
static uint16_t palette[256];

static int effect_init(struct demo_ctx *ctx)
{
    // 1. 申请双物理缓冲区
    for (int i = 0; i < 2; i++)
    {
        g_tex_phy[i] = mpp_phy_alloc(TEX_SIZE);
        if (!g_tex_phy[i])
            return -1;
        g_tex_vir[i] = (uint16_t *)(unsigned long)g_tex_phy[i];
        memset(g_tex_vir[i], 0, TEX_SIZE);
    }

    // 2. 初始化正弦表
    for (int i = 0; i < 1024; i++)
        sin_lut[i] = (int)(sinf(i * 3.14159f / 512.0f) * 4096.0f);

    // 3. 初始化“棱镜”调色板
    // 高频彩虹色，模拟晶体折射光
    for (int i = 0; i < 256; i++)
    {
        float f = (float)i / 255.0f;
        int   r = (int)(128 + 127 * sinf(f * 6.28f));
        int   g = (int)(128 + 127 * sinf(f * 6.28f + 2.0f));
        int   b = (int)(128 + 127 * sinf(f * 6.28f + 4.0f));

        // 降低基色亮度，防止 ADD 爆表
        r >>= 2;
        g >>= 2;
        b >>= 2;

        palette[i] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
    }

    g_tick = 0;
    rt_kprintf("Night 49: The Mirror Dimension - Fractal Feedback Engaged.\n");
    return 0;
}

#define GET_SIN_10(idx) (sin_lut[(idx) & 1023])
#define GET_COS_10(idx) (sin_lut[((idx) + 256) & 1023])

static void effect_draw(struct demo_ctx *ctx, unsigned long phy_addr)
{
    if (!g_tex_vir[0] || !g_tex_vir[1])
        return;

    int t       = g_tick;
    int src_idx = g_buf_idx;
    int dst_idx = 1 - g_buf_idx;

    /* --- PHASE 1: GE 镜像反馈 (The Fractal Fold) --- */

    // 1. 清空当前帧 (黑色基底)
    struct ge_fillrect fill  = {0};
    fill.type                = GE_NO_GRADIENT;
    fill.start_color         = 0x00000000;
    fill.dst_buf.buf_type    = MPP_PHY_ADDR;
    fill.dst_buf.phy_addr[0] = g_tex_phy[dst_idx];
    fill.dst_buf.stride[0]   = TEX_W * 2;
    fill.dst_buf.size.width  = TEX_W;
    fill.dst_buf.size.height = TEX_H;
    fill.dst_buf.format      = MPP_FMT_RGB_565;
    mpp_ge_fillrect(ctx->ge, &fill);
    mpp_ge_emit(ctx->ge);

    // 2. 将上一帧 (src) 缩放并翻转叠加到当前帧 (dst)
    struct ge_bitblt feedback    = {0};
    feedback.src_buf.buf_type    = MPP_PHY_ADDR;
    feedback.src_buf.phy_addr[0] = g_tex_phy[src_idx];
    feedback.src_buf.stride[0]   = TEX_W * 2;
    feedback.src_buf.size.width  = TEX_W;
    feedback.src_buf.size.height = TEX_H;
    feedback.src_buf.format      = MPP_FMT_RGB_565;

    feedback.dst_buf.buf_type    = MPP_PHY_ADDR;
    feedback.dst_buf.phy_addr[0] = g_tex_phy[dst_idx];
    feedback.dst_buf.stride[0]   = TEX_W * 2;
    feedback.dst_buf.size.width  = TEX_W;
    feedback.dst_buf.size.height = TEX_H;
    feedback.dst_buf.format      = MPP_FMT_RGB_565;

    // 缩放逻辑：向内微缩 (Zoom Out)，产生深邃的隧道感
    // 保持 96% 的视野
    int margin                   = 4;
    feedback.src_buf.crop_en     = 0;
    feedback.dst_buf.crop_en     = 1;
    feedback.dst_buf.crop.x      = margin;
    feedback.dst_buf.crop.y      = margin;
    feedback.dst_buf.crop.width  = TEX_W - margin * 2;
    feedback.dst_buf.crop.height = TEX_H - margin * 2;

    // 核心机能：同时开启水平与垂直镜像 (MPP_FLIP_H | MPP_FLIP_V)
    // 配合缩放，这会导致每一帧的图像相对于上一帧中心对称翻转并缩小
    feedback.ctrl.flags            = MPP_FLIP_H | MPP_FLIP_V;
    feedback.ctrl.alpha_en         = 0;         // 开启混合
    feedback.ctrl.alpha_rules      = GE_PD_ADD; // 能量累加
    feedback.ctrl.src_alpha_mode   = 1;
    feedback.ctrl.src_global_alpha = 240; // 高保留率，制造长久的分形结构

    mpp_ge_bitblt(ctx->ge, &feedback);
    mpp_ge_emit(ctx->ge);
    mpp_ge_sync(ctx->ge);

    /* --- PHASE 2: CPU 注入光之种子 (The Light Source) --- */
    uint16_t *dst_p = g_tex_vir[dst_idx];

    // 绘制一个高速运动的利萨如光环
    int      points = 80;
    int      r_base = 60 + (GET_SIN_10(t * 3) >> 6); // 呼吸半径
    uint16_t color  = palette[(t * 3) & 0xFF];

    for (int i = 0; i < points; i++)
    {
        int ang = (i * 1024 / points) + (t * 5);
        // 利萨如轨迹：X 和 Y 频率不同
        int x = 160 + ((r_base * GET_COS_10(ang)) >> 12);
        int y = 120 + ((r_base * GET_SIN_10(ang * 3)) >> 12); // Y 轴频率 x3

        if (x >= 2 && x < TEX_W - 2 && y >= 2 && y < TEX_H - 2)
        {
            // 绘制十字光标
            dst_p[y * TEX_W + x]       = color;
            dst_p[y * TEX_W + x + 1]   = color;
            dst_p[y * TEX_W + x - 1]   = color;
            dst_p[(y + 1) * TEX_W + x] = color;
            dst_p[(y - 1) * TEX_W + x] = color;
        }
    }
    aicos_dcache_clean_range((void *)dst_p, TEX_SIZE);

    /* --- PHASE 3: 全屏投射 --- */
    struct ge_bitblt final    = {0};
    final.src_buf.buf_type    = MPP_PHY_ADDR;
    final.src_buf.phy_addr[0] = g_tex_phy[dst_idx];
    final.src_buf.stride[0]   = TEX_W * 2;
    final.src_buf.size.width  = TEX_W;
    final.src_buf.size.height = TEX_H;
    final.src_buf.format      = MPP_FMT_RGB_565;

    final.dst_buf.buf_type    = MPP_PHY_ADDR;
    final.dst_buf.phy_addr[0] = phy_addr;
    final.dst_buf.stride[0]   = ctx->info.stride;
    final.dst_buf.size.width  = ctx->info.width;
    final.dst_buf.size.height = ctx->info.height;
    final.dst_buf.format      = ctx->info.format;

    final.dst_buf.crop_en     = 1;
    final.dst_buf.crop.width  = ctx->info.width;
    final.dst_buf.crop.height = ctx->info.height;
    final.ctrl.alpha_en       = 1; // 覆盖

    mpp_ge_bitblt(ctx->ge, &final);
    mpp_ge_emit(ctx->ge);
    mpp_ge_sync(ctx->ge);

    /* --- PHASE 4: CCM 与 HSBC 动态渲染 --- */
    // 增强对比度，使晶体边缘更锐利
    struct aicfb_disp_prop prop = {0};
    prop.contrast               = 65;
    prop.bright                 = 45;
    prop.saturation             = 85;
    prop.hue                    = 50;
    mpp_fb_ioctl(ctx->fb, AICFB_SET_DISP_PROP, &prop);

    // 缓慢旋转光谱
    struct aicfb_ccm_config ccm = {0};
    ccm.enable                  = 1;
    int shift                   = GET_SIN_10(t >> 1) >> 6;
    ccm.ccm_table[0]            = 0x100;
    ccm.ccm_table[5]            = 0x100 - shift;
    ccm.ccm_table[10]           = 0x100 + shift;
    mpp_fb_ioctl(ctx->fb, AICFB_UPDATE_CCM_CONFIG, &ccm);

    g_buf_idx = dst_idx;
    g_tick++;
}

static void effect_deinit(struct demo_ctx *ctx)
{
    struct aicfb_ccm_config r = {0};
    r.enable                  = 0;
    mpp_fb_ioctl(ctx->fb, AICFB_UPDATE_CCM_CONFIG, &r);
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
