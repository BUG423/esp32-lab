# 💡 GlowLink

**Two ordinary LEDs are enough to hold an encrypted call.** No WiFi, no Bluetooth, no radio — just light.

An LED is not only a light source: it is also a photodiode. GlowLink puts one LED on each of two ESP32 boards —
one blinks, one "watches" — and data travels across the line of sight. The moment they align, the green light
comes on and the text goes through.

Inspired by [SecurePair](https://github.com/toggio/SecurePair) (Hackaday 2026-10-05),
we added a **link oscilloscope visualization**, Chinese tutorial, and a lower barrier to reproduction.

[中文文档](./README_zh.md) · [Architecture](./docs/architecture.md)

---

## ✨ Why it is fun

| | |
|---|---|
| 🎥 **Camera-friendly** | LEDs blinking text is natural short-video material |
| 💰 **Dirt cheap** | 1 LED + 1 resistor per node, BOM under ¥3 |
| 🔒 **Has a story** | "Pair with light, exchange keys with light" — security narrative |
| 📓 **No RF** | Immune to jamming, no spectrum used, directional |
| 📊 **Visualizable** | Built-in oscilloscope panel — you can *see* the signal |

---

## 🧰 Hardware (per node)

| Part | Notes | Price |
|---|---|---|
| ESP32 DevKit | any | ¥15 |
| LED | brighter is better (white / high-bright green) | ¥0.5 |
| Resistor | 220Ω (TX current limit) | ¥0.1 |
| (optional) second LED | dedicated RX, higher sensitivity | ¥0.5 |

### Wiring

```
        ┌──────────────┐
        │    ESP32     │
        │              │
   GPIO4├──[220Ω]──|>|──┐
        │   (TX LED)    │
        │               GND
        │              │
   GPIO34├────────|>|──┘
        │   (RX LED, can share with TX)
       GND
```

- **TX**: GPIO4 drives the LED through 220Ω
- **RX**: GPIO34 (ADC1_CH6) reads the LED voltage directly
- Place the two boards' LEDs **facing each other**, 2–20 cm apart

---

## 🚀 Quick start

```bash
cd firmware
. ../../esp/esp-idf/export.sh
idf.py set-target esp32
idf.py menuconfig     # WiFi + TX/RX pins + baud rate + Sender/Receiver
idf.py build flash monitor
```

Open `http://glowlink.local` → watch the oscilloscope → type a message → **💡 Send**

Set one board as **Sender** (auto beacon), the other as **Receiver**. Done when both hear each other.

---

## 📡 Protocol

```
Manchester coding @ 250 bps (adjustable 50–2000)

Frame:  0xAA 0xAA 0xAA | LEN | PAYLOAD[LEN] | XOR-CRC8
```

- **Manchester**: self-clocking, no DC component — ideal for optical channels
- **Preamble 0xAA×3**: receiver synchronization
- **CRC8 XOR**: minimal checksum; bad frames are dropped and counted

---

## 📊 Link oscilloscope

Real-time display of:
- **Signal amplitude** (peak-to-peak)
- **SNR**
- **Link quality 0–100%**
- **Error counter**
- **Raw waveform** (green phosphor line — very photogenic)

During a live demo, cast the oscilloscope to a big screen. LED blinks, waveform jumps — maximum effect.

---

## 📁 Layout

```
glowlink/
├── firmware/
│   └── main/
│       ├── optical.c     LED TX/RX + Manchester + scope sampling
│       ├── protocol.c    frame sync + CRC + dispatch
│       ├── web_server.c  / /api/stats /api/send /ws
│       └── web/index.html  oscilloscope panel
└── docs/
```

---

## 🗺️ Roadmap

- [x] LED half-duplex TX/RX (Manchester)
- [x] Frame protocol + CRC
- [x] Web oscilloscope + message console
- [x] **Optical pairing**: exchange a shared key via blink sequences
- [x] **Encrypted messages** after pairing (demo-grade keystream XOR)
- [ ] **Hybrid mode**: optical pairing + ESP-NOW bulk transfer (SecurePair idea)
- [ ] Full duplex at once (two-color LED / two wavelengths)
- [ ] Adaptive baud rate (speed up when the link is clean)
- [ ] 3D-printed "alignment jig"

---

## ⚠️ Notes

- Strong ambient light (sunlight, direct lamps) will drown the signal — demo in a **dimmer** room
- The same LED doing TX and RX is **half-duplex**: only one direction at a time
- Higher baud rates demand better alignment / brightness
- This is **demo-grade** optical comms, not an industrial optical module

---

## License

MIT
