# RA8 — RZ/G2L SPI 接线与数据连接完整指南

> **目标平台:** MYiR MYD-YG2LX (Renesas RZ/G2L, Cortex-A55)  
> **数据采集 MCU:** Renesas RA8 系列 (Cortex-M85)  
> **通信接口:** SPI 全双工 (RZ/G2L 做 Master, RA8 做 Slave)  
> **对应代码:** `dual_model/hmi/data/SpiDataProvider.h/.cpp`

---

## 目录

1. [系统架构概览](#1-系统架构概览)
2. [硬件接线](#2-硬件接线)
3. [SPI 总线电气规范](#3-spi-总线电气规范)
4. [MYD-YG2LX 端配置](#4-myd-yg2lx-端配置)
5. [RA8 端配置与固件](#5-ra8-端配置与固件)
6. [SPI 帧协议详解](#6-spi-帧协议详解)
7. [数据流与控制流](#7-数据流与控制流)
8. [板端验证步骤](#8-板端验证步骤)
9. [故障排查](#9-故障排查)
10. [参考资料](#10-参考资料)

---

## 1. 系统架构概览

```
┌─────────────────────────────────────────────────────────────────────┐
│                        MYD-YG2LX 开发板                              │
│                                                                      │
│  ┌──────────────────────────────────────────────┐                    │
│  │          RZ/G2L (Cortex-A55 @ 1.2GHz)        │                    │
│  │                                              │                    │
│  │  ┌──────────────────┐  ┌──────────────────┐  │                    │
│  │  │  battery_hmi     │  │  SPI Driver      │  │                    │
│  │  │  (Qt5 HMI)       │  │  (spidev)        │  │                    │
│  │  │                  │  │                  │  │                    │
│  │  │  SpiDataProvider ├──┤  /dev/spidev0.0  │  │                    │
│  │  │  ├─ read()       │  │  RSPI0 Master    │  │                    │
│  │  │  ├─ parseFrame() │  │                  │  │                    │
│  │  │  ├─ CRC32 校验   │  │  MOSI ────────────┼──┼───┐               │
│  │  │  └─ fixFeatures  │  │  MISO ────────────┼──┼─┐ │               │
│  │  └──────────────────┘  │  SCK  ────────────┼──┼─│─│─┐             │
│  │                        │  CS0  ────────────┼──┼─│─│─│─┐           │
│  └────────────────────────┴──────────────────┼──┼─│─│─│─│─┼───┐       │
│                                              │  │ │ │ │ │ │   │       │
│  ┌──────────────────────────────────────────┐│  │ │ │ │ │ │   │       │
│  │  Mali-G31 GPU  │  DDR4 (1/2GB)           ││  │ │ │ │ │ │   │       │
│  │  eMMC / NAND   │  Ethernet / Wi-Fi       ││  │ │ │ │ │ │   │       │
│  └──────────────────────────────────────────┘│  │ │ │ │ │ │   │       │
└──────────────────────────────────────────────┼──┼─┼─┼─┼─┼─┼───┘       │
                                               │  │ │ │ │ │ │           │
               ┌───────────────────────────────┘  │ │ │ │ │ │           │
               │  ┌───────────────────────────────┘ │ │ │ │ │           │
               │  │  ┌──────────────────────────────┘ │ │ │ │           │
               │  │  │  ┌─────────────────────────────┘ │ │ │           │
               │  │  │  │  ┌────────────────────────────┘ │ │           │
               │  │  │  │  │  ┌───────────────────────────┘ │           │
               ▼  ▼  ▼  ▼  ▼  ▼                              ▼           │
         ┌──────────────────────────────────────────────────────────┐    │
         │                   RA8 微控制器                            │    │
         │                                                           │    │
         │  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐  │    │
         │  │ ADC 采集  │  │ AFE 前端  │  │ SPI Slave│  │ CRC32    │  │    │
         │  │ 电压/电流 │  │ 电池管理  │  │ 响应引擎  │  │ 帧封装    │  │    │
         │  │ 温度/膨胀 │  │ 芯片      │  │          │  │          │  │    │
         │  └──────────┘  └──────────┘  └──────────┘  └──────────┘  │    │
         │                                                           │    │
         │  输入: 电池电压、电流、温度、膨胀传感器                      │    │
         │  输出: 1080 字节 SPI 帧 (10 Hz)                            │    │
         └──────────────────────────────────────────────────────────┘    │
                                                                         │
                             18650 / 21700 锂电池                        │
                         LiFePO4 / NMC / LCO 等化学体系                  │
```

### 数据流概述

```
电池传感器 → AFE (模拟前端) → RA8 ADC → RA8 特征工程 
    → SPI MISO → RZ/G2L spidev → SpiDataProvider::read()
    → 推理引擎 PINN/CNN → HMI 显示 (SOH, RUL, 阶段, 图表)
```

---

## 2. 硬件接线

### 2.1 所需信号线

SPI 总线至少需要 **4 根信号线**，建议 **6 根**（含 GPIO 中断 + 共地）：

| 信号 | 方向 | 必需 | 说明 |
|------|------|:----:|------|
| SCK | Master → Slave | ✅ | 串行时钟，由 RZ/G2L 产生 |
| MOSI (COPI) | Master → Slave | ✅ | 主出从入，RZ/G2L 发送全零字节 |
| MISO (CIPO) | Slave → Master | ✅ | 主入从出，RA8 返回 SPI 帧数据 |
| CS (SS) | Master → Slave | ✅ | 片选/从机选择，低有效 |
| GND | 双向 | ✅ | 共地参考 |
| RDY/IRQ | Slave → Master | 可选 | RA8 数据就绪中断通知 |
| RESET | Master → Slave | 可选 | RZ/G2L 复位 RA8 |

### 2.2 RZ/G2L SPI 引脚映射

RZ/G2L 提供 3 路 SPI 控制器（RSPI0 / RSPI1 / RSPI2）。下表列出每个控制器的引脚分配：

#### RSPI0 (推荐 — 对应 `/dev/spidev0.0`)

| 功能 | RZ/G2L 引脚 | MYD-YG2LX 扩展接口 | 备注 |
|------|------------|-------------------|------|
| RSPI0_CK (SCK) | P17_4 | J19 Pin 12 | 时钟 |
| RSPI0_MOSI | P17_0 | J19 Pin 8 | 主出从入 |
| RSPI0_MISO | P17_1 | J19 Pin 10 | 主入从出 |
| RSPI0_SSL0 (CS0) | P17_3 | J19 Pin 14 | 片选 0 |
| RSPI0_SSL1 (CS1) | P17_7 | — | 片选 1 (第二从机) |
| RSPI0_SSL2 (CS2) | P47_0 | — | 片选 2 (第三从机) |
| GND | — | J19 Pin 6 | 共地 |

#### RSPI1 (对应 `/dev/spidev1.0`)

| 功能 | RZ/G2L 引脚 | 备注 |
|------|------------|------|
| RSPI1_CK | P27_3 | 时钟 |
| RSPI1_MOSI | P26_5 | 主出从入 |
| RSPI1_MISO | P26_4 | 主入从出 |
| RSPI1_SSL0 | P27_4 | 片选 |

#### RSPI2 (对应 `/dev/spidev2.0`)

| 功能 | RZ/G2L 引脚 | 备注 |
|------|------------|------|
| RSPI2_CK | P16_0 | 时钟 |
| RSPI2_MOSI | P16_1 | 主出从入 |
| RSPI2_MISO | P16_2 | 主入从出 |
| RSPI2_SSL0 | P16_3 | 片选 |

> **⚠ 重要:** 以上 MYD-YG2LX 扩展接口引脚号可能因底板版本不同而异。请以实际底板的原理图为准。J19 是 MYD-YG2LX 底板上 40-pin Raspberry Pi 兼容扩展接口。

### 2.3 RA8 SPI 引脚映射 (通用)

RA8 系列 MCU 的 SPI 引脚因具体型号和引脚封装不同而有所差异。以下为常见配置：

| 功能 | RA8D1 (100-pin) | RA8M1 (100-pin) | RA8T1 (64-pin) |
|------|:---:|:---:|:---:|
| SCK | P109 / P302 | P109 | P205 |
| MOSI | P110 / P303 | P110 | P206 |
| MISO | P111 / P304 | P111 | P207 |
| SS | P112 / P305 | P112 | P208 |
| VCC (3.3V) | VCC | VCC | VCC |
| GND | VSS | VSS | VSS |

> **⚠ 注意:** 以上引脚仅为示例。实际引脚分配请参照你使用的 RA8 具体型号的数据手册和硬件原理图。

### 2.4 完整接线表 (RSPI0 ↔ RA8)

```
RZ/G2L (MYD-YG2LX)               RA8 MCU
═══════════════════               ══════════

RSPI0_CK  (SCK)   ─────────────── SCK
RSPI0_MOSI (COPI) ─────────────── MOSI  (RA8 的 MOSI，即 SDI)
RSPI0_MISO (CIPO) ─────────────── MISO  (RA8 的 MISO，即 SDO)
RSPI0_SSL0 (CS)   ─────────────── SS    (或 CS, NSS)
GND               ─────────────── GND

可选:
GPIO (任意)       ─────────────── RDY/IRQ  (RA8 通知 RZ/G2L 新数据就绪)
GPIO (任意)       ─────────────── RESET   (RZ/G2L 复位 RA8)

3.3V (可选)       ─────────────── VCC     (如果 RA8 由开发板供电)
```

### 2.5 接线实物示意图

```
       MYD-YG2LX 扩展接口 (J19)                         RA8 开发板
  ┌────────────────────────────┐                 ┌──────────────────┐
  │ 3.3V ── Pin 1    Pin 2 ── 5V                 │                  │
  │ SDA  ── Pin 3    Pin 4 ── 5V                 │  VCC  ── 3.3V    │
  │ SCL  ── Pin 5    Pin 6 ── GND  ══════════════│═ GND             │
  │       ── Pin 7    Pin 8 ── TX  (MOSI) ───────│→ MOSI/SDI        │
  │ GND  ── Pin 9    Pin10 ── RX  (MISO) ────────│─ MISO/SDO        │
  │       ── Pin11   Pin12 ── RSPI0_CK  (SCK) ───│→ SCK             │
  │       ── Pin13   Pin14 ── GND                │                  │
  │       ── Pin15   Pin16 ── RSPI0_SSL0 (CS) ───│→ SS/NSS          │
  │  ...                                     ... │                  │
  └────────────────────────────┘                 └──────────────────┘

  杜邦线颜色建议:
    红色   — 3.3V / VCC
    黑色   — GND
    黄色   — SCK
    蓝色   — MOSI (COPI)
    绿色   — MISO (CIPO)
    橙色   — CS (SS)
    白色   — RDY/IRQ (可选)
```

---

## 3. SPI 总线电气规范

### 3.1 电平标准

| 参数 | RZ/G2L | RA8 | 兼容性 |
|------|--------|-----|:------:|
| I/O 电压 | 3.3V | 3.3V | ✅ 一致 |
| VIH (min) | 2.0V | 2.0V | ✅ |
| VIL (max) | 0.8V | 0.8V | ✅ |
| VOH (min) | 2.4V | 2.4V | ✅ |
| 耐受 5V | ❌ | ❌ | 不可接 5V |

> **电平匹配:** RZ/G2L 和 RA8 均为 3.3V CMOS 电平，无需电平转换。如果是其他 MCU (如 5V Arduino)，必须加电平转换芯片 (如 TXB0104)。

### 3.2 时序参数

| 参数 | 推荐值 | 最大值 | 说明 |
|------|--------|--------|------|
| SCK 频率 | 5 MHz | 20 MHz | 代码默认 5 MHz (`SpiDataProvider.h:55`) |
| SPI 模式 | Mode 0 | — | CPOL=0, CPHA=0 |
| 位宽 | 8 bits | — | 标准 SPI |
| CS 建立时间 | ≥ 100 ns | — | 片选拉低到首个 SCK 边沿 |
| CS 保持时间 | ≥ 100 ns | — | 末个 SCK 边沿到片选拉高 |
| 帧间隔 | 100 ms | — | 对应 10 Hz 采样率 |

### 3.3 SPI Mode 0 时序图

```
CS   ─┐                                 ┌────────────────
      └─────────────────────────────────┘
         ┌──┐   ┌──┐   ┌──┐   ┌──┐   ┌──
SCK   ───┘  └───┘  └───┘  └───┘  └───┘
      (idle LOW)
         ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐
MOSI  ───┤ D7  ├─┤ D6  ├─┤ D5  ├─┤ ... ├────
         └─────┘ └─────┘ └─────┘ └─────┘
         ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐
MISO  ───┤ D7  ├─┤ D6  ├─┤ D5  ├─┤ ... ├────
         └─────┘ └─────┘ └─────┘ └─────┘
         ↑                             ↑
    采样边沿 (SCK 上升沿)         数据移位 (SCK 下降沿)
```

### 3.4 物理布线建议

| 项目 | 建议 |
|------|------|
| 线材 | 杜邦线 (母-母)，长度 ≤ 20cm |
| SCK 频率 ≤ 10 MHz | 普通杜邦线可接受 |
| SCK 频率 10-20 MHz | 使用短排线 (≤ 10cm) + 每根信号线串接 22Ω-33Ω 终端电阻 |
| 共地 | 至少 2 根 GND 线 (减少地弹噪声) |
| 屏蔽 | 高速 (>15 MHz) 建议屏蔽线或 FPC 排线 |
| 上拉 | CS 线在 RZ/G2L 端加 10kΩ 上拉到 3.3V (防止启动时误选通) |
| 串联电阻 | 每根数据线串接 22Ω (抑制反射) |

---

## 4. MYD-YG2LX 端配置

### 4.1 检查内核 SPI 驱动

```bash
# 在开发板上执行 — 检查 SPI 驱动是否已加载
ls /dev/spidev*
# 期望输出: /dev/spidev0.0  /dev/spidev0.1  (或其他 RSPI 通道)

# 检查内核模块
lsmod | grep spi
# 期望输出: spidev, spi_rzg2l (或类似)

# 检查设备树中的 SPI 节点
ls /proc/device-tree/soc/spi*/
```

### 4.2 设备树配置 (参考)

如果 `/dev/spidev0.0` 不存在，需要在设备树中使能 RSPI 节点：

```dts
/* 设备树片段 — 使能 RSPI0 */
&spi0 {
    pinctrl-0 = <&spi0_pins>;
    pinctrl-names = "default";
    status = "okay";
    
    spidev@0 {
        compatible = "rohm,dh2228fv";  /* 通用 spidev 兼容字符串 */
        reg = <0>;                     /* CS0 */
        spi-max-frequency = <20000000>; /* 20 MHz */
        status = "okay";
    };
};

/* 引脚复用 */
&pinctrl {
    spi0_pins: spi0 {
        pinmux = <
            RZG2L_PORT_PINMUX(17, 4, 2)>  /* RSPI0_CK   — P17_4 */
            RZG2L_PORT_PINMUX(17, 0, 2)>  /* RSPI0_MOSI — P17_0 */
            RZG2L_PORT_PINMUX(17, 1, 2)>  /* RSPI0_MISO — P17_1 */
            RZG2L_PORT_PINMUX(17, 3, 2)>  /* RSPI0_SSL0 — P17_3 */
        >;
    };
};
```

### 4.3 用户空间 SPI 访问权限

```bash
# 方法一: 以 root 运行 HMI (推荐用于调试)
battery_hmi --spi /dev/spidev0.0

# 方法二: 将 spidev 加入用户组 (推荐用于生产)
chmod 666 /dev/spidev0.0
# 或
usermod -a -G spi root

# 方法三: udev 规则 (永久生效)
echo 'SUBSYSTEM=="spidev", GROUP="spi", MODE="0660"' > /etc/udev/rules.d/99-spi.rules
udevadm control --reload-rules
udevadm trigger
```

### 4.4 验证 SPI 通信 (板端)

```bash
# 使用 spidev_test 工具 (Linux 内核自带)
# 需交叉编译:
#   aarch64-poky-linux-gcc -o spidev_test \
#     /path/to/linux/tools/spi/spidev_test.c

# 发送/接收测试数据
./spidev_test -D /dev/spidev0.0 -s 5000000 -b 8 -H
# 参数: -D 设备, -s 速度(Hz), -b 位宽, -H 十六进制输出

# 用 xxd 检查原始数据 (如果有示波器，可跳过)
# 短接 MOSI 和 MISO → 应该收到自己发送的数据
```

---

## 5. RA8 端配置与固件

### 5.1 RA8 硬件要求

RA8 固件需要完成以下功能（按优先级排列）：

1. **ADC 数据采集** — 电压、电流、温度、膨胀传感器
2. **IC 曲线计算** — dQ/dV 增量容量分析 (128 点)
3. **特征工程** — 生成 132 维 PINN 特征向量
4. **SPI 帧封装** — 按协议格式打包 1080 字节帧
5. **CRC32 计算** — IEEE 802.3 CRC (覆盖字节 0-1075)
6. **SPI 从机响应** — 全双工模式下响应对应长度数据

### 5.2 SPI 从机配置 (RA8 端伪代码)

```c
/* RA8 端 SPI 从机初始化 */
void ra8_spi_slave_init(void) {
    /* ── RA8 SCI (串行通信接口) SPI 模式 ── */
    
    /* 1. 关闭 SCI 模块 */
    R_SCI0->SCR = 0x00;
    
    /* 2. 配置引脚 */
    /*    SCK  — 输入 (从机模式，时钟由主机提供) */
    /*    MOSI — 输入 (SDI) */
    /*    MISO — 输出 (SDO) */
    /*    SS   — 输入 (从机选择) */
    
    /* 3. 设置 SPI 模式 */
    R_SCI0->SMR = 0x00;   /* SPI 模式, 8-bit, MSB first */
    R_SCI0->SPMR = 0x00;  /* CPOL=0, CPHA=0 (Mode 0) — 与 RZ/G2L 一致 */
    
    /* 4. 波特率 */
    R_SCI0->BRR = 0;      /* 从机模式 — 时钟由主机控制, BRR 忽略 */
    
    /* 5. 使能中断 */
    /*    RXI (接收中断)  — 主机发来数据 */
    /*    TXI (发送中断)  — 可以发送数据 */
    /*    TEI (传输结束)  — 帧结束 */
    
    /* 6. 使能 SCI */
    R_SCI0->SCR = 0x70;   /* RE=1, TE=1, RIE=1, TIE=1 */
}
```

### 5.3 RA8 帧组装与响应流程

```c
/* RA8 固件主循环伪代码 */
#define FRAME_SIZE    1080
#define FRAME_MAGIC   0x52413848  /* "RA8H" */

/* 发送缓冲区 (DMA 或中断驱动) */
static uint8_t g_tx_frame[FRAME_SIZE];
static uint32_t g_frame_seq = 0;
static bool g_has_new_data = false;

/* 由 ADC 完成中断调用 */
void on_adc_complete(const BatteryData *raw) 
{
    SpiFrameV1 *frame = (SpiFrameV1 *)g_tx_frame;
    
    /* 1. 帧头部 */
    frame->magic    = TO_LE32(FRAME_MAGIC);    /* 小端 */
    frame->sequence = TO_LE32(g_frame_seq++);
    frame->status   = TO_LE32(0x00000000);      /* 0=有新数据 */
    
    /* 2. 计算 IC 曲线 (dQ/dV, 128 点) */
    compute_ic_curve(raw->voltage, raw->current, 
                     raw->capacity, frame->ic_curve);
    
    /* 3. 拼装 132 维特征向量 */
    memcpy(frame->features, frame->ic_curve, 128 * sizeof(float));
    frame->features[128] = raw->temperature;
    frame->features[129] = log10f(raw->cycle_count + 1.0f);
    frame->features[130] = raw->start_dv;        /* dV proxy */
    frame->features[131] = raw->capacity_mah / 2000.0f;  /* normalized */
    
    /* 4. 标量遥测 */
    frame->temperature   = TO_LE_FLOAT(raw->temperature);
    frame->voltage       = TO_LE_FLOAT(raw->voltage);
    frame->current       = TO_LE_FLOAT(raw->current);
    frame->cycle_count   = TO_LE32(raw->cycle_count);
    frame->capacity_mah  = TO_LE_FLOAT(raw->capacity_mah);
    frame->cell_swelling = TO_LE_FLOAT(raw->swelling);
    
    /* 5. CRC32 校验 (覆盖字节 0–1075) */
    frame->crc32 = TO_LE32(crc32_ieee8023(g_tx_frame, 1076));
    
    /* 6. 通知 SPI 从机引擎: 数据就绪，准备响应下一个主帧 */
    g_has_new_data = true;
    
    /* 可选: 拉高 RDY 引脚通知 RZ/G2L */
    gpio_set(RDY_PIN, 1);
}

/* 如果没有新数据: 快速响应 0xFF 占位帧 */
void fill_no_data_frame(void)
{
    SpiFrameV1 *frame = (SpiFrameV1 *)g_tx_frame;
    memset(frame, 0, FRAME_SIZE);
    frame->magic  = TO_LE32(FRAME_MAGIC);
    frame->status = TO_LE32(0xFF000000);  /* 0xFF = 无新数据 */
    frame->crc32  = TO_LE32(crc32_ieee8023(g_tx_frame, 1076));
}

/* SPI 发送完成中断 */
void on_spi_tx_complete(void)
{
    g_has_new_data = false;
    gpio_set(RDY_PIN, 0);
    /* 准备下一帧发送缓冲区 */
}
```

### 5.4 RA8 CRC32 实现

```c
/* IEEE 802.3 CRC32 (与 Ethernet / PNG / gzip 相同)
 * 多项式: 0x04C11DB7 (normal), 0xEDB88320 (reversed)
 * 初始值: 0xFFFFFFFF, 最终异或: 0xFFFFFFFF
 */
static const uint32_t crc32_table[256] = {
    0x00000000u, 0x77073096u, 0xEE0E612Cu, 0x990951BAu,
    0x076DC419u, 0x706AF48Fu, 0xE963A535u, 0x9E6495A3u,
    /* ... 完整 256 项见 SpiDataProvider.cpp:164-197 ... */
};

uint32_t crc32_ieee8023(const uint8_t *data, size_t len)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) {
        crc = crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFu;
}
```

> 完整的 CRC32 查找表 (256 项) 见 `dual_model/hmi/data/SpiDataProvider.cpp:164-197`

---

## 6. SPI 帧协议详解

### 6.1 帧结构 (v1)

```
  Byte offset
    0        4        8       12                                       524                                     1052  1056  1060  1064  1068  1072  1076  1080
    ├────────┼────────┼────────┼──────────────────────────────────────┼──────────────────────────────────────┼─────┼─────┼─────┼─────┼─────┼─────┼─────┤
    │ magic  │  seq   │ status │           ic_curve[128]              │          features[132]               │temp │volt │curr │cycle│cap  │swell│crc32│
    │ uint32 │ uint32 │ uint32 │         512 bytes (128×float32)       │        528 bytes (132×float32)       │ f32 │ f32 │ f32 │ u32 │ f32 │ f32 │ u32 │
    ├────────┴────────┴────────┴──────────────────────────────────────┴──────────────────────────────────────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┤
    │                                                             1080 bytes total                                                                   │
    └─────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┘
    │                                                                                   CRC32 coverage (bytes 0–1075)                                  │
    └───────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┘
```

### 6.2 字段详细说明

#### 6.2.1 magic — 帧同步魔数 (offset 0, 4 bytes)

```
值: 0x52413848 (ASCII: 'R' 'A' '8' 'H')
作用: 帧起始标识，快速判断是否为合法 SPI 帧
检查: validateFrame() 首字节不匹配 → 丢弃 (SpiDataProvider.cpp:220-227)
```

#### 6.2.2 sequence — 帧序号 (offset 4, 4 bytes)

```
类型: uint32_t, 小端序
范围: 0 → 0xFFFFFFFF (回绕允许)
作用: 
  - 检测重复帧 (seq <= lastSeq → 丢弃)
  - 检测丢帧 (seq > lastSeq + 1 → 有帧丢失)
  - 首次读到 seq=0 时，不触发重复检测 (lastSeq=0 初始值)
检查: parseFrame() 第 281-285 行
```

#### 6.2.3 status — 数据状态字节 (offset 8, 1 byte)

```
值:
  0x00 = 有新数据 — HMI 正常处理此帧，更新 UI
  0xFF = 无新数据 — HMI 跳过此帧 (read() 返回 false)

说明: status 是 reserved[4] 的第 1 字节。
      当 RA8 ADC 未完成采集，但 RZ/G2L 已发起 SPI 传输时，
      RA8 返回 status=0xFF 的空帧，避免阻塞 SPI 总线。

检查: parseFrame() 第 288-291 行
```

#### 6.2.4 ic_curve[128] — 增量容量曲线 (offset 12, 512 bytes)

```
类型: float[128], 小端序
含义: dQ/dV 曲线，在 128 点电压网格上的增量容量值
单位: 无量纲 (已归一化)
来源: RA8 固件从原始电压/电流/容量数据在线计算

示例值范围 (18650 LiFePO4):
  健康电池:  峰值 ~2.5–3.0
  衰退电池:  峰值 ~1.5–2.0 (峰高下降，峰位右移)
  寿命终止:  峰值 ~0.5–1.0 (峰形扁平化)

用途:
  - CNN 推理: 经 CnnPreprocessor 处理为双通道 [256] (IC + 梯度)
  - PINN 推理: features[0..127] 透传 (由 fixFeatures 保持一致)
```

#### 6.2.5 features[132] — PINN 特征向量 (offset 524, 528 bytes)

```
类型: float[132], 小端序
结构:
  features[0..127]   — IC 曲线 (与 ic_curve 相同)
  features[128]       — 温度 (°C)
  features[129]       — log10(cycle_count + 1)
  features[130]       — dV 代理量 (起始电压降，mV/V)
  features[131]       — 归一化容量 (measured_mah / nominal_mah)

一致性保证: 
  DataProvider::fixFeatures() 在推理前用标量字段
  (temperature, cycle_count, capacity_mah) 覆盖 features[128..131]，
  因此即使 RA8 固件此处有偏差也不影响推理结果。
  (DataProvider.h:98-121)
```

#### 6.2.6 标量遥测字段 (offset 1052–1075)

| 字段 | 偏移 | 类型 | 单位 | 说明 |
|------|:----:|------|------|------|
| temperature | 1052 | float32 | °C | 电池温度 |
| voltage | 1056 | float32 | V | 电池端电压 |
| current | 1060 | float32 | A | 充放电流 (正=充电) |
| cycle_count | 1064 | uint32 | — | 累计循环次数 |
| capacity_mah | 1068 | float32 | mAh | 实测容量 |
| cell_swelling | 1072 | float32 | 0-1 | 电池膨胀比率 |

#### 6.2.7 crc32 — 帧校验 (offset 1076, 4 bytes)

```
算法:  IEEE 802.3 CRC32 (与 Ethernet / PNG / gzip 相同)
多项式: 0x04C11DB7 (normal form), 0xEDB88320 (reversed form)
初始值: 0xFFFFFFFF
最终异或: 0xFFFFFFFF
覆盖范围: offset 0–1075 (整个帧除了 CRC 自身)

错误处理 (SpiDataProvider.cpp:234-256):
  单次 CRC 错误 → 记录日志，丢弃此帧
  连续 10 次 CRC 错误 → 自动复位 SPI 总线 (close + reopen)
  总线复位后仍持续错误 → 切换 mock 模式
```

### 6.3 通信时序 (10 Hz 采样)

```
时间轴 (每 100ms 一个周期)
════════════════════════════════════════════════════════════

t=0ms          t=50ms     t=100ms       t=150ms      t=200ms
  │               │           │              │            │
  ├─ CS ──────────┐           ├─ CS ─────────┐
  │  拉低           │           │  拉低          │
  ├─ SCK (5MHz) ──┤           ├─ SCK (5MHz) ──┤
  │  1080×8=8640   │           │  8640 脉冲     │
  │  个时钟脉冲     │           │               │
  ├─ MOSI ────────┤           ├─ MOSI ────────┤
  │  [1080 x 0x00] │           │  [1080 x 0x00] │
  ├─ MISO ────────┤           ├─ MISO ────────┤
  │  [1080-byte frame]         │  [1080-byte frame]
  ├─ CS ──────────┘           ├─ CS ──────────┘
  │  拉高           │           │  拉高           │
  │               │           │              │
  │◄── 传输 (~1.7ms @5MHz) ─►│              │
  │◄──────── 帧间隔 100ms ────────────────►│
```

传输时间估算:
- 5 MHz SCK: 1080 × 8 / 5,000,000 ≈ 1.73 ms
- 10 MHz SCK: 1080 × 8 / 10,000,000 ≈ 0.86 ms
- 20 MHz SCK: 1080 × 8 / 20,000,000 ≈ 0.43 ms

---

## 7. 数据流与控制流

### 7.1 RZ/G2L 端完整数据流

```
start_hmi.sh 启动
    │
    ▼
battery_hmi --spi /dev/spidev0.0
    │
    ├─ main.cpp:257 — SpiDataProvider::SpiDataProvider(config)
    │       │
    │       ├─ spiOpen()
    │       │   ├─ open("/dev/spidev0.0", O_RDWR)
    │       │   ├─ ioctl(SPI_IOC_WR_MODE, mode=0)
    │       │   ├─ ioctl(SPI_IOC_WR_BITS_PER_WORD, 8)
    │       │   └─ ioctl(SPI_IOC_WR_MAX_SPEED_HZ, 5000000)
    │       │
    │       └─ 若失败 → m_mockMode = true (透明回退)
    │
    ├─ MainWindow::startAcquisition()
    │       │
    │       └─ QTimer::start(DATA_ACQUISITION_MS = 100ms)
    │
    └─ [每 100ms] onDataAcquisition()
            │
            ├─ m_provider->read(sample)
            │       │
            │       ├─ [若 mock] mockGenerate(sample)
            │       │
            │       └─ [若真实 SPI]
            │           ├─ memset(tx, 0, 1080)   // 全零发送
            │           ├─ spiTransfer(tx, rx, 1080)
            │           │   └─ ioctl(SPI_IOC_MESSAGE(1), &tr)
            │           └─ parseFrame(rx, 1080, sample)
            │               ├─ validateFrame() — magic + CRC32
            │               ├─ 检查 seq 重复
            │               ├─ 检查 status != 0xFF
            │               └─ memcpy 各字段 → sample
            │
            ├─ DataProvider::fixFeatures(sample)
            │   └─ features[128..131] ← 标量字段 (权威)
            │
            ├─ checkAlarms(sample)
            │   └─ 温度/电压/电流/SOH 阈值检查 → AlarmPopup
            │
            ├─ appendOscilloscopeData()
            │   └─ 60 秒滚动窗口 ChartWidget 更新
            │
            ├─ [每 500ms] onPinnInference()
            │   ├─ InferenceEngine::predictSOH(features[132])
            │   ├─ SohAccumulator::push(soh)
            │   ├─ 收敛检测
            │   └─ setHealth(displaySoh)
            │
            └─ [每 2000ms] onCnnInference()
                ├─ emit requestCnnInference(ic_curve)
                ├─ [InferenceWorker QThread]
                │   ├─ CnnPreprocessor::process(ic_curve) → [256]
                │   └─ InferenceEngine::predictStageAndRUL() → stage, rul
                └─ onCnnResult() → setHealth(rul, confidence)
```

### 7.2 RA8 端完整固件流程

```
上电复位
    │
    ├─ 时钟初始化 (PLL, 系统时钟)
    ├─ GPIO 初始化
    ├─ ADC 初始化 (12-bit, 多通道扫描)
    ├─ 定时器初始化 (100ms 采集周期)
    ├─ SPI 从机初始化 (CPOL=0, CPHA=0, 8-bit)
    ├─ CRC32 查表初始化
    │
    └─ 主循环
        │
        ├─ [每 100ms 定时器中断]
        │   ├─ ADC 扫描 (电压/电流/温度/膨胀)
        │   ├─ 数字滤波 (EMA / 中值 / 卡尔曼)
        │   ├─ 计算 IC 曲线 (dQ/dV → 128 点)
        │   ├─ 拼装 132 维特征向量
        │   ├─ 填充 SpiFrameV1 → g_tx_frame[1080]
        │   ├─ 计算 CRC32
        │   ├─ g_has_new_data = true
        │   └─ 可选: 触发 RDY 中断
        │
        └─ [SPI 从机中断服务]
            ├─ CS 拉低 → 开始传输
            ├─ [每字节] RXI/TXI 中断
            │   ├─ 收到 MOSI 字节 (忽略 — 全零)
            │   └─ 发送 g_tx_frame 中的下一字节
            ├─ CS 拉高 → 帧结束
            └─ 重置 SPI 从机状态
```

---

## 8. 板端验证步骤

### 8.1 准备工作

```bash
# 1. 确认 SPI 设备存在
ls -la /dev/spidev*
# 期望: crw-rw---- 1 root spi 153, 0 Jan  1 00:00 /dev/spidev0.0

# 2. 确认 HMI 二进制存在
ls -la /opt/battery_hmi/battery_hmi
file /opt/battery_hmi/battery_hmi
# 期望: ELF 64-bit LSB executable, ARM aarch64, ...

# 3. 确认 RA8 已上电并运行
# (检查 RA8 电源指示灯、晶振起振等)
```

### 8.2 分阶段验证

#### 阶段 1 — 无 RA8 硬件，验证 mock 回退

```bash
# 此阶段无需任何硬件连接
battery_hmi --spi /dev/spidev0.0

# 期望输出:
#   [SpiDataProvider] /dev/spidev0.0: No such device — falling back to mock mode
#   [SpiDataProvider] Using mock mode (synthetic RA8-like data)
#   [HMI] SPI 模式: /dev/spidev0.0 (模拟回退)
# 
# UI 应正常显示合成数据 (SOH 缓慢下降, 电压/电流 波形)
```

#### 阶段 2 — 仅接线，无 RA8 固件，验证总线

```bash
# 接好 SPI 4 线 + GND，RA8 上电但不烧录 SPI 固件
# 此时 RA8 SPI 引脚为高阻态，MISO 可能读到全 0xFF 或随机值

battery_hmi --spi /dev/spidev0.0

# 期望输出:
#   [SpiDataProvider] Opened /dev/spidev0.0 @ 5000000 Hz, mode=0
#   [SpiDataProvider] SPI config verified: mode=0 bits=8 speed=5000000 Hz
#   [SpiDataProvider] Bad magic: 0xFFFFFFFF (expected 0x52413848)
#   [SpiDataProvider] CRC mismatch: ... (total CRC errors: 50)
#   ... 连续 10 次错误后 SPI 总线复位 ...
#   [SpiDataProvider] SPI bus reset FAILED — switching to mock mode
```

这说明 SPI 驱动正常，但 RA8 固件未就绪。符合预期。

#### 阶段 3 — RA8 固件就绪，完整验证

```bash
# RA8 固件已烧录并运行，产生有效 SPI 帧

battery_hmi --spi /dev/spidev0.0

# 期望输出:
#   [SpiDataProvider] Opened /dev/spidev0.0 @ 5000000 Hz, mode=0
#   [SpiDataProvider] SPI config verified: mode=0 bits=8 speed=5000000 Hz
#   [HMI] SPI 模式: /dev/spidev0.0 (已连接)
#
# 退出时:
#   [SpiDataProvider] Shutdown: 1200 samples, 0 errors (0 CRC)

# 期望 UI 行为:
#   - 示波器图表实时更新 (电压/电流/温度 曲线)
#   - SOH 缓慢收敛 (PINN 滑动窗口)
#   - Stage 分类稳定 (CNN)
#   - 循环计数递增
#   - 存储使用率增长
```

#### 阶段 4 — SPI Mock 模式快速验证协议

```bash
# 无需硬件, 验证完整的 HMI 流程与 SPI 协议行为
battery_hmi --spi-mock

# 与 --spi /dev/spidev0.0 的区别:
#   --spi-mock   → 强制 mock, 不尝试打开 spidev, 无硬件依赖
#   --spi /dev/  → 先尝试硬件, 失败后回退 mock

# 用 mock 模式验证:
#   - 报警系统 (温度/电压/电流/SOH)
#   - SOH 收敛检测
#   - Stage 分类结果
#   - 循环进度
#   - 中文/英文切换
```

### 8.3 使用逻辑分析仪验证

```bash
# 如果你的逻辑分析仪支持 SPI 协议解码 (如 Saleae, DSLogic, Kingst):
#
# 通道分配:
#   CH0 — CS
#   CH1 — SCK
#   CH2 — MOSI
#   CH3 — MISO
#
# 触发条件: CH0 下降沿 (CS 拉低)
# 解码设置: SPI Mode 0, 8-bit, MSB first
#
# 期望看到:
#   - 每 ~100ms 一次 CS 低脉冲
#   - CS 低期间约 1.7ms (5MHz)
#   - MOSI: 8640 个 0x00 时钟脉冲
#   - MISO: 1080 字节帧数据 (可解码验证 magic=0x52413848)
```

---

## 9. 故障排查

### 9.1 常见问题

| 症状 | 可能原因 | 解决方案 |
|------|---------|---------|
| `/dev/spidev0.0` 不存在 | 设备树未使能 RSPI0 | 检查内核配置: `zcat /proc/config.gz \| grep SPI`; 修改设备树 |
| 打开设备报 Permission denied | 非 root 用户无权限 | `chmod 666 /dev/spidev0.0` 或 udev 规则 |
| 数据一直是 0x00 或 0xFF | MISO 浮空或短路 | 用万用表检查 MISO 对地/对 VCC 是否短路 |
| 数据全是同一字节 | SPI 模式不匹配 | 确认 RA8 也是 Mode 0 (CPOL=0, CPHA=0) |
| 数据偶尔错误 (CRC 偶发失败) | 信号完整性问题 | 降低 SCK 频率到 2 MHz; 缩短线缆; 加串联电阻 |
| 数据持续 CRC 错误 | RA8 端序错误 | 确认 RA8 也是小端序 (little-endian) |
| 连续 10 次 CRC 失败后切 mock | SPI 总线复位后仍失败 | 检查 RA8 是否正常运行; 检查供电 |
| magic 始终不匹配 | RA8 未发送有效帧 | 检查 RA8 固件是否正确填充 magic=0x52413848 |
| sequence 始终为 0 | RA8 帧序号未递增 | 检查 RA8 固件 `g_frame_seq++` 逻辑 |
| status 始终为 0xFF | RA8 未标记新数据 | 检查 RA8 ADC 完成回调是否正确设置 status=0x00 |
| HMI 无更新 | `read()` 持续返回 false | 查看日志: `journalctl -u hmi-startup -f` |

### 9.2 调试命令速查

```bash
# 检查 SPI 设备节点
ls -la /dev/spi*

# 查看内核 SPI 消息
dmesg | grep -i spi

# 查看设备树 SPI 节点
ls /proc/device-tree/soc/spi*/
cat /proc/device-tree/soc/spi*/status    # 应为 "okay"

# 查看 spidev 驱动信息
cat /sys/class/spidev/spidev0.0/dev

# 用 spidev_test 发送测试数据
echo -n -e '\x52\x41\x38\x48' | dd of=/dev/spidev0.0 bs=4 count=1 2>/dev/null | xxd

# 用示波器/逻辑分析仪查看 (如果有):
#   - SCK 是否有方波 (CS 低期间)
#   - MISO 是否有数据 (不是恒高/恒低)
#   - CS 是否每 100ms 拉低一次
```

### 9.3 性能调优

```bash
# 降低 SCK 频率 (如果信号质量差)
battery_hmi --spi /dev/spidev0.0
# 修改 SpiDataProvider::Config::speedHz = 2000000  (从 5MHz → 2MHz)

# 提高采样率 (如果数据更新不够快)
# 修改 AppConfig.h: DATA_ACQUISITION_MS 从 100 → 50 (10Hz → 20Hz)
# 注意: 这要求 RA8 端也能以 20Hz 产生数据

# CNN 推理频率
# 修改 AppConfig.h: CNN_INFERENCE_MS 从 2000 → 1000 (0.5Hz → 1Hz)
# 注意: CNN 推理约 45ms, 1Hz 对 CPU 压力不大
```

---

## 10. 参考资料

### 10.1 项目内文件

| 文件 | 路径 | 说明 |
|------|------|------|
| SPI 协议定义 | `dual_model/hmi/data/SpiDataProvider.h` | 帧格式、字段、状态码 |
| SPI 实现 | `dual_model/hmi/data/SpiDataProvider.cpp` | 总线管理、CRC32、帧解析、mock |
| 数据接口 | `dual_model/hmi/data/DataProvider.h` | BatterySample、fixFeatures |
| 命令行入口 | `dual_model/hmi/main.cpp` | --spi / --spi-mock 处理 |
| 应用配置 | `dual_model/hmi/config/AppConfig.h` | 采样率、阈值、电池参数 |
| 演示数据生成 | `dual_model/hmi/scripts/generate_demo_data.py` | IC 曲线生成参考 |

### 10.2 外部参考

| 资源 | 说明 |
|------|------|
| Renesas RZ/G2L 硬件手册 | RSPI 章节, 引脚复用表 |
| Renesas RA8 数据手册 | SCI SPI 模式, 引脚分配 |
| MYD-YG2LX 原理图 | 扩展接口 J19 引脚定义 |
| Linux spidev 文档 | `Documentation/spi/spidev.rst` |
| Linux spidev_test | `tools/spi/spidev_test.c` |
| IEEE 802.3 CRC32 | 多项式 0x04C11DB7, 初始值 0xFFFFFFFF |

---

> **文档版本:** v1.0  
> **最后更新:** 2026-07-10  
> **对应代码版本:** battery_hmi v3.0, SpiDataProvider v1 (placeholder)
