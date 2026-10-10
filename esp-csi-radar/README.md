# 📡 esp-csi-radar

**Turn any $5 ESP32 into a WiFi presence radar — no camera, no wearable, no cloud.**

ESP32 listens to ordinary WiFi packets and reads the **Channel State Information (CSI)**
— tiny distortions in the radio waves caused by people moving, breathing, even
shifting in a chair. One board, one USB cable, a browser dashboard. That's it.

```
 👤 human moves  ──►  WiFi multipath changes  ──►  ESP32 CSI  ──►  web dashboard
```

[中文文档](./README_zh.md) · [Architecture](./docs/architecture.md)

---

## ✨ Features

| Feature | Status |
|---|---|
| Real-time presence detection (empty / occupied) | ✅ |
| Motion energy + adaptive baseline | ✅ |
| Per-subcarrier amplitude & variance plots | ✅ |
| Live WebSocket dashboard (dark UI) | ✅ |
| Crude breathing-rate estimate | 🚧 experimental |
| Home Assistant MQTT discovery | ✅ |
| Matter occupancy sensor | 🔜 planned |
| Multi-node mesh sensing | 🔜 planned |

- **Privacy-first** — nothing leaves your LAN. No camera. No microphone.
- **One-file firmware** — pure ESP-IDF, no Arduino, no cloud SDK.
- **Cheap** — works on a bare ESP32 DevKit (~$5). No extra sensors needed.
- **Visual wow** — animated orb + spectrogram, perfect for videos.

---

## 🧰 Hardware

| Part | Notes | Price |
|---|---|---|
| ESP32 DevKit (or ESP32-S3 / C3) | any board with WiFi | ~$3–8 |
| USB cable | power + flash | — |
| (optional) 2nd ESP32 | as a dedicated "illuminator" transmitter | ~$4 |

> A home WiFi router already fills the room with radio waves — those packets are
> enough to get started. For best results, have *some* WiFi traffic (phones,
> IoT devices, or a second ESP32 sending packets).

---

## 🚀 Quick start

### 1. Install ESP-IDF (once)

```bash
# Linux / WSL2
sudo apt-get install git wget flex bison gperf python3 python3-pip \
  python3-venv cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0

# ESP-IDF ships in-repo (v5.5.1) — from the workspace root:
cd ../../esp/esp-idf && ./install.sh esp32
. ../../esp/esp-idf/export.sh
```

### 2. Configure WiFi

```bash
cd firmware
idf.py menuconfig
#  → CSI Radar Configuration
#     WiFi SSID / WiFi Password / hostname
```

### 3. Build & flash

```bash
idf.py set-target esp32     # or esp32s3 / esp32c3
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

**WSL2 users:** attach the serial port first (`tools/flash_wsl.sh` has the commands).

### 4. Open the dashboard

```
http://csi-radar.local
http://<ip printed in serial log>
```

1. Leave the room **empty** for ~15 seconds.
2. Press **Recalibrate baseline**.
3. Walk back in — the orb turns green. 🎉

---

## 🖼️ Dashboard

- **Presence orb** — glows green when someone is detected
- **Live score chart** — presence score + motion energy over time
- **CSI amplitude** — per-subcarrier signal strength (64 bars)
- **CSI variance** — the "motion signature" heatmap

---

## 🧠 How it works

```
raw CSI (int8 I/Q)
   ↓
per-subcarrier amplitude  |I + jQ|
   ↓
temporal variance over sliding window (32 frames)
   ↓
motion_energy = mean(variance across 64 subcarriers)
   ↓
adaptive baseline (EMA of quiet-room energy)
   ↓
presence = motion_energy > baseline × threshold
```

More detail in [docs/architecture.md](./docs/architecture.md).

---

## 📁 Repository layout

```
esp-csi-radar/
├── firmware/               ESP-IDF project
│   ├── main/
│   │   ├── main.c          boot, CSI callback, publish task
│   │   ├── csi_processor.c signal processing + presence
│   │   ├── wifi_manager.c  STA connect
│   │   ├── web_server.c    HTTP + WebSocket
│   │   └── web/index.html  dashboard (embedded at build time)
│   ├── CMakeLists.txt
│   └── sdkconfig.defaults
├── tools/flash_wsl.sh      WSL2 flash helper
├── docs/architecture.md
└── LICENSE
```

---

## 🏠 Home Assistant (MQTT discovery)

Enable in `idf.py menuconfig` → *CSI Radar Configuration* → *Enable Home Assistant MQTT discovery*,
set the broker URI (`mqtt://homeassistant.local:1883`), flash, done. The device publishes:

| Entity | Type | State topic |
|---|---|---|
| Presence | `binary_sensor` (occupancy) | `csi_radar/state` → `presence` |
| Motion Energy | `sensor` | `csi_radar/state` → `energy` |
| Presence Score | `sensor` | `csi_radar/state` → `score` |
| Breathing Rate | `sensor` (bpm) | `csi_radar/state` → `breath` |

Discovery configs are retained on `homeassistant/.../config`; availability on `csi_radar/status`.
Topic prefix is configurable.

## 🗺️ Roadmap

- [x] Presence detection with adaptive baseline
- [x] Real-time WebSocket dashboard
- [ ] Breathing-rate (bandpass on CSI phase) — polish & validate
- [ ] Gesture recognition (ML on CSI variance patterns)
- [x] Home Assistant MQTT discovery
- [ ] Matter occupancy sensor
- [ ] Two-node TX/RX mode for higher sensitivity
- [ ] 3D-printable enclosure

---

## 🤝 Contributing

PRs welcome. Ideas:

- better DSP (STFT spectrogram, PCA on subcarriers)
- ESP32-C6 WiFi 6 HE-LTF CSI
- ESPHome custom component
- more dashboard themes

---

## ⚠️ Disclaimer

CSI sensing is probabilistic, not medical-grade. Do not use as a sole
safety / medical device. Performance depends heavily on environment,
router placement, and WiFi traffic.

## License

[MIT](./LICENSE)
