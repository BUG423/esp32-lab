# Expansion-board firmware

The former ESP32-Factory repository vendored the complete MicroPython source tree. This component keeps only the project-specific TXD overlay and pins the upstream base to **MicroPython v1.27.0**.

## Build

```bash
git clone --branch v1.27.0 --depth 1 https://github.com/micropython/micropython.git
MICROPYTHON_DIR="$PWD/micropython" bash firmware/expansion-board/build.sh
```

Install and export ESP-IDF v5.5.1 first. It is the recommended upstream toolchain for MicroPython 1.27.0.

The script copies the overlay into the temporary upstream checkout, builds `mpy-cross` and the ESP32 port, and writes images to `firmware/binaries/generated/`. It never modifies files outside `MICROPYTHON_DIR` and this component's output directory.

## Provenance

- Source repository: `BUG423/ESP32-Factory`
- Upstream base detected from `mpconfig.h`: MicroPython 1.27.0
- Custom content: `extmod/txd` and a composed frozen manifest
- The manifest freezes every Python file in `txd`; an empty file list previously excluded all hardware helpers
