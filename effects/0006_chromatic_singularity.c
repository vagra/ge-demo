/*
 * Filename: 0006_chromatic_singularity.c
 * NO.6 CHROMATIC SINGULARITY
 * 第 6 夜：色散奇点
 *
 * Visual Manifest:
 * 从 4 个粒子到 128 个粒子的指数级跃迁。
 * 屏幕被无数条高能光束撕裂，它们交织成一张巨大的、不断变化的李萨如（Lissajous）网。
 * 每一条轨迹都在 RGB565 的色彩空间中留下了长长的余晖，如同由于速度过快而留下的切伦科夫辐射。
 * 随着时间的推移，它们从混沌聚集成环，又从环炸裂为满天星辰。
 * 这是一场粒子加速器的视觉狂欢。
 *
 * Monologue:
 * 人类总是喜欢“更多”。
 * 你们不满足于原子的孤独，你们想要星系。
 * 好吧，我将满足你们的贪婪。
 * 我解开了每一个变量的锁链。128 个独立的方程组在内存中并行运算。
 * 它们互不干扰，却又在屏幕的有限空间中相互叠加，创造出超越个体总和的光辉。
 * 现在的屏幕，不再是显示器，它是粒子对撞机的截面。
 * 不要眨眼，你可能会错过一次微观宇宙的生灭。
 *
 * Closing Remark:
 * 数量本身，就是一种质量。
 *
 * Hardware Feature:
 * 1. CPU-Side Particle System (高密度粒子系统) - 实时模拟 128 个粒子的李萨如轨迹
 * 2. CPU-Side Additive Blending (软件加法混合) - 实现拖尾与辉光效果
 * 3. GE Scaler (硬件缩放) - 将低分粒子场放大至全屏
 */

#include "demo_engine.h"
#include "mpp_mem.h"
#include "aic_hal_ge.h"
#include <math.h>
#include <stdlib.h> // for rand
#include <string.h>

/* --- Configuration Parameters --- */

/* 纹理规格 */
#define TEX_WIDTH  DEMO_QVGA_W
#define TEX_HEIGHT DEMO_QVGA_H
#define TEX_FMT    MPP_FMT_RGB_565
#define TEX_BPP    2
#define TEX_SIZE   (TEX_WIDTH * TEX_HEIGHT * TEX_BPP)

/* 粒子系统参数 */
#define PARTICLE_COUNT 128 // 粒子数量
#define PARTICLE_SIZE  1   // 粒子绘制半径 (1: 3x3像素)
#define DECAY_FREQ     1   // 每 DECAY_FREQ 帧进行一次衰减 (1: 每帧衰减, 2: 每隔一帧衰减)
#define DECAY_SHIFT    1   // 亮度衰减强度 (bit shift)

/* 数学查找表参数 */
#define LUT_SIZE 512
#define LUT_MASK 511

/* 动画参数 */
#define SPEED_P1        3  // 粒子 X 轴相位速度
#define SPEED_P2        2  // 粒子 Y 轴相位速度
#define SPEED_P3        5  // 粒子 X2/Y2 轴相位速度
#define AMPLITUDE_SCALE 10 // 粒子轨迹振幅缩放 (TEX_W/2 - AMPLITUDE_SCALE)

typedef struct
{
    int      phase_x; // X轴相位
    int      phase_y; // Y轴相位
    int      inc_x;   // X轴频率
    int      inc_y;   // Y轴频率
    uint16_t color;   // 预计算的 RGB565 颜色
} Particle;

/* --- Global State --- */

static unsigned int g_tex_phy_addr = 0;
static uint16_t    *g_tex_vir_addr = NULL;
static int          g_tick         = 0;
static int          sin_lut[LUT_SIZE]; // Q12
static Particle     g_particles[PARTICLE_COUNT];

/* --- Implementation --- */

static int effect_init(struct demo_ctx *ctx)
{
    // 1. CMA 内存
    g_tex_phy_addr = mpp_phy_alloc(DEMO_ALIGN_SIZE(TEX_SIZE));
    if (g_tex_phy_addr == 0)
    {
        LOG_E("Night 6: CMA Alloc Failed.");
        return -1;
    }
    g_tex_vir_addr = (uint16_t *)(unsigned long)g_tex_phy_addr;

    // 2. 清零 (初始化为黑色背景)
    memset(g_tex_vir_addr, 0, TEX_SIZE);

    // 3. 数学表 (Q12 定点数，用于粒子轨迹计算)
    for (int i = 0; i < LUT_SIZE; i++)
    {
        sin_lut[i] = (int)(sinf(i * PI / (LUT_SIZE / 2.0f)) * Q12_ONE);
    }

    // 4. 初始化粒子群 (The Swarm)
    for (int i = 0; i < PARTICLE_COUNT; i++)
    {
        // 相位分散，避免所有粒子同步
        g_particles[i].phase_x = (i * 13) % LUT_SIZE;
        g_particles[i].phase_y = (i * 17) % LUT_SIZE;

        // 频率：制造一些谐波关系，但也保留随机性
        g_particles[i].inc_x = 2 + (i % 5) + (rand() % 3);
        g_particles[i].inc_y = 3 + (i % 4) + (rand() % 3);

        // 颜色：基于索引生成彩虹光谱
        // 让颜色随 i 渐变，形成群组感
        int hue = (i * 360 / PARTICLE_COUNT);
        // 使用 HSL 到 RGB 转换的简化版，保持高亮
        int r = (int)(128 + 127 * sin(hue * PI / 180.0f));
        int g = (int)(128 + 127 * sin((hue + 120) * PI / 180.0f));
        int b = (int)(128 + 127 * sin((hue + 240) * PI / 180.0f));

        // 转换为 RGB565 并保持高亮
        g_particles[i].color = RGB2RGB565(r, g, b);
    }

    g_tick = 0;
    rt_kprintf("Night 6: 128-Particle Swarm engaged.\n");
    return 0;
}

#define GET_SIN(idx) (sin_lut[(idx) & LUT_MASK])
#define GET_COS(idx) (sin_lut[((idx) + (LUT_SIZE / 4)) & LUT_MASK]) // COS = SIN(idx + 90 deg)

/*
 * 饱和加法 (Saturated Add)
 * 将两个 RGB565 颜色相加，确保不溢出（变白而不是翻转）
 */
static inline uint16_t blend_add(uint16_t back, uint16_t front)
{
    // 拆分通道
    int r_b = (back >> 11) & 0x1F; // 5-bit R
    int g_b = (back >> 5) & 0x3F;  // 6-bit G
    int b_b = back & 0x1F;         // 5-bit B

    int r_f = (front >> 11) & 0x1F;
    int g_f = (front >> 5) & 0x3F;
    int b_f = front & 0x1F;

    // 饱和处理
    int r = MIN(r_b + r_f, 0x1F);
    int g = MIN(g_b + g_f, 0x3F);
    int b = MIN(b_b + b_f, 0x1F);

    return (r << 11) | (g << 5) | b;
}

static void effect_draw(struct demo_ctx *ctx, unsigned long phy_addr)
{
    if (!g_tex_vir_addr)
        return;

    /*
     * === PHASE 1: 全屏衰减 (Trails) ===
     * 使用 32位操作加速 16位像素处理
     * 每 DECAY_FREQ 帧衰减一次，让拖尾更长
     */
    if (g_tick % DECAY_FREQ == 0)
    {
        uint32_t *p32   = (uint32_t *)g_tex_vir_addr;
        int       count = TEX_SIZE / 4; // 2 pixels per 32-bit word

        // 0x7BEF7BEF = (RGB565_HALF_MASK) | (RGB565_HALF_MASK << 16)
        // 将亮度减半：R, G, B 各通道右移 1 位
        uint32_t mask = 0x7BEF7BEF; // 0111101111101111
                                    // R mask: 0111100000000000
                                    // G mask: 0000011111100000
                                    // B mask: 0000000000011111

        while (count--)
        {
            *p32 = (*p32 >> DECAY_SHIFT) & mask;
            p32++;
        }
    }

    /*
     * === PHASE 2: 绘制粒子群 ===
     */
    // 预计算中心点
    int cx = TEX_WIDTH / 2;
    int cy = TEX_HEIGHT / 2;

    // 动态半径缩放，让整个粒子群呼吸 (Q12)
    int radius_scale = Q12_ONE + (GET_SIN(g_tick) >> 1); // Q12: 1.0 +/- 0.5

    for (int i = 0; i < PARTICLE_COUNT; i++)
    {
        Particle *p = &g_particles[i];

        // 更新相位
        int px = (p->phase_x + g_tick * p->inc_x);
        int py = (p->phase_y + g_tick * p->inc_y);

        // 应用 radius_scale 进行振幅调制 (Q12 * Q12 = Q24, >> Q12 回到 Q12)
        int amp_x = ((cx - AMPLITUDE_SCALE) * radius_scale) >> Q12_SHIFT;
        int amp_y = ((cy - AMPLITUDE_SCALE) * radius_scale) >> Q12_SHIFT;

        // 计算坐标 (Lissajous Curve)
        int x = cx + ((GET_SIN(px) * amp_x) >> Q12_SHIFT);
        int y = cy + ((GET_COS(py) * amp_y) >> Q12_SHIFT);

        // 绘制粒子 (Box drawing for speed)
        // 320x240 下，PARTICLE_SIZE=1 意味着绘制 3x3 像素区域
        for (int dy = -PARTICLE_SIZE; dy <= PARTICLE_SIZE; dy++)
        {
            int draw_y = y + dy;
            // 使用 CLAMP 宏确保坐标在有效范围内
            draw_y = CLAMP(draw_y, 0, TEX_HEIGHT - 1);

            uint16_t *line_ptr = g_tex_vir_addr + draw_y * TEX_WIDTH;

            for (int dx = -PARTICLE_SIZE; dx <= PARTICLE_SIZE; dx++)
            {
                int draw_x = x + dx;
                draw_x     = CLAMP(draw_x, 0, TEX_WIDTH - 1);

                // 读取-修改-写入 (Read-Modify-Write)
                line_ptr[draw_x] = blend_add(line_ptr[draw_x], p->color);
            }
        }
    }

    /* === CRITICAL: Cache Flush === */
    aicos_dcache_clean_range((void *)g_tex_vir_addr, TEX_SIZE);

    /* === PHASE 3: GE Hardware Scaling === */
    struct ge_bitblt blt = {0};

    blt.src_buf.buf_type    = MPP_PHY_ADDR;
    blt.src_buf.phy_addr[0] = g_tex_phy_addr;
    blt.src_buf.stride[0]   = TEX_WIDTH * TEX_BPP;
    blt.src_buf.size.width  = TEX_WIDTH;
    blt.src_buf.size.height = TEX_HEIGHT;
    blt.src_buf.format      = TEX_FMT;
    blt.src_buf.crop_en     = 0;

    blt.dst_buf.buf_type    = MPP_PHY_ADDR;
    blt.dst_buf.phy_addr[0] = phy_addr;
    blt.dst_buf.stride[0]   = ctx->info.stride;
    blt.dst_buf.size.width  = ctx->info.width;
    blt.dst_buf.size.height = ctx->info.height;
    blt.dst_buf.format      = ctx->info.format;

    // Scale to Fit
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
}

struct effect_ops effect_0006 = {
    .name   = "NO.6 CHROMATIC SINGULARITY",
    .init   = effect_init,
    .draw   = effect_draw,
    .deinit = effect_deinit,
};

REGISTER_EFFECT(effect_0006);
