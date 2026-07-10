# Battery State-of-Health Assessment System for MYD-YG2LX Embedded Platform

# 基于MYD-YG2LX嵌入式平台的电池健康状态评估系统

---

## Abstract

This repository contains a complete embedded software suite for real-time lithium iron phosphate (LiFePO4) battery State-of-Health (SOH) assessment, targeting the MYiR MYD-YG2LX development board based on the Renesas RZ/G2L system-on-chip (ARM Cortex-A55, ARMv8.2-A). The system integrates dual neural network inference engines --- a Physics-Informed Neural Network (PINN) for rapid SOH screening and a one-dimensional residual convolutional neural network (1D-ResNet CNN) for precise aging stage classification with Remaining Useful Life (RUL) prediction --- alongside a Qt5-based industrial human-machine interface (HMI). All inference code is implemented in pure C with zero external dependencies and validated against ONNX Runtime reference outputs.

## 摘要

本仓库包含一套完整的嵌入式软件组件，用于磷酸铁锂（LiFePO4）电池的实时健康状态（SOH）评估，目标平台为基于瑞萨RZ/G2L片上系统（ARM Cortex-A55，ARMv8.2-A）的MYiR MYD-YG2LX开发板。系统集成了双神经网络推理引擎——物理信息神经网络（PINN）用于快速SOH筛查，以及一维残差卷积神经网络（1D-ResNet CNN）用于精确的老化阶段分类与剩余使用寿命（RUL）预测——同时包含基于Qt5的工业人机界面（HMI）。所有推理代码以纯C语言实现，无任何外部依赖，并已通过ONNX Runtime参考输出验证。

---

## Table of Contents / 目录

1. [System Architecture / 系统架构](#system-architecture)
2. [Repository Structure / 仓库结构](#repository-structure)
3. [Dual-Model Inference Engine / 双模型推理引擎](#dual-model-inference-engine)
4. [Human-Machine Interface / 人机界面](#human-machine-interface)
5. [Build and Deployment / 构建与部署](#build-and-deployment)
6. [Cross-Compilation Toolchain / 交叉编译工具链](#cross-compilation-toolchain)
7. [Validation and Benchmarking / 验证与基准测试](#validation-and-benchmarking)
8. [Target Platform / 目标平台](#target-platform)
9. [License / 许可证](#license)

---

## System Architecture

The system follows a multi-tier architecture comprising three principal layers:

| Layer | Component | Description |
|-------|-----------|-------------|
| Inference | `dual_model/` | Dual-model engine (PINN + CNN), pure C, with CLI and library interfaces |
| Presentation | `dual_model/hmi/` | Qt5 graphical application with multi-screen wizard, charting, and internationalization |
| Platform | `sources/` | Bootloader (TF-A, U-Boot), Linux kernel image, device tree blobs, and SPI flash writer |

Data flows from either a physical sensor interface (RA8 microcontroller via SPI) or pre-recorded session files through a preprocessing pipeline, into the dual-model inference engine, and results are rendered on the HMI with real-time charting and alarm indicators.

## 系统架构

系统采用多层架构，包含三个主要层次：

| 层级 | 组件 | 描述 |
|-------|-----------|-------------|
| 推理层 | `dual_model/` | 双模型引擎（PINN + CNN），纯C实现，提供CLI和库接口 |
| 表现层 | `dual_model/hmi/` | Qt5图形应用程序，具备多屏向导、图表绘制和国际化功能 |
| 平台层 | `sources/` | 引导加载程序（TF-A、U-Boot）、Linux内核镜像、设备树二进制文件及SPI闪存烧录工具 |

数据流从物理传感器接口（RA8微控制器通过SPI）或预录会话文件经预处理管道进入双模型推理引擎，推理结果在HMI上以实时图表和报警指示器形式呈现。

---

## Repository Structure

## 仓库结构

```
yg2lx-work/
|
|-- dual_model/                    # 主双模型推理引擎 + HMI
|   |-- include/                   #   公共头文件
|   |   |-- battery_inference.h    #     统一API（PINN + CNN）
|   |   |-- pinn_weights.h         #     PINN预解量化权重（float32）
|   |   |-- pinn_meta.h            #     PINN架构元数据
|   |   |-- cnn_weights.h          #     CNN预解量化权重（float32）
|   |   |-- cnn_meta.h             #     CNN架构元数据
|   |   |-- scalers.h              #     输入归一化缩放参数
|   |-- src/                       #   C推理实现
|   |   |-- pinn_inference.c       #     PINN前向传播
|   |   |-- cnn_inference.c        #     CNN前向传播
|   |   |-- battery_infer.c        #     命令行接口 + 性能基准测试
|   |-- hmi/                       #   Qt5 HMI应用程序
|   |   |-- main.cpp               #     应用程序入口点
|   |   |-- CMakeLists.txt         #     CMake构建文件（Qt5 + 推理引擎）
|   |   |-- inference/             #     C++推理封装与预处理器
|   |   |-- data/                  #     数据提供器（演示、文件、SPI）
|   |   |-- ui/                    #     UI控件与屏幕向导
|   |   |-- threads/               #     工作线程（非阻塞推理）
|   |   |-- translations/          #     Qt翻译文件（zh_CN）
|   |   |-- scripts/               #     构建与翻译编译脚本
|   |   |-- resources/             #     QRC资源（QSS主题）
|   |-- tests/                     #   验证与基准测试脚本
|   |   |-- benchmark_accuracy.py  #     与ONNX Runtime的数值精度对比
|   |   |-- verify_preprocess.cpp  #     预处理管道等价性测试
|   |-- deploy/                    #   部署自动化
|   |   |-- deploy.sh              #     交叉编译 + 通过USB或SCP部署
|   |-- export_weights.py          #   ONNX到C头文件的权重导出脚本
|   |-- verify_accuracy.py         #   顶层精度验证
|   |-- Makefile                   #   构建系统（本地、交叉、验证）
|
|-- cnn_inference/                 # 独立CNN推理原型
|   |-- include/                   #   公共头文件
|   |   |-- cnn_inference.h        #     CNN推理API
|   |   |-- model_weights.h        #     解量化权重
|   |   |-- model_meta.h           #     模型元数据
|   |-- src/                       #   纯C CNN实现
|   |   |-- cnn_inference.c        #     前向传播（Conv1D、GELU、池化）
|   |-- test/                      #   测试框架与验证
|   |   |-- test_cnn.c             #     单元测试与推理测试
|   |   |-- verify_accuracy.py     #     ONNX Runtime对比
|   |   |-- debug_cnn.c            #     调试工具
|   |   |-- debug_layers.py        #     逐层调试脚本
|   |-- export_weights.py          #   权重导出脚本
|   |-- analyze_onnx.py            #   ONNX模型分析工具
|   |-- Makefile                   #   构建系统
|
|-- sdk/                           # Yocto SDK工具链
|   |-- poky-glibc-x86_64-myir-image-core-...sh    # 核心镜像SDK
|   |-- poky-glibc-x86_64-myir-image-full-...sh    # 完整镜像SDK（含Qt5）
|
|-- sources/                       # 平台级组件
|   |-- myir-renesas-flash-writer/  # SPI闪存烧录器（子模块）
|   |-- myir-renesas-tf-a/          # ARM可信固件-A（子模块）
|   |-- myir-renesas-uboot/         # U-Boot引导加载程序
|   |-- myir-renesas-linux/         # Linux内核源码树
|   |-- build/                      # 预编译平台二进制文件
|       |-- Image                   #   Linux内核镜像
|       |-- myb-rzg2l-disp.dtb      #   设备树（显示配置）
|       |-- myb-rzg2l-hdmi.dtb      #   设备树（HDMI配置）
|
|-- .gitignore
|-- .vscode/                       # VSCode工作区设置
```

---

## Dual-Model Inference Engine

### Overview

The inference engine implements two complementary neural network models for battery health assessment, both written in pure C (C11 standard) with zero external library dependencies beyond `libm`. All model weights are pre-dequantized to IEEE 754 single-precision floating-point (float32) at build time and embedded directly as C header files via `export_weights.py`.

### PINN --- Physics-Informed Neural Network

The PINN model serves as a fast screening tool for rapid SOH estimation from raw 132-dimensional feature vectors.

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
| [129] | log(cycle count) |
| [130] | dV proxy (start voltage drop) |
| [131] | Measured capacity |

**Output:** SOH scalar in the range [0, 1], representing the fraction of nominal capacity retained.

### CNN --- 1D Residual Convolutional Network

The CNN model provides precise aging stage classification and RUL prediction from dual-channel incremental capacity curves.

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

**Input:** 256-element float32 array comprising:
- Channel 0 (indices [0, 127]): normalized IC curve (dQ/dV)
- Channel 1 (indices [128, 255]): normalized IC gradient

**Output:**
- `stage_logits[3]`: raw logits for three aging stages --- healthy (0), degrading (1), end-of-life (2)
- `rul[1]`: remaining useful life, normalized to [0, 1]

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

PINN模型用作快速筛查工具，从原始132维特征向量中快速估计SOH。

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
| [129] | log（循环次数） |
| [130] | dV代理量（起始电压降） |
| [131] | 实测容量 |

**输出：** SOH标量，范围[0, 1]，表示相对标称容量的保留比例。

### CNN --- 一维残差卷积网络

CNN模型从双通道增量容量曲线提供精确的老化阶段分类和RUL预测。

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

**输入：** 256元素float32数组，包含：
- 通道0（索引[0, 127]）：归一化IC曲线（dQ/dV）
- 通道1（索引[128, 255]）：归一化IC梯度

**输出：**
- `stage_logits[3]`：三个老化阶段的原始logits——健康（0）、衰退（1）、寿命终止（2）
- `rul[1]`：剩余使用寿命，归一化至[0, 1]

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

The HMI is a Qt5 (Qt 5.15+) graphical application designed for the MYD-YG2LX platform with Mali-G31 GPU acceleration via EGLFS, and fallback support for Linux framebuffer.

### Features

- **Multi-screen wizard:** InitScreen, ModelSelectScreen, and ResultScreen provide a guided workflow
- **Real-time charting:** IC curve visualization via Qt Charts
- **Alarm system:** Visual alarm indicators and popup notifications for threshold violations
- **Internationalization:** Chinese (zh_CN) and English (en) language support via Qt Linguist
- **Crash diagnostics:** Signal handlers with backtrace generation for embedded debugging
- **Font fallback:** Automatic CJK font discovery with multiple candidates for Chinese character rendering

### Data Providers

| Provider | Flag | Description |
|----------|------|-------------|
| Demo | `--demo` | Synthetic data generation (default, seed=42) |
| File | `--file <path>` | Replay pre-recorded binary session files |
| SPI | `--spi <device>` | Real-time data acquisition from RA8 via SPI bus |
| SPI Mock | `--spi-mock` | SPI protocol simulation without hardware |

### Usage

```bash
# Demo mode with synthetic data
battery_hmi --demo

# Fullscreen on embedded display
battery_hmi --demo --fullscreen

# Replay recorded session
battery_hmi --file /data/session_001.bin

# Real-time SPI data acquisition
battery_hmi --spi /dev/spidev0.0

# Chinese UI
battery_hmi --demo --lang zh

# Environment variables for QPA backend
export QT_QPA_PLATFORM=eglfs              # GPU-accelerated (Mali-G31)
export QT_QPA_PLATFORM=linuxfb:fb=/dev/fb0 # Framebuffer fallback
```

---

## 人机界面

HMI是一个基于Qt5（Qt 5.15+）的图形应用程序，专为MYD-YG2LX平台设计，通过EGLFS实现Mali-G31 GPU加速，同时支持Linux帧缓冲（framebuffer）回退模式。

### 功能特性

- **多屏向导：** InitScreen、ModelSelectScreen和ResultScreen提供引导式工作流
- **实时图表：** 通过Qt Charts实现IC曲线可视化
- **报警系统：** 阈值超限时的可视化报警指示和弹出通知
- **国际化：** 通过Qt Linguist支持中文（zh_CN）和英文（en）界面语言
- **崩溃诊断：** 信号处理器生成回溯信息，便于嵌入式调试
- **字体回退：** 自动发现CJK字体，提供多个候选字体以支持中文字符渲染

### 数据提供器

| 提供器 | 标志参数 | 描述 |
|----------|------|-------------|
| Demo | `--demo` | 合成数据生成（默认，随机种子=42） |
| File | `--file <path>` | 回放预录二进制会话文件 |
| SPI | `--spi <device>` | 通过SPI总线从RA8实时采集数据 |
| SPI Mock | `--spi-mock` | 无硬件情况下的SPI协议模拟 |

### 用法

```bash
# 合成数据演示模式
battery_hmi --demo

# 嵌入式显示全屏模式
battery_hmi --demo --fullscreen

# 回放已录制的会话
battery_hmi --file /data/session_001.bin

# 实时SPI数据采集
battery_hmi --spi /dev/spidev0.0

# 中文界面
battery_hmi --demo --lang zh

# QPA后端环境变量
export QT_QPA_PLATFORM=eglfs              # GPU加速（Mali-G31）
export QT_QPA_PLATFORM=linuxfb:fb=/dev/fb0 # 帧缓冲回退
```

---

## Build and Deployment

### Prerequisites

- **Host build:** GCC with C11 support; Qt5 development libraries (for HMI)
- **Cross-compilation:** Yocto Project SDK for MYD-YG2LX (see [SDK section](#cross-compilation-toolchain))
- **Verification:** Python 3 with ONNX Runtime and NumPy

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

# Benchmark accuracy
make benchmark-accuracy
```

### HMI Application

```bash
cd dual_model

# Cross-compile HMI (requires SDK environment)
source /opt/yg2lx-full/environment-setup-aarch64-poky-linux
make hmi-cross

# Deploy to target board
./deploy/deploy.sh --scp <board-ip>
./deploy/deploy.sh --usb
```

### Standalone CNN Library

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

---

## 构建与部署

### 前置条件

- **宿主机构建：** 支持C11的GCC；Qt5开发库（用于HMI）
- **交叉编译：** MYD-YG2LX的Yocto Project SDK（参见[SDK章节](#cross-compilation-toolchain)）
- **验证：** Python 3及ONNX Runtime和NumPy

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

# 精度基准测试
make benchmark-accuracy
```

### HMI应用程序

```bash
cd dual_model

# 交叉编译HMI（需要SDK环境）
source /opt/yg2lx-full/environment-setup-aarch64-poky-linux
make hmi-cross

# 部署至目标板卡
./deploy/deploy.sh --scp <board-ip>
./deploy/deploy.sh --usb
```

### 独立CNN库

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

---

## Cross-Compilation Toolchain

Two Yocto Project SDK installers are provided in the `sdk/` directory:

| Installer | Contents |
|-----------|----------|
| `poky-glibc-x86_64-myir-image-core-...sh` | Core toolchain (GCC, binutils, sysroot) |
| `poky-glibc-x86_64-myir-image-full-...sh` | Full SDK (includes Qt5 libraries and CMake toolchain) |

**Installation:**

```bash
chmod +x sdk/poky-glibc-x86_64-myir-image-full-aarch64-myir-yg2lx-toolchain-3.1.20.sh
./sdk/poky-glibc-x86_64-myir-image-full-aarch64-myir-yg2lx-toolchain-3.1.20.sh
```

The full SDK is required for HMI cross-compilation; the core SDK suffices for the inference engine alone.

## 交叉编译工具链

`sdk/`目录中提供了两个Yocto Project SDK安装程序：

| 安装程序 | 内容 |
|-----------|----------|
| `poky-glibc-x86_64-myir-image-core-...sh` | 核心工具链（GCC、binutils、sysroot） |
| `poky-glibc-x86_64-myir-image-full-...sh` | 完整SDK（包含Qt5库和CMake工具链） |

**安装方法：**

```bash
chmod +x sdk/poky-glibc-x86_64-myir-image-full-aarch64-myir-yg2lx-toolchain-3.1.20.sh
./sdk/poky-glibc-x86_64-myir-image-full-aarch64-myir-yg2lx-toolchain-3.1.20.sh
```

完整SDK为HMI交叉编译所必需；仅推理引擎使用核心SDK即可满足需求。

---

## Validation and Benchmarking

All inference implementations are validated against ONNX Runtime reference outputs to ensure numerical equivalence. The verification pipeline operates as follows:

1. **Weight export:** Python scripts (`export_weights.py`) extract and dequantize PyTorch/ONNX model weights to float32 C header files
2. **Forward pass comparison:** Test harnesses feed identical inputs to both the pure C implementation and ONNX Runtime, comparing outputs with a tolerance of 1e-4 for float32 precision
3. **Preprocessing verification:** `verify_preprocess.cpp` validates that the C preprocessing pipeline (IC normalization, gradient computation, scaler application) produces outputs numerically equivalent to the Python reference implementation
4. **Benchmarking:** `benchmark_accuracy.py` evaluates inference accuracy across a range of synthetic and recorded data points

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
```

## 验证与基准测试

所有推理实现均经过ONNX Runtime参考输出的验证，以确保数值等价性。验证管道按以下步骤进行：

1. **权重导出：** Python脚本（`export_weights.py`）从PyTorch/ONNX模型中提取并解量化权重，导出为float32 C头文件
2. **前向传播对比：** 测试框架将相同输入分别送入纯C实现和ONNX Runtime，以1e-4的容差（float32精度）比较输出结果
3. **预处理验证：** `verify_preprocess.cpp`验证C预处理管道（IC归一化、梯度计算、缩放器应用）产生的输出在数值上与Python参考实现等价
4. **基准测试：** `benchmark_accuracy.py`在一系列合成数据和记录数据点上评估推理精度

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
| Display | 7-inch LCD (1024x600) with capacitive touch, HDMI output |
| Storage | eMMC (4 GB / 8 GB), microSD card slot |
| Connectivity | Gigabit Ethernet, Wi-Fi, Bluetooth, CAN, RS485, SPI |
| Operating system | Linux (CIP kernel, Yocto Project Poky distribution) |
| Display server | Wayland (Weston compositor) |

## 目标平台

| 参数 | 规格 |
|-----------|---------------|
| 开发板 | MYiR MYD-YG2LX |
| 片上系统 | Renesas RZ/G2L (R9A07G044L) |
| CPU | ARM Cortex-A55（单核），ARMv8.2-A |
| GPU | ARM Mali-G31（OpenGL ES 3.2） |
| 内存 | DDR4（1 GB / 2 GB配置） |
| 显示 | 7英寸LCD（1024x600）电容触摸屏，HDMI输出 |
| 存储 | eMMC（4 GB / 8 GB），microSD卡槽 |
| 连接性 | 千兆以太网、Wi-Fi、蓝牙、CAN、RS485、SPI |
| 操作系统 | Linux（CIP内核，Yocto Project Poky发行版） |
| 显示服务器 | Wayland（Weston合成器） |

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
