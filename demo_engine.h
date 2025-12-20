#ifndef _DEMO_ENGINE_H_
#define _DEMO_ENGINE_H_

#include <rtthread.h>
#include <aic_core.h>
#include "mpp_fb.h"
#include "mpp_ge.h"
#include "artinchip_fb.h" // 包含底层 FB 定义
#include "aic_drv_ge.h"   // 包含底层 GE 定义(GE_NO_GRADIENT等)

struct demo_ctx
{
    struct mpp_fb          *fb;
    struct mpp_ge          *ge;
    struct aicfb_screeninfo info;
    int                     screen_w;
    int                     screen_h;
};

/* 所有特效必须实现这个接口 */
struct effect_ops
{
    const char *name;
    /* 初始化资源 */
    int (*init)(struct demo_ctx *ctx);
    /* 绘制一帧. phy_addr 是当前后台缓冲区的物理地址 */
    void (*draw)(struct demo_ctx *ctx, unsigned long phy_addr);
    /* 释放资源 */
    void (*deinit)(struct demo_ctx *ctx);
};

/*
 * === 自动注册宏 ===
 * 原理：将指向 effect_ops 的指针放入名为 "EffectTab" 的链接器段中。
 *
 * 使用方法：
 * 在每个特效 .c 文件末尾：
 * REGISTER_EFFECT(effect_0001);
 */
#define REGISTER_EFFECT(ops_struct)                                                                                    \
    __attribute__((section("EffectTab"), used)) static struct effect_ops *_ptr_##ops_struct = &ops_struct

/* API for external control */
void demo_core_init(void);
void demo_core_start(void); // 启动渲染线程
void demo_next_effect(void);
void demo_prev_effect(void);
void demo_jump_effect(int index);

#endif
