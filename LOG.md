# Project: ArtInChip GE Demos (1001 Nights)
**Target**: D13CCS / Luban-Lite v1.2.2
**Date**: 2025-12-18

## Phase 5: System Interaction & Stability (2025-12-20) - [Completed]
**目标**: 实现非阻塞的交互系统，修复编译架构问题，确保系统稳健运行。

*   **[Arch] 多线程渲染架构**:
    *   将渲染循环从 `main` 线程剥离至独立的 `ge_render` 线程。
    *   `main` 函数在启动渲染后正常退出，释放控制台 (MSH) 资源，解决了串口无法输入的问题。
*   **[Input] 交互系统集成**:
    *   引入 `AIC_GE_DEMO_WITH_KEY` Kconfig 选项，实现按键代码的条件编译。
    *   实现了基于中断的按键控制 (Prev/Next) 和 MSH 串口命令 (`demo_jump`, `demo_list`)。
*   **[Fix] 构建系统修复**:
    *   **Kconfig**: 移除了嵌套的 `if/endif` 语法，改用扁平的 `depends on`，修复了 Kconfig 解析错误。
    *   **Compilation**: 修复了 `demo_entry.c` 中因大括号缺失导致的函数嵌套定义错误。
    *   **Linker**: 验证了 `REGISTER_EFFECT` 宏与链接器段 (`EffectTab`) 的自动收集机制工作正常。

## Phase 4: The First Ten Nights - Milestone (2025-12-20) - [Completed]
**目标**: 完成前 10 个特效的创作，并将所有早期特效升级为“重生版”以符合全屏美学。

*   **[Content] 前 10 夜达成**:
    *   完成了从 0001 到 0010 的所有特效代码编写。
    *   **重生计划 (Reborn)**: 对 0001(Cellular), 0002(Plasma), 0003(Tunnel), 0004(Twister), 0006(Particles) 进行了彻底重写。
    *   **美学统一**: 所有特效现在均遵循“全屏覆盖、无缝循环、混合渲染”的高标准。

*   **[Validation] 架构验证**:
    *   理论对比了本框架与 LVGL 的渲染机制。
    *   确认了 **"CPU Compute (Low-Res) -> GE Scale (Hi-Res)"** 模式在 D13x 平台上对于过程化图形(Procedural Graphics)具有不可替代的性能优势。

## Phase 3: The Great Refactoring & Standardization (2025-12-19) - [Completed]
**目标**: 将所有早期特效 (Night 1-4, 6) 重构为标准的混合渲染管线，彻底消除性能瓶颈。

*   **[Refactor] 全面普及混合管线 (Hybrid Pipeline Standard)**:
    *   **标准化**: 确立了 `320x240 (RGB565) CPU Render` -> `GE Scale to 640x480` 的黄金标准。
    *   **成果**:
        *   **NO.1 (Cellular)**: 修复了大量 `fillrect` 导致的队列溢出，改为单次 BitBLT。
        *   **NO.2 (Plasma)**: 修复了画面撕裂，实现了液态般的平滑缩放。
        *   **NO.3 (Tunnel)**: 修复了指令同步问题，利用预计算 LUT 实现了完美的透视深度。
        *   **NO.4 (Twister)**: 利用逐行渲染 + 硬件插值，消除了锯齿感。
        *   **NO.6 (Particles)**: 在 RGB565 限制下，通过软件算法实现了“加法混合”与“光子衰减”，粒子数从 4 提升至 128。

*   **[Perf] 性能里程碑**:
    *   所有已实现特效均在 D13CCS 上实现了全屏流畅运行，无 `write() failed` 报错，无画面撕裂。

## Phase 2: Deep Dive & Architecture Upgrade (2025-12-19) - [Completed]
**目标**: 研读官方手册，解决 GE 挂死问题，探索 D13x 极限性能架构。

*   **[Doc] 官方文档研读**:
    *   重点研究了《10.2 GE》、《5.5 DMA》、《3. 地址映射》。
    *   确认了 GE Command Queue 的 Ring Buffer 机制，验证了流控的必要性。
    *   确认了 Scaler0 子模块支持 1/16x ~ 16x 缩放及 Bilinear 滤波。

*   **[Fix] DMA 内存一致性修复**:
    *   **问题**: 使用 `rt_malloc` 分配纹理导致 GE `BitBLT` 报 `-116` 超时错误。
    *   **原因**: `rt_malloc` 内存可能位于 Cache 中未同步，或位于非 DMA 区域（如 TCM），导致 GE 读取异常。
    *   **解决**: 引入 `mpp_mem.h`，改用 `mpp_phy_alloc` 分配 CMA (Continuous Memory Allocator) 内存，并配合 `aicos_dcache_clean_range` 维护一致性。

*   **[Arch] 混合渲染管线 (Hybrid Pipeline)**:
    *   确立了 **"Low-Res CPU Gen (RGB565) -> GE Scaling -> Hi-Res Screen (RGB888)"** 的高性能架构。
    *   大幅降低了 CPU 负载和发热，同时利用 GE 的格式转换能力兼容了 RGB888 屏幕。

*   **[Content] 新增特效**:
    5.  **NO.5 THE FRACTAL DREAM**: 动态朱利亚集 (Julia Set)。验证了混合渲染架构，实现了从“像素积木”到“细腻流体”的质变。
    6.  **NO.6 CHROMATIC SINGULARITY**: 32位真彩色 (ARGB8888) 光流拖尾。验证了 D13CCS 在高带宽压力下的稳定性及 Alpha Blending 能力。

## Phase 1: Genesis & Pipeline Stabilization (2025-12-18) - [Completed]
**目标**: 建立基于 GE 硬件加速的裸机级绘图框架，跑通第一个 Demo。

*   **[Arch] 项目架构搭建**:
    *   建立了 `packages/artinchip/ge-demos` 独立软件包结构。
    *   设计了 `demo_engine.h` 接口 (`init`/`draw`/`deinit`)，实现了插件式特效扩展。
    *   解决了 Kconfig 依赖关系 (`depends on AIC_DISP_BOOTUP_LOGO`) 和语法嵌套错误。
    *   配置了 SConscript 以支持子目录源码自动扫描。

*   **[Boot] 启动流无缝接管**:
    *   **问题**: `startup_ui` 与文件系统挂载优先级冲突，导致 Logo 无法读取 `/data` 图片。
    *   **解决**: 将 `absystem_os.c` 中的挂载优先级从 Level 7 (`LATE`) 提升至 Level 6 (`APP`)。
    *   **策略**: 采用 "Logo -> Main -> Demo" 的接力策略。`main()` 函数中重新初始化 FB 和 GE，接管屏幕控制权，实现了从开机 Logo 到 Demo 的无缝转场。

*   **[Driver] 驱动适配与修复**:
    *   **头文件对齐**: 修复了 SDK v1.2.2 中 `struct ge_fillrect` (无 `rect` 成员) 和 `aicfb_screeninfo` (无 `smem_start`) 的定义差异。
    *   **物理地址获取**: 采用 `(unsigned long)info.framebuffer` 作为 GE 物理基地址的兼容写法。
    *   **Layer 配置**: 修正了 `AICFB_PAN_DISPLAY` 参数传递方式 (传指针) 和 Layer 属性初始化逻辑。

*   **[Perf] 硬件特性调优 (The "Flush Failed" Fix)**:
    *   **现象**: 在高频提交指令时出现 `flush_cmd: write() failed!`，且画面只有部分渲染。
    *   **原因**: D13x GE 命令队列 (Ring Buffer) 容量有限，CPU 提交速度远超 GPU 消耗速度。
    *   **解决**: 
        1.  **流控 (Flow Control)**: 引入 `BATCH_SIZE` 机制，每 N 条指令强制 `sync` 一次。
        2.  **自适应策略**: 大面积绘图 (全屏 Clear) 立即同步；小面积绘图 (Raster Line) 批量同步。
    *   **裁剪**: 实现了软件层面的 `Clipping`，防止因坐标越界 (`invalid dst crop`) 导致的驱动报错。

*   **[Content] 已实现的特效**:
    1.  **NO.1 PRIMORDIAL SOUP**: 异或纹理 (Cellular Automata)，验证了基础填充能力。
    2.  **NO.2 WAVE FUNCTION**: 正弦波干涉 (Plasma)，验证了 LUT 优化和批量提交。
    3.  **NO.3 INFINITE CORRIDOR**: 递归透视隧道，验证了画家算法和大面积图层同步策略。
    4.  **NO.4 LIQUID SPINE**: 光栅扫描线扭曲 (Raster Distortion)，验证了高频小包指令的吞吐稳定性。