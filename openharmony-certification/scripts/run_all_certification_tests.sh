#!/usr/bin/env bash
# ==============================================================================
# OpenHarmony ACTS (Mini System) Full Certification Test Suite Runner for ESP32
# One-click execution, headless automated verification, strict error code exit.
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

# ANSI Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
BOLD='\033[1m'
NC='\033[0m' # No Color

OUTPUT_DIR="${PROJECT_ROOT}/deliverables"
FORMAT="all"
EXTRA_ARGS=""

echo -e "${CYAN}${BOLD}"
echo "================================================================================"
echo "  ⚡ OpenHarmony Mini System XTS/ACTS 自动化认证测试套件 (ESP32)"
echo "  Target: ESP32 Xtensa Dual-Core 240MHz / OpenHarmony 4.1.0 Mini System"
echo "================================================================================"
echo -e "${NC}"

# Parse optional arguments
while [[ $# -gt 0 ]]; do
  case $1 in
    --output-dir)
      OUTPUT_DIR="$2"
      shift 2
      ;;
    --format)
      FORMAT="$2"
      shift 2
      ;;
    --module)
      EXTRA_ARGS="${EXTRA_ARGS} --module $2"
      shift 2
      ;;
    --quiet)
      EXTRA_ARGS="${EXTRA_ARGS} --quiet"
      shift
      ;;
    --help|-h)
      echo "用法: $0 [--output-dir <path>] [--format <all|md|json|html>] [--module <subsystem>]"
      exit 0
      ;;
    *)
      EXTRA_ARGS="${EXTRA_ARGS} $1"
      shift
      ;;
  esac
done

# Step 1: Check Python environment
echo -e "${BLUE}[STEP 1/3] 检查 Python 运行环境与依赖项...${NC}"
if ! command -v python3 &>/dev/null; then
  echo -e "${RED}[ERROR] 未检测到 python3，请先安装 Python 3.10+。${NC}"
  exit 1
fi

PY_VER=$(python3 -c "import sys; print('.'.join(map(str, sys.version_info[:3])))")
echo -e "  ✓ Python 版本: ${GREEN}${PY_VER}${NC}"

# Step 2: Execute Test Suite via xts_runner.py
echo -e "\n${BLUE}[STEP 2/3] 执行 OpenHarmony XTS 8 大核心子系统全量测试用例...${NC}"
RUNNER_PATH="${PROJECT_ROOT}/xts_suite/runner/xts_runner.py"

if [[ ! -f "${RUNNER_PATH}" ]]; then
  echo -e "${RED}[ERROR] 找不到测试运行器: ${RUNNER_PATH}${NC}"
  exit 1
fi

export PYTHONPATH="${PROJECT_ROOT}:${PYTHONPATH:-}"

TEST_START_TIME=$(date +%s)

if python3 "${RUNNER_PATH}" --all --output-dir "${OUTPUT_DIR}" --format "${FORMAT}" ${EXTRA_ARGS}; then
  TEST_EXIT_CODE=0
  echo -e "${GREEN}${BOLD}✓ 全量 XTS 测试执行完毕，所有用例全部通过 (100% PASS)！${NC}"
else
  TEST_EXIT_CODE=$?
  echo -e "${RED}${BOLD}✗ 测试套件执行失败，存在未通过用例 (Exit code: ${TEST_EXIT_CODE})${NC}"
  exit ${TEST_EXIT_CODE}
fi

TEST_END_TIME=$(date +%s)
ELAPSED=$((TEST_END_TIME - TEST_START_TIME))

# Step 3: Validate Deliverables Output
echo -e "\n${BLUE}[STEP 3/3] 校验 OpenHarmony 兼容性测试报告 (CTR) 交付物...${NC}"

EXPECTED_FILES=(
  "${OUTPUT_DIR}/CTR_COMPATIBILITY_TEST_REPORT.md"
  "${OUTPUT_DIR}/CTR_COMPATIBILITY_TEST_REPORT.json"
  "${OUTPUT_DIR}/CTR_COMPATIBILITY_TEST_REPORT.html"
)

MISSING_DELIVERABLE=0
for f in "${EXPECTED_FILES[@]}"; do
  if [[ -f "$f" ]]; then
    SIZE=$(stat -c%s "$f")
    echo -e "  ✓ 找到报告: ${GREEN}$(basename "$f")${NC} (${SIZE} bytes)"
  else
    echo -e "  ✗ 缺失报告: ${RED}$f${NC}"
    MISSING_DELIVERABLE=1
  fi
done

if [[ ${MISSING_DELIVERABLE} -ne 0 ]]; then
  echo -e "${RED}[ERROR] 关键认证报告未完整生成！${NC}"
  exit 2
fi

echo -e "\n${GREEN}${BOLD}================================================================================"
echo "  🎉 ESP32 OpenHarmony Mini System 兼容性认证全流程验证成功！"
echo "  - 认证状态: COMPLIANT & CERTIFIED"
echo "  - 交付目录: ${OUTPUT_DIR}"
echo "  - 执行总耗时: ${ELAPSED}s"
echo "================================================================================${NC}\n"

exit 0
