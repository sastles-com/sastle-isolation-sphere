> [English](SPECIFICATION.md) · **日本語**

# Isolation Sphere Server 要求仕様書 v3.0

**⚠️ このドキュメントは最新版です。旧版（v2.x）のmicro-ROS関連記述は無効です。**

## 📌 最新アーキテクチャドキュメント

詳細な通信設計については以下を参照してください：
- **[通信アーキテクチャ設計](../docs/architecture/communication_design.md)** - 完全な通信仕様
- **[ROS2削除計画](../docs/architecture/ros2_removal_plan.md)** - 設計変更の経緯

---

## 1. システム概要

### 1.1 目的
ESP32との通信を**MQTT + UDP**に統一し、シンプルで保守しやすいアーキテクチャを実現する。

### 1.2 技術スタック

#### サーバー
- **OS**: Ubuntu 22.04 LTS
- **言語**: Python 3.10+
- **フレームワーク**: FastAPI
- **通信**:
  - MQTT (Mosquitto) - 制御・状態管理
  - UDP - 映像ストリーミング
  - WebSocket - UI同期

#### フロントエンド
- **フレームワーク**: React (Vite)
- **UI**: Material-UI, React Three Fiber
- **通信**: WebSocket

---

## 2. 通信プロトコル

### 2.1 MQTT

#### ブローカー設定
- Host: `192.168.49.1`
- Port: `1883`
- Protocol: MQTT v3.1.1

#### トピック設計

**コマンド系（Server → ESP32）**
```
sphere/all/command/params      # パラメータ変更
sphere/all/command/playback    # 再生制御
sphere/all/command/led         # LED制御
sphere/all/command/system      # システムコマンド
```

**状態系（ESP32 → Server）**
```
sphere/{device_id}/state       # 完全状態 (retained)
sphere/{device_id}/imu         # IMUデータ (10Hz)
sphere/{device_id}/status      # ステータス
```

### 2.2 UDP

- **用途**: 映像ストリーミング (Server → ESP32)
- **宛先**: `192.168.49.101:8889`
- **フォーマット**: JPEG (320x160, 10fps)
- **パケット**: Header (8 bytes) + JPEG data

### 2.3 WebSocket

- **エンドポイント**: `ws://[server]:9000/ws`
- **メッセージ**:
  - `STATE_UPDATE` (Server → UI)
  - `SET_PARAMS` (UI → Server)
  - `SET_PLAYBACK` (UI → Server)

---

## 3. アーキテクチャ

### 3.1 StateManager (中央集権)
- 唯一の状態保持者
- すべてのコマンドを処理
- MQTT/WebSocketへ配信

### 3.2 MQTT Service
- ESP32との双方向通信
- StateManagerとの連携

### 3.3 Video Daemon (実装予定)
- プレイリスト管理
- UDP映像配信

### 3.4 Joystick Daemon (実装予定)
- USB入力取得
- MQTT直接パブリッシュ

---

## 4. 実装状況

### ✅ 完了
- FastAPI サーバー
- StateManager
- MQTT通信
- WebSocket通信
- React WebUI

### 🚧 実装中
- Video Daemon

### 📋 予定
- Joystick Daemon

---

## 5. 参照ドキュメント

- [Communication Design](../docs/architecture/communication_design.md)
- [ROS2 Removal Plan](../docs/architecture/ros2_removal_plan.md)
- [README.md](../README.md)

