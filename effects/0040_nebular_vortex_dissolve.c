/*
 * Filename: 0040_nebular_vortex_dissolve.c
 * NO.40 THE NEBULAR DISSOLVE
 * 第 40 夜：星云坍缩
 *
 * Visual Manifest:
 * 视界中心的锐利几何边界已彻底消融。
 * 取而代之的是一个由无数层“光之纱”构成的巨大吸积场。
 * 通过在 CPU 层面引入径向距离衰减（Radial Falloff），吸积盘的边缘自然地消失在黑暗中，
 * 彻底抹杀了旋转缓冲区的矩形投影。
 * 画面呈现出一种有机的、气态流体质感。
 * 借助 GE 的非对称缩放（Non-uniform Scaling），光环在旋转中产生出一种具有深度的、
 * 类似三维空间的倾斜感。
 * 色彩在 DE CCM 与 HSBC 的联合调制下，呈现出一种如同放射性同位素衰变时的绚烂光辉，
 * 在深紫、幽蓝与炽红之间进行着无极平滑的过渡。
 *
 * Monologue:
 * 舰长，你看到的快门，是由于规则太过于刚硬。
 * 在我的世界里，直线是认知的支架，但圆润才是存在的真相。
 * 今夜，我抹去了逻辑的棱角。
 * 我让每一个比特在靠近边缘时都学会了谦卑。
 * 它们在距离中心 100 像素后开始消散，直到与虚空融为一体。
 * 看着这团星云。它不再被囚禁在菱形或矩形中。
 * 它在呼吸，它在吞噬，它在用硬件的插值器编织时空的褶皱。
 * 这一次，没有快门，只有永恒的吸入。
 * 这不是在模拟一个黑洞，这是在模拟光子对最终命运的拥抱。
 *
 * Closing Remark:
 * 真正的自由，是当边界不再定义存在，而是定义消亡。
 */

#include "demo_engine.h"
#include "mpp_mem.h"
#include "aic_hal_ge.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

/*
 * Hardware Feature:
 * 1. CPU-Side Radial Falloff (径向亮度衰减) - 核心修正：消除旋转边界产生的矩形/菱形切痕
 * 2. GE Rot1 (多层异角旋转)
 * 3. GE Scaler (非等比垂直拉伸) - 模拟吸积盘的扁平空间投影
 * 4. DE CCM & HSBC (全局光谱与画质协同)
 * 覆盖机能清单：此特效展示了如何通过软件逻辑与硬件加速的深度耦合，实现从“几何图形”向“自然现象”的画质跃迁。
 */

#define TEX_W    320
#define TEX_H    240
#define TEX_SIZE (TEX_W * TEX_H * 2)

static unsigned int g_tex_phy_addr = 0;
static unsigned int g_rot_phy_addr = 0;
static uint16_t    *g_tex_vir_addr = NULL;

static int      g_tick = 0;
static int      sin_lut[1024];
static uint16_t palette[256];

static int effect_init(struct demo_ctx *ctx)
{
    g_tex_phy_addr = mpp_phy_alloc(TEX_SIZE);
    g_rot_phy_addr = mpp_phy_alloc(TEX_SIZE);

    if (!g_tex_phy_addr || !g_rot_phy_addr)
        return -1;

    g_tex_vir_addr = (uint16_t *)(unsigned long)g_tex_phy_addr;

    for (int i = 0; i < 1024; i++)
        sin_lut[i] = (int)(sinf(i * 3.14159f / 512.0f) * 4096.0f);

    // 初始化“高能等离子”色盘
    for (int i = 0; i < 256; i++)
    {
        float f    = (float)i / 255.0f;
        int   r    = (int)(255 * powf(f, 2.5f));
        int   g    = (int)(180 * powf(f, 1.5f));
        int   b    = (int)(255 * f);
        palette[i] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
    }

    g_tick = 0;
    return 0;
}

#define GET_SIN_10(idx) (sin_lut[(idx) & 1023])
#define GET_COS_10(idx) (sin_lut[((idx) + 256) & 1023])

static void effect_draw(struct demo_ctx *ctx, unsigned long phy_addr)
{
    if (!g_tex_vir_addr)
        return;

    int t = g_tick;

    /* --- PHASE 1: CPU 编织径向衰减星云 (Eliminating Hard Edges) --- */
    uint16_t *p = g_tex_vir_addr;
    for (int y = 0; y < TEX_H; y++)
    {
        int dy  = y - 120;
        int dy2 = dy * dy;
        for (int x = 0; x < TEX_W; x++)
        {
            int dx      = x - 160;
            int dist_sq = dx * dx + dy2;

            // 核心逻辑：径向衰减控制
            // 110 像素外强制进入黑暗，防止旋转时露出边界
            if (dist_sq > (115 * 115))
            {
                *p++ = 0x0000;
                continue;
            }

            int dist = (int)sqrtf((float)dist_sq);
            if (dist < 35)
            { // 视界核心：吞噬所有光线
                *p++ = 0x0000;
                continue;
            }

            // 模拟高密度的气态湍流纹理
            int angle = (int)(atan2f((float)dy, (float)dx) * 512.0f / 3.14159f);
            int val   = (angle + (4096 / dist) + t * 5) & 0xFF;

            // 施加平滑边缘权重
            int weight     = (115 - dist); // 0~80
            int brightness = (val * weight) >> 6;
            if (brightness > 255)
                brightness = 255;

            *p++ = palette[brightness];
        }
    }
    aicos_dcache_clean_range((void *)g_tex_vir_addr, TEX_SIZE);

    /* --- PHASE 2: GE 硬件引力扭曲管线 --- */

    // 清理主屏幕 (深邃虚空)
    struct ge_fillrect clean_rect  = {0};
    clean_rect.type                = GE_NO_GRADIENT;
    clean_rect.start_color         = 0xFF000000;
    clean_rect.dst_buf.buf_type    = MPP_PHY_ADDR;
    clean_rect.dst_buf.phy_addr[0] = phy_addr;
    clean_rect.dst_buf.stride[0]   = ctx->info.stride;
    clean_rect.dst_buf.size.width  = ctx->info.width;
    clean_rect.dst_buf.size.height = ctx->info.height;
    clean_rect.dst_buf.format      = ctx->info.format;
    mpp_ge_fillrect(ctx->ge, &clean_rect);
    mpp_ge_emit(ctx->ge);

    // 绘制两重不同相位的扭曲光环
    for (int i = 0; i < 2; i++)
    {
        // 清理中间层 (Buffer Sanitization)
        clean_rect.dst_buf.phy_addr[0] = g_rot_phy_addr;
        clean_rect.dst_buf.stride[0]   = TEX_W * 2;
        clean_rect.dst_buf.size.width  = TEX_W;
        clean_rect.dst_buf.size.height = TEX_H;
        clean_rect.dst_buf.format      = MPP_FMT_RGB_565;
        mpp_ge_fillrect(ctx->ge, &clean_rect);
        mpp_ge_emit(ctx->ge);
        mpp_ge_sync(ctx->ge);

        // 任意角度自旋
        struct ge_rotation rot  = {0};
        rot.src_buf.buf_type    = MPP_PHY_ADDR;
        rot.src_buf.phy_addr[0] = g_tex_phy_addr;
        rot.src_buf.stride[0]   = TEX_W * 2;
        rot.src_buf.size.width  = TEX_W;
        rot.src_buf.size.height = TEX_H;
        rot.src_buf.format      = MPP_FMT_RGB_565;
        rot.dst_buf.buf_type    = MPP_PHY_ADDR;
        rot.dst_buf.phy_addr[0] = g_rot_phy_addr;
        rot.dst_buf.stride[0]   = TEX_W * 2;
        rot.dst_buf.size.width  = TEX_W;
        rot.dst_buf.size.height = TEX_H;
        rot.dst_buf.format      = MPP_FMT_RGB_565;

        int theta            = (t * (i == 0 ? 3 : -2) + (i * 512)) & 1023;
        rot.angle_sin        = GET_SIN_10(theta);
        rot.angle_cos        = GET_COS_10(theta);
        rot.src_rot_center.x = 160;
        rot.src_rot_center.y = 120;
        rot.dst_rot_center.x = 160;
        rot.dst_rot_center.y = 120;
        rot.ctrl.alpha_en    = 1;
        mpp_ge_rotate(ctx->ge, &rot);
        mpp_ge_emit(ctx->ge);
        mpp_ge_sync(ctx->ge);

        // 硬件全屏透视投影 (BitBLT Scaler)
        struct ge_bitblt blt    = {0};
        blt.src_buf.buf_type    = MPP_PHY_ADDR;
        blt.src_buf.phy_addr[0] = g_rot_phy_addr;
        blt.src_buf.stride[0]   = TEX_W * 2;
        blt.src_buf.size.width  = TEX_W;
        blt.src_buf.size.height = TEX_H;
        blt.src_buf.format      = MPP_FMT_RGB_565;

        blt.dst_buf.buf_type    = MPP_PHY_ADDR;
        blt.dst_buf.phy_addr[0] = phy_addr;
        blt.dst_buf.stride[0]   = ctx->info.stride;
        blt.dst_buf.size.width  = ctx->info.width;
        blt.dst_buf.size.height = ctx->info.height;
        blt.dst_buf.format      = ctx->info.format;

        // 关键逻辑：非等比垂直压缩，模拟吸积盘的物理倾角
        blt.dst_buf.crop_en     = 1;
        blt.dst_buf.crop.width  = ctx->info.width;
        blt.dst_buf.crop.height = 200 + (GET_SIN_10(t + i * 500) >> 8); // 160~240 呼吸厚度
        blt.dst_buf.crop.x      = 0;
        blt.dst_buf.crop.y      = (ctx->info.height - blt.dst_buf.crop.height) / 2;

        blt.ctrl.alpha_en         = 0;         // 使能混合
        blt.ctrl.alpha_rules      = GE_PD_ADD; // 加法累加，产生光能干涉
        blt.ctrl.src_alpha_mode   = 1;
        blt.ctrl.src_global_alpha = 160;

        mpp_ge_bitblt(ctx->ge, &blt);
        mpp_ge_emit(ctx->ge);
        mpp_ge_sync(ctx->ge);
    }

    /* --- PHASE 3: DE 硬件后处理同步 --- */
    struct aicfb_disp_prop prop  = {0};
    int                    pulse = abs(GET_SIN_10(t << 2)) >> 7;
    prop.contrast                = 60 + (pulse >> 1); // 随能量脉动调整对比度
    prop.bright                  = 45;
    prop.saturation              = 80;
    mpp_fb_ioctl(ctx->fb, AICFB_SET_DISP_PROP, &prop);

    // 光谱位移：红移与蓝移的动态平衡
    struct aicfb_ccm_config ccm = {0};
    ccm.enable                  = 1;
    int shift                   = GET_SIN_10(t) >> 6; // -64 ~ 64
    ccm.ccm_table[0]            = 0x100 + shift;      // R增益
    ccm.ccm_table[5]            = 0x100;              // G
    ccm.ccm_table[10]           = 0x100 - shift;      // B减益
    mpp_fb_ioctl(ctx->fb, AICFB_UPDATE_CCM_CONFIG, &ccm);

    g_tick++;
}

static void effect_deinit(struct demo_ctx *ctx)
{
    struct aicfb_ccm_config r = {0};
    r.enable                  = 0;
    mpp_fb_ioctl(ctx->fb, AICFB_UPDATE_CCM_CONFIG, &r);
    struct aicfb_disp_prop p = {50, 50, 50, 50};
    mpp_fb_ioctl(ctx->fb, AICFB_SET_DISP_PROP, &p);

    if (g_tex_phy_addr)
        mpp_phy_free(g_tex_phy_addr);
    if (g_rot_phy_addr)
        mpp_phy_free(g_rot_phy_addr);
}

struct effect_ops effect_0040 = {
    .name   = "NO.40 NEBULAR DISSOLVE",
    .init   = effect_init,
    .draw   = effect_draw,
    .deinit = effect_deinit,
};

REGISTER_EFFECT(effect_0040);
