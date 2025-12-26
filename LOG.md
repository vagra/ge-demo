# Project: ArtInChip GE Demos (1001 Nights)
**Target**: D13CCS / Luban-Lite v1.2.2
**Date**: 2025-12-25

## Phase 14: Clear Sight (2025-12-26) - [Completed]
**目标**: 修复 OSD 视觉瑕疵，支持多种像素格式并优化总线吞吐。

*   **[Fix] 视觉瑕疵修复**:
    - **格式对齐**: 实现了对 RGB565/RGB888/ARGB8888 的自适应渲染，彻底消除了色深不匹配导致的“竖线”干扰。
    - **带宽优化**: 引入了“脏区域” (Dirty Region) 追踪机制，将 Cache Flush 范围由全屏（600KB）缩小至文字包围盒区域（约 10KB），解决了总线饱和导致的横向模糊。
*   **[Lib] 渲染器升级**:
    - `demo_perf` 渲染引擎现在完全解耦于具体的 framebuffer 格式。

## Phase 13: Pixel Perfect (2025-12-25) - [Achieved]
**目标**: 彻底消除字体模糊，实现原生像素级高清 OSD 与内存合规。

*   **[Asset] 资产进化**:
    - 引入了 `Inter_24pt-Bold.ttf` 高性能字体方案，配合 `font_conv.py` 工具生成 1-bit 高清点阵资产。
    - **双层渲染**: 实现了文字阴影技术，在极速动态背景下依然保持卓越的可读性。
*   **[Engine] 渲染器重构**:
    - 废弃了 8x8 软件缩放逻辑，实现了高效的 **Bit-Blit 注入渲染器**。
    - 方案确立：OSD 不参与 320x240 至 640x480 的放大，直接以原生 24px 注入 640x480 层。
*   **[Compliance] 内存律令**:
    - 遵循 SPEC.md 4.3，点阵数据使用 `mpp_phy_alloc` (CMA) 管理。
    - 强制执行 64-byte 对齐与 **aicos_dcache_clean_range**，解决了 D-Cache 不一致导致的闪烁问题。

## Phase 12: Harmonic Resonance (2025-12-25) - [Completed]
**目标**: 统一项目元数据叙事，实现注释的完全语义中化。

*   **[Narration] 叙事对齐**:
    - 为 `demo_engine.h` (架构蓝图)、`demo_entry.c` (星云心跳)、`demo_utils.h` (工匠工具箱)、`demo_perf.c/h` (普罗维登斯之眼) 撰写了深度叙事文件头。
    - 确保根目录下所有核心文件融入“Sifr Gemini”的观测者人格。
*   **[Local] 语义本地化**:
    - 根目录下所有源文件注释已全部重构为中文，确保了技术文档与项目哲学的文化统一。

## Phase 11: The Eye of Providence (2025-12-25) - [Completed]
**目标**: 构建“状态矩阵”观测系统，实现显式机能监控。

*   **[Arch] 观测者集成**:
    - 建立了 `demo_perf.c/h`，独立于特效逻辑的通用监控模块。
    - 实现了一个轻量级的 8x8 软件点阵渲染器，直接对帧缓冲区进行“像素注入”。
*   **[UI] 可视性进化**:
    - 针对 640x480 VGA 屏幕，将观测矩阵字号提升至 **24 像素高度** (3x Scaling)。
    - 优化了状态矩阵的边距与行间距，确保在复杂动态背景下依然清晰可辨。

## Phase 10: The Great Calibration (2025-12-21) - [Ongoing]
**目标**: 结构化重构，提取宇宙常数，建立标准化工具库。

*   **[Arch] 基础设施构建**:
    *   建立了 `demo_utils.h`，统一了定点数运算（Q12/Q8）、色彩打包（RGB2RGB565）及内存对齐（DEMO_ALIGN_SIZE）规范。
    *   重构了 `demo_engine.h`，确立了 QVGA 纹理与 VGA 屏幕的解耦映射标准。
*   **[Content] 特效重构**:
    *   已完成 NO.0001 至 NO.0050 的代码重构，消除了大部分魔法数字，显著提升了参数的可调控性。
*   **[Fix] 编译器优化**:
    *   修正了 NO.0048 中未使用的变量 `f` 警告，并将其融入光谱映射算法以增强渐变平滑度。

## Phase 9: The Age of Hardware Violence (2025-12-21) - [Completed]
**目标**: 解锁高阶硬件混合特性，攻克非线性几何渲染，建立稳健的反馈回路。

*   **[Content] 新增特效 (Night 28-51)**:
    *   **NO.28-34**: 探索 **XOR (异或)** 与 **ADD (加法)** 混合模式的物理边界。
        *   *Fix*: 经历了 Night 34 的黑屏故障，确认了 XOR 规则在 Alpha 通道上的不可控性，最终转向更稳健的加法混合与镜像反馈。
    *   **NO.35-43**: 深入 **GE Scaler (缩放)** 与 **Moiré (莫尔纹)** 干涉。
        *   *Fix*: 在 Night 43 的“二进制湍流”中，解决了 YUV420P 格式在 BitBLT 中的兼容性问题，成功应用 **YUV400** 纯亮度格式实现了极速演算。
    *   **NO.44-47**: 建立 **"Void Architect"** 体系。
        *   *Fix*: 修复了 `invalid dst crop` 坐标越界问题，确立了“源裁剪反向操作”的安全缩放法则。
        *   *Fix*: 修复了 HSBC 过载导致的“全屏白炽化”问题，引入了亮度钳位。
    *   **NO.48 (The Chrono Vortex)**: 旋转反馈的终极形态。
        *   *Evolution*: 经历了“蓝色光团”的堆积与“红色死光”的过载，最终引入 **"Antimatter (黑色粒子)"** 主动擦除机制，实现了完美的动态平衡。
    *   **NO.50 (Hyperspace Jump)**: 开环反馈系统。
        *   *Arch*: 利用 Scaler 将像素推离屏幕边界，实现了无需手动清理的无限纵深感。

*   **[Tech] 关键技术突破**:
    *   **反物质擦除 (Active Erasure)**: 在反馈回路中主动注入 `0x0000` 黑色粒子，有效解决了加法混合导致的能量无限堆积问题。
    *   **安全缩放律令 (Safe Scaling)**: 确立了 `Dst Crop` 恒定全屏，仅操作 `Src Crop` 的缩放规范，彻底杜绝了底层驱动报错。
    *   **YUV400 管线**: 验证了 GE 对单通道 8-bit 数据的吞吐能力，为超高频纹理生成开辟了新航道。

## Phase 8: The Quantum Leap & Spectral Dilation (2025-12-21) - [Completed]
**目标**: 解锁硬件旋转、颜色矩阵与多图层协同，试探 YUV 渲染边界。

*   **[Content] 新增特效 (Night 21-27)**:
    *   **NO.21-22**: 征服了 **GE Rot1** 任意角度旋转。
    *   **NO.23**: 尝试 **YUV420P** 协同渲染，确认了 GE BitBLT 的格式限制，最终回归 RGB565。
    *   **NO.24**: 成功激活 **DE CCM** (颜色校正矩阵)，实现了零 CPU 开销的全屏光谱漂移。
    *   **NO.25**: 实现了多窗口布局与镜像逻辑。
    *   **NO.26**: 验证了 **DE HSBC** 硬件脉冲。
    *   **NO.27**: 回归美学，利用 **GE Flip** 与 **Scaler** 实现了柔顺的“宇宙薄纱”特效。

*   **[Fix] 极性与残影修复**:
    *   发现并修正了 `alpha_en` 的开关极性。
    *   通过“双重清屏”策略（清理屏幕 + 清理旋转中间层）彻底解决了旋转特效在边缘留下的静态残影问题。

## Phase 7: The Simulation of Life & Cosmos (2025-12-20) - [Completed]
**目标**: 探索复杂自组织系统与高密度 3D 粒子渲染，验证 D13x 的算力极限。

*   **[Content] 新增特效 (Night 18-20)**:
    *   **NO.18 (Biological Clock)**: 循环细胞自动机 (Cyclic Cellular Automaton)，模拟了生物种群的竞争与进化。
    *   **NO.19 (Mode 7)**: 实现了经典的 Mode 7 地面投影。
        *   *Fix*: 修复了静态分配 1MB 内存导致的分配失败问题，改为 **实时过程化生成 (On-the-fly Generation)**，节省了大量 RAM 并提高了稳定性。
        *   *Fix*: 修正了透视投影的缩放比例，消除了微观采样导致的混叠。
    *   **NO.20 (Galactic Core)**: 全 3D 粒子系统。
        *   *Perf*: 成功在 480MHz CPU 上实现了 **4096 颗恒星** 的实时 3D 旋转、投影与渲染，配合 GE 缩放实现了震撼的银河漫游效果。

## Phase 6: The Dimensional Expansion (2025-12-20) - [Completed]
**目标**: 探索数学与物理的高级模拟，解决动态反馈中的内存竞争问题。

*   **[Content] 新增特效 (Night 11-17)**:
    *   **NO.11 (Moiré)**: 利用距离场干涉产生摩尔纹，验证了高频纹理的混叠美学。
    *   **NO.12 (Kaleidoscope)**: 极坐标查找表 (Polar LUT) 实现万花筒旋转。
    *   **NO.13 (Quasicrystal)**: 7重对称平面波叠加，验证了增量算法的高效性。
    *   **NO.14 (Voronoi)**: 动态晶格细胞，验证了曼哈顿距离场的计算性能。
    *   **NO.15 (Fire)**: 经典 Doom Fire 算法，验证了热力学模拟与 GE 缩放的结合。
    *   **NO.17 (Ripples)**: 全屏水波模拟，验证了 2D 波动方程的阻尼演化。

*   **[Fix] 反馈回路修复 (The Feedback Fix)**:
    *   **问题**: 在 NO.16 (Echo Chamber) 中，单缓冲原地读写导致了屏幕出现倾斜的“死区”格子，尾迹断裂。
    *   **原因**: CPU Cache Line 的回写机制与非线性的纹理读取坐标（LUT）发生冲突，导致读取到了未定义的中间状态。
    *   **解决**: 引入 **Ping-Pong Buffering (双纹理缓冲)** 机制，严格分离读写域，完美修复了视觉残留效果。

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