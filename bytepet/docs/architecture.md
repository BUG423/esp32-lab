# bytepet Architecture

## Overview

bytepet is a single-target ESP-IDF application (esp32) with four independent
subsystems wired together in `app_main`:

```
app_main
 ├── nvs_flash_init        persistence backend
 ├── pet_init              state machine + tick task
 ├── button_init           GPIO short/long-press → pet_do_action()
 ├── wifi_manager_start    STA (preferred) or soft-AP fallback
 └── web_server_start      HTTP + WebSocket, 1 Hz push task
```

## Data flow

```
    button (GPIO)             web browser
        │                         │  WebSocket / HTTP
        ▼                         ▼
   pet_do_action()  ◄──────  web_server handlers
        │
        ▼
   pet state (mutex-protected)
        │
        ├── pet_tick_task (FreeRTOS, 1 s period)
        │     ├── age the pet
        │     ├── drift stats
        │     ├── derive emotion
        │     └── evolve stage → pet_save_nvs()
        │
        └── pet_get_state() → JSON → WS broadcast (1 Hz)
```

## Module breakdown

### pet.c / pet.h — state machine

| Field | Range | Drift |
| --- | --- | --- |
| `hunger` | 0–100 | +1 / virtual min |
| `happiness` | 0–100 | drifts toward mid target |
| `energy` | 0–100 | −1 / virtual min (sleep: +8 / min) |
| `cleanliness` | 0–100 | −1 / virtual min |

* Virtual minute = 60 000 ms wall time (independent of tick period).
* Stage transitions are driven by `tick / TICKS_PER_MINUTE`.
* Emotion is **derived** every tick from stats + sleeping/ghost flags.
* All mutation happens under `s_lock` (FreeRTOS mutex).
* NVS namespace `bytepet`, keys `hunger/happiness/energy/clean/tick/stage/sleep`.

### wifi_manager.c

1. If `CONFIG_BYTEPET_WIFI_SSID` is non-empty → STA with 20 retries / 30 s timeout.
2. On failure (or empty SSID) → soft-AP `bytepet-setup` / `bytepet123`.
3. `wifi_manager_get_ip()` returns the assigned IPv4 string.

### web_server.c

* Embedded `web/index.html` via `EMBED_FILES` → `_binary_index_html_start/end`.
* Routes:
  * `GET /` and `GET /index.html` → dashboard
  * `GET /api/state` → JSON snapshot
  * `POST /api/action` → `{"action":"feed|play|clean|sleep"}`
  * `GET /ws` → WebSocket, 1 Hz state push, accepts the same action JSON
* `WEB_MAX_OPEN_SOCKETS` is a **local define** (not `CONFIG_HTTPD_MAX_OPEN_SOCKETS`).
* WebSocket is enabled with `CONFIG_HTTPD_WS_SUPPORT=y`.
* `cJSON` builds the state payload (~768 bytes).

### button.c

* Active-low GPIO with internal pull-up (default GPIO0 = BOOT button).
* 20 ms poll, 40 ms debounce, 800 ms long-press threshold.
* Short press → `PET_ACTION_FEED`, long press → `PET_ACTION_PLAY`.

### web/index.html

* Pure HTML/CSS/JS, no images, no external fonts/CDN.
* CSS blob with morphing border-radius, blink animation, blush cheeks.
* State-driven classes: `.sad`, `.sleeping`, `.ghost`, `.hungry`.
* WebSocket client with 3 s auto-reconnect, REST fallback.

## Configuration

`Kconfig.projbuild` (string/int/bool only — **no float**):

| Option | Type | Default |
| --- | --- | --- |
| `CONFIG_BYTEPET_WIFI_SSID` | string | `""` |
| `CONFIG_BYTEPET_WIFI_PASS` | string | `""` |
| `CONFIG_BYTEPET_BUTTON_GPIO` | int | `0` |
| `CONFIG_BYTEPET_TICK_PERIOD_MS` | int | `1000` |

## Memory / flash

* Custom partition table `partitions.csv`: 1 MB app + 24 KB NVS.
* Web page is compiled into the app binary (embedded), so the image grows
  by roughly the size of `index.html` (~12 KB).

## Future hooks

* `pet_do_action()` is the single entry point for user intents — the planned
  LLM voice companion can call it directly.
* WebSocket protocol is JSON, so a mobile app / PWA can replace the bundled
  dashboard without firmware changes.
