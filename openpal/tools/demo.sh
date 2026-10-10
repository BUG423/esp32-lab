#!/usr/bin/env bash
# OpenPal demo script — walks through the fun bits for a screen recording.
set -euo pipefail

BASE="${1:-http://127.0.0.1:8080}"

say() { printf "\n\033[1;36m▶ %s\033[0m\n" "$1"; }

say "OpenPal demo → $BASE"
curl -s "$BASE/api/status" | python3 -m json.tool

for q in "你好" "你是谁？" "现在几点了？" "讲个笑话" "12*13" "记住买牛奶" "北京天气怎么样？" "开灯"; do
  say "USER: $q"
  curl -s -X POST "$BASE/api/text" \
    -H 'Content-Type: application/json' \
    -d "{\"text\":\"$q\"}" \
    | python3 -c "import sys,json; d=json.load(sys.stdin); print('PAL :', d.get('reply','')); e=d.get('emotion',{}); print('情绪:', e.get('emoji',''), e.get('emotion',''))"
  sleep 0.4
done

say "打开浏览器体验完整语音闭环: $BASE"
