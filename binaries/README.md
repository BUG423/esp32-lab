# Firmware binaries

These images were imported from ESP32-Factory and retain their flash offsets in the filenames.
The imported `micropython_0x10000.bin` is currently an empty placeholder and must not be flashed.

| File | Flash offset |
| --- | --- |
| `bootloader_0x1000.bin` | `0x1000` |
| `partition-table_0x8000.bin` | `0x8000` |
| `micropython_0x10000.bin` | `0x10000` |

Run `npm run build:firmware` with `MICROPYTHON_DIR` pointing to a MicroPython v1.27.0 checkout and ESP-IDF v5.5.1 exported to create reproducible images under the Git-ignored `generated/` directory. The unified portal automatically prefers a complete generated set and embeds it as the desktop updater's verified offline fallback. If neither remote nor complete local firmware exists, flashing stays disabled.

Use a compatible ESP32 flashing tool and verify the target board before writing. Promote generated images to this directory only after hardware validation.

After hardware validation, run `npm run release:firmware -- --version X.Y.Z` to generate a release package under `dist/firmware-release`. Pushing a `firmware-vX.Y.Z` tag rebuilds from the pinned upstream baselines and publishes the resulting GitHub Release assets. Both the web updater and the Rust desktop updater resolve the `latest` release URL, so publishing firmware does not require rebuilding the application. The desktop updater verifies every image with SHA-256 and keeps both the last verified package and its bundled build-time firmware as fallbacks.
