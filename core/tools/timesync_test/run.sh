#!/usr/bin/env bash
# TimeSync ロジックのホスト単体テスト (実機・broker 不要)。
# millis() をシムで差し替え、実際の core/src/TimeSync.cpp をそのままリンクして検証する。
#
# 使い方:  core/ で一度 `pio run` してから (ArduinoJson が .pio/libdeps に落ちる)、
#          bash tools/timesync_test/run.sh
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
CORE_DIR="$(cd "$HERE/../.." && pwd)"   # .../core
SRC_DIR="$CORE_DIR/src"

# ArduinoJson ヘッダ (.pio/libdeps/<env>/ArduinoJson/src) を自動探索
AJ_SRC="$(find "$CORE_DIR/.pio/libdeps" -maxdepth 3 -type d -name src -path '*ArduinoJson*' 2>/dev/null | head -1 || true)"
if [ -z "$AJ_SRC" ]; then
    echo "ERROR: ArduinoJson が見つかりません。先に core/ で 'pio run' を実行してください。" >&2
    exit 1
fi

OUT="$HERE/test_timesync"
g++ -std=c++14 -Wall \
    -I "$HERE/shim" \
    -I "$SRC_DIR" \
    -I "$AJ_SRC" \
    "$HERE/test_timesync.cpp" "$SRC_DIR/TimeSync.cpp" \
    -o "$OUT"

"$OUT"
