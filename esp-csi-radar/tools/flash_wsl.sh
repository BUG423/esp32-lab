#!/usr/bin/env bash
# Flash esp-csi-radar from WSL2 (requires usbipd on Windows side).
#
# Usage:
#   ./tools/flash_wsl.sh            # auto-detect port
#   ./tools/flash_wsl.sh /dev/ttyUSB0
set -euo pipefail

PORT="${1:-}"
cd "$(dirname "$0")/../firmware"

if [[ -z "${PORT}" ]]; then
  for p in /dev/ttyUSB0 /dev/ttyUSB1 /dev/ttyACM0; do
    if [[ -e "$p" ]]; then PORT="$p"; break; fi
  done
fi

if [[ -z "${PORT}" ]]; then
  echo "No serial port found."
  echo "WSL2: on Windows PowerShell (admin) run:"
  echo "  usbipd list"
  echo "  usbipd bind --busid <BUSID>"
  echo "  usbipd attach --wsl --busid <BUSID>"
  exit 1
fi

# shellcheck disable=SC1091
source "$HOME/esp/esp-idf/export.sh" > /dev/null 2>&1

idf.py -p "$PORT" flash monitor
