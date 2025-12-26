/*
 * Filename: demo_perf.c
 * THE OBSERVER'S SCANNER
 * 观测者的扫描仪
 */

#include "demo_perf.h"
#include <rtthread.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "mpp_mem.h"
#include "aic_core.h"

#ifdef LPKG_USING_CPU_USAGE
#include "cpu_usage.h"
#endif

/* 默认字体资产路径 */
#define FONT_ASSET_PATH "/data/ge_demos/font_24px.bin"

static struct performance_matrix g_perf;

/* 遵循 SPEC.md 4.3: 载入点阵字体资产并确保内存合规 */
static void load_font_asset(void)
{
    int fd = open(FONT_ASSET_PATH, O_RDONLY);
    if (fd < 0)
    {
        rt_kprintf("Demo Error: Failed to open font asset at %s\n", FONT_ASSET_PATH);
        return;
    }

    /* 1. 验证幻数 */
    char magic[4];
    read(fd, magic, 4);
    if (memcmp(magic, "FONT", 4) != 0)
    {
        rt_kprintf("Demo Error: Invalid font format.\n");
        close(fd);
        return;
    }

    read(fd, &g_perf.font_height, 2);
    read(fd, &g_perf.char_count, 2);

    /* 2. 分配控制表内存 (使用 mpp_alloc 统一管理) */
    size_t offset_table_size = DEMO_ALIGN_SIZE(g_perf.char_count * sizeof(uint32_t));
    g_perf.offsets           = mpp_alloc(offset_table_size);
    if (!g_perf.offsets)
    {
        rt_kprintf("Demo Error: Offset table alloc failed.\n");
        close(fd);
        return;
    }
    read(fd, g_perf.offsets, g_perf.char_count * sizeof(uint32_t));

    /* 3. 分配点阵资产内存 (遵循 SPEC.md: 使用 mpp_phy_alloc 以支持 DMA) */
    off_t current_pos = lseek(fd, 0, SEEK_CUR);
    off_t total_size  = lseek(fd, 0, SEEK_END);
    lseek(fd, current_pos, SEEK_SET);

    size_t data_size = total_size - current_pos;
    /* 对齐至 Cache Line (64-byte) */
    size_t aligned_size = DEMO_ALIGN_SIZE(data_size);

    unsigned int phy_addr = mpp_phy_alloc(aligned_size);
    if (phy_addr)
    {
        g_perf.font_data = (uint8_t *)(unsigned long)phy_addr;
        read(fd, g_perf.font_data, data_size);

        /* 重要：执行 Cache Flush 确保物理内存与缓存一致性 (SPEC.md 4.3) */
        aicos_dcache_clean_range(g_perf.font_data, aligned_size);

        rt_kprintf("Demo: High-res font loaded (CMA: %d bytes, height %d)\n", (int)aligned_size, g_perf.font_height);
    }
    else
    {
        rt_kprintf("Demo Error: Font CMA alloc failed.\n");
    }

    close(fd);
}

void demo_perf_init(void)
{
    rt_memset(&g_perf, 0, sizeof(g_perf));
    g_perf.last_tick        = rt_tick_get();
    g_perf.last_report_tick = g_perf.last_tick;

    load_font_asset();
}

void demo_perf_update(void)
{
    rt_tick_t now = rt_tick_get();
    g_perf.frame_count++;

    /* 周期性统计 (约 1 秒) */
    if (now - g_perf.last_report_tick >= RT_TICK_PER_SECOND)
    {
        rt_tick_t delta         = now - g_perf.last_report_tick;
        g_perf.fps              = (float)g_perf.frame_count * RT_TICK_PER_SECOND / delta;
        g_perf.frame_count      = 0;
        g_perf.last_report_tick = now;

#ifdef LPKG_USING_CPU_USAGE
        g_perf.cpu_usage = cpu_load_average();
#else
        g_perf.cpu_usage = 0.0f;
#endif
        rt_memory_info(&g_perf.mem_total, &g_perf.mem_used, RT_NULL);
    }
}

static inline void update_dirty_region(int x, int y, int w, int h)
{
    if (g_perf.dirty_w == 0 || g_perf.dirty_h == 0)
    {
        g_perf.dirty_x = x;
        g_perf.dirty_y = y;
        g_perf.dirty_w = w;
        g_perf.dirty_h = h;
    }
    else
    {
        int x2 = x + w;
        int y2 = y + h;
        int gx2 = g_perf.dirty_x + g_perf.dirty_w;
        int gy2 = g_perf.dirty_y + g_perf.dirty_h;

        if (x < g_perf.dirty_x) g_perf.dirty_x = x;
        if (y < g_perf.dirty_y) g_perf.dirty_y = y;
        if (x2 > gx2) g_perf.dirty_w = x2 - g_perf.dirty_x;
        else g_perf.dirty_w = gx2 - g_perf.dirty_x;
        if (y2 > gy2) g_perf.dirty_h = y2 - g_perf.dirty_y;
        else g_perf.dirty_h = gy2 - g_perf.dirty_y;
    }
}

static inline void draw_pixel(uint8_t *fb_vir, int stride, int format, int x, int y, uint32_t color, int screen_w,
                              int screen_h)
{
    if (x >= 0 && x < screen_w && y >= 0 && y < screen_h)
    {
        if (format == MPP_FMT_RGB_565)
        {
            uint16_t *p = (uint16_t *)(fb_vir + y * stride + x * 2);
            *p = (uint16_t)color;
        }
        else if (format == MPP_FMT_RGB_888)
        {
            uint8_t *p = fb_vir + y * stride + x * 3;
            p[0] = color & 0xFF;         /* B */
            p[1] = (color >> 8) & 0xFF;  /* G */
            p[2] = (color >> 16) & 0xFF; /* R */
        }
        else if (format == MPP_FMT_ARGB_8888 || format == MPP_FMT_XRGB_8888)
        {
            uint32_t *p = (uint32_t *)(fb_vir + y * stride + x * 4);
            *p = color;
        }
    }
}

/* 高效 Bit-Blit 渲染器 */
static void draw_char_bitblit(uint8_t *fb_vir, int stride, int format, int x, int y, char c, uint32_t color, int screen_w,
                              int screen_h)
{
    if (!g_perf.font_data || c < 32 || (c - 32) >= g_perf.char_count)
        return;

    uint32_t header_size = 8 + (g_perf.char_count * 4);
    uint32_t offset      = g_perf.offsets[c - 32] - header_size;
    uint8_t *data_ptr    = &g_perf.font_data[offset];

    uint8_t width  = *data_ptr++;
    int     height = g_perf.font_height;

    update_dirty_region(x, y, width, height);

    int bit_idx = 0;
    for (int row = 0; row < height; row++)
    {
        for (int col = 0; col < width; col++)
        {
            if (data_ptr[bit_idx >> 3] & (0x80 >> (bit_idx & 7)))
            {
                draw_pixel(fb_vir, stride, format, x + col, y + row, color, screen_w, screen_h);
            }
            bit_idx++;
        }
    }
}

/* 绘制高清字符串（带阴影增强对比度） */
static int draw_string_highres(uint8_t *fb_vir, int stride, int format, int x, int y, const char *str, uint32_t color,
                               int screen_w, int screen_h)
{
    int         start_x = x;
    const char *p       = str;

    /* 1. 绘制阴影 (偏移 2 像素) */
    while (*p)
    {
        char c = *p++;
        if (c < 32 || (c - 32) >= g_perf.char_count)
            continue;

        uint32_t header_size = 8 + (g_perf.char_count * 4);
        uint32_t offset      = g_perf.offsets[c - 32] - header_size;
        uint8_t  width       = g_perf.font_data[offset];

        draw_char_bitblit(fb_vir, stride, format, x + 2, y + 2, c, 0x00000000, screen_w, screen_h);
        x += width;
    }

    /* 2. 绘制主体文字 */
    x = start_x;
    p = str;
    while (*p)
    {
        char c = *p++;
        if (c < 32 || (c - 32) >= g_perf.char_count)
            continue;

        uint32_t header_size = 8 + (g_perf.char_count * 4);
        uint32_t offset      = g_perf.offsets[c - 32] - header_size;
        uint8_t  width       = g_perf.font_data[offset];

        draw_char_bitblit(fb_vir, stride, format, x, y, c, color, screen_w, screen_h);
        x += width;
    }

    return x - start_x;
}

void demo_perf_draw(struct demo_ctx *ctx, unsigned long phy_addr)
{
    char      buf[64];
    uint32_t  color_cyan;
    uint8_t  *fb_vir = (uint8_t *)phy_addr;
    int       stride = ctx->info.stride;
    int       format = ctx->info.format;

    /* 重置脏区域 */
    g_perf.dirty_x = 0;
    g_perf.dirty_y = 0;
    g_perf.dirty_w = 0;
    g_perf.dirty_h = 0;

    /* 根据格式确定青色 (Cyan) 值 */
    if (format == MPP_FMT_RGB_565)
        color_cyan = 0x07FF;
    else if (format == MPP_FMT_RGB_888)
        color_cyan = 0x00FFFF;
    else
        color_cyan = 0xFF00FFFF;

    /*
     * 确保 GE 硬件操作完全结束，防止 GPU 与 CPU 同时操作同一缓冲区导致撕裂。
     * 虽然特效 draw 函数内部通常有 sync，但在此进行二次确认是稳定性的保障。
     */
    mpp_ge_sync(ctx->ge);

    int start_x = 32; /* 向右微调，避免贴边 */
    int start_y = 20;
    int line_h  = g_perf.font_height + 4;

    /* FPS 渲染 */
    rt_snprintf(buf, sizeof(buf), "FPS: %d.%d", (int)g_perf.fps, (int)(g_perf.fps * 10) % 10);
    draw_string_highres(fb_vir, stride, format, start_x, start_y, buf, color_cyan, ctx->screen_w, ctx->screen_h);

    /* CPU 占比渲染 */
    rt_snprintf(buf, sizeof(buf), "CPU: %d%%", (int)g_perf.cpu_usage);
    draw_string_highres(fb_vir, stride, format, start_x, start_y + line_h, buf, color_cyan, ctx->screen_w,
                        ctx->screen_h);

    /* RAM 消耗渲染 */
    rt_snprintf(buf, sizeof(buf), "RAM: %d/%d KB", (int)(g_perf.mem_used / 1024), (int)(g_perf.mem_total / 1024));
    draw_string_highres(fb_vir, stride, format, start_x, start_y + line_h * 2, buf, color_cyan, ctx->screen_w,
                        ctx->screen_h);

    /*
     * [OPTIMIZED FIX] 局部 D-Cache Flush
     * 仅刷新受 OSD 影响的脏区域。这极大地降低了对 DDR 总线带宽的瞬间压力，
     * 彻底解决了由于全屏刷新导致的显示引擎（DE）Underrun（横向模糊/漂移）问题。
     */
    if (g_perf.dirty_w > 0 && g_perf.dirty_h > 0)
    {
        /* 对齐脏区域到 Cache Line 边界 (虽然这里直接刷新包含行的范围也行，但精确一点更好) */
        unsigned long flush_start = (unsigned long)fb_vir + g_perf.dirty_y * stride;
        unsigned long flush_size  = g_perf.dirty_h * stride;
        aicos_dcache_clean_range((unsigned long *)flush_start, flush_size);
    }
}
