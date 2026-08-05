> [English](README_verify_comm.md) · **日本語**

# サーバー⇔デバイス通信検証

実機 ESP32 が手元になくても、サーバーの MQTT 通信契約を検証できる。
Python のデバイスシミュレータが ESP32 ファーム (core/) の MQTT 挙動を再現し、
ローカルの MQTT ブローカー + FastAPI サーバーと双方向通信して合否を判定する。

## 実行

```bash
bash server/scripts/verify_server_comm.sh
```

mosquitto と FastAPI サーバーを自動で起動・検証・停止する。前提:

- `brew install mosquitto`
- `server/.venv` に `paho-mqtt`, `websockets`, `fastapi`, `uvicorn`

## 検証する MQTT 契約

`core/src/MqttTopics.h` ⇔ `server/app/core/config.py` の対応を確認する。

| 方向 | トピック | ペイロード |
|---|---|---|
| デバイス→サーバー | `sphere/sphere001/imu` | `{"w","x","y","z"}` クォータニオン |
| デバイス→サーバー | `sphere/sphere001/status` | `"online"`/`"offline"` (retained) |
| サーバー→デバイス | `sphere/all/command/params` | `{"brightness":..}` 等 |
| サーバー→デバイス | `sphere/all/state` | 全状態 (retained, 状態ブロードキャスト) |

## 検証ケース (4件)

1. **Server→Device コマンド伝搬**: UI クライアントが `command/params {"brightness":73}`
   を publish → デバイスsim が受信 (ファームのコマンド受信路)。
2. **Server 状態再配信**: 上記コマンドをサーバーが処理し `sphere/all/state` を
   retained 再配信、`params.brightness=73` の反映を確認。
3. **Device→Server IMU 取り込み**: デバイスsim が `imu` を publish → サーバーが
   StateManager に取り込み、WebSocket `/ws` の `STATE_UPDATE` に反映されることを確認。
4. **status online**: デバイスsim が retained `online` を publish。

## ブローカーの差し替え

サーバーは環境変数 `SPHERE_MQTT_BROKER` でブローカーを上書きできる
(`config.json` を編集せずローカル検証や別環境にデプロイ可能)。
未指定時は `config.json` の `wifi.broker` → `localhost` の順で解決する。

```bash
SPHERE_MQTT_BROKER=localhost .venv/bin/python -m uvicorn app.main:app
```

## 実機 ESP32 での検証 (次段階)

本ハーネスはサーバー側契約の検証用。実機との結合確認は:

1. `core/data/config.json` の `wifi.broker` を実ブローカー IP に設定
2. ファームを書き込み、AtomS3R を WiFi 接続
3. 本ハーネスのデバイスsim を止め、実機の `sphere/sphere001/imu` 配信や
   `sphere/all/command/#` 受信をブローカー越しに観測
   (`mosquitto_sub -t 'sphere/#' -v` で全トピックを傍受可能)
