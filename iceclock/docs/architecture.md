# IceClock Architecture / 架构设计

## Overview / 概览

IceClock separates hard real-time display refresh from network and UI work. The HUB75 scanner owns Core 1; ESP-IDF Wi-Fi/SNTP, RTC maintenance, and face rendering run independently.

```text
                    Wi-Fi AP / NTP server
                              │
                     ESP-IDF Wi-Fi + SNTP
                              │ system clock
                 ┌────────────┴────────────┐
                 │                         │
          DS3231 over I2C           clock_face.c
       boot restore / NTP save     time/date/status/snow
                 │                         │ back buffer
                 └────────────┬────────────┘
                              ▼
                       hub75.c (Core 1)
                 row scan + RGB bit-plane PWM
                              │
                         HUB75 panel
```

## Boot and synchronization sequence / 启动与校时

1. Initialize NVS and start the HUB75 refresh task with a boot frame.
2. Configure the POSIX timezone.
3. Probe DS3231. If its oscillator-stop flag is clear, restore system time from it.
4. Otherwise seed the clock from the firmware build timestamp so the UI remains usable.
5. If an SSID is configured, start station mode and SNTP.
6. On each SNTP callback, mark the source as synchronized and queue a local-time write to DS3231.

DS3231 stores local civil time in this implementation. Changing `ICECLOCK_TIMEZONE` therefore requires a successful NTP sync (or manually setting the RTC) before the stored value represents the new zone.

## Display pipeline / 显示链路

The UI writes only to the back RGB888 framebuffer. `hub75_present()` requests a swap; the refresh task applies it at the next complete PWM frame boundary. This avoids scanning a buffer while it is being redrawn.

For each PWM bit plane and row pair, the refresh task:

1. blanks output (`OE=1`);
2. selects row address A–D/E;
3. shifts upper and lower RGB bits;
4. latches the row;
5. enables output for a binary-weighted interval.

Only the most significant `CONFIG_ICECLOCK_COLOR_DEPTH` bits of each RGB channel are scanned. Global brightness scales the output-enable interval without rewriting the framebuffer.

## Task model / 任务模型

| Context | Responsibility | Priority/core |
|---|---|---|
| `hub75_refresh` | GPIO scan and bit-plane PWM | priority 20, Core 1 |
| `app_main` | face render, brightness schedule, chime | normal app task, Core 0 |
| `rtc_update` | deferred NTP → DS3231 write | priority 4 |
| ESP-IDF system tasks | Wi-Fi, IP, SNTP callbacks | ESP-IDF managed |

The SNTP callback never performs I2C work. It sets a flag and lets `rtc_update` execute the blocking transaction in a normal task context.

## Failure behavior / 故障行为

| Failure | Behavior |
|---|---|
| Wi-Fi not configured or unavailable | Clock continues from RTC/system time; reconnect attempts continue when configured |
| NTP unavailable | The active source remains RTC or build time |
| DS3231 missing / OSF set | Firmware logs a warning and continues without holdover |
| Invalid/duplicate HUB75 pins | Display initialization fails fast before enabling the scanner |
| Panel uses a non-linear scan map | Image may be scrambled; a panel-specific mapper is required |
