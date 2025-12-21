// ================================================================================
// FILE: demo_utils.h (NEW)
// ================================================================================
#ifndef _DEMO_UTILS_H_
#define _DEMO_UTILS_H_

#include <rtthread.h>
#include <math.h>

/* --- Debugging --- */
#define DBG_TAG "GE_DEMO"

/* --- Math Helpers --- */
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

/* --- Fixed Point Arithmetic (Q12: 4096 = 1.0) --- */
#define Q12_SHIFT  12
#define Q12_ONE    (1 << Q12_SHIFT)
#define Q12(f)     ((int)((f) * Q12_ONE))
#define Q12_INT(i) ((i) << Q12_SHIFT)

/* --- Fixed Point Arithmetic (Q8: 256 = 1.0) --- */
#define Q8_SHIFT 8
#define Q8_ONE   (1 << Q8_SHIFT)
#define Q8(f)    ((int)((f) * Q8_ONE))

/* --- Color Helpers (RGB565) --- */
/* Pack 8-bit R, G, B into 16-bit RGB565 */
#define RGB2RGB565(r, g, b) (uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | (((b) & 0xF8) >> 3))

/* Extract components from RGB565 */
#define RGB565_R(c) ((((c) & 0xF800) >> 11) << 3)
#define RGB565_G(c) ((((c) & 0x07E0) >> 5) << 2)
#define RGB565_B(c) (((c) & 0x001F) << 3)

/* --- Memory Helpers --- */
/* Align size to cache line (typically 32 or 64 bytes) for safe DMA */
#define DEMO_ALIGN_SIZE(x) (((x) + 63) & ~63)

#endif // _DEMO_UTILS_H_
