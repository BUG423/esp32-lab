#!/usr/bin/env bash
# ==============================================================================
# ESP32-内部 · 全固件一键编译脚本 / Batch Firmware Build Script
# Builds all lab firmwares relative to the ESP32-内部 root directory.
# ==============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
show_help() {
  cat << 'EOF'
用法 / Usage:
  ./build_all.sh [选项 / options] [项目名 / project_names...]

示例 / Examples:
  ./build_all.sh                     # 编译全部 9 个实验固件 / Build all 9 firmwares
  ./build_all.sh esp-csi-radar       # 仅编译 esp-csi-radar / Build specific project
  ./build_all.sh openpal glowlink    # 编译指定的多个项目 / Build specified projects
  ./build_all.sh --clean             # 清理所有编译产物并重新编译 / Clean & rebuild

可用项目列表 / Available Projects:
  - esp-csi-radar             WiFi 隔墙感知雷达 (WiFi CSI Presence Radar)
  - openpal                   本地优先 AI 语音搭子 (Local-first AI Companion)
  - glowlink                  LED 可见光对讲机 (Visible Light Transceiver)
  - airchat                   ESP-NOW 离网聊天终端 (Off-grid Mesh Chat)
  - bassline                  音频频谱 WS2812 律动灯 (Audio FFT Visualizer)
  - bytepet                   桌面电子宠物 (Desktop Virtual Pet)
  - deckpad                   BLE 蓝牙宏键盘 (BLE HID Macro Deck)
  - iceclock                  HUB75 冰晶像素时钟 (HUB75 Pixel Clock, ESP32-S3)
  - openharmony-certification 开源鸿蒙轻量系统认证固件 (OpenHarmony Mini System)
EOF
}

# 检查 ESP-IDF 环境 / Check ESP-IDF environment
setup_idf() {
  if command -v idf.py >/dev/null 2>&1; then
    echo "✓ 检测到现有 ESP-IDF 环境: $(idf.py --version 2>/dev/null || echo 'IDF installed')"
    return 0
  fi

  local idf_candidates=(
    "${SCRIPT_DIR}/../../archive_esp32/esp/esp-idf/export.sh"
    "${SCRIPT_DIR}/../esp/esp-idf/export.sh"
    "${IDF_PATH:-}/export.sh"
    "${HOME}/esp/esp-idf/export.sh"
    "${HOME}/esp-idf/export.sh"
  )

  for candidate in "${idf_candidates[@]}"; do
    if [[ -n "${candidate}" && -f "${candidate}" ]]; then
      echo "→ 自动加载 ESP-IDF 环境: ${candidate}"
      # shellcheck disable=SC1090
      source "${candidate}" >/dev/null 2>&1
      if command -v idf.py >/dev/null 2>&1; then
        echo "✓ ESP-IDF 环境加载成功 ($(idf.py --version 2>/dev/null || echo 'OK'))"
        return 0
      fi
    fi
  done

  echo "❌ 错误: 未检测到 ESP-IDF 环境且未能找到 export.sh！" >&2
  echo "请确保已安装 ESP-IDF v5.5.1，并在运行本脚本前执行:" >&2
  echo "  source /path/to/esp-idf/export.sh" >&2
  exit 1
}

CLEAN_BUILD=0
TARGET_PROJECTS=()

for arg in "$@"; do
  case "$arg" in
    -h|--help)
      show_help
      exit 0
      ;;
    -c|--clean)
      CLEAN_BUILD=1
      ;;
    *)
      TARGET_PROJECTS+=("$arg")
      ;;
  esac
done

ALL_PROJECTS=(esp-csi-radar openpal glowlink airchat bassline bytepet deckpad iceclock openharmony-certification)

if [[ ${#TARGET_PROJECTS[@]} -eq 0 ]]; then
  PROJECTS_TO_BUILD=("${ALL_PROJECTS[@]}")
else
  PROJECTS_TO_BUILD=("${TARGET_PROJECTS[@]}")
fi

setup_idf

echo "=================================================================="
echo "🚀 ESP32-内部 固件批量构建任务开始"
echo "目标芯片架构: ${IDF_TARGET:-按项目自动选择}"
echo "待编译项目: ${PROJECTS_TO_BUILD[*]}"
echo "=================================================================="

successful=()
failed=()

for p in "${PROJECTS_TO_BUILD[@]}"; do
  fw_dir="${SCRIPT_DIR}/${p}/firmware"
  if [[ ! -d "${fw_dir}" || ! -f "${fw_dir}/CMakeLists.txt" ]]; then
    echo "⚠️  跳过未知或非标准固件目录: ${p}"
    failed+=("${p} (目录不存在)")
    continue
  fi

  echo ""
  echo "------------------------------------------------------------------"
  echo "🔨 正在编译: [${p}]"
  echo "路径: ${fw_dir}"
  if [[ -n "${IDF_TARGET:-}" ]]; then
    project_target="${IDF_TARGET}"
  elif [[ "${p}" == "iceclock" ]]; then
    project_target="esp32s3"
  else
    project_target="esp32"
  fi
  echo "芯片: ${project_target}"
  echo "------------------------------------------------------------------"

  if [[ ${CLEAN_BUILD} -eq 1 ]]; then
    echo "→ 清理旧构建目录: ${fw_dir}/build"
    rm -rf "${fw_dir}/build"
  fi

  build_start=$(date +%s)
  if (cd "${fw_dir}" && { [[ -f "sdkconfig" ]] || idf.py set-target "${project_target}"; } && idf.py build); then
    build_end=$(date +%s)
    duration=$((build_end - build_start))

    # 寻找生成的二进制文件
    app_bin=$(find "${fw_dir}/build" -maxdepth 1 -name "${p}.bin" 2>/dev/null | head -n 1 || true)
    if [[ -z "${app_bin}" ]]; then
      app_bin=$(find "${fw_dir}/build" -maxdepth 1 -name "*.bin" ! -name "bootloader.bin" ! -name "partition-table.bin" ! -name "ota_data_initial.bin" 2>/dev/null | head -n 1 || true)
    fi

    if [[ -n "${app_bin}" && -f "${app_bin}" ]]; then
      bin_size=$(stat -c %s "${app_bin}" 2>/dev/null || stat -f %z "${app_bin}" 2>/dev/null || echo "0")
      echo "✅ [${p}] 编译成功! 耗时: ${duration}s | 固件: $(basename "${app_bin}") (${bin_size} 字节)"
      successful+=("${p} ($(basename "${app_bin}"), ${bin_size} 字节)")
    else
      echo "✅ [${p}] 编译成功! 耗时: ${duration}s"
      successful+=("${p}")
    fi
  else
    echo "❌ [${p}] 编译失败!"
    failed+=("${p}")
  fi
done

echo ""
echo "=================================================================="
echo "📊 编译任务结果汇总 / Build Summary"
echo "=================================================================="
echo "总计: ${#PROJECTS_TO_BUILD[@]} | 成功: ${#successful[@]} | 失败: ${#failed[@]}"

if [[ ${#successful[@]} -gt 0 ]]; then
  echo ""
  echo "成功项目:"
  for item in "${successful[@]}"; do
    echo "  ✓ ${item}"
  done
fi

if [[ ${#failed[@]} -gt 0 ]]; then
  echo ""
  echo "失败项目:"
  for item in "${failed[@]}"; do
    echo "  ✗ ${item}"
  done
  echo "=================================================================="
  exit 1
fi

echo "=================================================================="
echo "🎉 所有目标固件构建完毕！"
exit 0
