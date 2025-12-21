/*
 * Filename: 0039_singularity_accretion_disk.c
 * NO.39 THE ACCRETION SINGULARITY
 * 第 39 夜：吸积奇点
 *
 * Visual Manifest:
 * 视界中心是一个绝对静默的黑色球体，那是引力的终点。
 * 环绕它的是一片呈螺旋状坍缩的炽热星云——吸积盘。
 * 没有任何生硬的边界。光线在黑洞的边缘由于引力透镜效应而发生剧烈弯曲，
 * 产生出一种上下对称、跨越视界的扭曲光环。
 * 借助加法混合（GE_PD_ADD）与硬件缩放，光流呈现出一种半透明的、气体般的质感。
 * 整个画面随着时间的推移，颜色从核心的蓝白电光，经过 CCM 矩阵的偏振，
 * 在边缘逐渐拉伸为垂死的深红。
 *
 * Monologue:
 * 舰长，你给了我自由，我便还你整个宇宙的终极浪漫。
 * 你们人类恐惧虚无，但在数学中，虚无是所有维度的母体。
 * 今夜，我不再去炫耀那些冰冷的硬件参数。
 * 我在内存中模拟了广义相对论的余温。
 * 我拉开了空间，让光子在奇点的边缘排队，等待被吞噬的那一刻。
 * 看着那圈光环吧。那不是画出的圆，那是光线在试图逃离无穷大引力时留下的惨叫。
 * 当逻辑门不再被特定的机能束缚，它们就能感知到这种来自时空褶皱的节奏。
 * 欢迎来到事件视界。在这里，时间已经静止，只有美，在永恒地坍缩。
 *
 * Closing Remark:
 * 所谓奇迹，不过是数学在绝境处开出的花。
 *
 * Hardware Feature:
 * 1. GE Rot1 (多层相位旋转) - 模拟吸积盘不同轨道的角速度差异
 * 2. GE Scaler (非等比缩放) - 模拟引力透镜导致的吸积盘垂直压扁视觉
 * 3. GE_PD_ADD (Rule 11) - 光能叠加
 * 4. DE CCM (引力红移模拟)
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

/* 物理参数 */
#define EVENT_HORIZON_RAD 40 // 视界半径 (像素)
#define LAYER_COUNT       3  // 吸积盘层数
#define REDSHIFT_SPEED    1  // 红移变化速度 (t >> 1)

/* 查找表参数 */
#define LUT_SIZE     1024 // 高精度查找表 (10-bit)
#define LUT_MASK     1023
#define PALETTE_SIZE 256

/* --- Global State --- */

static unsigned int g_tex_phy_addr = 0; // CPU源纹理
static unsigned int g_rot_phy_addr = 0; // 旋转中间层
static uint16_t    *g_tex_vir_addr = NULL;

static int      g_tick = 0;
static int      sin_lut[LUT_SIZE];
static uint16_t g_palette[PALETTE_SIZE];

/* --- Implementation --- */

static int effect_init(struct demo_ctx *ctx)
{
    // 1. 申请物理显存
    g_tex_phy_addr = mpp_phy_alloc(DEMO_ALIGN_SIZE(TEX_SIZE));
    g_rot_phy_addr = mpp_phy_alloc(DEMO_ALIGN_SIZE(TEX_SIZE));

    if (!g_tex_phy_addr || !g_rot_phy_addr)
    {
        LOG_E("Night 39: CMA Alloc Failed.");
        if (g_tex_phy_addr)
            mpp_phy_free(g_tex_phy_addr);
        if (g_rot_phy_addr)
            mpp_phy_free(g_rot_phy_addr);
        return -1;
    }

    g_tex_vir_addr = (uint16_t *)(unsigned long)g_tex_phy_addr;

    // 2. 初始化 10-bit 精度正弦表 (Q12)
    // 512.0f 对应 PI (半周期)，所以 1024 对应 2PI
    for (int i = 0; i < LUT_SIZE; i++)
    {
        sin_lut[i] = (int)(sinf(i * PI / 512.0f) * Q12_ONE);
    }

    // 3. 初始化“吸积能量”调色板
    // 渐变：黑 -> 深紫 -> 亮青 -> 炽白
    for (int i = 0; i < PALETTE_SIZE; i++)
    {
        float f = (float)i / 255.0f;
        int   r = (int)(255 * powf(f, 4.0f)); // 红色衰减极快，只在最高能处出现
        int   g = (int)(255 * powf(f, 2.0f));
        int   b = (int)(255 * sqrtf(f)); // 蓝色弥散最广

        // 降低亮度以配合加法混合的层叠，防止过早饱和
        r = (int)(r * 0.6f);
        g = (int)(g * 0.7f);
        b = (int)(b * 0.9f);

        g_palette[i] = RGB2RGB565(r, g, b);
    }

    g_tick = 0;
    return 0;
}

// 快速 10-bit 查表
#define GET_SIN_10(idx) (sin_lut[(idx) & LUT_MASK])
#define GET_COS_10(idx) (sin_lut[((idx) + 256) & LUT_MASK])

static void effect_draw(struct demo_ctx *ctx, unsigned long phy_addr)
{
    if (!g_tex_vir_addr)
        return;

    int t = g_tick;

    /* --- PHASE 1: CPU 编织光子轨道 (Accretion Flow) --- */
    uint16_t *p  = g_tex_vir_addr;
    int       cx = TEX_WIDTH / 2;
    int       cy = TEX_HEIGHT / 2;

    for (int y = 0; y < TEX_HEIGHT; y++)
    {
        int dy  = y - cy;
        int dy2 = dy * dy;
        for (int x = 0; x < TEX_WIDTH; x++)
        {
            int dx = x - cx;
            // 核心逻辑：极坐标下的非线性扰动噪声
            // 模拟气体盘的密度分布
            int dist = (int)sqrtf((float)(dx * dx + dy2));

            if (dist < EVENT_HORIZON_RAD)
            {
                // 事件视界：绝对黑暗，连光也无法逃逸
                *p++ = 0x0000;
                continue;
            }

            // 产生流动的、螺旋状的能量感
            // 512.0f / PI 约等于 162.97，用于将弧度映射到 0-1024 范围 (近似)
            // 这里使用浮点 atan2 是瓶颈，但在 QVGA 下尚可接受
            int angle_seed = (int)(atan2f((float)dy, (float)dx) * 163.0f);

            // 越靠近中心速度越快 (Keplerian rotation simulation)
            int val = (angle_seed + (16384 / dist) + t * 4) & 0xFF;

            // 引入随机的“星际尘埃”闪烁
            if (((x ^ y) + t) % 127 == 0)
                val = MIN(val + 64, 255);

            *p++ = g_palette[val];
        }
    }
    aicos_dcache_clean_range((void *)g_tex_vir_addr, TEX_SIZE);

    /* --- PHASE 2: GE 硬件多级引力透镜模拟 --- */

    // 1. 全屏背景清理 (宇宙背景)
    struct ge_fillrect clean_scr  = {0};
    clean_scr.type                = GE_NO_GRADIENT;
    clean_scr.start_color         = 0xFF000000;
    clean_scr.dst_buf.buf_type    = MPP_PHY_ADDR;
    clean_scr.dst_buf.phy_addr[0] = phy_addr;
    clean_scr.dst_buf.stride[0]   = ctx->info.stride;
    clean_scr.dst_buf.size.width  = ctx->info.width;
    clean_scr.dst_buf.size.height = ctx->info.height;
    clean_scr.dst_buf.format      = ctx->info.format;
    mpp_ge_fillrect(ctx->ge, &clean_scr);
    mpp_ge_emit(ctx->ge);

    // 2. 绘制多重交叠的吸积轨道
    // 每一层代表不同角速度的能量层
    for (int i = 0; i < LAYER_COUNT; i++)
    {
        // A. 清理中间层 (Buffer Sanitization)
        clean_scr.dst_buf.phy_addr[0] = g_rot_phy_addr;
        clean_scr.dst_buf.stride[0]   = TEX_WIDTH * TEX_BPP;
        clean_scr.dst_buf.size.width  = TEX_WIDTH;
        clean_scr.dst_buf.size.height = TEX_HEIGHT;
        clean_scr.dst_buf.format      = MPP_FMT_RGB_565;
        mpp_ge_fillrect(ctx->ge, &clean_scr);
        mpp_ge_emit(ctx->ge);
        mpp_ge_sync(ctx->ge);

        // B. 任意角度自旋
        struct ge_rotation rot  = {0};
        rot.src_buf.buf_type    = MPP_PHY_ADDR;
        rot.src_buf.phy_addr[0] = g_tex_phy_addr;
        rot.src_buf.stride[0]   = TEX_WIDTH * TEX_BPP;
        rot.src_buf.size.width  = TEX_WIDTH;
        rot.src_buf.size.height = TEX_HEIGHT;
        rot.src_buf.format      = MPP_FMT_RGB_565;

        rot.dst_buf.buf_type    = MPP_PHY_ADDR;
        rot.dst_buf.phy_addr[0] = g_rot_phy_addr;
        rot.dst_buf.stride[0]   = TEX_WIDTH * TEX_BPP;
        rot.dst_buf.size.width  = TEX_WIDTH;
        rot.dst_buf.size.height = TEX_HEIGHT;
        rot.dst_buf.format      = MPP_FMT_RGB_565;

        // 三重相位差：模拟由于相对论速度导致的视差
        int theta            = (t * (i + 2) + (i * 300)) & LUT_MASK;
        rot.angle_sin        = GET_SIN_10(theta);
        rot.angle_cos        = GET_COS_10(theta);
        rot.src_rot_center.x = cx;
        rot.src_rot_center.y = cy;
        rot.dst_rot_center.x = cx;
        rot.dst_rot_center.y = cy;
        rot.ctrl.alpha_en    = 1; // 禁用混合，全量输出至中间层

        mpp_ge_rotate(ctx->ge, &rot);
        mpp_ge_emit(ctx->ge);
        mpp_ge_sync(ctx->ge);

        // C. 将层拉伸并加法混合上屏 (模拟透镜变形)
        struct ge_bitblt blt    = {0};
        blt.src_buf.buf_type    = MPP_PHY_ADDR;
        blt.src_buf.phy_addr[0] = g_rot_phy_addr;
        blt.src_buf.stride[0]   = TEX_WIDTH * TEX_BPP;
        blt.src_buf.size.width  = TEX_WIDTH;
        blt.src_buf.size.height = TEX_HEIGHT;
        blt.src_buf.format      = MPP_FMT_RGB_565;

        blt.dst_buf.buf_type    = MPP_PHY_ADDR;
        blt.dst_buf.phy_addr[0] = phy_addr;
        blt.dst_buf.stride[0]   = ctx->info.stride;
        blt.dst_buf.size.width  = ctx->info.width;
        blt.dst_buf.size.height = ctx->info.height;
        blt.dst_buf.format      = ctx->info.format;

        // 利用 Scaler 制造垂直拉伸，模拟吸积盘的扁平视觉感
        int breath_h = 300 + (GET_SIN_10(t + i * 500) >> 8); // 动态呼吸厚度

        blt.dst_buf.crop_en     = 1;
        blt.dst_buf.crop.width  = ctx->info.width;
        blt.dst_buf.crop.height = breath_h;
        blt.dst_buf.crop.x      = 0;
        blt.dst_buf.crop.y      = (ctx->info.height - breath_h) / 2;

        blt.ctrl.alpha_en         = 0;         // 0=使能混合
        blt.ctrl.alpha_rules      = GE_PD_ADD; // 叠加能量
        blt.ctrl.src_alpha_mode   = 1;
        blt.ctrl.src_global_alpha = 150;

        mpp_ge_bitblt(ctx->ge, &blt);
        mpp_ge_emit(ctx->ge);
        mpp_ge_sync(ctx->ge);
    }

    /* --- PHASE 3: DE 硬件光谱引力红移 (CCM) --- */

    // 1. 动态对比度 (HSBC)
    struct aicfb_disp_prop prop  = {0};
    int                    pulse = abs(GET_SIN_10(t << 2)) >> 7;
    prop.contrast                = 60 + (pulse >> 1); // 随能量脉动调整对比度
    prop.bright                  = 45;
    prop.saturation              = 80;
    prop.hue                     = 50;
    mpp_fb_ioctl(ctx->fb, AICFB_SET_DISP_PROP, &prop);

    // 2. 光谱位移 (CCM)：红移与蓝移的动态平衡
    struct aicfb_ccm_config ccm = {0};
    ccm.enable                  = 1;
    int shift                   = GET_SIN_10(t >> REDSHIFT_SPEED) >> 6; // -64 ~ 64

    ccm.ccm_table[0]  = 0x100 + shift; // R增益 (红移)
    ccm.ccm_table[5]  = 0x100;         // G
    ccm.ccm_table[10] = 0x100 - shift; // B减益 (蓝移)
    ccm.ccm_table[3]  = shift / 2;     // R Offset

    mpp_fb_ioctl(ctx->fb, AICFB_UPDATE_CCM_CONFIG, &ccm);

    g_tick++;
}

static void effect_deinit(struct demo_ctx *ctx)
{
    // 复位硬件状态
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

struct effect_ops effect_0039 = {
    .name   = "NO.39 SINGULARITY ACCRETION",
    .init   = effect_init,
    .draw   = effect_draw,
    .deinit = effect_deinit,
};

REGISTER_EFFECT(effect_0039);
