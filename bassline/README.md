# 🎵 bassline

**Audio spectrum / music visualizer for ESP32** — FFT from a mic, a WS2812 light show, and a neon web dashboard.

[中文文档 → README_zh.md](README_zh.md)

---

## ✨ Features

- 🎤 **Audio capture** — I2S digital mic (INMP441 / ICS-43434) **or** analog ADC mic, selected in `menuconfig`
- 📈 **Real FFT** — self-contained radix-2 FFT (256 / 512 samples) + Hann window, no external deps
- 🌈 **32–64 log-spaced bins** — perceptual frequency mapping (bass → treble)
- 💡 **WS2812 / NeoPixel strip** — spectrum bars, VU meter, breathing, sparkle modes
- 🎨 **Colour themes** — Rainbow (blue→cyan→green→yellow→red), Fire, Ice
- 📉 **Peak hold + smooth falloff** — looks great on camera
- 🌐 **Web dashboard** — dark neon UI (`#0b1020`), real-time canvas spectrum, VU meter, LED mirror, full controls
- 🔌 **WebSocket live stream** — ~30 fps spectrum frames + JSON control channel
- 📱 **SoftAP + STA** — works out of the box as `bassline / bassline123`, joins your Wi-Fi if configured

## 🎬 Where to use it

| Scene | Vibe |
|---|---|
| 🖥️ Desktop music visualizer | Neon bars dancing with your playlist |
| 🎤 Concert pen-light / 演唱会应援 | Hand-held glow that follows the beat |
| 🕹️ Gaming room lighting | Audio-reactive ambience for streams |
| 📹 30-second video material | Instant high-energy B-roll shots |

## 🛒 BOM (≈ ¥25 total)

| Part | Price | Notes |
|---|---|---|
| ESP32 DevKit (WROOM-32) | ~¥12 | any classic ESP32 board |
| INMP441 I2S mic module | ~¥8 | or ICS-43434 / MSM261 |
| WS2812 / NeoPixel strip, 60 LEDs | ~¥15 | 5V, 60 LED/m is a good density |
| USB cable + 5V supply | — | ≥1 A for the strip |

## 🔌 Wiring

### INMP441 (I2S)

| INMP441 | ESP32 |
|---|---|
| VDD | 3V3 |
| GND | GND |
| SD (data) | GPIO 33 (`BASSLINE_I2S_DIN_GPIO`) |
| SCK (BCLK) | GPIO 26 (`BASSLINE_I2S_BCLK_GPIO`) |
| WS | GPIO 25 (`BASSLINE_I2S_WS_GPIO`) |
| L/R | GND (left channel) |

### WS2812 strip

| WS2812 | ESP32 |
|---|---|
| DIN | GPIO 5 (`BASSLINE_LED_GPIO`) |
| 5V | 5 V supply (≥1 A) |
| GND | common GND with ESP32 |

> 💡 Add a 300–500 Ω resistor in series with DIN and a 1000 µF cap across 5V/GND for clean power.

### ADC mic (optional)

Wire the mic amp output to an ADC1 pin (GPIO 32–39), set `BASSLINE_MIC_ADC=y` and `BASSLINE_ADC_GPIO` in menuconfig.

## 🚀 Quick start

```bash
cd firmware
. ../../esp/esp-idf/export.sh
idf.py set-target esp32
idf.py menuconfig          # optional: WiFi SSID, pins, LED count
idf.py build flash monitor
```

Then join the **`bassline`** Wi-Fi (password `bassline123`) and open **http://192.168.4.1/**

If you set a STA SSID in menuconfig, the board also gets a LAN IP — open that instead.

## 🧠 How it works (FFT)

1. **Capture** 256 (or 512) samples at 16 kHz (~16 ms window).
2. **Hann window** tapers the edges so the FFT doesn't smear energy across bins.
3. **Radix-2 FFT** turns the time window into a magnitude spectrum (`n/2` bins, 31.25 Hz each at 256/16k).
4. **Log folding** merges linear bins into 48 log-spaced bands from 55 Hz to 8 kHz — matching how we *hear* pitch.
5. **Normalize** with gain + log compression, **peak-hold** with exponential falloff.
6. **Render** to LEDs (colour gradient by intensity) **and** broadcast JSON over WebSocket at 30 fps.

```
mic ──► window(256) ──► Hann ──► FFT ──► |mag| ──► log-bins(48) ──┬──► WS2812
                                                                     └──► WS /dashboard
```

## 🆚 vs WLED

| | **WLED** | **bassline** |
|---|---|---|
| Focus | 100+ LED *effects* | **Audio-reactive spectrum** |
| Audio | external sound-sync, limited | **on-device FFT + mic** |
| Dashboard | huge UI, effects-first | **real-time spectrum + VU** |
| Codebase | large, feature-rich | **tiny, hackable, IDF-native** |

Use **WLED** when you want pretty patterns. Use **bassline** when you want the lights to *dance to the music*.

## 🗺️ Roadmap

- [ ] BLE MIDI / button mode switching
- [ ] Microphone AGC + noise gate
- [ ] More themes (sunset, matrix, pastel)
- [ ] OLED / LED-matrix output
- [ ] OTA firmware updates
- [ ] Stereo mics + spatial effects

## 📄 License

MIT — see [LICENSE](LICENSE).
