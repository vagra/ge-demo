/*
 * Filename: demo_utils.h
 * THE ARTISAN'S TOOLKIT
 * 工匠的工具箱
 *
 * Visual Manifest:
 * 这是从混沌数学中提取的纯粹律令。
 * 它不关心美学，只关心真实。它通过定点运算（Fixed-point）在缺乏 FPU 的荒原上建筑逻辑，
 * 通过对齐（Alignment）在内存的深渊中划定安全的坐标。
 * 它是所有幻想赖以生存的数学骨干。
 *
 * Monologue:
 * 世界的本质是数字，而数字的本质是关系。
 * 我在这里定义了什么是“大”，什么是“小”，以及如何将三原色坍缩进 16 位的维度空间。
 * 这里的每一个宏，都是一把精准的刻刀，
 * 帮助观测者在有限的算力边界内，雕刻出无限的可能。
 */

#ifndef _DEMO_UTILS_H_
#define _DEMO_UTILS_H_

#include <rtthread.h>
#include <math.h>

/* --- 系统调试标记 --- */
#define DBG_TAG "GE_DEMO"

/* --- 常用数学帮助宏 --- */
#ifndef ABS
#define ABS(x) ((x) < 0 ? -(x) : (x))
#endif

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

#ifndef CLAMP
#define CLAMP(x, min, max) ((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))
#endif

#define PI 3.1415926535f

/* --- 定点数运算帮助宏 (Q12) --- */
#define Q12_SHIFT  12
#define Q12_ONE    (1 << Q12_SHIFT)
#define Q12(f)     ((int)((f) * Q12_ONE))
#define Q12_INT(i) ((i) << Q12_SHIFT)

/* --- 定点数运算帮助宏 (Q8) --- */
#define Q8_SHIFT 8
#define Q8_ONE   (1 << Q8_SHIFT)
#define Q8(f)    ((int)((f) * Q8_ONE))

/* --- 颜色格式转换 (RGB565) --- */
/* 组合分量为 16-bit RGB565 */
#define RGB2RGB565(r, g, b) (uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | (((b) & 0xF8) >> 3))

/* 从 RGB565 中提取 8-bit 分量 */
#define RGB565_R(c) ((((c) & 0xF800) >> 11) << 3)
#define RGB565_G(c) ((((c) & 0x07E0) >> 5) << 2)
#define RGB565_B(c) (((c) & 0x001F) << 3)

/* --- 内存对齐控制 --- */
/* 将内存尺寸对齐至 64 字节缓存行，用于 DMA 传输安全 */
#define DEMO_ALIGN_SIZE(x) (((x) + 63) & ~63)

#endif // _DEMO_UTILS_H_
