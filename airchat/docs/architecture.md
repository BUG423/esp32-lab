# AirChat Architecture / 架构设计

## Overview / 概览

AirChat turns a bare ESP32 into an off-grid text messenger. Each node runs
three cooperating layers on one chip: a SoftAP web dashboard, an ESP-NOW
broadcast radio stack, and a small chat core that glues them together.

```
┌─────────────────────────────────────────────────────────┐
│  Browser (phone / laptop on airchat-XXXX SoftAP)        │
│  dark cyberpunk dashboard · WebSocket + REST            │
└───────────────────────────┬─────────────────────────────┘
                            │ HTTP / WS  (192.168.4.1)
┌───────────────────────────▼─────────────────────────────┐
│  web_server.c                                           │
│  GET /            → embedded index.html                 │
│  GET /api/state   → JSON snapshot                       │
│  POST /api/send   → submit text                         │
│  WS  /ws          → live push of state changes          │
└───────────────────────────┬─────────────────────────────┘
                            │ callbacks
┌───────────────────────────▼─────────────────────────────┐
│  espnow_chat.c                                          │
│  frame codec · dedup by msg_id · rate limit             │
│  peer table (MAC, nickname, RSSI) · LED pulse           │
└───────────────────────────┬─────────────────────────────┘
                            │ esp_now_send / recv cb
┌───────────────────────────▼─────────────────────────────┐
│  wifi_manager.c                                         │
│  WIFI_MODE_APSTA · SoftAP airchat-XXXX · channel pin    │
│  STA interface idle (never associates) for ESP-NOW      │
└─────────────────────────────────────────────────────────┘
```

## Radio model / 无线模型

- SoftAP + ESP-NOW share one radio and therefore **one channel**
  (`CONFIG_AIRCHAT_CHANNEL`, default 6). Every node in a group must use
  the same channel.
- ESP-NOW frames go to the broadcast MAC `FF:FF:FF:FF:FF:FF`, so any node
  in range hears them — no pairing, no router.
- The STA interface is started but never calls `esp_wifi_connect()`;
  `esp_wifi_set_channel()` pins the frequency after `esp_wifi_start()`.

## Wire protocol / 空中协议

One ESP-NOW payload = one packed 240-byte frame:

| offset | size | field   | notes                          |
|-------:|-----:|---------|--------------------------------|
| 0      | 2    | magic   | `0xAC48`                       |
| 2      | 1    | version | `1`                            |
| 3      | 1    | type    | `1` = text chat                |
| 4      | 4    | msg_id  | random seed + increment        |
| 8      | 32   | nick    | NUL-padded, truncated to 32    |
| 40     | 200  | text    | NUL-padded, truncated to 200   |

Dedup: a 64-slot ring of recently seen `msg_id` values drops duplicates.
Frames whose source MAC equals our own STA MAC are ignored (echo guard).

## Flood control / 防刷

| limit              | value   | where                |
|--------------------|---------|----------------------|
| nickname length    | 32      | frame field size     |
| message length     | 200     | frame field size     |
| send rate          | 500 ms  | `espnow_chat_send()` |
| RX queue depth     | 16      | FreeRTOS queue       |
| history / peers    | 48 / 16 | ring buffers         |

## Threading / 并发

- ESP-NOW receive callback runs in the WiFi task — it only copies the
  frame into a queue and returns.
- `chat_rx` task parses, dedups, stores, blinks the LED, and fires the
  notify callback that pushes JSON over WebSocket.
- `httpd_ws_send_frame_async()` is used so WS pushes never block the
  radio path on a slow client.

## Web dashboard / 网页面板

`main/web/index.html` is compiled into the firmware via `EMBED_FILES`
(symbol `_binary_index_html_start`). Style: dark `#0b1020` background,
neon cyan/violet accents, card layout.

- chat room with nickname, RSSI badge, timestamp
- composer with send button (WS primary, POST fallback)
- node list with per-peer RSSI and message counts
- signal quality bars derived from best peer RSSI
- TX/RX LED indicators that flash on activity

## Build / 构建

```
cd firmware
. ~/esp/esp-idf/export.sh
idf.py set-target esp32
idf.py build
```

Partition table (4 MB flash): `nvs` 24K, `phy_init` 4K, `factory` app 1M,
`storage` spiffs 960K.
