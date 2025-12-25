# ArtInChip GE Demo Suite - Technical Specification

> **Version**: 1.3.1
> **Target**: ArtInChip D13x Series (Tested on D13CCS)
> **OS**: Luban-Lite (RT-Thread)

## 1. System Overview (系统概览)
本项目是一个基于 ArtInChip **GE (Graphics Engine)** 硬件加速器的裸机级高性能图形演示框架。它跳过通用 GUI 库，直接通过底层驱动操作显存和图形引擎，旨在挖掘芯片的极限 2D 渲染潜力，展示纯数学生成的过程化视觉特效。

## 2. Hardware Constraints (机能限制)
*   **MCU**: D13CCS (RISC-V 64bit @ 480MHz, 16MB SIP RAM).
*   **Display**: 640x480 @ 60FPS (MIPI-DSI 4-Lane, RGB888).
*   **GE Queue Size**: **2KB / 4KB** (极小).
    *   *Implication*: 无法一次性提交大量微小绘图指令，会导致 `flush_cmd: write() failed!`，且画面只有部分渲染。
*   **Clipping**: **Hardware does NOT clip**.
    *   *Implication*: 坐标越界会导致驱动报错 `invalid dst crop`。必须在软件层手动计算裁剪或使用安全钳位。
*   **GE Scaler**: 支持 **1/16x ~ 16x** 硬件缩放（双线性插值）。
*   **DMA Coherency**: GE 独立于 CPU Cache。
    *   *Implication*: 源纹理内存必须使用 CMA 分配，且在 CPU 写完数据后，必须调用 `aicos_dcache_clean_range` 同步缓存，否则 GE 读到的是旧数据（全黑或噪点）。

## 3. Software Architecture (软件架构)

### 3.1 Directory Structure
```text
packages/artinchip/ge-demos/
├── Kconfig             # 菜单配置
├── SConscript          # 构建脚本
├── demo_engine.h       # 核心接口定义
├── demo_utils.h        # 基础设施宏与数学工具
├── demo_entry.c        # 引擎入口，负责 FB 初始化、双缓冲管理
└── effects/            # 特效插件目录 (0001 ~ 1001)
```

### 3.2 The Rendering Pipelines (渲染管线)

#### A. Standard Pipeline (标准几何绘图)
适用于简单的几何图形（矩形、线条）。
1.  **Calculate** -> **Draw (to FB)** -> **Emit & Sync** -> **Flip**.

#### B. Hybrid Pipeline (混合渲染 - 推荐标准)
适用于复杂纹理（分形、流体）。
1.  **Off-screen Buffer**: CPU 在低分辨率（如 320x240, RGB565）的 CMA 内存中生成纹理。
2.  **Cache Clean**: 刷新 D-Cache 到 DRAM。
3.  **Hardware Scaling**: 使用 GE `BitBLT` 将纹理放大并搬运至全屏。
4.  **Benefit**: 降低 75% 的像素计算量，降低 50% 的内存写入带宽。

#### C. Feedback Pipeline (反馈渲染管线)
适用于需要历史帧数据的特效（如无限回廊、动态模糊）。
1.  **Allocation**: 分配两个纹理 Buffer A 和 Buffer B (CMA)。
2.  **Ping-Pong**: 定义 `src_idx` 和 `dst_idx`。
3.  **Process**: CPU 从 `src` 读取旧像素，经过衰减/变换后写入 `dst`；或注入新的逻辑种子。
4.  **Scaling**: GE 将 `src` 纹理变换（缩放/旋转/镜像）后叠加至 `dst`。
5.  **Swap**: 交换 `src` 和 `dst` 索引。**严禁**在同一个 Buffer 上同时读写。
6.  **Benefit**: 彻底消除读写竞争（Read-Write Hazard）导致的画面伪影。

#### D. OSD Overlay Pipeline (屏显观测管线)
适用于监控信息或 UI 元素的最后叠加。
1.  **Rendering Sync**: 必须调用 `mpp_ge_sync` 等待硬件渲染队列彻底清空，防止硬软件竞态冲突。
2.  **Pixel Injection (High-Fidelity)**: CPU 直接对目标缓冲区进行 Bit-Blit 点阵渲染。
    - **Font**: 采用 24px 变宽点阵（Inter-Bold）。
    - **Effect**: 引入 2-pixel 黑色偏移阴影以增强高对比度环境下的可读性。
3.  **Cache Power Flush**: 渲染完成后，必须执行 `aicos_dcache_clean_range` 覆盖 OSD 区域，确保 DE 能读取到最新的点阵数据。
4.  **Flip**: 完成最终的画面翻转。
5.  **Constraint**: 建议仅在 640x480 的全屏 UI 层进行注入，避开由于 Scaler 导致的二次失真。

## 4. Coding Standard & Best Practices (编程规范)

### 4.1 Sync Logic (同步律令)
*   **大面积绘图**：**Draw one, Wait one**. 每发一条指令必须立即执行 `mpp_ge_emit` 与 `mpp_ge_sync`。
*   **小面积绘图**：**Batching**. 每 16~64 条指令执行一次 `emit` 和 `sync`。

### 4.2 Clipping & Safety (裁剪与安全)
*   所有绘图坐标 (`dst_buf.crop`) 必须在提交前进行数学截断，确保在 `0 ~ width/height` 范围内。
*   **安全缩放**：严禁设置 `dst_buf.crop` 坐标为负数。若需实现移出屏幕的效果，应反向操作 `src_buf.crop`，使其向内收缩或偏移。

### 4.3 Memory Management (内存管理)
*   **Texture Allocation**: 必须使用 `mpp_phy_alloc()` (CMA)。严禁使用 `rt_malloc` 或静态数组。
*   **Alignment**: 每次分配必须确保物理地址对齐，并使用 `DEMO_ALIGN_SIZE` 确保内存长度对齐 Cache Line（64-byte），这是 DMA 安全的基础。
*   **Cache Flush**: 每次 CPU 更新纹理后，必须调用 `aicos_dcache_clean_range` 同步缓存。

## 5. Hardware Interop (硬件交互进阶)

### 5.1 Blending Polarity (混合极性)
*   `ge_ctrl.alpha_en` 采用硬件反转极性：`0` 为 **Enable** (开启混合)；`1` 为 **Disable** (禁用混合/覆盖)。

### 5.2 Intermediate Sanitization (缓冲区除垢)
*   **Rot1 Artifacts**: 旋转操作无法自动覆盖矩形容的死角。在使用旋转、混合等操作前，必须先用 `mpp_ge_fillrect(0x00)` 清空目标缓冲区。
*   **Antimatter**: 在反馈回路中，可利用 CPU 写入 `0x0000` 或 GE 覆盖黑色来主动消除能量堆积。

### 5.3 Format Boundary (格式边界)
*   GE `BitBLT` 不支持 YUV420P 多平面源输入，仅支持 **YUV400** 纯亮度格式。过程化渲染统一建议使用 **RGB565**。

