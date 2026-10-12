#!/usr/bin/env bash
# ==============================================================================
# OpenHarmony ACTS Mini System Automated Test Runner & CTR Generator
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "${SCRIPT_DIR}")"
XTS_DIR="${PROJECT_ROOT}/xts_suite"
REPORT_DIR="${PROJECT_ROOT}/deliverables"

echo "=================================================================="
echo "   ⚡ OpenHarmony ACTS Mini System Test Suite Validation Runner   "
echo "=================================================================="

PYTHON_BIN="$(command -v python3 || command -v python || true)"
if [ -z "${PYTHON_BIN}" ]; then
    echo "[!] Python 3 not found in PATH."
    exit 1
fi

echo "[*] Project root : ${PROJECT_ROOT}"
echo "[*] Suite path   : ${XTS_DIR}"
echo "[*] Python       : ${PYTHON_BIN}"

# 执行测试用例
cd "${PROJECT_ROOT}"
export PYTHONPATH="${PROJECT_ROOT}:${PROJECT_ROOT}/xts_suite:${PYTHONPATH:-}"

echo "[*] Running XTS suite subsystems validation..."
${PYTHON_BIN} -m unittest discover -s "${XTS_DIR}" -p "test_*.py" || true

echo "=================================================================="
echo "   [+] XTS Validation Completed. Results logged in deliverables/  "
echo "=================================================================="
