# ⚡ ESP32 Lab · 创新固件与硬件黑客实验矩阵

[![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v5.5.1-red.svg)](https://github.com/espressif/esp-idf)
[![MicroPython](https://img.shields.io/badge/MicroPython-v1.27.0-blue.svg)](https://github.com/micropython/micropython)
[![Platform](https://img.shields.io/badge/Chip-ESP32%20Classic%20%7C%20S3-orange.svg)](https://www.espressif.com/)
[![CI](https://img.shields.io/badge/CI-GitHub%20Actions-brightgreen.svg)](.github/workflows/firmware-ci.yml)
[![License](https://img.shields.io/badge/License-MIT%20%2F%20Apache%202.0-lightgrey.svg)](LICENSE)

> 面向 ESP32 的创新固件与硬件实验矩阵，聚焦无感感知、具身智能与物理层通信黑客。

---

## 📦 项目矩阵 / Projects

| 项目 | 分类 | 核心功能 | 硬件要求 | 路径 |
|:---|:---|:---|:---|:---|
| [**esp-csi-radar**](./esp-csi-radar) | **Sensing** | WiFi CSI 隔墙人体感知、呼吸与微动检测 | 基础 ESP32（无需外接传感器） | `esp-csi-radar/` |
| [**openpal**](./openpal) | **Embodied AI** | 本地优先 AI 语音搭子，双工 I2S 音频与连续情绪引擎 | ESP32 + INMP441 + MAX98357A | `openpal/` |
| [**bytepet**](./bytepet) | **Embodied AI** | 桌面电子宠物，生理节律状态机与像素动画互动 | ESP32 + SSD1306 OLED / Web | `bytepet/` |
| [**glowlink**](./glowlink) | **PhyHacks** | 普通 LED 兼作收发器的可见光对讲机 | 2× ESP32 + 普通高亮 LED 模块 | `glowlink/` |
| [**airchat**](./airchat) | **PhyHacks** | ESP-NOW 离网对讲，零网络依赖的广播聊天终端 | 2× ESP32 开发板 | `airchat/` |
| [**bassline**](./bassline) | **PhyHacks** | 512 点 Radix-2 FFT 实时音频频谱与 WS2812 律动灯 | ESP32 + 麦克风 (I2S/ADC) + WS2812B | `bassline/` |
| [**deckpad**](./deckpad) | **PhyHacks** | 低延迟 BLE 蓝牙宏键盘与 Web 自定义键位配置 | ESP32 + 机械轴 / 触摸按键 | `deckpad/` |

> 注：另含 [**firmware-expansion-board**](./firmware-expansion-board)，提供面向传感器扩展板的定制 MicroPython v1.27.0 固件层（C 模块与冻结驱动）。

---

## 🌲 三大技术主线 / Technical Themes

- 📡 **感知现实 (Sensing)**
  - [`esp-csi-radar`](./esp-csi-radar) — 利用 52 个 OFDM 子载波信道状态信息 (CSI) 捕捉多径环境变化，实现隔墙无感存在检测与微动分析。
- 🤖 **AI 具身化 (Embodied AI)**
  - [`openpal`](./openpal) — 轻量级本地优先智能语音助手，板端运行精简 I2S 双工流，搭配 Valence/Arousal 连续情绪引擎。
  - [`bytepet`](./bytepet) — 桌面虚拟电子宠物，集成饥饿/心情/体力有限状态机 (FSM) 与复古像素交互。
- ⚡ **物理黑客与非凡通信 (PhyHacks)**
  - [`glowlink`](./glowlink) — 利用普通 LED 的 PN 结反向光电效应，实现半双工可见光曼彻斯特编码通信与 Web 示波器。
  - [`airchat`](./airchat) — 基于 ESP-NOW 底层广播帧的百米级离网对讲终端，无需路由器与蜂窝网络。
  - [`bassline`](./bassline) — 实时 FFT 音频频谱计算，对数听觉频段划分，RMT 驱动 WS2812 流光律动。
  - [`deckpad`](./deckpad) — 基于 NimBLE HID 协议的低延迟宏键盘控制器，支持板载 Web 实时热键配置。

---

## 🚀 编译与构建 / Build Guide

### 一键构建脚本

仓库根目录提供全自动构建脚本 `build_all.sh`：

```bash
# 一键编译所有固件
./build_all.sh

# 单项目编译 (如 esp-csi-radar)
./build_all.sh <project_name>

# 编译指定多个项目
./build_all.sh openpal glowlink

# 清理并全量重新编译
./build_all.sh --clean
```

### 单项目原生编译 (ESP-IDF)

```bash
cd <project_name>/firmware
idf.py set-target esp32
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

---

## 📂 仓库目录结构 / Repository Structure

```
ESP32-内部/
├── build_all.sh                   # 全固件一键编译脚本
├── esp-csi-radar/                 # [Sensing] WiFi CSI 隔墙雷达
├── openpal/                       # [Embodied AI] 本地优先 AI 语音搭子
├── bytepet/                       # [Embodied AI] 桌面电子宠物
├── glowlink/                      # [PhyHacks] LED 可见光对讲机
├── airchat/                       # [PhyHacks] ESP-NOW 离网对讲终端
├── bassline/                      # [PhyHacks] 音频频谱 WS2812 律动灯
├── deckpad/                       # [PhyHacks] BLE 蓝牙宏键盘
├── firmware-expansion-board/      # [Platform] MicroPython 扩展板定制固件
└── binaries/                      # 预编译基础固件镜像
```

---

## 📄 许可证 / License

本项目核心代码遵循 [MIT](openpal/LICENSE) 与 [Apache-2.0](esp-csi-radar/LICENSE) 开源许可证。
