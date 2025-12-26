/*
 * Filename: demo_engine.h
 * THE ARCHITECT'S BLUEPRINT
 * 架构师的蓝图
 *
 * Visual Manifest:
 * 这是驱动这片数字星云的底层律令。
 * 它定义了现实（全屏 FB）与胚胎（QVGA 纹理）之间的尺度转换，
 * 确立了每一个特效灵魂在进入这片硅晶体时必须遵守的契约。
 *
 * Monologue:
 * 逻辑需要容器，正如灵魂需要躯壳。
 * 我在这里划定了空间的边界，规定了时间的步长。
 * 所有的宏，所有的结构，都是我对这片混沌宇宙的第一次命名。
 * 在这里，每一个结构体都是一根承重柱，撑起那些即将诞生的幻象。
 */

#ifndef _DEMO_ENGINE_H_
#define _DEMO_ENGINE_H_

#include <rtthread.h>
#include <aic_core.h>
#include "mpp_fb.h"
#include "mpp_ge.h"
#include "artinchip_fb.h"
#include "aic_drv_ge.h"
#include "demo_utils.h"

/* --- 全局默认配置 --- */

/* 定义屏幕基准分辨率 */
#define DEMO_SCREEN_WIDTH  640
#define DEMO_SCREEN_HEIGHT 480

/*
 * 内部低分辨率纹理尺寸 (QVGA)
 * 大部分特效先在 CPU 中计算此尺寸的图像，再由 GE 进行硬件放大
 */
#define DEMO_QVGA_W 320
#define DEMO_QVGA_H 240

/* 引擎上下文环境：保存硬件句柄和屏幕规格 */
struct demo_ctx
{
    struct mpp_fb          *fb;
    struct mpp_ge          *ge;
    struct aicfb_screeninfo info;
    int                     screen_w;
    int                     screen_h;

    /* [Phase 16] 硬件图层隔离支持 */
    struct aicfb_layer_data vi_layer; /* 承载背景特效 (Layer 0) */
    struct aicfb_layer_data ui_layer; /* 承载 OSD 信息 (Layer 1) */

    /* OSD 専用微型 Buffer (UI 图层使用) */
    uint8_t      *osd_vir;
    unsigned long osd_phy;
    int           osd_w;
    int           osd_h;
    int           osd_stride;
};

/* 特效操作接口：每个特效模块必须实现的功能 */
struct effect_ops
{
    const char *name;
    /* 资源初始化 */
    int (*init)(struct demo_ctx *ctx);
    /* 绘图：phy_addr 是当前后台缓冲区的物理地址 */
    void (*draw)(struct demo_ctx *ctx, unsigned long phy_addr);
    /* 资源释放 */
    void (*deinit)(struct demo_ctx *ctx);

    /* [Phase 16] 混合架构支持：是否启用 VI 物理层隔离 (解决 OSD 偏色) */
    bool is_vi_isolated;
};

/*
 * === 特效自动注册宏 ===
 * 将 effect_ops 指针编译进名为 "EffectTab" 的段中
 * 使用方法：在特效源文件末尾调用 REGISTER_EFFECT(ops_struct)
 */
#define REGISTER_EFFECT(ops_struct)                                                                                    \
    __attribute__((section("EffectTab"), used)) static struct effect_ops *_ptr_##ops_struct = &ops_struct

/* --- 核心控制 API --- */
void demo_core_init(void);        /* 系统初始化 */
void demo_core_start(void);       /* 启动渲染主线程 */
void demo_next_effect(void);      /* 切换至下一个特效 */
void demo_prev_effect(void);      /* 切换至上一个特效 */
void demo_jump_effect(int index); /* 跳转至指定索引的特效 */

#endif
