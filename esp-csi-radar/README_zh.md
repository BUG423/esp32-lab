# 📡 esp-csi-radar

**把任意一块 $5 的 ESP32 变成 WiFi 隔墙感知雷达 —— 无摄像头、无穿戴、不联网。**

ESP32 监听房间里的普通 WiFi 报文，读取 **信道状态信息（CSI）** —— 人体移动、
呼吸、甚至轻微挪动都会让无线电波产生可测的扰动。一块板、一根 USB 线、
一个浏览器面板，搞定。

```
 👤 人移动  ──►  WiFi 多径变化  ──►  ESP32 CSI  ──►  网页可视化
```

[English](./README.md) · [架构说明](./docs/architecture.md)

---

## ✨ 特性

| 特性 | 状态 |
|---|---|
| 实时存在检测（有人 / 无人） | ✅ |
| 运动能量 + 自适应基线 | ✅ |
| 每个子载波幅度 / 方差曲线 | ✅ |
| WebSocket 实时暗色 Dashboard | ✅ |
| 呼吸频率粗估 | 🚧 实验中 |
| Home Assistant MQTT discovery | ✅ |
| Matter 占用传感器 | 🔜 计划中 |
| 多节点 mesh 感知 | 🔜 计划中 |

- **隐私优先** —— 数据不出局域网。没有摄像头，没有麦克风。
- **单文件固件** —— 纯 ESP-IDF，不依赖 Arduino / 云 SDK。
- **便宜** —— 裸 ESP32 DevKit 即可，无需额外传感器。
- **视觉冲击** —— 会发光的球体 + 频谱图，非常适合拍视频。

---

## 🧰 硬件

| 部件 | 说明 | 价格 |
|---|---|---|
| ESP32 DevKit（或 S3 / C3） | 任意带 WiFi 的板子 | ¥10–30 |
| USB 线 | 供电 + 烧录 | — |
| （可选）第二块 ESP32 | 做专用「照射源」发射机 | ¥10 |

> 家里的 WiFi 路由器本身就在房间里充满无线电波 —— 这些报文就足够启动。
> 效果最佳场景：房间里**有一定 WiFi 流量**（手机、IoT，或第二块 ESP32 发包）。

---

## 🚀 快速开始

### 1. 安装 ESP-IDF（一次即可）

```bash
sudo apt-get install git wget flex bison gperf python3 python3-pip \
  python3-venv cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0

# ESP-IDF 已随工作区放在 esp/esp-idf（v5.5.1）— 在工作区根目录执行：
cd ../../esp/esp-idf && ./install.sh esp32
. ../../esp/esp-idf/export.sh
```

### 2. 配置 WiFi

```bash
cd firmware
idf.py menuconfig
#  → CSI Radar Configuration
#     WiFi SSID / WiFi Password / hostname
```

### 3. 编译 & 烧录

```bash
idf.py set-target esp32     # 或 esp32s3 / esp32c3
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

**WSL2 用户：** 先把串口 attach 进来（`tools/flash_wsl.sh` 里有命令）。

### 4. 打开面板

```
http://csi-radar.local
http://<串口日志里打印的 IP>
```

1. 让房间**空 15 秒**左右。
2. 点 **Recalibrate baseline**。
3. 走回房间 —— 球变绿。🎉

---

## 🖼️ Dashboard

- **存在感光球** —— 检测到人时变绿发光
- **实时评分曲线** —— presence score + 运动能量
- **CSI 幅度** —— 64 个子载波信号强度
- **CSI 方差** —— 运动特征「热力图」

---

## 🧠 原理

```
原始 CSI (int8 I/Q)
   ↓
每个子载波幅度  |I + jQ|
   ↓
滑动窗口（32 帧）时间方差
   ↓
motion_energy = mean(64 个子载波的方差)
   ↓
自适应基线（空房能量的 EMA）
   ↓
presence = motion_energy > baseline × 阈值
```

详见 [docs/architecture.md](./docs/architecture.md)。

---

## 📁 目录结构

```
esp-csi-radar/
├── firmware/               ESP-IDF 工程
│   ├── main/
│   │   ├── main.c          启动、CSI 回调、发布任务
│   │   ├── csi_processor.c 信号处理 + 存在检测
│   │   ├── wifi_manager.c  STA 连接
│   │   ├── web_server.c    HTTP + WebSocket
│   │   └── web/index.html  面板（编译时内嵌）
│   ├── CMakeLists.txt
│   └── sdkconfig.defaults
├── tools/flash_wsl.sh      WSL2 烧录脚本
├── docs/architecture.md
└── LICENSE
```

---

## 🏠 接入 Home Assistant（MQTT discovery）

`idf.py menuconfig` → *CSI Radar Configuration* → *Enable Home Assistant MQTT discovery*，
填好 MQTT 地址（`mqtt://homeassistant.local:1883`），烧录即可自动发现：

| 实体 | 类型 | 状态主题 |
|---|---|---|
| 存在检测 | `binary_sensor`（occupancy） | `csi_radar/state` → `presence` |
| 运动能量 | `sensor` | `csi_radar/state` → `energy` |
| 存在置信度 | `sensor` | `csi_radar/state` → `score` |
| 呼吸频率 | `sensor`（bpm） | `csi_radar/state` → `breath` |

发现配置 retained 发布到 `homeassistant/.../config`，可用性在 `csi_radar/status`。主题前缀可配置。

## 🗺️ 路线图

- [x] 自适应基线存在检测
- [x] WebSocket 实时面板
- [ ] 呼吸频率（CSI 相位带通）—— 打磨与验证
- [ ] 手势识别（CSI 方差模式 + ML）
- [x] Home Assistant MQTT discovery
- [ ] Matter 占用传感器
- [ ] 双节点 TX/RX 高灵敏度模式
- [ ] 3D 打印外壳

---

## 🤝 欢迎贡献

欢迎 PR。想法：

- 更好的 DSP（STFT 频谱、子载波 PCA）
- ESP32-C6 WiFi 6 HE-LTF CSI
- ESPHome 自定义组件
- 更多 Dashboard 主题

---

## ⚠️ 免责声明

CSI 感知是概率性的，不是医疗级。不要作为唯一的安全 / 医疗设备。
效果与环境、路由器位置、WiFi 流量密切相关。

## License

[MIT](./LICENSE)
