#!/usr/bin/env bash
# ==============================================================================
# OpenHarmony Mini System on ESP32 - Firmware Build & Release Packaging Script
# Automated build, binary extraction, hashing, and archive generation.
# ==============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
FIRMWARE_DIR="${PROJECT_ROOT}/firmware"
DELIVERABLES_DIR="${PROJECT_ROOT}/deliverables"
PACKAGE_VERSION="v1.0.0"
ARCHIVE_BASE_NAME="openharmony_esp32_firmware_${PACKAGE_VERSION}"
TARGET_CHIP="${IDF_TARGET:-esp32}"

echo "=================================================================="
echo "🚀 OpenHarmony Mini System 固件构建与镜像打包发布"
echo "版本: ${PACKAGE_VERSION} | 目标芯片: ${TARGET_CHIP}"
echo "工程路径: ${FIRMWARE_DIR}"
echo "交付物目录: ${DELIVERABLES_DIR}"
echo "=================================================================="

# 1. 确保并加载 ESP-IDF 环境
setup_idf() {
    if command -v idf.py >/dev/null 2>&1; then
        echo "✓ 检测到现有 ESP-IDF 环境: $(idf.py --version 2>/dev/null || echo 'OK')"
        return 0
    fi

    local idf_candidates=(
        "${PROJECT_ROOT}/../../../archive_esp32/esp/esp-idf/export.sh"
        "${PROJECT_ROOT}/../../esp/esp-idf/export.sh"
        "${PROJECT_ROOT}/../esp/esp-idf/export.sh"
        "${IDF_PATH:-}/export.sh"
        "${HOME}/esp/esp-idf/export.sh"
        "${HOME}/esp-idf/export.sh"
    )

    for candidate in "${idf_candidates[@]}"; do
        if [[ -n "${candidate}" && -f "${candidate}" ]]; then
            echo "→ 加载 ESP-IDF 交叉编译环境: ${candidate}"
            # shellcheck disable=SC1090
            source "${candidate}" >/dev/null 2>&1
            if command -v idf.py >/dev/null 2>&1; then
                echo "✓ ESP-IDF 环境加载成功 ($(idf.py --version 2>/dev/null || echo 'OK'))"
                return 0
            fi
        fi
    done

    echo "❌ 错误: 未找到有效 ESP-IDF 环境，请配置 IDF_PATH 并先执行 export.sh" >&2
    exit 1
}

setup_idf

# 2. 执行固件编译
echo ""
echo "------------------------------------------------------------------"
echo "🔨 正在编译 OpenHarmony 轻量系统适配固件..."
echo "------------------------------------------------------------------"

BUILD_START=$(date +%s)
cd "${FIRMWARE_DIR}"
if [[ ! -f "${FIRMWARE_DIR}/sdkconfig" ]]; then
    idf.py set-target "${TARGET_CHIP}"
fi
idf.py build
BUILD_END=$(date +%s)
DURATION=$((BUILD_END - BUILD_START))
echo "✅ 固件编译完成! 耗时: ${DURATION}s"

# 3. 提取产物并建立发布目录
BUILD_DIR="${FIRMWARE_DIR}/build"
RELEASE_STAGING="${DELIVERABLES_DIR}/${ARCHIVE_BASE_NAME}"
rm -rf "${RELEASE_STAGING}"
mkdir -p "${RELEASE_STAGING}" "${DELIVERABLES_DIR}"

echo ""
echo "------------------------------------------------------------------"
echo "📦 正在提取镜像与生成归档包..."
echo "------------------------------------------------------------------"

BOOTLOADER_BIN="${BUILD_DIR}/bootloader/bootloader.bin"
PARTITION_BIN="${BUILD_DIR}/partition_table/partition-table.bin"
APP_BIN="${BUILD_DIR}/openharmony_cert_firmware.bin"
OTA_BIN="${BUILD_DIR}/ota_data_initial.bin"

if [[ ! -f "${BOOTLOADER_BIN}" || ! -f "${PARTITION_BIN}" || ! -f "${APP_BIN}" ]]; then
    echo "❌ 错误: 关键编译镜像未找到！" >&2
    exit 1
fi

cp "${BOOTLOADER_BIN}" "${RELEASE_STAGING}/bootloader.bin"
cp "${PARTITION_BIN}" "${RELEASE_STAGING}/partition-table.bin"
cp "${APP_BIN}" "${RELEASE_STAGING}/openharmony_cert_firmware.bin"
if [[ -f "${OTA_BIN}" ]]; then
    cp "${OTA_BIN}" "${RELEASE_STAGING}/ota_data_initial.bin"
fi
if [[ -f "${BUILD_DIR}/flash_args" ]]; then
    cp "${BUILD_DIR}/flash_args" "${RELEASE_STAGING}/flash_args"
fi

# 写入一键烧录脚本到发布包
cat << 'FLASHSCRIPT' > "${RELEASE_STAGING}/flash_firmware.sh"
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
FLASHSCRIPT
chmod +x "${RELEASE_STAGING}/flash_firmware.sh"

# 4. 生成归档压缩包 (tar.gz & zip)
cd "${DELIVERABLES_DIR}"
TAR_GZ_FILE="${ARCHIVE_BASE_NAME}.tar.gz"
ZIP_FILE="${ARCHIVE_BASE_NAME}.zip"

rm -f "${TAR_GZ_FILE}" "${ZIP_FILE}"
tar -czf "${TAR_GZ_FILE}" "${ARCHIVE_BASE_NAME}"
if command -v zip >/dev/null 2>&1; then
    zip -rq "${ZIP_FILE}" "${ARCHIVE_BASE_NAME}"
else
    python3 -m zipfile -c "${ZIP_FILE}" "${ARCHIVE_BASE_NAME}"
fi

# 5. 计算 SHA256 校验和
echo ""
echo "------------------------------------------------------------------"
echo "🔒 计算 SHA256 镜像与发布包校验和..."
echo "------------------------------------------------------------------"

CHECKSUM_FILE="${DELIVERABLES_DIR}/SHA256SUMS"
cd "${DELIVERABLES_DIR}"
sha256sum \
    "${ARCHIVE_BASE_NAME}/bootloader.bin" \
    "${ARCHIVE_BASE_NAME}/partition-table.bin" \
    "${ARCHIVE_BASE_NAME}/openharmony_cert_firmware.bin" \
    "${TAR_GZ_FILE}" \
    "${ZIP_FILE}" > "${CHECKSUM_FILE}"

cat "${CHECKSUM_FILE}"

echo ""
echo "=================================================================="
echo "🎉 交付包打包完成！"
echo "发布目录: ${DELIVERABLES_DIR}"
echo "归档包 1: ${DELIVERABLES_DIR}/${TAR_GZ_FILE} ($(stat -c %s "${TAR_GZ_FILE}" 2>/dev/null || echo 0) 字节)"
echo "归档包 2: ${DELIVERABLES_DIR}/${ZIP_FILE} ($(stat -c %s "${ZIP_FILE}" 2>/dev/null || echo 0) 字节)"
echo "校验清单: ${CHECKSUM_FILE}"
echo "=================================================================="
