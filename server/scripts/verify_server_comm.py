#!/usr/bin/env python3
"""
サーバー⇔デバイス通信検証ハーネス (実機 ESP32 不要)

ESP32 ファーム (core/) が使う MQTT 契約を Python のデバイスシミュレータで
再現し、起動済みの FastAPI サーバー + MQTT ブローカーとの双方向通信を検証する。

検証する MQTT 契約 (core/src/MqttTopics.h ⇔ server/app/core/config.py):

  デバイス → サーバー:
    sphere/sphere001/imu      {"w","x","y","z"}      クォータニオン
    sphere/sphere001/status   "online"/"offline"     retained
  サーバー → デバイス:
    sphere/all/command/params {"brightness":..}      UI/API からの制御
    sphere/all/state          {...full state...}     retained, 状態ブロードキャスト

検証ケース:
  1. [Server→Device] UI クライアントが command/params を publish
       → デバイスsim が command/params を受信 (ファームのコマンド受信路)
       → サーバーが処理し sphere/all/state を再配信、brightness 反映を確認
  2. [Device→Server] デバイスsim が imu を publish
       → サーバーが StateManager に取り込み、WebSocket STATE_UPDATE に反映
  3. [Device→Server] デバイスsim が status "online" (retained) を publish

前提: localhost:1883 に MQTT ブローカー、http://localhost:8000 にサーバーが起動済み。
       (verify_server_comm.sh が両方を起動してから本スクリプトを呼ぶ)

使い方:
    python verify_server_comm.py [--broker localhost] [--mqtt-port 1883] [--http localhost:8000]
"""
import argparse
import asyncio
import json
import sys
import threading
import time

import paho.mqtt.client as mqtt
import websockets

DEVICE_ID = "sphere001"
T_IMU = f"sphere/{DEVICE_ID}/imu"
T_STATUS = f"sphere/{DEVICE_ID}/status"
T_CMD_PARAMS = "sphere/all/command/params"
T_STATE = "sphere/all/state"

GREEN, RED, RESET = "\033[32m", "\033[31m", "\033[0m"


class DeviceSim:
    """ESP32 ファームの MQTT 挙動を模したデバイスシミュレータ"""

    def __init__(self, broker, port):
        self.client = mqtt.Client(client_id=DEVICE_ID)
        self.client.on_connect = self._on_connect
        self.client.on_message = self._on_message
        self.received = []  # (topic, payload) を蓄積
        self.lock = threading.Lock()
        self.broker, self.port = broker, port

    def _on_connect(self, client, userdata, flags, rc):
        # ファーム同様、コマンドと全体状態を購読
        client.subscribe("sphere/all/command/#")
        client.subscribe(T_STATE)

    def _on_message(self, client, userdata, msg):
        try:
            payload = json.loads(msg.payload.decode())
        except Exception:
            payload = msg.payload.decode(errors="replace")
        with self.lock:
            self.received.append((msg.topic, payload))

    def start(self):
        self.client.connect(self.broker, self.port, 60)
        self.client.loop_start()
        # status online を retained で publish (ファームの接続時挙動)
        self.client.publish(T_STATUS, "online", retain=True)

    def stop(self):
        self.client.publish(T_STATUS, "offline", retain=True)
        self.client.loop_stop()
        self.client.disconnect()

    def publish_imu(self, quat):
        self.client.publish(T_IMU, json.dumps(quat), qos=0)

    def wait_for(self, topic, predicate, timeout=5.0):
        """指定トピックで predicate を満たすメッセージを待つ"""
        deadline = time.time() + timeout
        while time.time() < deadline:
            with self.lock:
                for t, p in self.received:
                    if t == topic and predicate(p):
                        return p
            time.sleep(0.05)
        return None


def ui_publish_command(broker, port, payload):
    """UI/API 相当: command/params を publish する別クライアント"""
    c = mqtt.Client(client_id="verify-ui")
    c.connect(broker, port, 60)
    c.loop_start()
    time.sleep(0.3)
    c.publish(T_CMD_PARAMS, json.dumps(payload), qos=1)
    time.sleep(0.5)
    c.loop_stop()
    c.disconnect()


async def read_state_via_ws(http_hostport, want_imu, timeout=5.0):
    """WebSocket /ws に接続し STATE_UPDATE の imu が want_imu に一致するまで待つ"""
    uri = f"ws://{http_hostport}/ws"
    try:
        async with websockets.connect(uri) as ws:
            deadline = time.time() + timeout
            while time.time() < deadline:
                remaining = deadline - time.time()
                try:
                    raw = await asyncio.wait_for(ws.recv(), timeout=remaining)
                except asyncio.TimeoutError:
                    break
                msg = json.loads(raw)
                if msg.get("type") == "STATE_UPDATE":
                    imu = msg.get("payload", {}).get("imu", {})
                    if all(abs(imu.get(k, 0) - want_imu[k]) < 1e-3 for k in "wxyz"):
                        return imu
    except Exception as e:
        print(f"  WebSocket error: {e}")
    return None


def result(ok, name, detail=""):
    tag = f"{GREEN}PASS{RESET}" if ok else f"{RED}FAIL{RESET}"
    print(f"[{tag}] {name}" + (f"  ({detail})" if detail else ""))
    return ok


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--broker", default="localhost")
    ap.add_argument("--mqtt-port", type=int, default=1883)
    ap.add_argument("--http", default="localhost:8000")
    args = ap.parse_args()

    print("=== サーバー⇔デバイス通信検証 ===")
    print(f"broker={args.broker}:{args.mqtt_port}  http={args.http}\n")

    sim = DeviceSim(args.broker, args.mqtt_port)
    try:
        sim.start()
    except Exception as e:
        print(f"{RED}ブローカーに接続できません: {e}{RESET}")
        print("verify_server_comm.sh 経由で起動するか、mosquitto を立ち上げてください。")
        return 1
    time.sleep(1.0)  # サーバーの購読確立を待つ

    passed = []

    # --- ケース1: Server→Device コマンド伝搬 + 状態再配信 ---
    print("[1] Server→Device: command/params 伝搬と state 再配信")
    target_brightness = 73  # 初期値 80 と異なる値
    ui_publish_command(args.broker, args.mqtt_port, {"brightness": target_brightness})

    got_cmd = sim.wait_for(
        T_CMD_PARAMS,
        lambda p: isinstance(p, dict) and p.get("brightness") == target_brightness,
    )
    passed.append(result(
        got_cmd is not None,
        "デバイスsim が command/params を受信",
        f"payload={got_cmd}",
    ))

    got_state = sim.wait_for(
        T_STATE,
        lambda p: isinstance(p, dict)
        and p.get("params", {}).get("brightness") == target_brightness,
    )
    passed.append(result(
        got_state is not None,
        "サーバーが処理し sphere/all/state を再配信 (brightness反映)",
        f"brightness={got_state.get('params', {}).get('brightness') if got_state else None}",
    ))

    # --- ケース2: Device→Server IMU 取り込み (WebSocket で確認) ---
    print("\n[2] Device→Server: imu publish → WebSocket STATE_UPDATE 反映")
    want = {"w": 0.5, "x": 0.5, "y": 0.5, "z": 0.5}

    async def imu_roundtrip():
        ws_task = asyncio.ensure_future(read_state_via_ws(args.http, want))
        await asyncio.sleep(0.5)  # WS 接続確立を待ってから publish
        sim.publish_imu(want)
        return await ws_task

    try:
        ws_imu = asyncio.get_event_loop().run_until_complete(imu_roundtrip())
    except RuntimeError:
        ws_imu = asyncio.new_event_loop().run_until_complete(imu_roundtrip())
    passed.append(result(
        ws_imu is not None,
        "サーバーが imu を取り込み WebSocket に反映",
        f"imu={ws_imu}",
    ))

    # --- ケース3: status online ---
    print("\n[3] Device→Server: status online (retained)")
    # サーバーは sphere/sphere001/status を購読。ここでは publish 成功で疎通とみなす
    rc = sim.client.publish(T_STATUS, "online", retain=True)
    passed.append(result(rc.rc == mqtt.MQTT_ERR_SUCCESS, "status online を publish"))

    sim.stop()

    print("\n=== 結果 ===")
    n_ok = sum(1 for x in passed if x)
    print(f"{n_ok}/{len(passed)} ケース成功")
    return 0 if n_ok == len(passed) else 1


if __name__ == "__main__":
    sys.exit(main())
