# Architecture

## Overview

```
┌─────────────────────────────────────────────────────────┐
│                      ESP32 firmware                     │
│                                                         │
│  ┌──────────┐   ┌──────────────┐   ┌────────────────┐   │
│  │ WiFi STA │──►│ CSI callback │──►│ csi_processor  │   │
│  └──────────┘   └──────────────┘   └───────┬────────┘   │
│                                            │ snapshot   │
│                                     ┌──────▼────────┐   │
│                                     │ publish task  │   │
│                                     │  (10 Hz)      │   │
│                                     └──────┬────────┘   │
│                                            │ JSON/WS    │
│                                     ┌──────▼────────┐   │
│                                     │  web_server   │   │
│                                     │  /  /ws  /api │   │
│                                     └───────────────┘   │
└─────────────────────────────────────────────────────────┘
                         ▲
                         │ WiFi packets (router / phones / 2nd ESP32)
                         │
                    📡 RF environment
```

## Pipeline

### 1. Capture
`esp_wifi_set_csi_rx_cb()` delivers a raw buffer per received packet.
On classic ESP32 (HT20) this is 64 subcarriers × 2 bytes (int8 I/Q) = 128 bytes,
plus a first/second LLTF+HT-LTF segment depending on config.

### 2. Amplitude
For each subcarrier `i`:

```
amp[i] = sqrt(I[i]^2 + Q[i]^2)
```

### 3. Temporal variance
Keep a sliding window of the last `N=32` amplitude vectors:

```
var[i] = mean_t (amp_t[i] - mean_amp[i])^2
```

Human motion perturbs some subcarriers more than others; the variance profile
is the "motion signature".

### 4. Motion energy
Aggregate across subcarriers:

```
motion_energy = mean_i(var[i])
```

### 5. Adaptive baseline
The quiet-room energy drifts (temperature, distant traffic). Track it with an EMA
that only updates when current energy is *close* to the baseline
(so a person standing still doesn't drag the baseline up):

```
if energy < baseline * 1.5:
    baseline += α * (energy - baseline)      # α ≈ 0.02
```

### 6. Presence decision
```
ratio  = motion_energy / baseline
present = ratio >= threshold                  # default threshold = 2.5
score   = clamp((ratio - 1) / (2 * threshold), 0, 1)
```

### 7. Breathing (experimental)
Pick the dominant subcarrier, take its phase `atan2(I,Q)`, bandpass ~0.1–0.5 Hz,
count positive-going zero crossings → BPM. Very environment-sensitive.

## WebSocket protocol

Client connects to `ws://<host>/ws`.

Server pushes (≈10 Hz):

```json
{
  "type": "csi",
  "ts": 12.345,
  "samples": 1024,
  "energy": 0.42,
  "baseline": 0.18,
  "score": 0.61,
  "present": true,
  "breath": 16.2,
  "amp":  [ ... 64 floats ... ],
  "var":  [ ... 64 floats ... ]
}
```

Client → server command:

```json
{ "cmd": "recalibrate" }
```

## Design notes

- **No malloc in the CSI callback.** The callback just feeds a preallocated ring buffer.
- **Mutex-protected snapshot.** The web layer pulls a consistent `csi_frame_t` on its own cadence.
- **Embedded HTML.** `EMBED_FILES` bakes `index.html` into `.rodata` — no SPIFFS/LittleFS needed for the UI.
- **Adaptive baseline** is more robust across rooms than a fixed threshold.

## Limitations

- Single-antenna ESP32 gives coarse sensing (presence / gross motion), not fine pose.
- Needs *some* WiFi traffic to produce CSI frames. A silent room = no packets = no data.
- Multipath is environment-specific; always recalibrate per room.
- Breathing rate is a demo feature — do not treat as medical data.

## Future work

| Idea | Why |
|---|---|
| STFT spectrogram on the dashboard | visualize micro-Doppler of breathing / walking |
| PCA / subspace on subcarriers | suppress static furniture, boost human motion |
| Two-node TX/RX | controlled illuminator = more reliable CSI |
| ESP32-C6 HE-LTF | WiFi 6 CSI with more subcarriers |
| TinyML gesture head | wave / fall / walk classification |
| Home Assistant MQTT discovery | drop into existing smart homes |
