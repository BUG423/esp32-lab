# 🐾 bytepet — 电子宠物复活了

> **The desktop virtual pet is back.** A Tamagotchi-style companion that lives on a ¥15 ESP32, feeds from your browser, and dreams of talking to an LLM someday.

**English** · [中文文档](README_zh.md)

---

## 📖 Story

90后童年的电子宠物（Tamagotchi）回来了 —— 但这次，它跑在你桌面上的 ESP32 上。
养一只「小帕」，喂它、陪它玩、给它洗澡。它会从一颗蛋破壳而出，慢慢长大成人；
如果你不理它两天，它会变成幽灵（再喂三次就能复活）。

电子宠物复活了，而且它准备好迎接 AI 时代：未来接入 LLM，小帕就能开口说话，
变成你的 AI 语音伴侣。

## ✨ Features

| Feature | Description |
| --- | --- |
| 🥚 Life cycle | `egg → baby → teen → adult → ghost`, with revival |
| 📊 Stats | Hunger / Happiness / Energy / Cleanliness (0–100) |
| 🎭 Emotions | happy · hungry · tired · dirty · sad · sick · sleeping · ghost |
| 🌐 Web dashboard | Dark-cute theme, animated CSS blob pet, stat bars, action buttons |
| 🔴 WebSocket | 1 Hz live state push to the browser |
| 🔘 Physical button | Short press = feed, long press = play |
| 💾 Persistence | NVS — pet survives reboot |
| ⏰ Time-based | `esp_timer` / FreeRTOS tick task ages the pet |

## 🧾 BOM (Bill of Materials)

| Part | Notes | Est. price |
| --- | --- | --- |
| Any ESP32 dev board | ESP32-WROOM-32 / ESP32-DevKitC / ESP32-C3 (adjust target) | ~¥15 |
| Momentary button | Optional, GPIO0 by default (BOOT button works) | ~¥1 |
| USB cable | Power + flashing | — |

**Total: under ¥20.** That's cheaper than a 1997 Tamagotchi.

## 🚀 Quick Start

```bash
# 1. Get ESP-IDF v5.5.x
#    https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/

# 2. Configure WiFi (optional — leave empty for soft-AP "bytepet-setup" / "bytepet123")
cd firmware
. ../../esp/esp-idf/export.sh
idf.py menuconfig   # bytepet Configuration → WiFi SSID / password

# 3. Build & flash
idf.py set-target esp32
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor

# 4. Open the dashboard
#    http://<device-ip>/   (or http://192.168.4.1/ in soft-AP mode)
```

### Button wiring

```
ESP32 GPIO0 (BOOT) ──── button ──── GND
```

Internal pull-up is enabled; no external resistor needed.

## 🐣 Pet Lifecycle

```
        feed once
           │
   🥚 egg ─┴─► 🐣 baby ──(1 hour)──► 🐥 teen ──(3 hours)──► 🐶 adult
                                                              │
                                                     (neglect 2 days)
                                                              ▼
                                                          👻 ghost
                                                              │
                                                     (feed 3× again)
                                                              ▼
                                                            🐶 adult
```

**Stats drift every virtual minute:** hunger ↑, cleanliness ↓, energy ↓,
happiness drifts toward a mid value (pushed down by bad stats).

**Emotions are derived** from stats — not stored. Hungry when hunger ≥ 85,
tired when energy ≤ 15, sad when happiness ≤ 25, and so on.

## 🆚 Comparison

| | Tamagotchi (1997) | Giga Pets | Open source ESP32 pets | **bytepet** |
| --- | --- | --- | --- | --- |
| Hardware cost | $20+ | $15+ | varies | **~¥15** |
| Web dashboard | ✗ | ✗ | some | **✓ cute & animated** |
| Physical button | ✓ | ✓ | ✓ | **✓** |
| Survives reboot | ✓ | ✓ | most | **✓ NVS** |
| Real-time push | ✗ | ✗ | ✗ | **✓ WebSocket 1 Hz** |
| AI-ready | ✗ | ✗ | ✗ | **✓ roadmap** |
| Open source | ✗ | ✗ | ✓ | **✓ MIT** |

## 🗺 Roadmap

- [ ] 🔊 Voice / speaker feedback
- [ ] 🤖 LLM speech (「小帕会说话了」— connect to an AI voice companion)
- [ ] 📱 PWA offline cache
- [ ] 🎨 Custom skins / themes
- [ ] 🥚 Multiple pets
- [ ] 🌍 MQTT remote control
- [ ] 📈 Health history charts

## 📁 Project Layout

```
bytepet/
├── README.md              ← you are here (EN)
├── README_zh.md           ← 中文
├── LICENSE                ← MIT
├── docs/architecture.md   ← design notes
└── firmware/
    ├── CMakeLists.txt
    ├── sdkconfig.defaults
    ├── partitions.csv
    └── main/
        ├── main.c           entry point
        ├── pet.c/.h         state machine, stats, evolution
        ├── wifi_manager.c/.h  STA + soft-AP fallback
        ├── web_server.c/.h  HTTP + WebSocket
        ├── button.c/.h      GPIO short/long press
        └── web/index.html   dark-cute dashboard
```

## 📜 License

[MIT](LICENSE)

---

> *电子宠物复活了 — and it lives in your browser.* 🩷
