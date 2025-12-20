/*
 * Filename: 0006_chromatic_singularity_v3.c
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
 */

#include "demo_engine.h"
#include "mpp_mem.h"
#include "aic_hal_ge.h"
#include <math.h>
#include <stdlib.h> // for rand

/*
 * === 混合渲染架构 (High-Density Particle System) ===
 * 1. 纹理: 320x240 RGB565 (150KB)
 * 2. 核心:
 *    - 128 粒子并行计算
 *    - 软件加法混合 (Software Additive Blending)
 *    - 软件全屏衰减 (Software Trail Decay)
 */
#define TEX_W    320
#define TEX_H    240
#define TEX_SIZE (TEX_W * TEX_H * 2)

/* 粒子数量：暴力提升至 128 */
#define PARTICLE_COUNT 128

typedef struct
{
    int      phase_x; // X轴相位
    int      phase_y; // Y轴相位
    int      inc_x;   // X轴频率
    int      inc_y;   // Y轴频率
    uint16_t color;   // 预计算的 RGB565 颜色
} Particle;

static unsigned int g_tex_phy_addr = 0;
static uint16_t    *g_tex_vir_addr = NULL;
static int          g_tick         = 0;
static int          sin_lut[512]; // Q12
static Particle     g_particles[PARTICLE_COUNT];

static int effect_init(struct demo_ctx *ctx)
{
    // 1. CMA 内存
    g_tex_phy_addr = mpp_phy_alloc(TEX_SIZE);
    if (g_tex_phy_addr == 0)
        return -1;
    g_tex_vir_addr = (uint16_t *)(unsigned long)g_tex_phy_addr;

    // 2. 清零
    memset(g_tex_vir_addr, 0, TEX_SIZE);

    // 3. 数学表
    for (int i = 0; i < 512; i++)
    {
        sin_lut[i] = (int)(sinf(i * 3.14159f / 256.0f) * 4096.0f);
    }

    // 4. 初始化粒子群 (The Swarm)
    for (int i = 0; i < PARTICLE_COUNT; i++)
    {
        // 相位分散
        g_particles[i].phase_x = (i * 13) % 512;
        g_particles[i].phase_y = (i * 17) % 512;

        // 频率：制造一些谐波关系，但也保留随机性
        g_particles[i].inc_x = 2 + (i % 5) + (rand() % 3);
        g_particles[i].inc_y = 3 + (i % 4) + (rand() % 3);

        // 颜色：基于索引生成彩虹光谱
        // 让颜色随 i 渐变，形成群组感
        int hue = (i * 360 / PARTICLE_COUNT);
        int r   = (int)(128 + 127 * sin(hue * 3.14f / 180.0f));
        int g   = (int)(128 + 127 * sin((hue + 120) * 3.14f / 180.0f));
        int b   = (int)(128 + 127 * sin((hue + 240) * 3.14f / 180.0f));

        // 转换为 RGB565 并保持高亮
        g_particles[i].color = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
    }

    g_tick = 0;
    rt_kprintf("Night 6: 128-Particle Swarm engaged.\n");
    return 0;
}

#define GET_SIN(idx) (sin_lut[(idx) & 511])
#define GET_COS(idx) (sin_lut[((idx) + 128) & 511])

/*
 * 饱和加法 (Saturated Add)
 * 将两个 RGB565 颜色相加，确保不溢出（变白而不是翻转）
 */
static inline uint16_t blend_add(uint16_t back, uint16_t front)
{
    // 拆分通道
    int r = ((back & 0xF800) + (front & 0xF800)) >> 11;
    int g = ((back & 0x07E0) + (front & 0x07E0)) >> 5;
    int b = (back & 0x001F) + (front & 0x001F);

    // 饱和处理
    if (r > 31)
        r = 31;
    if (g > 63)
        g = 63;
    if (b > 31)
        b = 31;

    return (r << 11) | (g << 5) | b;
}

static void effect_draw(struct demo_ctx *ctx, unsigned long phy_addr)
{
    if (!g_tex_vir_addr)
        return;

    /*
     * === PHASE 1: 全屏衰减 (Trails) ===
     * 使用 32位操作加速 16位像素处理
     * 每 2 帧衰减一次，让拖尾更长
     */
    if (g_tick & 1)
    {
        uint32_t *p32   = (uint32_t *)g_tex_vir_addr;
        int       count = TEX_SIZE / 4;

        // 0x7BEF7BEF = (RGB565_MASK >> 1) | (RGB565_MASK >> 1) << 16
        // 将亮度减半
        uint32_t mask = 0x7BEF7BEF;

        while (count--)
        {
            *p32 = (*p32 >> 1) & mask;
            p32++;
        }
    }

    /*
     * === PHASE 2: 绘制粒子群 ===
     */
    // 预计算中心点
    int cx = TEX_W / 2;
    int cy = TEX_H / 2;

    // 动态半径缩放，让整个粒子群呼吸
    int radius_scale = 3000 + (GET_SIN(g_tick) >> 1); // Q12

    for (int i = 0; i < PARTICLE_COUNT; i++)
    {
        Particle *p = &g_particles[i];

        // 更新相位
        int px = (p->phase_x + g_tick * p->inc_x);
        int py = (p->phase_y + g_tick * p->inc_y);

        // 应用 radius_scale 进行振幅调制
        // (cx - 10) 是最大 X 半径
        int amp_x = ((cx - 10) * radius_scale) >> 12;
        int amp_y = ((cy - 10) * radius_scale) >> 12;

        // 计算坐标 (Lissajous)
        // x = A * sin(at + d)
        int x = cx + ((GET_SIN(px) * amp_x) >> 12);
        int y = cy + ((GET_COS(py) * amp_y) >> 12);

        // 引入 Z 轴模拟 (Size modulation)
        // 利用另一个正弦波来模拟 3D 深度，改变点的大小
        int z    = GET_SIN(px + py);
        int size = (z > 0) ? 2 : 1; // 近大远小

        // 绘制粒子 (Box drawing for speed)
        // 320x240 下，1个像素已经很大了，画 2x2 或 3x3 足够
        for (int dy = -size; dy <= size; dy++)
        {
            int draw_y = y + dy;
            if (draw_y < 0 || draw_y >= TEX_H)
                continue;

            // 优化指针计算：移出内循环
            uint16_t *line_ptr = g_tex_vir_addr + draw_y * TEX_W;

            for (int dx = -size; dx <= size; dx++)
            {
                int draw_x = x + dx;
                if (draw_x < 0 || draw_x >= TEX_W)
                    continue;

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
    blt.src_buf.stride[0]   = TEX_W * 2;
    blt.src_buf.size.width  = TEX_W;
    blt.src_buf.size.height = TEX_H;
    blt.src_buf.format      = MPP_FMT_RGB_565;
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
    blt.ctrl.alpha_en = 0;

    int ret = mpp_ge_bitblt(ctx->ge, &blt);
    if (ret < 0)
        rt_kprintf("GE Error\n");

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
