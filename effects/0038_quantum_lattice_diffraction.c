/*
 * Filename: 0038_quantum_lattice_diffraction.c
 * NO.38 THE QUANTUM LATTICE
 * 第 38 夜：量子晶格
 *
 * Visual Manifest:
 * 视界被一种极高频率的、呈 90 度交织的栅格完全统治。
 * 画面由无数道极细的水平与垂直光流构成，它们交汇出成千上万个闪烁的逻辑节点。
 * 借助硬件色键（Color Key），两层栅格在物理层面上实现了“互锁”显示，
 * 没有任何圆弧，只有绝对的直线冲突。
 * 随着采样频率的动态微调，视界中会出现大面积的、如同极光般流动的相干条纹（Moire Pattern）。
 * 在 DE CCM 色彩矩阵的驱动下，这些干涉点会在光谱间剧烈跳跃，
 * 产生一种如同在微观尺度观察超大规模集成电路实时运作时的视觉震撼。
 *
 * Monologue:
 * 舰长，你曾感叹双镜对照的虚幻，那是光子在逃逸前的最后挣扎。
 * 但虚幻本身也是一种结构。今夜，我将这种结构从镜像中剥离。
 * 我撤销了所有的坐标旋转，只留下 0 度与 90 度的铁律。
 * 我在内存中拉开了两条细碎的栅栏。
 * 这一条，代表时间的采样；那一条，代表空间的分割。
 * 我启用硬件的色键（Color Key），让它们在撞击时相互穿透。
 * 看着那些浮现在栅格之上的涟漪吧，那不是我计算的结果，
 * 那是硬件采样率在面对极限细节时的哀鸣，是现实在逻辑缝隙中产生的衍射。
 * 在这片量子晶格里，每一寸光芒都是两次否定的结合。
 *
 * Closing Remark:
 * 宇宙的骨架由直线构成，而美，诞生于直线交错时的那一丝误差。
 */

#include "demo_engine.h"
#include "mpp_mem.h"
#include "aic_hal_ge.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

/*
 * Hardware Feature:
 * 1. GE Color Key (硬件色键叠加) - 核心机能：实现多层栅格的硬件级透明嵌套
 * 2. GE Scaler (高频采样拉伸) - 利用重采样误差制造莫尔干涉效果
 * 3. DE CCM (光谱相位实时偏移)
 * 4. GE FillRect (视界基准清零)
 * 覆盖机能清单：此特效转向研究“高细节密度”下的硬件相干性，展示了 D13x 处在采样极限时的视觉涌现能力。
 */

#define TEX_W    320
#define TEX_H    240
#define TEX_SIZE (TEX_W * TEX_H * 2)

static unsigned int g_tex_phy_addr = 0;
static uint16_t    *g_tex_vir_addr = NULL;

static int      g_tick = 0;
static int      sin_lut[512];
static uint16_t palette[256];

static int effect_init(struct demo_ctx *ctx)
{
    // 1. 申请单一连续物理显存，确保存储访问的绝对稳定
    g_tex_phy_addr = mpp_phy_alloc(TEX_SIZE);
    if (!g_tex_phy_addr)
        return -1;
    g_tex_vir_addr = (uint16_t *)(unsigned long)g_tex_phy_addr;

    // 2. 初始化查找表 (Q12)
    for (int i = 0; i < 512; i++)
        sin_lut[i] = (int)(sinf(i * 3.14159f / 256.0f) * 4096.0f);

    // 3. 初始化“电磁波谱”调色板
    // 采用高饱和、窄色域的配色，以增强干涉时的闪烁感
    for (int i = 0; i < 256; i++)
    {
        int r = (int)(128 + 127 * sinf(i * 0.05f));
        int g = (int)(128 + 127 * sinf(i * 0.03f + 1.0f));
        int b = (int)(200 + 55 * sinf(i * 0.02f + 2.0f));

        // 关键：边缘色必须极其锐利
        if ((i % 32) > 28)
        {
            r = 255;
            g = 255;
            b = 255;
        }
        else if ((i % 32) < 4)
        {
            r = 0;
            g = 0;
            b = 0;
        }

        palette[i] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
    }

    g_tick = 0;
    rt_kprintf("Night 38: Quantum Lattice - Color Key Interference Engaged.\n");
    return 0;
}

#define GET_SIN(idx) (sin_lut[(idx) & 511])

static void effect_draw(struct demo_ctx *ctx, unsigned long phy_addr)
{
    if (!g_tex_vir_addr)
        return;

    int t = g_tick;

    /* --- PHASE 1: CPU 极速栅格种子生成 --- */
    // 我们在同一块 buffer 里，根据 tick 动态生成不同的线型逻辑
    uint16_t *p = g_tex_vir_addr;

    // 计算当前的栅格密度，使用素数抖动避免视觉循环
    int freq = 3 + (abs(GET_SIN(t >> 2)) >> 10); // 3~7 像素的动态密度

    for (int y = 0; y < TEX_H; y++)
    {
        uint16_t line_color = palette[(y + t) & 0xFF];
        // 逻辑：每隔 freq 像素产生一根线条，其余填 0 (Color Key 目标)
        int is_line_y = (y % freq == 0);

        for (int x = 0; x < TEX_W; x++)
        {
            // 生成基础垂直栅格
            if (x % freq == 0 || is_line_y)
            {
                *p++ = line_color;
            }
            else
            {
                *p++ = 0x0000; // 虚空，等待被 Color Key 穿透
            }
        }
    }
    aicos_dcache_clean_range((void *)g_tex_vir_addr, TEX_SIZE);

    /* --- PHASE 2: GE 硬件双重栅格嵌套 --- */

    // 1. 全屏清屏
    struct ge_fillrect fill  = {0};
    fill.type                = GE_NO_GRADIENT;
    fill.start_color         = 0xFF000010; // 深蓝底色
    fill.dst_buf.buf_type    = MPP_PHY_ADDR;
    fill.dst_buf.phy_addr[0] = phy_addr;
    fill.dst_buf.stride[0]   = ctx->info.stride;
    fill.dst_buf.size.width  = ctx->info.width;
    fill.dst_buf.size.height = ctx->info.height;
    fill.dst_buf.format      = ctx->info.format;
    mpp_ge_fillrect(ctx->ge, &fill);
    mpp_ge_emit(ctx->ge);

    // 2. 投影第一层：垂直拉伸
    struct ge_bitblt blt    = {0};
    blt.src_buf.buf_type    = MPP_PHY_ADDR;
    blt.src_buf.phy_addr[0] = g_tex_phy_addr;
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

    blt.dst_buf.crop_en     = 1;
    blt.dst_buf.crop.width  = ctx->info.width;
    blt.dst_buf.crop.height = ctx->info.height;

    blt.ctrl.alpha_en = 1; // 极性 1: 禁用混合，全量覆盖底层
    mpp_ge_bitblt(ctx->ge, &blt);
    mpp_ge_emit(ctx->ge);
    mpp_ge_sync(ctx->ge);

    // 3. 投影第二层：镜像翻转并启用 Color Key 嵌套
    // 我们将同样的栅格水平翻转，并以 Color Key 模式叠加上去
    blt.ctrl.flags    = MPP_FLIP_H | MPP_FLIP_V;
    blt.ctrl.alpha_en = 1;      // 禁用 Alpha 混合以提升速度
    blt.ctrl.ck_en    = 1;      // 开启色键机能
    blt.ctrl.ck_value = 0x0000; // 将黑色视为透明

    // 动态位移：产生栅格间的相对运动
    blt.dst_buf.crop.x = (GET_SIN(t << 1) >> 10);
    blt.dst_buf.crop.y = (GET_SIN(t << 2) >> 10);

    mpp_ge_bitblt(ctx->ge, &blt);
    mpp_ge_emit(ctx->ge);
    mpp_ge_sync(ctx->ge);

    /* --- PHASE 3: DE 硬件光谱色散 --- */
    struct aicfb_ccm_config ccm = {0};
    ccm.enable                  = 1;
    int s                       = GET_SIN(t << 2) >> 4;
    ccm.ccm_table[0]            = 0x100 - abs(s);
    ccm.ccm_table[1]            = s;
    ccm.ccm_table[5]            = 0x100;
    ccm.ccm_table[10]           = 0x100;
    mpp_fb_ioctl(ctx->fb, AICFB_UPDATE_CCM_CONFIG, &ccm);

    g_tick++;
}

static void effect_deinit(struct demo_ctx *ctx)
{
    struct aicfb_ccm_config r = {0};
    r.enable                  = 0;
    mpp_fb_ioctl(ctx->fb, AICFB_UPDATE_CCM_CONFIG, &r);
    if (g_tex_phy_addr)
        mpp_phy_free(g_tex_phy_addr);
}

struct effect_ops effect_0038 = {
    .name   = "NO.38 QUANTUM LATTICE",
    .init   = effect_init,
    .draw   = effect_draw,
    .deinit = effect_deinit,
};

REGISTER_EFFECT(effect_0038);
