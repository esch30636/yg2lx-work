# Battery State-of-Health Assessment System for MYD-YG2LX Embedded Platform

# 基于MYD-YG2LX嵌入式平台的电池健康状态评估系统

---

## Abstract

This repository contains a complete embedded software suite for real-time lithium iron phosphate (LiFePO4) battery State-of-Health (SOH) assessment, targeting the MYiR MYD-YG2LX development board based on the Renesas RZ/G2L system-on-chip (ARM Cortex-A55, ARMv8.2-A). The system integrates dual neural network inference engines --- a Physics-Informed Neural Network (PINN) for rapid SOH screening and a one-dimensional residual convolutional neural network (1D-ResNet CNN) for precise aging stage classification with Remaining Useful Life (RUL) prediction --- alongside a Qt5-based industrial human-machine interface (HMI) deployed on Wayland. All inference code is implemented in pure C with zero external dependencies and validated against ONNX Runtime reference outputs. The repository also includes U-Boot, ARM Trusted Firmware-A, Linux kernel, device tree overlays, and a SPI flash writer for complete board-level deployment.

## 摘要

本仓库包含一套完整的嵌入式软件组件，用于磷酸铁锂（LiFePO4）电池的实时健康状态（SOH）评估，目标平台为基于瑞萨RZ/G2L片上系统（ARM Cortex-A55，ARMv8.2-A）的MYiR MYD-YG2LX开发板。系统集成了双神经网络推理引擎——物理信息神经网络（PINN）用于快速SOH筛查，以及一维残差卷积神经网络（1D-ResNet CNN）用于精确的老化阶段分类与剩余使用寿命（RUL）预测——同时包含基于Qt5的工业人机界面（HMI），部署于Wayland合成器之上。所有推理代码以纯C语言实现，无任何外部依赖，并已通过ONNX Runtime参考输出验证。仓库同时包含U-Boot、ARM可信固件-A、Linux内核、设备树覆盖层及SPI闪存烧录工具，支持完整的板级部署。

---

## Table of Contents / 目录

1. [System Architecture / 系统架构](#system-architecture)
2. [Repository Structure / 仓库结构](#repository-structure)
3. [Dual-Model Inference Engine / 双模型推理引擎](#dual-model-inference-engine)
4. [Human-Machine Interface / 人机界面](#human-machine-interface)
5. [Deployment on Target Board / 板端部署](#deployment-on-target-board)
6. [Build and Deployment / 构建与部署](#build-and-deployment)
7. [Cross-Compilation Toolchain / 交叉编译工具链](#cross-compilation-toolchain)
8. [Validation and Benchmarking / 验证与基准测试](#validation-and-benchmarking)
9. [Target Platform / 目标平台](#target-platform)
10. [License / 许可证](#license)

---

## System Architecture

The system follows a multi-tier architecture comprising three principal layers:

| Layer | Component | Description |
|-------|-----------|-------------|
| Inference | `dual_model/` | Dual-model engine (PINN + CNN), pure C, with CLI and library interfaces |
| Presentation | `dual_model/hmi/` | Qt5 graphical application with multi-screen wizard, charting, and internationalization, deployed on Wayland (Weston compositor) |
| Platform | `sources/` | Bootloader (TF-A, U-Boot), Linux kernel image, device tree blobs, and SPI flash writer |

Data flows from either a physical sensor interface (RA8 microcontroller via SPI) or pre-recorded session files through a preprocessing pipeline, into the dual-model inference engine. PINN inference executes on the main thread (~3.5 ms per prediction) feeding a sliding-window SOH accumulator with convergence detection, while CNN inference (~45 ms) runs on a dedicated QThread to avoid blocking the Qt event loop. Results are rendered on the HMI with real-time charting, cycle progress monitoring, and multi-level alarm indicators.

## 系统架构

系统采用多层架构，包含三个主要层次：

| 层级 | 组件 | 描述 |
|-------|-----------|-------------|
| 推理层 | `dual_model/` | 双模型引擎（PINN + CNN），纯C实现，提供CLI和库接口 |
| 表现层 | `dual_model/hmi/` | Qt5图形应用程序，具备多屏向导、图表绘制和国际化功能，部署于Wayland（Weston合成器）之上 |
| 平台层 | `sources/` | 引导加载程序（TF-A、U-Boot）、Linux内核镜像、设备树二进制文件及SPI闪存烧录工具 |

数据流从物理传感器接口（RA8微控制器通过SPI）或预录会话文件经预处理管道进入双模型推理引擎。PINN推理在主线程上执行（每次预测约3.5 ms），馈送至带收敛检测的滑动窗口SOH累加器；CNN推理（约45 ms）在专用QThread上运行，以避免阻塞Qt事件循环。推理结果在HMI上以实时图表、循环进度监控和多级报警指示器形式呈现。

---

## Repository Structure

## 仓库结构

```
yg2lx-work/
|
|-- dual_model/                         # 主双模型推理引擎 + HMI
|   |-- include/                        #   公共头文件
|   |   |-- battery_inference.h         #     统一API（PINN + CNN）
|   |   |-- pinn_weights.h              #     PINN预解量化权重（float32）
|   |   |-- pinn_meta.h                 #     PINN架构元数据
|   |   |-- cnn_weights.h               #     CNN预解量化权重（float32）
|   |   |-- cnn_meta.h                  #     CNN架构元数据
|   |   |-- scalers.h                   #     输入归一化缩放器参数（PINN + CNN）
|   |-- src/                            #   C推理实现
|   |   |-- pinn_inference.c            #     PINN前向传播
|   |   |-- cnn_inference.c             #     CNN前向传播
|   |   |-- battery_infer.c             #     命令行接口 + 性能基准测试
|   |   |-- debug_pinn.c                #     PINN逐层调试工具
|   |-- hmi/                            #   Qt5 HMI应用程序
|   |   |-- main.cpp                    #     应用程序入口点
|   |   |-- CMakeLists.txt              #     CMake构建文件（Qt5 + 推理引擎）
|   |   |-- config/                     #     应用配置
|   |   |   |-- AppConfig.h             #       全局配置常量与字体定义
|   |   |-- inference/                  #     C++推理封装与预处理器
|   |   |   |-- InferenceEngine.h/cpp   #       RAII封装（PINN + CNN上下文管理）
|   |   |   |-- CnnPreprocessor.h/cpp   #       CNN预处理管道（IC归一化 + 梯度计算）
|   |   |   |-- SohAccumulator.h        #       滑动窗口SOH累加器（中位数/均值/CI95/收敛检测）
|   |   |-- data/                       #     数据提供器
|   |   |   |-- DataProvider.h          #       抽象接口（BatterySample, StorageInfo, AlarmState）
|   |   |   |-- DemoDataProvider.h/cpp  #       合成电池数据生成（LiFePO4双峰模型）
|   |   |   |-- FileDataProvider.h/cpp  #       二进制会话文件回放
|   |   |   |-- SpiDataProvider.h/cpp   #       RA8 SPI实时数据采集
|   |   |-- ui/                         #     UI控件
|   |   |   |-- MainWindow.h/cpp        #       主窗口（标签页布局 + 数据定时器）
|   |   |   |-- ChartWidget.h/cpp       #       Qt Charts IC曲线实时可视化
|   |   |   |-- StatusPanel.h/cpp       #       状态面板（电压/电流/温度/SOH/RUL）
|   |   |   |-- CycleProgressBar.h/cpp  #       循环测试进度条（当前/总计 + 耗时估计）
|   |   |   |-- StorageWidget.h/cpp     #       Octa-NAND存储使用率与坏块监控
|   |   |   |-- AlarmIndicator.h/cpp    #       顶部栏常驻报警指示器
|   |   |   |-- AlarmPopup.h/cpp        #       报警弹出通知（带冷却间隔）
|   |   |   |-- screens/               #       多屏向导
|   |   |       |-- InitScreen.h/cpp    #         初始化屏幕（启动画面）
|   |   |       |-- ModelSelectScreen.h/cpp  #     模型选择屏幕
|   |   |       |-- ResultScreen.h/cpp  #         结果屏幕（SOH/RUL + 置信度）
|   |   |-- threads/                    #     工作线程
|   |   |   |-- InferenceWorker.h/cpp   #       QThread CNN推理（QueuedConnection）
|   |   |-- deploy/                     #     板端部署资源
|   |   |   |-- deploy.sh              #        交叉编译 + USB/SCP部署
|   |   |   |-- start_hmi.sh           #        板端一键启动脚本（Weston + HMI）
|   |   |   |-- show_splash.sh         #        Framebuffer开机画面脚本（ffmpeg）
|   |   |   |-- hmi-startup.service    #        systemd服务文件（Wayland + 崩溃恢复）
|   |   |-- translations/              #     Qt国际化文件
|   |   |   |-- battery_hmi_zh_CN.ts   #       中文翻译源文件
|   |   |   |-- battery_hmi_zh_CN.qm   #       已编译的中文翻译
|   |   |-- scripts/                   #     辅助脚本
|   |   |   |-- generate_demo_data.py  #       LiFePO4演示数据生成器（900帧双峰模型）
|   |   |   |-- ts2qm.py               #       Qt .ts到.qm编译脚本
|   |   |-- resources/                 #     Qt资源文件
|   |   |   |-- resources.qrc          #       QRC资源清单
|   |   |   |-- splash.jpg             #       启动画面图片
|   |   |   |-- icons/                 #       图标资源目录
|   |   |   |-- style/                 #       QSS样式主题
|   |   |       |-- industrial.qss     #         工业风格全局样式表
|   |   |-- demo_data_18650_lfp.bin    #     预生成的LiFePO4 18650演示数据
|   |-- tests/                         #   验证与基准测试脚本
|   |   |-- benchmark_accuracy.py      #     与ONNX Runtime的数值精度对比
|   |   |-- verify_preprocess.cpp      #     预处理管道等价性测试
|   |-- deploy/                        #   部署自动化
|   |   |-- deploy.sh                  #     交叉编译 + 通过USB或SCP部署
|   |-- export_weights.py              #   ONNX/PyTorch到C头文件的权重导出脚本
|   |-- verify_accuracy.py             #   顶层精度验证
|   |-- Makefile                        #   构建系统（本地、交叉、验证、HMI）
|
|-- cnn_inference/                     # 独立CNN推理原型（早期原型验证）
|   |-- include/                       #   公共头文件
|   |   |-- cnn_inference.h            #     CNN推理API
|   |   |-- model_weights.h            #     解量化权重
|   |   |-- model_meta.h               #     模型元数据
|   |   |-- gelu_consts.h              #     GELU激活函数近似常数
|   |-- src/                           #   纯C CNN实现
|   |   |-- cnn_inference.c            #     前向传播（Conv1D、GELU、池化、残差块）
|   |-- test/                          #   测试框架与验证
|   |   |-- test_cnn.c                 #     单元测试与推理正确性测试
|   |   |-- verify_accuracy.py         #     ONNX Runtime对比验证
|   |   |-- debug_cnn.c                #     调试工具（中间张量导出）
|   |   |-- debug_layers.py            #     逐层输出调试脚本
|   |-- export_weights.py              #   权重导出脚本
|   |-- analyze_onnx.py                #   ONNX模型结构与算子分析工具
|   |-- Makefile                       #   构建系统（本地、交叉、静态库、验证）
|
|-- sdk/                               # Yocto Project SDK工具链安装器
|   |-- poky-glibc-x86_64-myir-image-core-...sh        # 核心镜像SDK（GCC + binutils + sysroot）
|   |-- poky-glibc-x86_64-myir-image-full-...sh        # 完整镜像SDK（含Qt5库 + CMake工具链）
|
|-- sources/                           # 平台级源码与预编译二进制
|   |-- myir-renesas-flash-writer/      # SPI闪存烧录器（SCIF启动模式，RZ/G2L DDR4 2GB）
|   |-- myir-renesas-tf-a/              # ARM可信固件-A（BL2 + BL31 + BL32）
|   |-- myir-renesas-uboot/             # U-Boot引导加载程序
|   |-- myir-renesas-linux/             # Linux内核源码树（CIP内核）
|   |-- build/                          # 预编译平台二进制文件
|       |-- Image                       #   Linux内核镜像
|       |-- myb-rzg2l-disp.dtb          #   设备树（显示配置）
|       |-- myb-rzg2l-hdmi.dtb          #   设备树（HDMI配置）
|
|-- .gitignore
|-- .vscode/                           # VSCode工作区设置
```

---

## Dual-Model Inference Engine

### Overview

The inference engine implements two complementary neural network models for battery health assessment, both written in pure C (C11 standard) with zero external library dependencies beyond `libm`. All model weights are pre-dequantized to IEEE 754 single-precision floating-point (float32) at build time and embedded directly as C header files via `export_weights.py`.

### PINN --- Physics-Informed Neural Network

The PINN model serves as a fast screening tool for rapid SOH estimation from raw 132-dimensional feature vectors. It employs a compact multilayer perceptron with a residual skip connection and Gaussian Error Linear Unit (GELU) activations throughout.

**Architecture:**

```
Linear(132 -> 128) -> LayerNorm -> GELU
  -> Linear(128 -> 128) -> LayerNorm -> GELU
  -> Linear(128 -> 64)
  -> ResidualBlock: FC(64->64)+Norm+GELU -> FC(64->64)+Norm -> +skip -> GELU
  -> FC(64->32)+Norm+GELU -> FC(32->1) -> Sigmoid -> SOH
```

**Input features (132 dimensions):**

| Index Range | Description |
|-------------|-------------|
| [0, 127] | Incremental capacity (IC) curve on voltage grid |
| [128] | Temperature |
| [129] | log10(cycle_count + 1) |
| [130] | dV proxy (start voltage drop) |
| [131] | Normalized capacity (measured / nominal) |

**Output:** SOH scalar in the range [0, 1], representing the fraction of nominal capacity retained.

**Performance:** ~3.5 ms per inference on Cortex-A55 at 1.2 GHz (single-core, scalar-only; no NEON SIMD).

### CNN --- 1D Residual Convolutional Network

The CNN model provides precise aging stage classification and RUL prediction from dual-channel incremental capacity curves. The architecture employs three residual blocks with progressively increasing channel counts and a dual-head output.

**Architecture:**

```
Input [1, 2, 128] — IC curve + IC gradient, dual-channel
  -> Stem:  Conv1D(2->16, k7, stride=2, pad=3) + GELU          -> [16, 64]
  -> Body0: Conv(16->16, k7) + GELU + Conv(16->16, k7) + GELU
            + identity skip                                     -> [16, 64]
  -> MaxPool(k2)                                                -> [16, 32]
  -> Body1: Shortcut(16->32, k1) + Conv(16->32, k7) + GELU
            + Conv(32->32, k7) + GELU                           -> [32, 32]
  -> MaxPool(k2)                                                -> [32, 16]
  -> Body2: Shortcut(32->48, k1) + Conv(32->48, k5) + GELU
            + Conv(48->48, k5) + GELU                           -> [48, 16]
  -> MaxPool(k2)                                                -> [48, 8]
  -> Global Average Pooling                                     -> [48]
  -> cls_head: FC(48->48)+GELU -> FC(48->24)+GELU -> FC(24->3) -> stage_logits[3]
  -> rul_head: FC(48->48)+GELU -> FC(48->24)+GELU -> FC(24->1) -> RUL
```

**Preprocessing Pipeline:** Raw 128-point IC curves undergo a three-stage normalization:
1. StandardScaler normalization with [-5, +5] clipping
2. Central-difference gradient computation (matching NumPy `np.gradient` with `edge_order=1`)
3. Abs-max normalization followed by StandardScaler normalization, clipped to [-5, +5]

The preprocessed output is a 256-element dual-channel array: channel 0 carries the normalized IC curve, and channel 1 carries the normalized IC gradient.

**Input:** 256-element float32 array comprising:
- Channel 0 (indices [0, 127]): normalized IC curve (dQ/dV)
- Channel 1 (indices [128, 255]): normalized IC gradient

**Output:**
- `stage_logits[3]`: raw logits for three aging stages --- healthy (0), degrading (1), end-of-life (2)
- `rul`: remaining useful life, normalized to [0, 1]

**Performance:** ~45 ms per inference on Cortex-A55 at 1.2 GHz (scalar-only; no NEON SIMD).

### Sliding-Window SOH Accumulator

The `SohAccumulator` (`hmi/inference/SohAccumulator.h`) maintains a fixed-size deque of consecutive PINN SOH predictions (default window size: 1200 samples). It computes the following statistics on demand:

- Median, mean, standard deviation, min, max
- 95% confidence interval half-width (1.96 * stddev / sqrt(n))
- Convergence detection: when stddev < epsilon (default: 0.005) for *k* consecutive windows (default: *k* = 3), the accumulator signals that the SOH estimate has stabilized

This mechanism filters measurement noise and enables the HMI to terminate PINN acquisition automatically once a reliable SOH value has been established.

### C++ RAII Wrapper

`InferenceEngine` (`hmi/inference/InferenceEngine.h`) provides a C++ RAII wrapper around the opaque C contexts (`pinn_ctx_t`, `cnn_ctx_t`). It is non-copyable and non-movable, ensuring deterministic resource release. For CNN inference, the `InferenceWorker` (`hmi/threads/InferenceWorker.h`) runs a dedicated `QThread` instance and communicates results via Qt queued signal/slot connections, preventing the ~45 ms CNN forward pass from blocking the main UI thread.

### API Overview

```c
#include "battery_inference.h"

// PINN — Fast SOH screening
pinn_ctx_t *pinn = pinn_init();
float soh = pinn_predict(pinn, features_132);
pinn_free(pinn);

// CNN — Stage classification + RUL
cnn_ctx_t *cnn = cnn_init();
cnn_result_t r = cnn_predict(cnn, ic_curve_256);
// r.stage_logits[3], r.rul
cnn_free(cnn);
```

### Memory Model

Both inference contexts allocate scratch buffers dynamically at initialization via `malloc()`. No dynamic allocation occurs during inference; all intermediate tensors are computed in place within the pre-allocated scratch space. This design ensures deterministic latency suitable for real-time embedded deployment.

---

## 双模型推理引擎

### 概述

推理引擎实现了两个互补的神经网络模型用于电池健康评估，均以纯C语言（C11标准）编写，除`libm`外无任何外部库依赖。所有模型权重在构建时预解量化为IEEE 754单精度浮点数（float32），并通过`export_weights.py`直接嵌入为C头文件。

### PINN --- 物理信息神经网络

PINN模型用作快速筛查工具，从原始132维特征向量中快速估计SOH。模型采用紧凑的多层感知机结构，包含一个残差跳跃连接，全部使用GELU（高斯误差线性单元）激活函数。

**网络架构：**

```
Linear(132 -> 128) -> LayerNorm -> GELU
  -> Linear(128 -> 128) -> LayerNorm -> GELU
  -> Linear(128 -> 64)
  -> ResidualBlock: FC(64->64)+Norm+GELU -> FC(64->64)+Norm -> +skip -> GELU
  -> FC(64->32)+Norm+GELU -> FC(32->1) -> Sigmoid -> SOH
```

**输入特征（132维）：**

| 索引范围 | 描述 |
|-------------|-------------|
| [0, 127] | 电压网格上的增量容量（IC）曲线 |
| [128] | 温度 |
| [129] | log10(循环次数 + 1) |
| [130] | dV代理量（起始电压降） |
| [131] | 归一化容量（实测 / 标称） |

**输出：** SOH标量，范围[0, 1]，表示相对标称容量的保留比例。

**性能：** 每次推理约3.5 ms（Cortex-A55 @ 1.2 GHz，单核，纯标量运算，未使用NEON SIMD）。

### CNN --- 一维残差卷积网络

CNN模型从双通道增量容量曲线提供精确的老化阶段分类和RUL预测。网络采用三个残差块，通道数逐步递增，输出为双头结构。

**网络架构：**

```
Input [1, 2, 128] — IC曲线 + IC梯度，双通道
  -> Stem:  Conv1D(2->16, k7, stride=2, pad=3) + GELU          -> [16, 64]
  -> Body0: Conv(16->16, k7) + GELU + Conv(16->16, k7) + GELU
            + identity skip                                     -> [16, 64]
  -> MaxPool(k2)                                                -> [16, 32]
  -> Body1: Shortcut(16->32, k1) + Conv(16->32, k7) + GELU
            + Conv(32->32, k7) + GELU                           -> [32, 32]
  -> MaxPool(k2)                                                -> [32, 16]
  -> Body2: Shortcut(32->48, k1) + Conv(32->48, k5) + GELU
            + Conv(48->48, k5) + GELU                           -> [48, 16]
  -> MaxPool(k2)                                                -> [48, 8]
  -> Global Average Pooling                                     -> [48]
  -> cls_head: FC(48->48)+GELU -> FC(48->24)+GELU -> FC(24->3) -> stage_logits[3]
  -> rul_head: FC(48->48)+GELU -> FC(48->24)+GELU -> FC(24->1) -> RUL
```

**预处理管道：** 原始128点IC曲线经过三阶段归一化处理：
1. StandardScaler归一化 + [-5, +5]裁剪
2. 中心差分梯度计算（与NumPy `np.gradient`（`edge_order=1`）一致）
3. 绝对值最大值归一化 + StandardScaler归一化，裁剪至[-5, +5]

预处理输出为256元素双通道数组：通道0为归一化IC曲线，通道1为归一化IC梯度。

**输入：** 256元素float32数组，包含：
- 通道0（索引[0, 127]）：归一化IC曲线（dQ/dV）
- 通道1（索引[128, 255]）：归一化IC梯度

**输出：**
- `stage_logits[3]`：三个老化阶段的原始logits——健康（0）、衰退（1）、寿命终止（2）
- `rul`：剩余使用寿命，归一化至[0, 1]

**性能：** 每次推理约45 ms（Cortex-A55 @ 1.2 GHz，纯标量运算，未使用NEON SIMD）。

### 滑动窗口SOH累加器

`SohAccumulator`（`hmi/inference/SohAccumulator.h`）维护一个固定大小的双端队列（默认窗口大小：1200样本），用于累积连续的PINN SOH预测。它按需计算以下统计量：

- 中位数、均值、标准差、最小值、最大值
- 95%置信区间半宽（1.96 * stddev / sqrt(n)）
- 收敛检测：当stddev < epsilon（默认0.005）连续保持*k*个窗口（默认*k*=3），累加器判定SOH估计已趋于稳定

该机制可滤除测量噪声，使HMI在获取可靠SOH值后能够自动终止PINN采集。

### C++ RAII封装

`InferenceEngine`（`hmi/inference/InferenceEngine.h`）提供了对不透明C上下文（`pinn_ctx_t`、`cnn_ctx_t`）的C++ RAII封装。该类不可拷贝、不可移动，确保资源的确定性释放。对于CNN推理，`InferenceWorker`（`hmi/threads/InferenceWorker.h`）运行一个专用的`QThread`实例，通过Qt排队信号/槽连接传递结果，避免约45 ms的CNN前向传播阻塞主UI线程。

### API概览

```c
#include "battery_inference.h"

// PINN — 快速SOH筛查
pinn_ctx_t *pinn = pinn_init();
float soh = pinn_predict(pinn, features_132);
pinn_free(pinn);

// CNN — 阶段分类 + RUL
cnn_ctx_t *cnn = cnn_init();
cnn_result_t r = cnn_predict(cnn, ic_curve_256);
// r.stage_logits[3], r.rul
cnn_free(cnn);
```

### 内存模型

两个推理上下文在初始化时通过`malloc()`动态分配暂存缓冲区。推理过程中不产生任何动态内存分配；所有中间张量均在预分配的暂存空间内就地计算。该设计确保了适用于实时嵌入式部署的确定性时延。

---

## Human-Machine Interface

The HMI is a Qt5 (Qt 5.15+) graphical application designed for the MYD-YG2LX platform. It targets Wayland with EGL acceleration on the Mali-G31 GPU via the `wayland-egl` QPA backend, with fallback support for the Linux framebuffer (`linuxfb`) and the standalone EGL full-screen backend (`eglfs`).

### Features

- **Multi-screen wizard:** InitScreen (splash), ModelSelectScreen (PINN/CNN selection), and ResultScreen (final SOH/RUL with confidence intervals) provide a guided workflow
- **Real-time charting:** IC curve visualization via Qt Charts with overlay annotations
- **Cycle progress tracking:** `CycleProgressBar` displays current cycle / total cycles with elapsed and estimated remaining time
- **Storage monitoring:** `StorageWidget` shows Octa-NAND usage (used/total bytes) and bad block count
- **Alarm system:** `AlarmIndicator` (persistent status bar) and `AlarmPopup` (modal notification) for threshold violations with configurable cooldown intervals (default: 30 s) to prevent alarm flooding
- **SOH convergence detection:** `SohAccumulator` sliding-window statistics with 95% confidence interval
- **Internationalization:** Chinese (zh_CN) and English (en) language support via Qt Linguist
- **Crash diagnostics:** Signal handlers (SIGSEGV, SIGABRT, SIGFPE) with backtrace generation for embedded debugging
- **Font fallback:** Automatic CJK font discovery with multiple candidates (Noto Sans CJK SC, Source Han Sans SC, WenQuanYi) for Chinese character rendering
- **Industrial QSS theme:** Dark industrial-style global stylesheet (`industrial.qss`)
- **Splash screen:** Framebuffer splash display via ffmpeg (`show_splash.sh`) for professional boot experience

### Data Providers

| Provider | Flag | Description |
|----------|------|-------------|
| Demo | `--demo` | Synthetic LiFePO4 18650 data (dual-peak IC curve model, SOH 1.0->0.65 over 900 frames) |
| File | `--file <path>` | Replay pre-recorded binary session files (260 float32 per frame: 128 IC + 132 features) |
| SPI | `--spi <device>` | Real-time data acquisition from RA8 microcontroller via SPI bus |
| SPI Mock | `--spi-mock` | SPI protocol simulation without hardware (synthetic RA8 response) |

### Resource Files

| File | Description |
|------|-------------|
| `resources/style/industrial.qss` | Global Qt stylesheet --- dark industrial theme |
| `resources/splash.jpg` | Boot splash screen image |
| `resources/icons/` | Application icon resources |
| `demo_data_18650_lfp.bin` | Pre-generated LiFePO4 demo data (900 frames, ~936 KB) |

### Usage

```bash
# Demo mode with synthetic data
battery_hmi --demo

# Fullscreen on embedded display (Wayland)
battery_hmi --demo --fullscreen

# Replay recorded session
battery_hmi --file /data/session_001.bin

# Real-time SPI data acquisition
battery_hmi --spi /dev/spidev0.0

# Chinese UI
battery_hmi --demo --lang zh

# Environment variables for QPA backend
export QT_QPA_PLATFORM=wayland-egl           # Wayland + EGL (Mali-G31) — recommended
export QT_QPA_PLATFORM=eglfs                 # Standalone EGL full-screen
export QT_QPA_PLATFORM=linuxfb:fb=/dev/fb0   # Framebuffer fallback
```

---

## 人机界面

HMI是一个基于Qt5（Qt 5.15+）的图形应用程序，专为MYD-YG2LX平台设计。其首选显示后端为Wayland（通过`wayland-egl` QPA后端在Mali-G31 GPU上实现EGL加速），并支持Linux帧缓冲（`linuxfb`）和独立EGL全屏后端（`eglfs`）回退模式。

### 功能特性

- **多屏向导：** InitScreen（启动画面）、ModelSelectScreen（PINN/CNN模型选择）和ResultScreen（带置信区间的最终SOH/RUL）提供引导式工作流
- **实时图表：** 通过Qt Charts实现IC曲线可视化，带叠加标注
- **循环进度跟踪：** `CycleProgressBar`显示当前循环／总循环数，及已用时间和估计剩余时间
- **存储监控：** `StorageWidget`显示Octa-NAND使用情况（已用／总字节数）和坏块数量
- **报警系统：** `AlarmIndicator`（常驻状态栏）和`AlarmPopup`（模态通知），阈值超限时触发，带可配置冷却间隔（默认30 s）以防止报警泛滥
- **SOH收敛检测：** `SohAccumulator`滑动窗口统计，带95%置信区间
- **国际化：** 通过Qt Linguist支持中文（zh_CN）和英文（en）界面语言
- **崩溃诊断：** 信号处理器（SIGSEGV、SIGABRT、SIGFPE）生成回溯信息，便于嵌入式调试
- **字体回退：** 自动发现CJK字体，提供多个候选字体（Noto Sans CJK SC、Source Han Sans SC、WenQuanYi）以支持中文字符渲染
- **工业级QSS主题：** 深色工业风格全局样式表（`industrial.qss`）
- **开机画面：** 通过ffmpeg的帧缓冲开机画面显示（`show_splash.sh`），提供专业的启动体验

### 数据提供器

| 提供器 | 标志参数 | 描述 |
|----------|------|-------------|
| Demo | `--demo` | 合成LiFePO4 18650数据（双峰IC曲线模型，SOH 1.0->0.65，共900帧） |
| File | `--file <path>` | 回放预录二进制会话文件（每帧260个float32：128 IC + 132特征） |
| SPI | `--spi <device>` | 通过SPI总线从RA8微控制器实时采集数据 |
| SPI Mock | `--spi-mock` | 无硬件情况下的SPI协议模拟（合成RA8响应） |

### 资源文件

| 文件 | 描述 |
|------|-------------|
| `resources/style/industrial.qss` | 全局Qt样式表——深色工业主题 |
| `resources/splash.jpg` | 启动画面图片 |
| `resources/icons/` | 应用程序图标资源 |
| `demo_data_18650_lfp.bin` | 预生成的LiFePO4演示数据（900帧，约936 KB） |

### 用法

```bash
# 合成数据演示模式
battery_hmi --demo

# 嵌入式显示全屏模式（Wayland）
battery_hmi --demo --fullscreen

# 回放已录制的会话
battery_hmi --file /data/session_001.bin

# 实时SPI数据采集
battery_hmi --spi /dev/spidev0.0

# 中文界面
battery_hmi --demo --lang zh

# QPA后端环境变量
export QT_QPA_PLATFORM=wayland-egl           # Wayland + EGL（Mali-G31）——推荐
export QT_QPA_PLATFORM=eglfs                 # 独立EGL全屏
export QT_QPA_PLATFORM=linuxfb:fb=/dev/fb0   # 帧缓冲回退
```

---

## Deployment on Target Board

### Board-Side Files

The `dual_model/hmi/deploy/` directory contains all resources needed for on-target deployment:

| File | Purpose |
|------|---------|
| `deploy.sh` | Cross-compiles the HMI and copies the binary to a USB stick or via SCP |
| `start_hmi.sh` | One-click board-side launch script: cleans up residual processes, releases the framebuffer console, starts Weston with retry logic (3 attempts), then launches the HMI |
| `show_splash.sh` | Renders a JPEG splash image directly to `/dev/fb0` using ffmpeg (1920x1080 BGRX raw framebuffer) |
| `hmi-startup.service` | systemd service unit for automatic HMI launch on boot with Weston + Wayland, including crash recovery (Restart=on-failure, RestartSec=10s) |

### systemd Service Installation

```bash
# On target board (MYD-YG2LX)
cp hmi-startup.service /etc/systemd/system/
cp start_hmi.sh /usr/local/bin/
cp show_splash.sh /usr/local/bin/
chmod +x /usr/local/bin/start_hmi.sh /usr/local/bin/show_splash.sh
systemctl daemon-reload
systemctl enable hmi-startup
systemctl start hmi-startup
```

### Board-Side Startup Sequence

1. `show_splash.sh` displays a splash image on the framebuffer (pre-Weston, fast boot feedback)
2. `systemd` starts `hmi-startup.service`
3. The service releases the framebuffer console (`vtcon1`), starts Weston on `/dev/tty2`, and waits for the Wayland socket (`/run/user/0/wayland-0`) to become available
4. Once Weston is ready, the service launches `battery_hmi --demo` with `QT_QPA_PLATFORM=wayland-egl`

## 板端部署

### 板端文件

`dual_model/hmi/deploy/`目录包含板端部署所需的所有资源：

| 文件 | 用途 |
|------|---------|
| `deploy.sh` | 交叉编译HMI并通过USB或SCP将二进制文件复制到目标板 |
| `start_hmi.sh` | 板端一键启动脚本：清理残留进程，释放帧缓冲控制台，以重试逻辑（最多3次）启动Weston，然后启动HMI |
| `show_splash.sh` | 使用ffmpeg将JPEG启动画面直接渲染至`/dev/fb0`（1920x1080 BGRX原始帧缓冲） |
| `hmi-startup.service` | systemd服务单元，用于开机自动启动HMI（Weston + Wayland），包含崩溃恢复（Restart=on-failure，RestartSec=10s） |

### systemd服务安装

```bash
# 在目标板（MYD-YG2LX）上执行
cp hmi-startup.service /etc/systemd/system/
cp start_hmi.sh /usr/local/bin/
cp show_splash.sh /usr/local/bin/
chmod +x /usr/local/bin/start_hmi.sh /usr/local/bin/show_splash.sh
systemctl daemon-reload
systemctl enable hmi-startup
systemctl start hmi-startup
```

### 板端启动流程

1. `show_splash.sh`在帧缓冲上显示开机画面（Weston启动前，快速启动反馈）
2. `systemd`启动`hmi-startup.service`
3. 服务释放帧缓冲控制台（`vtcon1`），在`/dev/tty2`上启动Weston，等待Wayland套接字（`/run/user/0/wayland-0`）可用
4. Weston就绪后，服务以`QT_QPA_PLATFORM=wayland-egl`启动`battery_hmi --demo`

---

## Build and Deployment

### Prerequisites

- **Host build:** GCC with C11 support; Qt5 development libraries (for HMI)
- **Cross-compilation:** Yocto Project SDK for MYD-YG2LX (see [SDK section](#cross-compilation-toolchain))
- **Verification:** Python 3 with ONNX Runtime and NumPy
- **Docker (optional):** Pre-configured `yg2lx-dev` container with the full SDK

### Inference Engine (CLI)

```bash
cd dual_model

# Native host build (for testing and verification)
make native

# Cross-compile for MYD-YG2LX (aarch64)
source /opt/yg2lx/environment-setup-aarch64-poky-linux
make cross

# Stripped production binary
make cross-fast

# Verify numerical accuracy against ONNX Runtime
make verify

# Preprocessing pipeline equivalence test
make verify-preprocess

# Accuracy benchmark
make benchmark-accuracy
```

### HMI Application

```bash
cd dual_model

# Cross-compile HMI (requires full SDK with Qt5)
source /opt/yg2lx-full/environment-setup-aarch64-poky-linux
make hmi-cross

# Or using the deploy script (handles Docker + SDK automatically)
./hmi/deploy/deploy.sh

# Deploy to target board via SCP
./hmi/deploy/deploy.sh --scp <board-ip>

# Deploy via USB stick
./hmi/deploy/deploy.sh --usb
```

### Standalone CNN Library (Prototype)

```bash
cd cnn_inference

# Native host build + test
make

# Cross-compile for aarch64
make cross

# Build static library
make lib
make lib-cross

# Verify against ONNX Runtime
make verify
```

### Inference CLI Usage

```bash
# PINN: 132-d features to SOH
./battery_infer pinn features_132d.bin

# CNN: 128-point IC curve to stage + RUL
./battery_infer cnn ic_curve_128.bin

# Benchmark both models
./battery_infer benchmark
```

### Demo Data Generation

```bash
# Generate LiFePO4 18650 demo data (900 frames, ~936 KB)
cd dual_model/hmi
python3 scripts/generate_demo_data.py

# Custom frame count and output path
python3 scripts/generate_demo_data.py --frames 2000 --output /tmp/my_data.bin

# Test with generated data
./battery_hmi --file /tmp/my_data.bin
```

---

## 构建与部署

### 前置条件

- **宿主机构建：** 支持C11的GCC；Qt5开发库（用于HMI）
- **交叉编译：** MYD-YG2LX的Yocto Project SDK（参见[SDK章节](#cross-compilation-toolchain)）
- **验证：** Python 3及ONNX Runtime和NumPy
- **Docker（可选）：** 预配置的`yg2lx-dev`容器，包含完整SDK

### 推理引擎（命令行接口）

```bash
cd dual_model

# 本地宿主机构建（用于测试和验证）
make native

# 为MYD-YG2LX交叉编译（aarch64）
source /opt/yg2lx/environment-setup-aarch64-poky-linux
make cross

# 已剥离符号的生产二进制文件
make cross-fast

# 与ONNX Runtime验证数值精度
make verify

# 预处理管道等价性测试
make verify-preprocess

# 精度基准测试
make benchmark-accuracy
```

### HMI应用程序

```bash
cd dual_model

# 交叉编译HMI（需要含Qt5的完整SDK）
source /opt/yg2lx-full/environment-setup-aarch64-poky-linux
make hmi-cross

# 或使用部署脚本（自动处理Docker + SDK）
./hmi/deploy/deploy.sh

# 通过SCP部署至目标板卡
./hmi/deploy/deploy.sh --scp <board-ip>

# 通过USB闪存部署
./hmi/deploy/deploy.sh --usb
```

### 独立CNN库（原型）

```bash
cd cnn_inference

# 本地宿主机构建 + 测试
make

# 为aarch64交叉编译
make cross

# 构建静态库
make lib
make lib-cross

# 与ONNX Runtime验证
make verify
```

### 推理CLI用法

```bash
# PINN：132维特征 -> SOH
./battery_infer pinn features_132d.bin

# CNN：128点IC曲线 -> 阶段 + RUL
./battery_infer cnn ic_curve_128.bin

# 双模型基准测试
./battery_infer benchmark
```

### 演示数据生成

```bash
# 生成LiFePO4 18650演示数据（900帧，约936 KB）
cd dual_model/hmi
python3 scripts/generate_demo_data.py

# 自定义帧数和输出路径
python3 scripts/generate_demo_data.py --frames 2000 --output /tmp/my_data.bin

# 用生成的数据测试
./battery_hmi --file /tmp/my_data.bin
```

---

## Cross-Compilation Toolchain

Two Yocto Project SDK installers are provided in the `sdk/` directory:

| Installer | Contents |
|-----------|----------|
| `poky-glibc-x86_64-myir-image-core-...sh` | Core toolchain (GCC, binutils, sysroot) |
| `poky-glibc-x86_64-myir-image-full-...sh` | Full SDK (includes Qt5 libraries, CMake toolchain, Wayland development headers) |

**Installation:**

```bash
chmod +x sdk/poky-glibc-x86_64-myir-image-full-aarch64-myir-yg2lx-toolchain-3.1.20.sh
./sdk/poky-glibc-x86_64-myir-image-full-aarch64-myir-yg2lx-toolchain-3.1.20.sh
```

The full SDK is required for HMI cross-compilation (Qt5 + Wayland linkage); the core SDK suffices for the inference engine alone. The full SDK provides the `OEToolchainConfig.cmake` file required by the HMI CMake cross-compilation workflow.

## 交叉编译工具链

`sdk/`目录中提供了两个Yocto Project SDK安装程序：

| 安装程序 | 内容 |
|-----------|----------|
| `poky-glibc-x86_64-myir-image-core-...sh` | 核心工具链（GCC、binutils、sysroot） |
| `poky-glibc-x86_64-myir-image-full-...sh` | 完整SDK（包含Qt5库、CMake工具链、Wayland开发头文件） |

**安装方法：**

```bash
chmod +x sdk/poky-glibc-x86_64-myir-image-full-aarch64-myir-yg2lx-toolchain-3.1.20.sh
./sdk/poky-glibc-x86_64-myir-image-full-aarch64-myir-yg2lx-toolchain-3.1.20.sh
```

完整SDK为HMI交叉编译所必需（需要Qt5 + Wayland链接）；仅推理引擎使用核心SDK即可满足需求。完整SDK提供HMI CMake交叉编译工作流所需的`OEToolchainConfig.cmake`文件。

---

## Validation and Benchmarking

All inference implementations are validated against ONNX Runtime reference outputs to ensure numerical equivalence. The verification pipeline operates as follows:

1. **Weight export:** Python scripts (`export_weights.py`) extract and dequantize PyTorch/ONNX model weights to float32 C header files
2. **Forward pass comparison:** Test harnesses feed identical inputs to both the pure C implementation and ONNX Runtime, comparing outputs with a tolerance of 1e-4 for float32 precision
3. **Preprocessing verification:** `verify_preprocess.cpp` validates that the C preprocessing pipeline (IC normalization, gradient computation, scaler application) produces outputs numerically equivalent to the Python reference implementation (`np.gradient` with `edge_order=1`)
4. **Layer-by-layer debugging:** `debug_pinn.c` provides intermediate tensor statistics (mean, std) at each layer for PINN, enabling precise localization of numerical discrepancies
5. **Benchmarking:** `benchmark_accuracy.py` evaluates inference accuracy across a range of synthetic and recorded data points
6. **ONNX analysis:** `analyze_onnx.py` inspects the ONNX model graph to identify operator types, tensor shapes, and weight quantization parameters

### Verification Commands

```bash
# Dual-model engine verification
cd dual_model && make verify

# Preprocessing pipeline verification
cd dual_model && make verify-preprocess

# Accuracy benchmark
cd dual_model && make benchmark-accuracy

# Standalone CNN verification
cd cnn_inference && make verify

# PINN layer-by-layer debug (requires /tmp/pinn_test_input.bin)
cd dual_model && gcc -O0 -Iinclude -o build/debug_pinn src/debug_pinn.c -lm && ./build/debug_pinn
```

## 验证与基准测试

所有推理实现均经过ONNX Runtime参考输出的验证，以确保数值等价性。验证管道按以下步骤进行：

1. **权重导出：** Python脚本（`export_weights.py`）从PyTorch/ONNX模型中提取并解量化权重，导出为float32 C头文件
2. **前向传播对比：** 测试框架将相同输入分别送入纯C实现和ONNX Runtime，以1e-4的容差（float32精度）比较输出结果
3. **预处理验证：** `verify_preprocess.cpp`验证C预处理管道（IC归一化、梯度计算、缩放器应用）产生的输出在数值上与Python参考实现（`np.gradient`，`edge_order=1`）等价
4. **逐层调试：** `debug_pinn.c`提供PINN每层的中间张量统计（均值、标准差），便于精确定位数值差异
5. **基准测试：** `benchmark_accuracy.py`在一系列合成数据和记录数据点上评估推理精度
6. **ONNX分析：** `analyze_onnx.py`检查ONNX模型图以识别算子类型、张量形状和权重量化参数

### 验证命令

```bash
# 双模型引擎验证
cd dual_model && make verify

# 预处理管道验证
cd dual_model && make verify-preprocess

# 精度基准测试
cd dual_model && make benchmark-accuracy

# 独立CNN验证
cd cnn_inference && make verify

# PINN逐层调试（需要/tmp/pinn_test_input.bin）
cd dual_model && gcc -O0 -Iinclude -o build/debug_pinn src/debug_pinn.c -lm && ./build/debug_pinn
```

---

## Target Platform

| Parameter | Specification |
|-----------|---------------|
| Development board | MYiR MYD-YG2LX |
| System-on-Chip | Renesas RZ/G2L (R9A07G044L) |
| CPU | ARM Cortex-A55 (single-core), ARMv8.2-A |
| GPU | ARM Mali-G31 (OpenGL ES 3.2) |
| Memory | DDR4 (1 GB / 2 GB configuration) |
| Display | 7-inch LCD (1024x600) with capacitive touch, HDMI output (1920x1080) |
| Storage | eMMC (4 GB / 8 GB), microSD card slot, Octa-NAND SPI flash |
| Connectivity | Gigabit Ethernet, Wi-Fi, Bluetooth, CAN, RS485, SPI |
| Operating system | Linux (CIP kernel 5.10, Yocto Project Poky distribution) |
| Display server | Wayland (Weston 9.0 compositor) |
| QPA backend | wayland-egl (primary); eglfs, linuxfb (fallback) |

## 目标平台

| 参数 | 规格 |
|-----------|---------------|
| 开发板 | MYiR MYD-YG2LX |
| 片上系统 | Renesas RZ/G2L (R9A07G044L) |
| CPU | ARM Cortex-A55（单核），ARMv8.2-A |
| GPU | ARM Mali-G31（OpenGL ES 3.2） |
| 内存 | DDR4（1 GB / 2 GB配置） |
| 显示 | 7英寸LCD（1024x600）电容触摸屏，HDMI输出（1920x1080） |
| 存储 | eMMC（4 GB / 8 GB），microSD卡槽，Octa-NAND SPI闪存 |
| 连接性 | 千兆以太网、Wi-Fi、蓝牙、CAN、RS485、SPI |
| 操作系统 | Linux（CIP内核5.10，Yocto Project Poky发行版） |
| 显示服务器 | Wayland（Weston 9.0合成器） |
| QPA后端 | wayland-egl（主要）；eglfs、linuxfb（回退） |

---

## License

This project incorporates multiple open-source components distributed under their respective licenses:

- **Inference engine and HMI:** Proprietary; all rights reserved
- **ARM Trusted Firmware-A:** BSD 3-Clause License (see `sources/myir-renesas-tf-a/license.rst`)
- **U-Boot:** GNU General Public License v2.0 (see `sources/myir-renesas-uboot/Licenses/`)
- **Linux kernel:** GNU General Public License v2.0 (see `sources/myir-renesas-linux/COPYING`)
- **Flash writer:** MIT License (see `sources/myir-renesas-flash-writer/LICENSE.md`)

Please refer to the respective subdirectories for complete license texts and copyright notices.

## 许可证

本项目包含了多个依据各自许可证分发的开源组件：

- **推理引擎与HMI：** 专有软件；保留所有权利
- **ARM可信固件-A：** BSD 3-Clause许可证（参见`sources/myir-renesas-tf-a/license.rst`）
- **U-Boot：** GNU通用公共许可证v2.0（参见`sources/myir-renesas-uboot/Licenses/`）
- **Linux内核：** GNU通用公共许可证v2.0（参见`sources/myir-renesas-linux/COPYING`）
- **Flash Writer：** MIT许可证（参见`sources/myir-renesas-flash-writer/LICENSE.md`）

有关完整的许可证文本和版权声明，请参阅各自的子目录。
