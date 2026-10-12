#!/usr/bin/env bash
set -euo pipefail
PORT="${1:-/dev/ttyUSB0}"
BAUD="${2:-460800}"
echo "🚀 正在烧录 OpenHarmony Mini System 固件到 ESP32 (${PORT} @ ${BAUD})..."
python3 -m esptool --chip esp32 -p "${PORT}" -b "${BAUD}" \
    --before default_reset --after hard_reset write_flash \
    --flash_mode dio --flash_size 4MB --flash_freq 40m \
    0x1000 bootloader.bin \
    0x8000 partition-table.bin \
    0xe000 ota_data_initial.bin \
    0x20000 openharmony_cert_firmware.bin
echo "✅ 烧录完成！请使用串口监视器查看 OpenHarmony 启动日志 (波特率 115200)。"
