> [English](system_architecture.md) · **日本語**

# Isolation Sphere システムアーキテクチャ

最終更新: 2025-12-02

## 概要

Isolation Sphereは、ESP32ベースの球体ディスプレイとPythonバックエンドサーバーで構成される、リアルタイムIMU姿勢可視化システムです。

## システム構成

```
┌─────────────────┐      MQTT        ┌──────────────────┐
│   ESP32 Device  │◄────────────────►│  MQTT Broker     │
│   (M5Atom S3R)  │  (192.168.49.1)  │  (mosquitto)     │
│                 │                   └──────────────────┘
│  - IMU (BNO055) │                            │
│  - LED (800個)  │                            │
│  - LCD Display  │                            │
└─────────────────┘                            │
                                               │ MQTT Subscribe
                                               ▼
                                    ┌──────────────────────┐
                                    │  Python Server       │
                                    │  (FastAPI)           │
                                    │                      │
                                    │  - MQTT Service      │
                                    │  - State Manager     │
                                    │  - WebSocket Server  │
                                    └──────────────────────┘
                                               │
                                               │ WebSocket
                                               ▼
                                    ┌──────────────────────┐
                                    │  Web Frontend        │
                                    │  (React + Three.js)  │
                                    │                      │
                                    │  - HoloSphere (3D)   │
                                    │  - Dashboard UI      │
                                    └──────────────────────┘
```

## データフロー

### IMU姿勢データの流れ

1. **ESP32 (送信側)**
   - IMUセンサー（BNO055）から姿勢データ取得
   - Quaternion形式に変換: `{w, x, y, z}`
   - MQTTトピック `sphere/sphere001/imu` に送信
   - 送信頻度: 約10Hz（継続的）

2. **MQTT Broker**
   - ブローカーアドレス: `192.168.49.1:1883`
   - トピック: `sphere/sphere001/imu`
   - QoS: 0 (最新データ優先)

3. **Python Server (中継処理)**
   - **MQTTService**: ブローカーからサブスクライブ
   - **データフォーマット**: `{"w":0.707,"x":0.707,"y":0.0,"z":0.0}`
   - **StateManager**: 状態を保存
   - **WebSocket**: 接続中のクライアントに配信

4. **Web Frontend (表示)**
   - **WebSocketContext**: リアルタイム受信
   - **HoloSphere Component**: Quaternionを適用
   - **Three.js**: 3D球体をリアルタイム回転

## 通信プロトコル

### MQTT Topics

| Topic | Direction | Format | Description |
|-------|-----------|--------|-------------|
| `sphere/{id}/imu` | ESP32 → Server | `{"w":float,"x":float,"y":float,"z":float}` | IMU姿勢データ |
| `sphere/{id}/status` | ESP32 → Server | `{"status":string,"timestamp":string}` | デバイス状態 |

### WebSocket Messages

| Type | Direction | Payload | Description |
|------|-----------|---------|-------------|
| `STATE_UPDATE` | Server → Client | `{"imu":{...},"playback":{...},"params":{...}}` | 状態更新 |
| `SET_PLAYBACK` | Client → Server | `{"isPlaying":bool}` | 再生制御 |
| `SET_PARAMS` | Client → Server | `{"brightness":int,"speed":int,"hue":int}` | パラメータ設定 |

### REST API Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/config` | GET | システム設定取得 |
| `/api/playlist/playlists` | GET | プレイリスト一覧 |
| `/api/playlist/playlists` | POST | プレイリスト作成 |
| `/ws` | WebSocket | リアルタイム通信 |
| `/health` | GET | ヘルスチェック |

## 設定ファイル

### config.json (共有設定)

場所: `core/data/config.json` (ESP32とServer共通)

```json
{
  "wifi": {
    "SSID": "ESP32-P2P-Direct",
    "password": "isolation-sphere-p2p",
    "broker": "192.168.49.1",
    "mqtt_port": 1883
  },
  "sphere": {
    "id": "sphere001",
    "mac": "F0:9E:9E:32:67:D0",
    "features": {
      "IMU": "BNO055",
      "LED": true
    }
  }
}
```

## 技術スタック

### ESP32 Firmware
- **Platform**: ESP32-S3 (M5Atom S3R)
- **Framework**: Arduino
- **Libraries**:
  - PubSubClient (MQTT)
  - Adafruit BNO055 (IMU)
  - FastLED (LED制御)
  - M5Unified (Display)
  - ArduinoJson (設定管理)

### Python Server
- **Framework**: FastAPI 
- **Libraries**:
  - paho-mqtt (MQTT client)
  - uvicorn (ASGI server)
- **特徴**:
  - 非同期処理
  - WebSocket サポート
  - 自動リロード (開発時)

### Web Frontend
- **Framework**: React
- **Libraries**:
  - Three.js + @react-three/fiber (3D描画)
  - Material-UI (UIコンポーネント)
  - react-swipeable (ジェスチャー)
- **ビルド**: Vite

## デプロイ

### サーバー起動
```bash
cd server
python3 -m uvicorn app.main:app --reload --host 0.0.0.0 --port 9000
```

### フロントエンドビルド
```bash
cd server/frontend
npm run build
```

ビルド成果物は `server/frontend/dist/` に生成され、FastAPIが静的ファイルとして配信します。

## ネットワーク構成

- **WiFi SSID**: ESP32-P2P-Direct
- **サーバーIP**: 192.168.49.1 (WiFiアクセスポイント)
- **MQTTポート**: 1883
- **HTTPポート**: 9000
- **WebSocketポート**: 9000 (同一ポート)

## 実装済み機能

### ESP32
- ✅ IMUセンサー初期化とQuaternion取得
- ✅ MQTT接続とデータ送信
- ✅ LED制御 (FastLED)
- ✅ LCD表示 (デバッグ情報)
- ✅ 設定ファイル読み込み (config.json)

### Python Server
- ✅ MQTT接続とサブスクライブ
- ✅ WebSocket リアルタイム配信
- ✅ REST API (config, playlist)
- ✅ StateManager (状態管理)
- ✅ config.json からブローカーアドレス読み込み
- ✅ ESP32フォーマット対応
- ✅ asyncio イベントループ対応

### Web Frontend
- ✅ WebSocket接続 (動的ポート)
- ✅ 3D球体表示 (Three.js)
- ✅ IMU Quaternion適用
- ✅ リアルタイム姿勢同期
- ✅ ダッシュボードUI
- ✅ レスポンシブデザイン
- ✅ スワイプナビゲーション

## 今後の課題

- [ ] LED映像ストリーミング (UDP)
- [ ] プレイリスト再生機能
- [ ] 設定画面からのMQTT設定変更
- [ ] パフォーマンス最適化
- [ ] エラーハンドリング強化
