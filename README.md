# ArtInChip GE Demos: The 1001 Nights
**基于 ArtInChip D13x GE 硬件加速的高性能过程化图形演示框架**

> "虽身处芥子之内，仍自命为无限宇宙之王。"

> **⚠️ 适配版本**: ArtInChip Luban-Lite **v1.2.2**
> **测试硬件**: D13CCS (RISC-V)


https://github.com/user-attachments/assets/660bc408-6f46-48d8-9508-04a8a11ad756


## 📖 项目简介

本项目是一个专为 **ArtInChip D13x 系列 MCU** 设计的裸机级图形演示系统。它跳过了 LVGL 等通用 GUI 库的抽象层，直接通过底层驱动（HAL）操控 **Display Engine (DE)** 和 **Graphics Engine (GE)**。

**核心特性：**
*   **混合渲染管线 (Hybrid Pipeline)**：采用 CPU 生成低分纹理 (320x240) + GE 硬件实时缩放上屏 (640x480) 的架构，平衡了计算负载与显存带宽。
*   **极致性能**：在 D13CCS (480MHz) 上实现全屏、高帧率 (60FPS)、复杂的数学特效。
*   **过程化生成 (Procedural)**：不依赖外部图片资源，所有视觉效果均由数学公式实时演算。
*   **插件化架构**：利用 Linker Section 技术，新增特效只需添加一个 `.c` 文件即可自动注册，无需修改核心代码。

---

## 🚀 集成指南 (Integration Guide)

本项目作为一个独立的 `Local Package` 存在。
**注意：本项目深度依赖 Luban-Lite v1.2.2 的驱动接口与目录结构，在其他版本上可能需要手动适配。**

### 1. 放置代码
将本项目文件夹 `ge-demos` 放置在 SDK 的 `packages/artinchip/` 目录下：

```text
luban-lite/packages/artinchip/ge-demos/
├── Kconfig             # 菜单配置
├── SConscript          # 构建脚本
├── demo_engine.h       # 核心接口定义
├── demo_entry.c        # 引擎入口，负责 FB 初始化、双缓冲管理
└── effects/            # 特效插件目录
    ├── 0001_xxx.c
    ├── 0002_xxx.c
    └── ...
```

### 2. 修改父级 Kconfig
编辑 `packages/artinchip/Kconfig` 文件，在末尾添加一行：
```kconfig
source "packages/artinchip/ge-demos/Kconfig"
```

### 3. ⚠️ 关键修正：调整文件系统挂载优先级
**原因**：我们需要启用 ArtInChip 官方的 "Bootup Animation" 来确保屏幕和背光被正确初始化。但 Bootup Animation 运行在 `INIT_LATE_APP_EXPORT` (Level 7) 阶段，且依赖 `/data` 分区中的图片资源。系统默认的 `/data` 挂载也在 Level 7，这会导致竞态条件（UI 先运行但找不到图片）。
**解决**：必须将 `/data` 的挂载提前。

编辑文件：`packages/artinchip/env/absystem_os.c`
找到文件末尾，修改 `aic_absystem_mount_fs_prio1` 的导出级别：

```c
/* 修改前 */
// INIT_LATE_APP_EXPORT(aic_absystem_mount_fs_prio1);

/* 修改后：提升至 Level 6 (App)，确保在 Level 7 (Late) 的 UI 启动前挂载完毕 */
INIT_APP_EXPORT(aic_absystem_mount_fs_prio1);
```

### 4. 配置 Menuconfig
运行 `scons --menuconfig` 进行如下设置：

#### A. 禁用 LVGL (释放资源)
```sh
Application options
	[ ] LVGL (official): powerful and easy-to-use embedded GUI library  # 取消勾选
	[ ] ArtInChip LVGL demo  # 取消勾选
```

#### B. 启用显示驱动与开机动画 (接管硬件初始化)
```sh
 Board options
    Display Parameter
        [*] Bootup Logo  # 勾选
            select logo type (Bootup Animation)
```
*   *注意：必须选 Animation。选 Black Screen 会导致部分板型背光无法点亮。*

#### C. 启用本项目
```sh
Local packages options
    ArtInChip packages options
        [*] ArtInChip GE Demos (Direct Graphics) # 勾选
	        [*]   Auto start demo after boot
	        [*]   Enable Key Control
	        (PA.5)  Previous Effect Key Pin
	        (PE.4)  Next Effect Key Pin
```

### 5. 接管入口函数 (Main Handover)
修改 `applications/rt-thread/helloworld/main.c`，实现无缝接管：

```c
#include <rtthread.h>
/* ... */
#ifdef AIC_GE_DEMO_AUTO_START
#include "demo_engine.h"
#endif

int main(void)
{
    /* ... Log filter ... */

#ifdef AIC_GE_DEMO_AUTO_START
    // 1. 初始化 Demo 核心
    demo_core_init();

    // 2. 启动渲染线程 (后台运行)
    demo_core_start();

    rt_kprintf("Main: GE Demo running in background. Shell is ready.\n");
#else
    rt_kprintf("Main: System Started. (Demo Auto-Start is disabled)\n");
#endif

    return 0;
}
```

### 6. 编译与烧录
```bash
scons -j4
# 使用烧录工具更新固件
```

---

## 🎮 控制与交互

Demo 运行后，支持通过 **UART 串口命令行** 或 **物理按键** 进行控制。

### 串口命令 (MSH)
| 命令 | 描述 |
| :--- | :--- |
| `demo_next` | 切换到下一个特效 |
| `demo_prev` | 切换到上一个特效 |
| `demo_jump <id>` | 跳转到指定序号的特效 (如 `demo_jump 5`) |
| `demo_list` | 列出所有特效 |

### 物理按键 (需在 Menuconfig 中配置)
*   **Key Prev**: 上一个特效
*   **Key Next**: 下一个特效

---

## 🛠️ 开发新特效

1.  在 `effects/` 目录下新建文件 (如 `0011_new_effect.c`)。
2.  实现 `init`, `draw`, `deinit` 函数。
3.  定义 `struct effect_ops` 并使用 `REGISTER_EFFECT` 宏注册。
4.  **技术规范**：务必遵守 `SPEC.md` 中的混合渲染管线和内存安全规范。

---

## ⚠️ 免责与维护声明 (Disclaimer)

1.  **无维护承诺**：本项目属于一次性创作或实验性项目。作者**不保证**后续的代码更新、Bug 修复或针对新版 SDK 的适配。
2.  **按原样提供**：代码按“原样”提供，不带任何明示或暗示的担保。因使用本代码导致的任何硬件损坏（如屏幕烧毁、过热）或数据丢失，作者概不负责。

**Author**: Vagra & Gemini

**License**: The Unlicense (Public Domain)
