# 📡 AirChat — 离网对讲文字聊天器

**纯中文版** · 英文版见 [README.md](README.md)

> 户外徒步、应急救灾、校园宿舍、演唱会现场——网络瘫痪的时候，
> 只要每个人口袋里有一块 ¥15 的 ESP32，你们就有一个私密的文字对讲网。

AirChat 是一个类 Meshtastic 的离网聊天器，但**只需要 ESP32 开发板本身**。
没有 LoRa 模块、没有网关、没有手机卡。纯 ESP-NOW 广播 + 内置网页控制台。

---

## ✨ 功能

| 功能 | 说明 |
|---|---|
| 📻 ESP-NOW 广播聊天 | 所有节点都能听到所有消息，免配对 |
| 🌐 SoftAP 网页面板 | 连上 `airchat-XXXX` 热点，打开 `192.168.4.1` |
| 🏷️ 昵称 + RSSI | 看清谁发的、信号多强 |
| 📶 信号质量指示条 | 实时显示每个节点的信号强度 |
| 💡 LED 收发闪灯 | GPIO2 在每次收发时闪烁 |
| 🔁 消息 ID 去重 | 重传不会刷屏 |
| 🚦 防刷控制 | 昵称 ≤32 字符、消息 ≤200 字符、发送频率限制 |
| 🌙 赛博朋克界面 | 暗黑霓虹风格，手机浏览器友好 |

## 🛒 物料清单（BOM）

| 物品 | 数量 | 价格 | 备注 |
|---|---|---|---|
| ESP32 开发板（WROOM-32） | 每节点 1 块 | 约 ¥15 | 任意经典 ESP32 开发板 |
| USB 数据线 | 1 | — | 供电 + 烧录，充电头也行 |
| LED + 220Ω 电阻 | 1 | 可选 | 大多数板子 GPIO2 自带 LED |

**就这些。** 每个节点成本 ≈ 一杯奶茶。🧋

## 🔌 接线

基础使用**无需接线**——板载 LED（GPIO2）就是活动指示灯。
若要外接更亮的 LED：

```
ESP32 GPIO2 ----[ 220Ω ]----|>|---- GND
```

## 🚀 快速开始

### 1. 编译烧录

```bash
cd firmware
. ../../esp/esp-idf/export.sh
idf.py set-target esp32
idf.py build
idf.py -p /dev/ttyUSB0 flash
```

### 2. 配置（可选）

`idf.py menuconfig` → **AirChat Configuration**

| 选项 | 默认值 | 说明 |
|---|---|---|
| 节点昵称 | `anon` | 显示在你的每条消息旁 |
| WiFi / ESP-NOW 信道 | `6` | **所有节点必须一致** |
| 活动 LED GPIO | `2` | 多数开发板的板载 LED |
| SoftAP 密码 | *（空 = 开放）* | 设置则至少 8 位（WPA2） |

### 3. 开聊

1. 给**两块或更多**节点上电（信道相同！）。
2. 手机连接 WiFi 热点 **`airchat-XXXX`**（XXXX 为 MAC 后两字节）。
3. 浏览器打开 **`http://192.168.4.1/`**。
4. 输入消息——附近所有节点立即收到。

```
   [手机] )))  SoftAP airchat-A1B2  →  节点 A  )))  ESP-NOW  (((  节点 B
                 http://192.168.4.1
```

> 🖼️ *截图占位*
>
> ![dashboard](docs/screenshot.png)

## 🆚 对比

| | **AirChat** | Meshtastic | 微信 | 对讲机 |
|---|---|---|---|---|
| 硬件成本 | 约 ¥15 | 约 ¥200+（LoRa） | 免费 App | ¥50–500 |
| 需要联网 | ❌ | ❌ | ✅ | ❌ |
| 需要手机卡 | ❌ | ❌ | ✅ | ❌ |
| 文字消息 | ✅ | ✅ | ✅ | ❌（只有语音） |
| 空旷距离 | 约 100–200 米 | 约 1–10 公里 | 无限制 | 约 1–5 公里 |
| 上手时间 | 5 分钟 | 30 分钟 | — | — |
| 开源 | ✅ MIT | ✅ | ❌ | ❌ |
| 网页面板 | ✅ 内置 | ❌ | — | — |
| Mesh 路由 | 🚧 路线图 | ✅ | 云端 | — |

**AirChat 赢在**短距离聊天的极致性价比与简单。
**Meshtastic 赢在**距离（LoRa 射频）。两者解决不同问题。

## 📜 协议

一帧 ESP-NOW 广播包（240 字节）：

```
magic u16 = 0xAC48 | version u8 = 1 | type u8 = 1
msg_id u32 | nick char[32] | text char[200]
```

- 目标 MAC：`FF:FF:FF:FF:FF:FF`（广播）
- 去重环形缓冲：最近 64 个 `msg_id`
- 发送频率限制：最小间隔 500 ms
- 节点表：MAC、昵称、RSSI、最后出现时间、消息计数

完整设计见 [docs/architecture.md](docs/architecture.md)。

## 🗺️ 路线图

- [x] ESP-NOW 广播聊天
- [x] SoftAP 网页面板（暗黑赛博朋克 UI）
- [x] 去重、限流、节点列表、RSSI 徽章
- [x] 😊 Emoji 表情选择器
- [ ] 🕸️ Mesh 多跳路由转发
- [ ] 🔐 端到端加密
- [ ] 🔄 SoftAP OTA 升级
- [x] 📜 消息历史持久化（NVS/SPIFFS）
- [ ] 🌍 有路由器时支持 mDNS（`airchat.local`）

## 📁 工程结构

```
airchat/
├── README.md              （英中双语）
├── README_zh.md           ← 你在这里（纯中文）
├── LICENSE                (MIT)
├── docs/architecture.md
└── firmware/
    ├── CMakeLists.txt
    ├── sdkconfig.defaults
    ├── partitions.csv
    └── main/
        ├── main.c             入口
        ├── espnow_chat.c/.h   无线聊天核心
        ├── wifi_manager.c/.h  SoftAP + ESP-NOW 信道
        ├── web_server.c/.h    HTTP + WebSocket
        └── web/index.html     控制台界面
```

## 📄 许可证

MIT — 见 [LICENSE](LICENSE)。

---

> 拿两块 ESP32，五分钟就能在宿舍楼里搭一个「离网微信群」。