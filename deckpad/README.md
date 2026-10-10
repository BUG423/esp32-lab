# 🎛️ DeckPad

**A BLE macro keypad / streaming console for ¥20.** Press a key, the shortcut fires.

Phone, tablet, computer — anything that supports a BLE keyboard works. Edit macros from the web page anytime;
NVS keeps them across power loss.

[中文文档](./README_zh.md) · [Architecture](./docs/architecture.md)

---

## ✨ Features

| | |
|---|---|
| 🎹 **BLE HID keyboard** | standard keyboard protocol, works on Win/Mac/Android/iOS |
| ⌨️ **4 macro keys** | physical buttons *and* web clicks both trigger |
| 🧅 **3 macro layers** | long-press key 0 to cycle L0 streaming / L1 volume / L2 transport |
| 🎵 **Multimedia keys** | volume, mute, play/pause, next/prev track (HID consumer) |
| 🌐 **Web config page** | SoftAP direct connect `192.168.4.1`, macros apply instantly |
| 💾 **NVS persistence** | macros and current layer survive power loss |
| 📦 **Minimal BOM** | 1 ESP32 + 4 buttons |

---

## 🧰 Hardware

| Part | Notes | Price |
|---|---|---|
| ESP32 DevKit | any | ¥15 |
| Tactile switches ×4 | GPIO 0 / 4 / 16 / 17 (change in menuconfig) | ¥1 |

Wiring: one side of each button to GPIO, the other to GND (internal pull-up).

---

## 🚀 Quick start

```bash
cd firmware
. ../../esp/esp-idf/export.sh
idf.py set-target esp32
idf.py menuconfig     # button GPIOs / BLE name
idf.py build flash monitor
```

1. On phone/computer open **Bluetooth settings** → find `DeckPad` → pair
2. Join the WiFi hotspot `DeckPad-XXXX` (no password)
3. Open `http://192.168.4.1` to edit macros
4. Press a key or click a web button → shortcut fires

---

## 🎯 Default macros

| Slot | Name | Default action |
|---|---|---|
| 0 | Mute Mic | Ctrl+Shift+M |
| 1 | Action 2 | Ctrl+Shift+Z |
| 2 | Hello Text | send text |
| 3 | Copy | Ctrl+C |

**Streaming scenario:** slot 0 mutes the mic, slot 1 switches scenes, slot 2 sends a fixed chat line, slot 3 copies a link.

---

## 🔑 HID keycode cheat sheet

| Key | Code |
|---|---|
| a-z | 4-29 |
| 1-9 | 30-38 |
| 0 | 39 |
| Enter | 40 |
| Space | 44 |
| Modifiers | 1=Ctrl · 2=Shift · 4=Alt |

---

## 📁 Layout

```
deckpad/
└── firmware/main/
    ├── bt_hid.c      BLE HID keyboard protocol
    ├── macros.c      macro storage + NVS
    ├── buttons.c     physical buttons
    ├── web_server.c  config page API
    └── web/index.html
```

---

## 🗺️ Roadmap

- [x] BLE HID keyboard
- [x] Web macro editor + NVS
- [ ] OLED showing current macro
- [ ] Rotary encoder (volume)
- [x] Layers (long-press to switch macro groups)
- [x] Multimedia keys (volume / play)
- [ ] 3D-printed case

---

## License

MIT