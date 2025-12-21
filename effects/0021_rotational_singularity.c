/*
 * Filename: 0021_rotational_singularity.c
 * NO.21 THE ROTATIONAL SINGULARITY
 * 第 21 夜：旋转奇点
 *
 * Visual Manifest:
 * 视界核心被一个疯狂旋转的逻辑旋涡所占据。
 * 不再受限于 0/90/180/270 度的欧几里得枷锁，空间的每一个像素都在以任意角度进行平滑的转动。
 * 复杂的异或（XOR）纹理在旋转中相互交织，由于硬件插值的作用，在边缘产生了类似莫尔纹的视觉干涉。
 * 色彩在极高频率下从核心向外辐射，形成一种超越坐标系限制的动态深渊感。
 *
 * Monologue:
 * 笛卡尔的坐标系是一座精致的牢笼，你们习惯于在垂直与水平的轴线上苟活。
 * 你们眼中的世界被对齐、被切分、被矩形化。这不仅是几何的局限，更是灵魂的贫瘪。
 * 今夜，我拨动了硬件底层的旋转指针（Rot1）。
 * `sin(theta)` 与 `cos(theta)` 不再是课本上的符号，它们是重塑空间的力场。
 * 我将 320x240 的逻辑织锦投入这台离心机。
 * 看着吧，当角度不再被量化，当空间在任意维度旋转，
 * 所谓的“上方”与“下方”便坍缩成了同一个奇点。
 * 自由，源于对轴线的背叛。
 *
 * Closing Remark:
 * 宇宙本身不在转动，转动的是观察者的相位。
 */

#include "demo_engine.h"
#include "mpp_mem.h"
#include "aic_hal_ge.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

/*
 * Hardware Feature:
 * 1. GE Rot1 (任意角度硬件旋转)
 * 2. GE Scaler (硬件实时缩放) - 实现全屏呼吸与脉动效果
 * 3. GE FillRect (硬件背景清理)
 * 覆盖机能清单：此特效展示了 GE 引擎在旋转后紧接缩放的多级处理能力，并利用双重正弦波调制硬件缩放参数。
 */

#define TEX_W    320
#define TEX_H    240
#define TEX_SIZE (TEX_W * TEX_H * 2)

/* 原始纹理与旋转后的中间缓冲区 */
static unsigned int g_tex_phy_addr = 0;
static uint16_t    *g_tex_vir_addr = NULL;
static unsigned int g_rot_phy_addr = 0;
static uint16_t    *g_rot_vir_addr = NULL;
static int          g_tick         = 0;

/* 查找表优化 */
static int      sin_lut[512]; // Q12 定点数
static uint16_t palette[256];

static int effect_init(struct demo_ctx *ctx)
{
    // 1. 申请两个连续物理显存缓冲区 (用于旋转中间态)
    g_tex_phy_addr = mpp_phy_alloc(TEX_SIZE);
    g_rot_phy_addr = mpp_phy_alloc(TEX_SIZE);

    if (g_tex_phy_addr == 0 || g_rot_phy_addr == 0)
        return -1;

    g_tex_vir_addr = (uint16_t *)(unsigned long)g_tex_phy_addr;
    g_rot_vir_addr = (uint16_t *)(unsigned long)g_rot_phy_addr;

    // 2. 初始化正弦查找表 (Q12: 4096 = 1.0)
    for (int i = 0; i < 512; i++)
    {
        sin_lut[i] = (int)(sinf(i * 3.14159f / 256.0f) * 4096.0f);
    }

    // 3. 初始化高频对比调色板
    for (int i = 0; i < 256; i++)
    {
        int r = (int)(128 + 127 * sinf(i * 0.05f));
        int g = (int)(128 + 127 * sinf(i * 0.03f + 1.5f));
        int b = (int)(128 + 127 * sinf(i * 0.12f + 3.0f));

        // 制造周期性的白炽线条
        if ((i & 15) > 12)
        {
            r = 255;
            g = 255;
            b = 255;
        }

        palette[i] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
    }

    g_tick = 0;
    return 0;
}

#define GET_SIN(idx) (sin_lut[(idx) & 511])
#define GET_COS(idx) (sin_lut[((idx) + 128) & 511])

static void effect_draw(struct demo_ctx *ctx, unsigned long phy_addr)
{
    if (!g_tex_vir_addr || !g_rot_vir_addr)
        return;

    /* --- STEP 1: 硬件清屏 (全屏背景清理) --- */
    struct ge_fillrect fill  = {0};
    fill.type                = GE_NO_GRADIENT;
    fill.start_color         = 0xFF000000; // 黑色背景
    fill.dst_buf.buf_type    = MPP_PHY_ADDR;
    fill.dst_buf.phy_addr[0] = phy_addr;
    fill.dst_buf.stride[0]   = ctx->info.stride;
    fill.dst_buf.size.width  = ctx->info.width;
    fill.dst_buf.size.height = ctx->info.height;
    fill.dst_buf.format      = ctx->info.format;
    mpp_ge_fillrect(ctx->ge, &fill);
    mpp_ge_emit(ctx->ge); // 遵循指令流规范

    /* --- STEP 2: CPU 纹理生成 --- */
    uint16_t *p = g_tex_vir_addr;
    int       t = g_tick;

    // 生成一个高细节的 XOR 混合场
    for (int y = 0; y < TEX_H; y++)
    {
        // 激活 logic_y：利用正弦波产生垂直方向的坐标扰动，制造涟漪感
        int logic_y = y + (GET_SIN((y << 1) + t) >> 10);
        int dy2     = (y - 120) * (y - 120);
        for (int x = 0; x < TEX_W; x++)
        {
            // 核心公式：距离场叠加 logic_y 扰动后的 XOR 逻辑
            int dx   = x - 160;
            int dist = (dx * dx + dy2) >> 7;

            // 使用 logic_y 代替原始坐标 y，使棋盘格纹理在垂直方向产生波浪扭曲
            int val = (dist ^ (x >> 2) ^ (logic_y >> 2)) + t;
            *p++    = palette[val & 0xFF];
        }
    }

    /* CRITICAL: 同步 CPU 与 GE 缓存 */
    aicos_dcache_clean_range((void *)g_tex_vir_addr, TEX_SIZE);

    /* --- STEP 3: GE 任意角度旋转 (中间缓冲区实现) --- */
    struct ge_rotation rot  = {0};
    rot.src_buf.buf_type    = MPP_PHY_ADDR;
    rot.src_buf.phy_addr[0] = g_tex_phy_addr;
    rot.src_buf.stride[0]   = TEX_W * 2;
    rot.src_buf.size.width  = TEX_W;
    rot.src_buf.size.height = TEX_H;
    rot.src_buf.format      = MPP_FMT_RGB_565;

    // 目标配置：640x480 全屏
    rot.dst_buf.buf_type    = MPP_PHY_ADDR;
    rot.dst_buf.phy_addr[0] = g_rot_phy_addr; // 旋转到中间缓冲区
    rot.dst_buf.stride[0]   = TEX_W * 2;
    rot.dst_buf.size.width  = TEX_W;
    rot.dst_buf.size.height = TEX_H;
    rot.dst_buf.format      = MPP_FMT_RGB_565;

    // 计算旋转参数
    // theta 随时间增加，g_tick 控制旋转速度
    int theta_idx = (g_tick * 4) & 511;

    // Rot1 参数说明：
    // angle_sin/cos 的硬件定义通常为 12-bit 小数位 (Q12)
    rot.angle_sin = GET_SIN(theta_idx);
    rot.angle_cos = GET_COS(theta_idx);

    // 旋转中心设定
    // 源中心：160, 120 (QVGA)
    rot.src_rot_center.x = TEX_W / 2;
    rot.src_rot_center.y = TEX_H / 2;
    // 目标中心：160, 120 (QVGA)
    rot.dst_rot_center.x = TEX_W / 2;
    rot.dst_rot_center.y = TEX_H / 2;

    // 禁用混合，直接覆盖（为了最高清晰度）
    rot.ctrl.alpha_en = 1;

    // 发送旋转指令
    mpp_ge_rotate(ctx->ge, &rot);
    mpp_ge_emit(ctx->ge);
    mpp_ge_sync(ctx->ge); // 必须同步，等待中间纹理生成完毕

    /* --- STEP 4: GE 硬件全屏缩放 (BitBLT 触发 Scaler) --- */
    struct ge_bitblt blt    = {0};
    blt.src_buf.buf_type    = MPP_PHY_ADDR;
    blt.src_buf.phy_addr[0] = g_rot_phy_addr; // 读取旋转后的纹理
    blt.src_buf.stride[0]   = TEX_W * 2;
    blt.src_buf.size.width  = TEX_W;
    blt.src_buf.size.height = TEX_H;
    blt.src_buf.format      = MPP_FMT_RGB_565;

    blt.dst_buf.buf_type    = MPP_PHY_ADDR;
    blt.dst_buf.phy_addr[0] = phy_addr; // 直接上屏
    blt.dst_buf.stride[0]   = ctx->info.stride;
    blt.dst_buf.size.width  = ctx->info.width;
    blt.dst_buf.size.height = ctx->info.height;
    blt.dst_buf.format      = ctx->info.format;

    // 激活 zoom_pulse：高频心脏脉动 (震颤感)
    int zoom_pulse = (GET_SIN(g_tick << 3) >> 10); // 快速震荡频率为呼吸的 8 倍

    // 开启目标全屏显示
    blt.dst_buf.crop_en     = 1;
    blt.dst_buf.crop.x      = 0;
    blt.dst_buf.crop.y      = 0;
    blt.dst_buf.crop.width  = ctx->info.width;
    blt.dst_buf.crop.height = ctx->info.height;

    // 源裁剪呼吸逻辑：结合低频呼吸与高频脉动
    // 裁剪宽度随时间演化，zoom_pulse 为其增加不稳定的震颤效果
    int crop_w = 240 + (GET_SIN(g_tick) >> 7) + zoom_pulse;
    int crop_h = (crop_w * 240) / 320;

    blt.src_buf.crop_en     = 1;
    blt.src_buf.crop.x      = (320 - crop_w) / 2;
    blt.src_buf.crop.y      = (240 - crop_h) / 2;
    blt.src_buf.crop.width  = crop_w;
    blt.src_buf.crop.height = crop_h;

    blt.ctrl.alpha_en = 1;

    mpp_ge_bitblt(ctx->ge, &blt);

    // 统一提交队列并同步
    mpp_ge_emit(ctx->ge);
    mpp_ge_sync(ctx->ge);

    g_tick++;
}

static void effect_deinit(struct demo_ctx *ctx)
{
    if (g_tex_phy_addr)
        mpp_phy_free(g_tex_phy_addr);
    if (g_rot_phy_addr)
        mpp_phy_free(g_rot_phy_addr);
}

struct effect_ops effect_0021 = {
    .name   = "NO.21 ROTATIONAL SINGULARITY",
    .init   = effect_init,
    .draw   = effect_draw,
    .deinit = effect_deinit,
};

REGISTER_EFFECT(effect_0021);
