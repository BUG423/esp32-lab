#!/usr/bin/env bash
set -euo pipefail

component_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
micropython_dir="${MICROPYTHON_DIR:?Set MICROPYTHON_DIR to a MicroPython v1.27.0 checkout}"
output_dir="${component_dir}/../binaries/generated"

if [[ ! -f "${micropython_dir}/py/mpconfig.h" ]]; then
  echo "MICROPYTHON_DIR is not a MicroPython source checkout" >&2
  exit 1
fi

if ! grep -q 'MICROPY_VERSION_MAJOR 1' "${micropython_dir}/py/mpconfig.h" ||
   ! grep -q 'MICROPY_VERSION_MINOR 27' "${micropython_dir}/py/mpconfig.h" ||
   ! grep -q 'MICROPY_VERSION_MICRO 0' "${micropython_dir}/py/mpconfig.h"; then
  echo "Expected MicroPython v1.27.0" >&2
  exit 1
fi

if ! command -v idf.py >/dev/null 2>&1; then
  echo "idf.py is not available. Install and export ESP-IDF v5.5.1 before building." >&2
  exit 1
fi

mkdir -p "${micropython_dir}/extmod/txd"
cp -R "${component_dir}/overlay/extmod/txd/." "${micropython_dir}/extmod/txd/"

make -C "${micropython_dir}/mpy-cross"
make -C "${micropython_dir}/ports/esp32" \
  BOARD=ESP32_GENERIC \
  FROZEN_MANIFEST="${component_dir}/manifest.py" \
  submodules
make -C "${micropython_dir}/ports/esp32" \
  BOARD=ESP32_GENERIC \
  FROZEN_MANIFEST="${component_dir}/manifest.py"

build_dir="${micropython_dir}/ports/esp32/build-ESP32_GENERIC"
mkdir -p "${output_dir}"
cp "${build_dir}/bootloader/bootloader.bin" "${output_dir}/bootloader_0x1000.bin"
cp "${build_dir}/partition_table/partition-table.bin" "${output_dir}/partition-table_0x8000.bin"
cp "${build_dir}/micropython.bin" "${output_dir}/micropython_0x10000.bin"

echo "Firmware written to ${output_dir}"
