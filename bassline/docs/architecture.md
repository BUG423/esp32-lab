# bassline Architecture

## Overview

`bassline` is a single-target ESP-IDF application (ESP32, IDF v5.x) that:

1. Captures audio from an I2S or ADC microphone.
2. Runs a radix-2 FFT with a Hann window.
3. Folds the spectrum into 32–64 log-spaced bins.
4. Renders the result to a WS2812 LED strip (RMT).
5. Streams the same spectrum to a browser dashboard over WebSocket at ~30 fps.

```
                ┌──────────────┐
  INMP441 ────►│  audio_in.c   │ 16 kHz, 256-sample windows
  (I2S/ADC)    └──────┬───────┘
                      ▼
                ┌──────────────┐
                │    fft.c     │ Hann → radix-2 FFT → |X[k]|
                └──────┬───────┘
                       ▼
                ┌──────────────┐
                │   main.c     │ log-fold + normalise + peak-hold
                └──┬────────┬──┘
                   ▼        ▼
          ┌──────────┐  ┌────────────┐
          │led_strip │  │ web_server │  HTTP + WS JSON
          │  .c (RMT)│  │   .c       │  @ 30 fps
          └──────────┘  └─────┬──────┘
                              ▼
                      web/index.html
                      (canvas dashboard)
```

## Components

### `audio_in.c`
- **I2S path** (default): `driver/i2s_std.h` standard-mode RX, 32-bit Philips
  slots (INMP441 is 24-bit left-justified in 32-bit frames).
- **ADC path**: `esp_adc/adc_oneshot` + line-fitting calibration, paced with
  `esp_timer` to approximate the configured sample rate.
- Kconfig selects the backend and pins.

### `fft.c`
- Precomputed bit-reversal table + twiddle factors.
- Iterative in-place radix-2 Cooley-Tukey, `float` math, `math.h` only.
- Real input → magnitudes `0 .. n/2-1`.
- `fft_log_bins()` maps linear bins to log-spaced perceptual bands.

### `led_strip.c`
- WS2812 protocol driven by the IDF 5.x `driver/rmt_tx.h` API with the built-in
  `rmt_copy_encoder`.
- Each bit becomes one `rmt_symbol_word_t` at 10 MHz resolution
  (0-bit: 0.4 µs high / 0.8 µs low; 1-bit: 0.8 µs high / 0.4 µs low).
- GRB wire order, 80 µs reset gap after refresh.

### `web_server.c`
- `esp_http_server` with `CONFIG_HTTPD_WS_SUPPORT=y`.
- Serves the embedded `web/index.html` at `/`, JSON health at `/health`,
  WebSocket at `/ws`.
- Broadcasts frames: `{"t":"frame","bins":[...],"peaks":[...],"vu":0.5,...}`.
- Accepts controls: `{"gain":1.2,"decay":0.85,"theme":"rainbow","mode":"spectrum"}`.
- Up to 8 concurrent WS clients (local define, not `CONFIG_HTTPD_MAX_OPEN_SOCKETS`).

### `web/index.html`
- Single-file dark neon dashboard (`#0b1020`).
- Canvas spectrum with glow, VU meter, LED strip mirror, theme/mode buttons,
  gain + decay sliders. Auto-reconnecting WebSocket.

## Data flow & timing

| Stage | Rate | Buffer |
|---|---|---|
| I2S capture | 16 kHz | 256 samples ≈ 16 ms |
| FFT + fold | ~60 Hz | 48 bins |
| LED render | 30 Hz | 60 px |
| WS broadcast | 30 Hz | JSON frame |

The audio task runs on core 0; the visual/WS task on core 1.

## Kconfig surface

All configuration is `int` / `bool` / `string` (no floats — gain and decay are
sent as scaled integers in the web UI and stored as `float` in RAM only):

- Mic type, I2S/ADC pins, sample rate
- FFT size (128–512), log bin count (32–64)
- LED GPIO + count
- WiFi SSID / password (optional STA)

## Extension points

- New render modes: add a `bassline_mode_t` + a `s_render_*()` function in `main.c`.
- New themes: extend `s_color_theme()` and the JS `colorFor()` ramp together.
- Different mic front-ends: implement `audio_in_init/read` for the new backend.
