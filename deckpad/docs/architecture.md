# DeckPad Architecture / 架构设计

## Overview / 概览

DeckPad turns an ESP32 into a BLE HID macro keypad with a web-based
configuration page. Four layers cooperate on one chip: physical button
scanning, macro storage, BLE HID transport, and a SoftAP web UI.

```
┌─────────────────────────────────────────────────────────┐
│  Host (phone / PC)                                      │
│  BLE HID keyboard sink · sees keystrokes                │
└───────────────────────────┬─────────────────────────────┘
                            │ BLE GATT (HID over GATT)
┌───────────────────────────▼─────────────────────────────┐
│  bt_hid.c                                               │
│  HID report descriptor · notify keystroke reports       │
│  modifier + key bitmap · consumer page (media) planned  │
└───────────────────────────┬─────────────────────────────┘
                            │ macros_fire(slot)
┌───────────────────────────▼─────────────────────────────┐
│  macros.c                                               │
│  slot table (name, type, payload) · NVS load/save       │
│  key-sequence emitter · text-string emitter             │
└───────────────────────────┬─────────────────────────────┘
                            │ button events / HTTP
        ┌───────────────────┴───────────────────┐
┌───────▼──────────┐                 ┌──────────▼──────────┐
│  buttons.c       │                 │  web_server.c       │
│  GPIO 0/4/16/17  │                 │  SoftAP 192.168.4.1 │
│  debounce 30 ms  │                 │  GET /api/macros    │
│  long-press plan │                 │  POST /api/macros   │
└──────────────────┘                 │  web/index.html     │
                                     └─────────────────────┘
```

## BLE HID model / 蓝牙 HID 模型

- NimBLE-based HID device; standard keyboard boot protocol so hosts need
  no custom driver.
- Report format: `[modifier][reserved][key1..key6]` — one report per event,
  followed by an all-zero report to release.
- Pairing is Just Works; the device name is configurable via menuconfig
  (`CONFIG_DECKPAD_BLE_NAME`, default `DeckPad`).

## Macro model / 宏模型

| Field | Meaning |
|---|---|
| `slot` | 0..3 (extensible) |
| `name` | display label |
| `type` | `combo` (modifier+key) or `text` (UTF-8 string) |
| `payload` | keycodes or text bytes |

- NVS namespace `deckpad`, key per slot; written on every web edit.
- `type=text` is emitted as sequential HID key reports for printable ASCII;
  non-ASCII is a roadmap item.

## WiFi / 网络

- SoftAP only (`DeckPad-XXXX`), no STA association required.
- Captive-friendly: HTTP on `192.168.4.1`, single-page config UI.
- BLE and WiFi coexist via ESP32 coex; BLE is the latency-critical path.

## Timing / 时序

| Path | Budget |
|---|---|
| Button debounce | 30 ms |
| Button → HID report | < 50 ms end-to-end |
| Web edit → NVS write | synchronous, < 20 ms |

## Module map / 模块对照

| File | Responsibility |
|---|---|
| `main.c` | init order, event loop glue |
| `buttons.c` | GPIO ISR + debounce + edge events |
| `macros.c` | NVS-backed slot table + emitters |
| `bt_hid.c` | BLE GAP/GATT HID device |
| `web_server.c` | SoftAP + REST + static page |
| `web/index.html` | macro editor UI |
