#!/usr/bin/env bash
# Start OpenPal server
set -euo pipefail
cd "$(dirname "$0")/../server"

if [[ ! -d .venv ]]; then
  python3 -m venv .venv
fi
# shellcheck disable=SC1091
source .venv/bin/activate
pip install -q -r requirements.txt

# load .env if present
if [[ -f .env ]]; then
  set -a
  # shellcheck disable=SC1091
  source .env
  set +a
fi

echo "OpenPal → http://localhost:${PAL_PORT:-8080}"
exec python -m pal_server.main
