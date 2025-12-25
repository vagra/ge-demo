/*
 * Filename: demo_perf.h
 * THE EYE OF PROVIDENCE
 * 普罗维登斯之眼
 */

#ifndef _DEMO_PERF_H_
#define _DEMO_PERF_H_

#include "demo_engine.h"

/* 性能监控数据矩阵 */
struct performance_matrix
{
    float     fps;       /* 当前运行帧率 */
    float     cpu_usage; /* CPU 占用率 (0-100%) */
    rt_size_t mem_total; /* 堆内存总量 (Bytes) */
    rt_size_t mem_used;  /* 当前已用内存 (Bytes) */

    /* 内部记录：用于周期性计算指标 */
    rt_tick_t last_tick;
    uint32_t  frame_count;
    rt_tick_t last_report_tick;

    /* 字体资产句柄 */
    uint8_t  *font_data;   /* 预渲染点阵数据 */
    uint16_t  font_height; /* 字体全局高度 */
    uint16_t  char_count;  /* 字符总数 */
    uint32_t *offsets;     /* 字符偏移表 */
};

/**
 * 初始化性能监控系统
 */
void demo_perf_init(void);

/**
 * 更新性能数据指標 (每帧调用)
 */
void demo_perf_update(void);

/**
 * 绘制性能显示面板 (OSD 叠加层)
 */
void demo_perf_draw(struct demo_ctx *ctx, unsigned long phy_addr);

#endif /* _DEMO_PERF_H_ */
