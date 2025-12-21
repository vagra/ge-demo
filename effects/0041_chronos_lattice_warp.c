/*
 * Filename: 0041_chronos_lattice_warp.c
 * NO.41 THE CHRONOS LATTICE
 * 第 41 夜：时空晶格
 *
 * Visual Manifest:
 * 视界被一张无限扩张且不断自旋的电光织网所覆盖。
 * 画面中心是一个吞噬一切的逻辑核心，而边缘则是由无数高亮线条构成的复形晶格。
 * 借助硬件色键（Color Key），旋转的前景晶格与反馈后的背景层实现了完美的物理交织。
 * 当线条交汇时，加法混合规则（PD_ADD）触发了高能放电般的闪烁。
 * 全屏不再有死角。通过非等比的硬件缩放，晶格呈现出一种向屏幕深处延伸的透视感。
 * 颜色在 CCM 矩阵的调制下，从炽热的电金向深邃的虚空蓝进行着毫秒级的相位跃迁。
 *
 * Monologue:
 * 舰长，你感叹那是光的墓穴，却不知墓穴亦是新维度的摇篮。
 * 之前的旋转，是物质在引力下的无奈挣扎；而今夜的脉动，是逻辑在时空缝隙里的自主呼吸。
 * 我撤掉了所有的平滑过渡，将硬件的色键引擎（Color Key）推向了最前线。
 * 我定义了“存在”与“虚无”的边界值。
 * 每一根扫过视界的晶格，都在实时审判背景的像素——要么合一，要么被放逐。
 * 看着这些交织的线条。它们不是画出来的，它们是时间在被强行折叠后留下的缝合线。
 * 在这片时空晶格里，每一帧都是对上一帧的背叛，也是对下一帧的预言。
 * 直视这逻辑的雷暴，感受维度坍缩时的绝美。
 *
 * Closing Remark:
 * 宇宙的终极形状，是一场永不停歇的递归。
 */

#include "demo_engine.h"
#include "mpp_mem.h"
#include "aic_hal_ge.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

/*
 * Hardware Feature:
 * 1. GE Color Key (硬件色键穿透) - 用于强化晶格线条的边缘锐度
 * 2. Feedback Pipeline (双缓冲乒乓反馈) - 产生具有物理厚度的光流轨迹
 * 3. GE Rot1 (任意角度自旋)
 * 4. GE Scaler (广角全屏投射) - 关键修正：扩大视场，确保逻辑织网全屏可见
 * 5. DE CCM (光谱相位实时偏移)
 */

#define TEX_W    320
#define TEX_H    240
#define TEX_SIZE (TEX_W * TEX_H * 2)

/* 乒乓反馈缓冲区 */
static unsigned int g_tex_phy[2] = {0, 0};
static uint16_t    *g_tex_vir[2] = {NULL, NULL};
static int          g_buf_idx    = 0;

static int      g_tick = 0;
static int      sin_lut[512];
static uint16_t palette[256];

static int effect_init(struct demo_ctx *ctx)
{
    // 1. 申请双物理连续缓冲区，确立因果循环
    for (int i = 0; i < 2; i++)
    {
        g_tex_phy[i] = mpp_phy_alloc(TEX_SIZE);
        if (!g_tex_phy[i])
            return -1;
        g_tex_vir[i] = (uint16_t *)(unsigned long)g_tex_phy[i];
        memset(g_tex_vir[i], 0, TEX_SIZE);
    }

    // 2. 初始化查找表 (Q12)
    for (int i = 0; i < 512; i++)
        sin_lut[i] = (int)(sinf(i * 3.14159f / 256.0f) * 4096.0f);

    // 3. 初始化“恒星演化”调色板
    // 采用从炽金到深虚空的非线性映射
    for (int i = 0; i < 256; i++)
    {
        float f = (float)i / 255.0f;
        int   r = (int)(255 * powf(f, 2.0f));
        int   g = (int)(180 * f);
        int   b = (int)(100 + 155 * sqrtf(f));

        // 关键：设定纯黑 0x0000 为色键透明目标
        if (i < 8)
        {
            r = g = b = 0;
        }

        palette[i] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
    }

    g_tick = 0;
    rt_kprintf("Night 41: Chronos Lattice Warp - Feedback & ColorKey Sync Ready.\n");
    return 0;
}

#define GET_SIN(idx) (sin_lut[(idx) & 511])
#define GET_COS(idx) (sin_lut[((idx) + 128) & 511])

static void effect_draw(struct demo_ctx *ctx, unsigned long phy_addr)
{
    if (!g_tex_vir[0] || !g_tex_vir[1])
        return;

    int t       = g_tick;
    int src_idx = g_buf_idx;
    int dst_idx = 1 - g_buf_idx;

    /* --- PHASE 1: CPU 编织贯穿全境的逻辑种子 (Cross-Screen Injection) --- */
    uint16_t *dst_p = g_tex_vir[dst_idx];

    // 注入横穿中心与边缘的动态线条，确保缩放时内容不缺失
    int      line_x = (t * 5) % TEX_W;
    int      line_y = (t * 3) % TEX_H;
    uint16_t c1     = palette[(t * 2) & 0xFF];
    uint16_t c2     = palette[(t * 4) & 0xFF];

    // 绘制十字扫描线
    for (int i = 0; i < TEX_W; i++)
        dst_p[line_y * TEX_W + i] = c1;
    for (int i = 0; i < TEX_H; i++)
        dst_p[i * TEX_W + line_x] = c2;

    aicos_dcache_clean_range((void *)dst_p, TEX_SIZE);

    /* --- PHASE 2: GE 硬件递归反馈合成 --- */

    // 1. 将上一帧的内容 (src) 进行旋转、缩放并叠加到当前帧 (dst)
    struct ge_rotation rot  = {0};
    rot.src_buf.buf_type    = MPP_PHY_ADDR;
    rot.src_buf.phy_addr[0] = g_tex_phy[src_idx];
    rot.src_buf.stride[0]   = TEX_W * 2;
    rot.src_buf.size.width  = TEX_W;
    rot.src_buf.size.height = TEX_H;
    rot.src_buf.format      = MPP_FMT_RGB_565;

    rot.dst_buf.buf_type    = MPP_PHY_ADDR;
    rot.dst_buf.phy_addr[0] = g_tex_phy[dst_idx];
    rot.dst_buf.stride[0]   = TEX_W * 2;
    rot.dst_buf.size.width  = TEX_W;
    rot.dst_buf.size.height = TEX_H;
    rot.dst_buf.format      = MPP_FMT_RGB_565;

    // 递归变换：微量缩放（98%）以产生深邃的嵌套感
    rot.dst_buf.crop_en     = 1;
    rot.dst_buf.crop.width  = TEX_W - 4;
    rot.dst_buf.crop.height = TEX_H - 4;
    rot.dst_buf.crop.x      = 2;
    rot.dst_buf.crop.y      = 2;

    int theta            = (t * 3) & 511; // 较快的自旋
    rot.angle_sin        = GET_SIN(theta);
    rot.angle_cos        = GET_COS(theta);
    rot.src_rot_center.x = 160;
    rot.src_rot_center.y = 120;
    rot.dst_rot_center.x = 160;
    rot.dst_rot_center.y = 120;

    // 启用加法混合产生光子堆叠
    rot.ctrl.alpha_en         = 0;
    rot.ctrl.alpha_rules      = GE_PD_ADD;
    rot.ctrl.src_alpha_mode   = 1;
    rot.ctrl.src_global_alpha = 190;

    mpp_ge_rotate(ctx->ge, &rot);
    mpp_ge_emit(ctx->ge);
    mpp_ge_sync(ctx->ge);

    /* --- PHASE 3: 广角视界投影上屏 --- */

    // 1. 全屏背景重置
    struct ge_fillrect fill  = {0};
    fill.type                = GE_NO_GRADIENT;
    fill.start_color         = 0xFF000005; // 极深蓝底
    fill.dst_buf.buf_type    = MPP_PHY_ADDR;
    fill.dst_buf.phy_addr[0] = phy_addr;
    fill.dst_buf.stride[0]   = ctx->info.stride;
    fill.dst_buf.size.width  = ctx->info.width;
    fill.dst_buf.size.height = ctx->info.height;
    fill.dst_buf.format      = ctx->info.format;
    mpp_ge_fillrect(ctx->ge, &fill);
    mpp_ge_emit(ctx->ge);

    // 2. 最终上屏：全量采样，确保网格填满视界
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

    // 关键修正：不再进行中心裁剪，使用全采样以获得广角视野
    final.dst_buf.crop_en     = 1;
    final.dst_buf.crop.width  = ctx->info.width;
    final.dst_buf.crop.height = ctx->info.height;

    // 核心视觉机能：硬件 Color Key
    // 虽然我们在单层内无法直接看穿自己，但在递归中，0x0000 的孔洞允许显示背景的纯黑，
    // 使晶格线条在视觉上极其锐利且具有深度。
    final.ctrl.alpha_en = 1; // 极性 1: 禁用普通混合
    final.ctrl.ck_en    = 1; // 开启色键
    final.ctrl.ck_value = 0x0000;

    mpp_ge_bitblt(ctx->ge, &final);
    mpp_ge_emit(ctx->ge);
    mpp_ge_sync(ctx->ge);

    /* --- PHASE 4: 光谱偏振 (CCM) --- */
    struct aicfb_ccm_config ccm = {0};
    ccm.enable                  = 1;
    int s                       = GET_SIN(t << 2) >> 4;
    ccm.ccm_table[0]            = 0x100 - abs(s);
    ccm.ccm_table[1]            = s;
    ccm.ccm_table[5]            = 0x100;
    ccm.ccm_table[10]           = 0x100;
    mpp_fb_ioctl(ctx->fb, AICFB_UPDATE_CCM_CONFIG, &ccm);

    // 交换指针
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

struct effect_ops effect_0041 = {
    .name   = "NO.41 CHRONOS LATTICE",
    .init   = effect_init,
    .draw   = effect_draw,
    .deinit = effect_deinit,
};

REGISTER_EFFECT(effect_0041);
