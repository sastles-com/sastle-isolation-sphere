#!/usr/bin/env bash
# サーバー⇔デバイス通信検証の一括実行スクリプト (実機 ESP32 不要)
#
#   1. ローカル mosquitto ブローカーを起動 (localhost:1883)
#   2. FastAPI サーバーを localhost ブローカー向けで起動 (SPHERE_MQTT_BROKER=localhost)
#   3. verify_server_comm.py でデバイスシミュレータによる双方向通信を検証
#   4. 起動した全プロセスを終了
#
# 使い方:  bash server/scripts/verify_server_comm.sh
# 前提:    brew install mosquitto / server/.venv (paho, websockets, fastapi, uvicorn)

set -uo pipefail

# server/ ディレクトリを基準にする
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SERVER_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$SERVER_DIR"

PY="$SERVER_DIR/.venv/bin/python"
MOSQ="$(command -v mosquitto || echo /opt/homebrew/sbin/mosquitto)"
HTTP_HOST=localhost
HTTP_PORT=8000
MQTT_PORT=1883

MOSQ_PID=""
SERVER_PID=""
LOGDIR="$(mktemp -d)"

cleanup() {
  echo ""
  echo "--- クリーンアップ ---"
  [[ -n "$SERVER_PID" ]] && kill "$SERVER_PID" 2>/dev/null && echo "server 停止 (pid $SERVER_PID)"
  [[ -n "$MOSQ_PID" ]] && kill "$MOSQ_PID" 2>/dev/null && echo "mosquitto 停止 (pid $MOSQ_PID)"
  wait 2>/dev/null
  echo "ログ: $LOGDIR"
}
trap cleanup EXIT

echo "=== 通信検証環境の起動 ==="

# 1. mosquitto ブローカー
if ! command -v "$MOSQ" >/dev/null 2>&1 && [[ ! -x "$MOSQ" ]]; then
  echo "mosquitto が見つかりません。'brew install mosquitto' を実行してください。"
  exit 1
fi
"$MOSQ" -p "$MQTT_PORT" >"$LOGDIR/mosquitto.log" 2>&1 &
MOSQ_PID=$!
echo "mosquitto 起動 (pid $MOSQ_PID, port $MQTT_PORT)"
sleep 1

# 2. FastAPI サーバー (localhost ブローカー向け)
SPHERE_MQTT_BROKER=localhost "$PY" -m uvicorn app.main:app \
  --host "$HTTP_HOST" --port "$HTTP_PORT" >"$LOGDIR/server.log" 2>&1 &
SERVER_PID=$!
echo "server 起動 (pid $SERVER_PID, http://$HTTP_HOST:$HTTP_PORT)"

# サーバーの /health が応答するまで待機 (最大15秒)
echo -n "server 起動待ち"
for i in $(seq 1 30); do
  if "$PY" - "$HTTP_HOST" "$HTTP_PORT" <<'PYEOF' 2>/dev/null
import socket, sys
s = socket.socket()
s.settimeout(0.5)
try:
    s.connect((sys.argv[1], int(sys.argv[2])))
    sys.exit(0)
except Exception:
    sys.exit(1)
PYEOF
  then echo " OK"; break; fi
  echo -n "."
  sleep 0.5
  if [[ $i -eq 30 ]]; then
    echo " タイムアウト"
    echo "--- server.log ---"; tail -20 "$LOGDIR/server.log"
    exit 1
  fi
done
sleep 1  # MQTT 購読確立の猶予

# 3. 検証実行
echo ""
"$PY" "$SCRIPT_DIR/verify_server_comm.py" \
  --broker localhost --mqtt-port "$MQTT_PORT" --http "$HTTP_HOST:$HTTP_PORT"
RESULT=$?

if [[ $RESULT -ne 0 ]]; then
  echo ""
  echo "--- server.log (末尾) ---"
  tail -25 "$LOGDIR/server.log"
fi

exit $RESULT
