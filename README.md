# Battery State-of-Health Assessment System for MYD-YG2LX Embedded Platform

## Abstract

This repository contains a complete embedded software suite for real-time lithium iron phosphate (LiFePO4) battery State-of-Health (SOH) assessment, targeting the MYiR MYD-YG2LX development board based on the Renesas RZ/G2L system-on-chip (ARM Cortex-A55, ARMv8.2-A). The system integrates dual neural network inference engines --- a Physics-Informed Neural Network (PINN) for rapid SOH screening and a one-dimensional residual convolutional neural network (1D-ResNet CNN) for precise aging stage classification with Remaining Useful Life (RUL) prediction --- alongside a Qt5-based industrial human-machine interface (HMI). All inference code is implemented in pure C with zero external dependencies and validated against ONNX Runtime reference outputs.

---

## Table of Contents

1. [System Architecture](#system-architecture)
2. [Repository Structure](#repository-structure)
3. [Dual-Model Inference Engine](#dual-model-inference-engine)
4. [Human-Machine Interface](#human-machine-interface)
5. [Build and Deployment](#build-and-deployment)
6. [Cross-Compilation Toolchain](#cross-compilation-toolchain)
7. [Validation and Benchmarking](#validation-and-benchmarking)
8. [Target Platform](#target-platform)
9. [License](#license)

---

## System Architecture

The system follows a multi-tier architecture comprising three principal layers:

| Layer | Component | Description |
|-------|-----------|-------------|
| Inference | `dual_model/` | Dual-model engine (PINN + CNN), pure C, with CLI and library interfaces |
| Presentation | `dual_model/hmi/` | Qt5 graphical application with multi-screen wizard, charting, and internationalization |
| Platform | `sources/` | Bootloader (TF-A, U-Boot), Linux kernel image, device tree blobs, and SPI flash writer |

Data flows from either a physical sensor interface (RA8 microcontroller via SPI) or pre-recorded session files through a preprocessing pipeline, into the dual-model inference engine, and results are rendered on the HMI with real-time charting and alarm indicators.

---

## Repository Structure

```
yg2lx-work/
|
|-- dual_model/                    # Main dual-model inference engine + HMI
|   |-- include/                   # Public headers
|   |   |-- battery_inference.h    #   Unified API (PINN + CNN)
|   |   |-- pinn_weights.h         #   PINN pre-dequantized weights (float32)
|   |   |-- pinn_meta.h            #   PINN architecture metadata
|   |   |-- cnn_weights.h          #   CNN pre-dequantized weights (float32)
|   |   |-- cnn_meta.h             #   CNN architecture metadata
|   |   |-- scalers.h              #   Input normalization scaler parameters
|   |-- src/                       # C inference implementations
|   |   |-- pinn_inference.c       #   PINN forward pass
|   |   |-- cnn_inference.c        #   CNN forward pass
|   |   |-- battery_infer.c        #   Command-line interface + benchmark
|   |-- hmi/                       # Qt5 HMI application
|   |   |-- main.cpp               #   Application entry point
|   |   |-- CMakeLists.txt         #   CMake build (Qt5 + inference engine)
|   |   |-- inference/             #   C++ inference wrappers and preprocessors
|   |   |-- data/                  #   Data providers (demo, file, SPI)
|   |   |-- ui/                    #   UI widgets and screen wizard
|   |   |-- threads/               #   Worker threads for non-blocking inference
|   |   |-- translations/          #   Qt translation files (zh_CN)
|   |   |-- scripts/               #   Build and translation compilation scripts
|   |   |-- resources/             #   QRC resources (QSS theme)
|   |-- tests/                     # Verification and benchmark scripts
|   |   |-- benchmark_accuracy.py  #   Numerical accuracy vs. ONNX Runtime
|   |   |-- verify_preprocess.cpp  #   Preprocessing pipeline equivalence test
|   |-- deploy/                    # Deployment automation
|   |   |-- deploy.sh              #   Cross-compile + deploy via USB or SCP
|   |-- export_weights.py          #   ONNX-to-C header weight exporter
|   |-- verify_accuracy.py         #   Top-level accuracy verification
|   |-- Makefile                   #   Build system (native, cross, verify)
|
|-- cnn_inference/                 # Standalone CNN inference prototype
|   |-- include/                   #   Public headers
|   |   |-- cnn_inference.h        #     CNN inference API
|   |   |-- model_weights.h        #     Dequantized weights
|   |   |-- model_meta.h           #     Model metadata
|   |-- src/                       #   Pure C CNN implementation
|   |   |-- cnn_inference.c        #     Forward pass (Conv1D, GELU, pooling)
|   |-- test/                      #   Test harness and verification
|   |   |-- test_cnn.c             #     Unit tests and inference tests
|   |   |-- verify_accuracy.py     #     ONNX Runtime comparison
|   |   |-- debug_cnn.c            #     Debugging utilities
|   |   |-- debug_layers.py        #     Layer-by-layer debugging script
|   |-- export_weights.py          #   Weight exporter script
|   |-- analyze_onnx.py            #   ONNX model analysis tool
|   |-- Makefile                   #   Build system
|
|-- sdk/                           # Yocto SDK toolchains
|   |-- poky-glibc-x86_64-myir-image-core-...sh    # Core image SDK
|   |-- poky-glibc-x86_64-myir-image-full-...sh    # Full image SDK (with Qt5)
|
|-- sources/                       # Platform-level components
|   |-- myir-renesas-flash-writer/  # SPI flash programmer (submodule)
|   |-- myir-renesas-tf-a/          # ARM Trusted Firmware-A (submodule)
|   |-- myir-renesas-uboot/         # U-Boot bootloader
|   |-- myir-renesas-linux/         # Linux kernel source tree
|   |-- build/                      # Pre-built platform binaries
|       |-- Image                   #   Linux kernel image
|       |-- myb-rzg2l-disp.dtb      #   Device tree (display configuration)
|       |-- myb-rzg2l-hdmi.dtb      #   Device tree (HDMI configuration)
|
|-- .gitignore
|-- .vscode/                       # VSCode workspace settings
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

---

## License

This project incorporates multiple open-source components distributed under their respective licenses:

- **Inference engine and HMI:** Proprietary; all rights reserved
- **ARM Trusted Firmware-A:** BSD 3-Clause License (see `sources/myir-renesas-tf-a/license.rst`)
- **U-Boot:** GNU General Public License v2.0 (see `sources/myir-renesas-uboot/Licenses/`)
- **Linux kernel:** GNU General Public License v2.0 (see `sources/myir-renesas-linux/COPYING`)
- **Flash writer:** MIT License (see `sources/myir-renesas-flash-writer/LICENSE.md`)

Please refer to the respective subdirectories for complete license texts and copyright notices.
