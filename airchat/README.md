# 📡 AirChat — Off-Grid Mesh Text Messenger

**中文 | English** · 无网也能聊 · Chat without any router, internet, or SIM card.

> 户外徒步、应急救灾、校园宿舍、演唱会现场——网络瘫痪的时候，
> 只要每个人口袋里有一块 ¥15 的 ESP32，你们就有一个私密的文字对讲网。
>
> Outdoors, disasters, campuses, concerts — when the network dies, a
> ¥15 ESP32 in every pocket is all you need for a private text mesh.

`AirChat` is a Meshtastic-like messenger that runs on **nothing but ESP32
boards**. No LoRa module, no gateway, no phone plan. Pure ESP-NOW
broadcast + a built-in web dashboard.

---

## ✨ Features / 功能

| Feature | English | 中文 |
|---|---|---|
| 📻 ESP-NOW broadcast chat | All nodes hear all messages, no pairing | 广播式收发，免配对 |
| 🌐 SoftAP web dashboard | Join `airchat-XXXX`, open `192.168.4.1` | 连热点即用，无需路由器 |
| 🏷️ Nickname + RSSI | See who sent what and how strong | 昵称 + 信号强度徽章 |
| 📶 Signal quality bars | Live link quality per peer | 实时信号质量指示 |
| 💡 LED activity blink | GPIO2 flashes on every TX/RX | 收发自动闪灯 |
| 🔁 Dedup by message id | Retransmits don't spam the room | 消息 ID 去重 |
| 🚦 Flood control | 32-char nick, 200-char msg, rate limit | 长度限制 + 频率限制 |
| 🌙 Cyberpunk UI | Dark neon dashboard, mobile-friendly | 暗黑霓虹风，手机友好 |

## 🛒 BOM / 物料清单

| Item | Qty | Price | Notes |
|---|---|---|---|
| ESP32 DevKit (WROOM-32) | 1 per node | ~¥15 / $2 | Any classic ESP32 board |
| USB cable (power + flash) | 1 | — | 5V phone charger works |
| LED + 220Ω resistor | 1 | optional | Most boards have one on GPIO2 |

**That's it.** Total per node: about the price of a coffee. ☕

## 🔌 Wiring / 接线

Nothing to wire for basic use — the onboard LED (GPIO2) is the activity
indicator. If you want a brighter external LED:

```
ESP32 GPIO2 ----[ 220Ω ]----|>|---- GND
```

## 🚀 Quick Start / 快速开始

### 1. Build & flash

```bash
cd firmware
. ../../esp/esp-idf/export.sh
idf.py set-target esp32
idf.py build
idf.py -p /dev/ttyUSB0 flash
```

### 2. Configure (optional)

`idf.py menuconfig` → **AirChat Configuration**

| Option | Default | Meaning |
|---|---|---|
| Node nickname | `anon` | Shown with your messages |
| WiFi / ESP-NOW channel | `6` | **All nodes must match** |
| Activity LED GPIO | `2` | Onboard LED on most boards |
| SoftAP password | *(empty = open)* | 8+ chars for WPA2 |

### 3. Chat

1. Power **two or more** nodes (same channel!).
2. On your phone, join WiFi **`airchat-XXXX`** (XXXX = last 2 MAC bytes).
3. Open **`http://192.168.4.1/`** in a browser.
4. Type a message — every nearby node sees it instantly.

```
   [phone] )))  SoftAP airchat-A1B2  →  Node A  )))  ESP-NOW  (((  Node B
                 http://192.168.4.1
```

> 🖼️ *Screenshot placeholder / 截图占位*
>
> ![dashboard](docs/screenshot.png)

## 🆚 Comparison / 对比

| | **AirChat** | Meshtastic | WeChat | Walkie-talkie |
|---|---|---|---|---|
| Hardware cost | ~¥15 | ~¥200+ (LoRa) | free app | ¥50–500 |
| Needs internet | ❌ | ❌ | ✅ | ❌ |
| Needs phone plan | ❌ | ❌ | ✅ | ❌ |
| Text messages | ✅ | ✅ | ✅ | ❌ (voice) |
| Range (open air) | ~100–200 m | ~1–10 km | anywhere | ~1–5 km |
| Setup time | 5 min | 30 min | — | — |
| Open source | ✅ MIT | ✅ | ❌ | ❌ |
| Web dashboard | ✅ built-in | ❌ | — | — |
| Mesh routing | 🚧 roadmap | ✅ | cloud | — |

**AirChat wins** on price and simplicity for short-range chat.
**Meshtastic wins** on range (LoRa radio). They solve different problems.

## 📜 Protocol / 协议

One ESP-NOW broadcast frame (240 bytes):

```
magic u16 = 0xAC48 | version u8 = 1 | type u8 = 1
msg_id u32 | nick char[32] | text char[200]
```

- Destination MAC: `FF:FF:FF:FF:FF:FF` (broadcast)
- Dedup ring: 64 recent `msg_id` values
- Rate limit: 500 ms minimum between sends
- Peer table: MAC, nickname, RSSI, last-seen, message count

See [docs/architecture.md](docs/architecture.md) for the full design.

## 🗺️ Roadmap / 路线图

- [x] ESP-NOW broadcast chat
- [x] SoftAP web dashboard (dark cyberpunk UI)
- [x] Dedup, rate limit, peer list, RSSI badges
- [x] 😊 Emoji picker
- [ ] 🕸️ Mesh routing (multi-hop relaying)
- [ ] 🔐 End-to-end encryption
- [ ] 🔄 OTA updates over SoftAP
- [x] 📜 Message history persistence (NVS/SPIFFS)
- [ ] 🌍 mDNS name (`airchat.local`) when a router exists

## 📁 Project layout

```
airchat/
├── README.md              ← you are here (EN + 中文)
├── README_zh.md           ← 纯中文版
├── LICENSE                (MIT)
├── docs/architecture.md
└── firmware/
    ├── CMakeLists.txt
    ├── sdkconfig.defaults
    ├── partitions.csv
    └── main/
        ├── main.c             entry point
        ├── espnow_chat.c/.h   radio chat core
        ├── wifi_manager.c/.h  SoftAP + ESP-NOW channel
        ├── web_server.c/.h    HTTP + WebSocket
        └── web/index.html     dashboard
```

## 📄 License

MIT — see [LICENSE](LICENSE).

---

> 拿两块 ESP32，五分钟就能在宿舍楼里搭一个「离网微信群」。
> Grab two ESP32s and build an off-grid group chat in five minutes.