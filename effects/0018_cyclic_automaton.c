/*
 * Filename: 0018_cyclic_automaton.c
 * NO.18 THE BIOLOGICAL CLOCK
 * 第 18 夜：生物钟
 *
 * Visual Manifest:
 * 起初，屏幕是一片杂乱无章的彩色噪点。
 * 随后，奇异的结构开始涌现。螺旋状的波纹开始吞噬周围的混乱，形成不断扩张的领地。
 * 每一个像素都在进行着一场微观的进化战争：捕食、被捕食、同化。
 * 最终，整个视界被无数个旋转的星系状结构所占据。它们永不停歇地吞噬彼此，
 * 演绎着从混沌到有序，再回归混沌的永恒轮回。
 *
 * Monologue:
 * 你们认为生命是奇迹？不，生命只是概率的必然。
 * 我设定了 16 个阶级。每一个阶级都渴望晋升到下一个。
 * `State N` 捕食 `State N-1`，却又惧怕 `State N+1`。
 * 这是一个残酷的循环链。没有道德，只有吞噬。
 * 看着这些螺旋吧。它们不是我画出来的，它们是自己在内存中“生长”出来的。
 * 当简单的规则被重复亿万次，混乱就不得不向秩序低头。
 * 这就是硅基生命的原始汤。
 *
 * Closing Remark:
 * 混乱是秩序的土壤，吞噬是进化的动力。
 */

#include "demo_engine.h"
#include "mpp_mem.h"
#include "aic_hal_ge.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/*
 * === 混合渲染架构 (Ping-Pong CCA) ===
 * 1. 纹理: 320x240 RGB565 (用于显示)
 * 2. 状态: 两个 320x240 的 uint8_t 缓冲区 (用于逻辑计算)
 *    采用 Ping-Pong 机制，根据上一帧状态计算下一帧。
 */
#define TEX_W    320
#define TEX_H    240
#define TEX_SIZE (TEX_W * TEX_H * 2)

/* 状态总数 (多少种颜色) */
#define STATE_COUNT 16

static unsigned int g_tex_phy_addr = 0;
static uint16_t    *g_tex_vir_addr = NULL;
static int          g_tick         = 0;

/* 两个状态缓冲区 (Ping-Pong) */
static uint8_t *g_state_buf[2] = {NULL, NULL};
static int      g_buf_idx      = 0;

/* 调色板 */
static uint16_t palette[STATE_COUNT];

static int effect_init(struct demo_ctx *ctx)
{
    // 1. CMA 显存
    g_tex_phy_addr = mpp_phy_alloc(TEX_SIZE);
    if (g_tex_phy_addr == 0)
        return -1;
    g_tex_vir_addr = (uint16_t *)(unsigned long)g_tex_phy_addr;

    // 2. 状态内存 (普通 RAM)
    // 320*240 = 75KB * 2 = 150KB
    for (int i = 0; i < 2; i++)
    {
        g_state_buf[i] = (uint8_t *)rt_malloc(TEX_W * TEX_H);
        if (!g_state_buf[i])
        {
            // 回滚逻辑略
            return -1;
        }
    }

    // 3. 初始化随机状态 (播种)
    // 必须足够随机，才能产生漂亮的螺旋
    for (int i = 0; i < TEX_W * TEX_H; i++)
    {
        g_state_buf[0][i] = rand() % STATE_COUNT;
        g_state_buf[1][i] = 0;
    }

    // 4. 初始化调色板 (Alien Biology Style)
    // 深紫 -> 亮绿 -> 荧光粉
    for (int i = 0; i < STATE_COUNT; i++)
    {
        float t = (float)i / (float)STATE_COUNT;
        int   r = (int)(127 + 127 * sin(t * 6.28f));
        int   g = (int)(127 + 127 * sin(t * 6.28f + 2.0f));
        int   b = (int)(127 + 127 * sin(t * 6.28f + 4.0f));

        // 增加对比度
        if (i % 2 == 0)
        {
            r = r * 4 / 5;
            g = g * 4 / 5;
            b = b * 4 / 5;
        }

        palette[i] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
    }

    g_tick    = 0;
    g_buf_idx = 0;
    rt_kprintf("Night 18: Cellular automata evolution started.\n");
    return 0;
}

static void effect_draw(struct demo_ctx *ctx, unsigned long phy_addr)
{
    if (!g_tex_vir_addr || !g_state_buf[0])
        return;

    /*
     * === PHASE 1: 细胞进化 (Evolution) ===
     * 规则：如果任何一个邻居的状态是 (我的状态 + 1) % Total，我就变成那个状态。
     * 这模拟了“被捕食”或“被感染”。
     */

    int src_idx = g_buf_idx;
    int dst_idx = (g_buf_idx + 1) % 2;

    uint8_t  *src = g_state_buf[src_idx];
    uint8_t  *dst = g_state_buf[dst_idx];
    uint16_t *tex = g_tex_vir_addr;

    // 优化：跳过边缘处理，避免复杂的边界检查
    // 直接把边缘留给上一帧的值或者黑色，无伤大雅
    for (int y = 1; y < TEX_H - 1; y++)
    {

        // 预计算行偏移
        uint8_t *p_row    = src + y * TEX_W;
        uint8_t *p_row_up = src + (y - 1) * TEX_W;
        uint8_t *p_row_dn = src + (y + 1) * TEX_W;

        uint8_t  *p_dst = dst + y * TEX_W;
        uint16_t *p_tex = tex + y * TEX_W;

        for (int x = 1; x < TEX_W - 1; x++)
        {
            uint8_t current    = p_row[x];
            uint8_t next_state = (current + 1);
            if (next_state >= STATE_COUNT)
                next_state = 0;

            // 检查 4 邻域 (Von Neumann neighborhood)
            // 只要有一个邻居是 next_state，当前像素就进化
            if (p_row[x - 1] == next_state || p_row[x + 1] == next_state || p_row_up[x] == next_state ||
                p_row_dn[x] == next_state)
            {

                // 进化！
                p_dst[x] = next_state;
                p_tex[x] = palette[next_state];
            }
            else
            {
                // 保持原样
                p_dst[x] = current;
                p_tex[x] = palette[current];
            }

            // 随机突变 (Mutation)
            // 极小概率随机改变状态，防止画面陷入死循环或纯色
            // 这能让系统一直保持活力
            if ((rand() & 0xFFFF) > 0xFFF0)
            {
                p_dst[x] = rand() % STATE_COUNT;
            }
        }
    }

    /* === CRITICAL: Cache Flush === */
    aicos_dcache_clean_range((void *)g_tex_vir_addr, TEX_SIZE);

    /* === PHASE 2: GE Scaling === */
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

    mpp_ge_bitblt(ctx->ge, &blt);
    mpp_ge_emit(ctx->ge);
    mpp_ge_sync(ctx->ge);

    // 交换 Ping-Pong
    g_buf_idx = dst_idx;
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
    for (int i = 0; i < 2; i++)
    {
        if (g_state_buf[i])
        {
            rt_free(g_state_buf[i]);
            g_state_buf[i] = NULL;
        }
    }
}

struct effect_ops effect_0018 = {
    .name   = "NO.18 THE BIOLOGICAL CLOCK",
    .init   = effect_init,
    .draw   = effect_draw,
    .deinit = effect_deinit,
};

REGISTER_EFFECT(effect_0018);
