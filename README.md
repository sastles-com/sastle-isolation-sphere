# Isolation Sphere

**球体型LED ディスプレイシステム** - 800個のLEDを搭載した110mm径の球体ディスプレイとその制御システム

![Project Status](https://img.shields.io/badge/status-active-success.svg)
![Platform](https://img.shields.io/badge/platform-ESP32--S3%20%7C%20Ubuntu-blue.svg)

## 概要

Isolation Sphereは、M5Atom S3Rをベースとした球体型LEDディスプレイと、Ubuntu MiniPC/Raspberry Piで動作する制御サーバーで構成される統合システムです。リアルタイム映像表示、IMUによる姿勢補正、WebUIおよび物理ジョイスティックによる制御を実現します。

### 主な特徴

- 🎨 **800個のWS2812B LED** による高密度球体ディスプレイ
- 🎯 **IMU姿勢補正** (BNO055) による常に正立した映像表示
- 🌐 **WebUI制御** - スマホ/タブレット/PCから操作可能
- 🎮 **物理ジョイスティック対応** - USB接続による直感的操作
- 🚀 **micro-ROS統合** - リアルタイム通信フレームワーク
- 📡 **デュアルネットワーク** - WebUI用と専用ESP32通信の分離

## システム構成

```
┌─────────────────────────────────────────────────────────────┐
│                     Isolation Sphere System                  │
└─────────────────────────────────────────────────────────────┘
                              │
        ┌─────────────────────┼─────────────────────┐
        │                     │                     │
        ▼                     ▼                     ▼
┌───────────────┐    ┌────────────────┐    ┌──────────────┐
│   WebUI       │    │     Server     │    │  ESP32 Core  │
│  (Clients)    │    │  Ubuntu/RasPi  │    │  (M5Atom S3R)│
├───────────────┤    ├────────────────┤    ├──────────────┤
│• Smartphone   │───▶│• FastAPI       │───▶│• 800 LEDs    │
│• Tablet       │    │• React UI      │    │• IMU (BNO055)│
│• PC/Mac       │    │• ROS2 Humble   │    │• WiFi STA    │
│               │    │• micro-ROS     │    │• UDP/MQTT    │
│               │    │• Joystick      │    │• PSRAM 8MB   │
└───────────────┘    │  Daemon        │    └──────────────┘
                     │• Video Stream  │
                     └────────────────┘
```

## プロジェクト構成

```
isolation-sphere/
├── core/                    # ESP32 ファームウェア (PlatformIO)
│   ├── src/                 # C++ ソースコード
│   │   ├── main.cpp         # メインエントリポイント
│   │   ├── LEDManager.*     # LED制御 (FastLED, 800 LEDs)
│   │   ├── IMUManager.*     # IMU制御 (BNO055)
│   │   ├── ImageManager.*   # UDP画像受信・デコード
│   │   ├── NetworkManager.* # WiFi・UDP・MQTT通信
│   │   ├── GestureManager.* # ジェスチャー認識
│   │   ├── SoundManager.*   # サウンド制御
│   │   └── ConfigManager.*  # 設定管理 (JSON)
│   ├── data/                # LittleFS データ
│   │   ├── config.json      # WiFi、MQTT設定
│   │   ├── led_layout.csv   # LED座標マッピング
│   │   └── images/          # デモ画像データ
│   ├── doc/                 # ドキュメント
│   │   ├── dual_core_design.md       # デュアルコア設計
│   │   ├── image_manager_design.md   # 画像管理設計
│   │   ├── imu_compensation.md       # IMU補償設計
│   │   └── udp_image_protocol.md     # UDPプロトコル仕様
│   ├── platformio.ini       # PlatformIO設定
│   └── README.md            # Core README
│
├── server/                  # 制御サーバー (Python/Node.js)
│   ├── app/                 # FastAPI アプリケーション
│   │   ├── main.py          # サーバーエントリポイント
│   │   ├── api/             # REST API エンドポイント
│   │   ├── core/            # コア機能 (ROS2統合)
│   │   └── services/        # ビジネスロジック
│   ├── frontend/            # React WebUI (Vite)
│   │   ├── src/
│   │   │   ├── components/  # UIコンポーネント
│   │   │   ├── contexts/    # React Context
│   │   │   └── pages/       # ページコンポーネント
│   │   └── package.json
│   ├── joystick/            # ジョイスティックデーモン
│   │   ├── daemon.py        # メインデーモン
│   │   ├── device_manager.py
│   │   └── mapper.py        # 入力マッピング
│   ├── scripts/             # セットアップスクリプト
│   │   ├── setup_network.sh # AP設定
│   │   └── setup_services.sh# systemd設定
│   ├── docker/              # Docker構成
│   │   └── docker-compose.yml # micro-ROS Agent
│   ├── docs/                # サーバードキュメント
│   ├── pyproject.toml       # Python依存関係
│   └── README.md            # Server README
│
└── README.md                # このファイル
```

## クイックスタート

### 必要要件

#### ハードウェア
- **ESP32デバイス**: M5Atom S3R (ESP32-S3, 8MB Flash, 8MB PSRAM)
- **サーバー**: Ubuntu 22.04 MiniPC または Raspberry Pi 4+
- **ネットワーク**: USB WiFiアダプター (ESP32専用AP用)
- **オプション**: USB ジョイスティック

#### ソフトウェア
- **Core (ESP32)**:
  - PlatformIO Core または PlatformIO IDE
  - Python 3.7+ (PlatformIO用)

- **Server (Ubuntu)**:
  - Ubuntu 22.04 LTS
  - Python 3.10+
  - Node.js 18+
  - ROS2 Humble
  - Docker (micro-ROS Agent用)

### インストール

#### 1. ESP32 ファームウェアのビルド & フラッシュ

```bash
# coreディレクトリに移動
cd core

# PlatformIOでビルド & フラッシュ
pio run -t upload

# ファイルシステム (LittleFS) のアップロード
pio run -t uploadfs
```

詳細は [core/README.md](core/README.md) を参照してください。

#### 2. サーバーのセットアップ

```bash
# serverディレクトリに移動
cd server

# Python依存関係のインストール
pip install -e .

# フロントエンドのビルド
cd frontend
npm install
npm run build
cd ..

# ネットワーク設定 (AP Mode)
sudo scripts/setup_network.sh

# サービスの設定 (オプション)
sudo scripts/setup_services.sh
```

詳細は [server/README.md](server/README.md) を参照してください。

### 実行

#### 開発モード

```bash
# Terminal 1: micro-ROS Agent起動
cd server/docker
docker-compose up

# Terminal 2: FastAPIサーバー起動
cd server
uvicorn app.main:app --host 0.0.0.0 --port 9000 --reload

# Terminal 3: フロントエンド開発サーバー (オプション)
cd server/frontend
npm run dev

# Terminal 4: ジョイスティックデーモン (ジョイスティック接続時)
cd server
python -m joystick.daemon
```

#### 本番モード

```bash
# すべてのサービスを起動
sudo systemctl start isolation-sphere-server
sudo systemctl start isolation-sphere-joystick
sudo systemctl start micro-ros-agent
```

### アクセス

- **WebUI**: http://[server-ip]:9000 (例: http://192.168.1.100:9000)
- **mDNS**: http://[hostname].local:9000 (設定されている場合)
- **ESP32 AP**: SSID `IsolationSphere-Direct`, IP: 192.168.100.1

## 技術スタック

### ESP32 Core
- **言語**: C++17
- **フレームワーク**: Arduino (ESP32-S3)
- **主要ライブラリ**:
  - FastLED - LED制御
  - Adafruit_BNO055 - IMU制御
  - AsyncMqttClient - MQTT通信
  - JPEGDecoder - 画像デコード
  - ArduinoJson - JSON処理

### Server Backend
- **言語**: Python 3.10+
- **フレームワーク**: FastAPI (非同期Web)
- **通信**:
  - ROS2 Humble (rclpy)
  - micro-ROS Agent (Docker)
  - WebSocket (リアルタイム通信)
  - MQTT (paho-mqtt)

### Server Frontend
- **フレームワーク**: React 18 (Vite)
- **UIライブラリ**: Material-UI (MUI)
- **3Dグラフィックス**: React Three Fiber (Three.js)
- **状態管理**: React Context API
- **スタイリング**: Emotion (CSS-in-JS)

## 通信プロトコル

### micro-ROS (XRCE-DDS)
- **用途**: 主要な制御通信
- **トピック**:
  - `/joy_data` - ジョイスティック入力
  - `/video_control` - ビデオ制御コマンド
  - `/sphere/status` - デバイス状態

### UDP
- **用途**: 高速画像ストリーミング
- **ポート**: 8889 (ESP32受信)
- **フォーマット**: JPEG圧縮画像 (320x160, 10fps)

### MQTT
- **用途**: レガシー状態監視・コマンド
- **ブローカー**: 192.168.100.1:1883
- **トピック**:
  - `sphere/[device_id]/command` - コマンド受信
  - `sphere/[device_id]/status` - ステータス送信
  - `sphere/[device_id]/imu` - IMUデータ (10Hz)
  - `sphere/[device_id]/response` - コマンド応答

### WebSocket
- **用途**: WebUI リアルタイム通信
- **エンドポイント**: `ws://[server]:9000/api/ws`
- **データ形式**: JSON

## 機能

### 実装済み ✅
- ESP32ファームウェア基本機能 (WiFi, MQTT, UDP)
- LittleFS ファイルシステム管理
- IMU (BNO055) クォータニオン取得
- FastLED による800 LED制御
- UDP画像受信・JPEGデコード
- FastAPI Webサーバー
- React WebUI (Material-UI + Three.js)
- WebSocket リアルタイム通信
- ジョイスティックデーモン
- micro-ROS Agent統合
- デュアルWiFi構成 (WebUI + ESP32専用AP)

### 開発中 🚧
- ジェスチャー認識
- サウンド再生機能
- プレイリスト管理
- 高度な画像エフェクト
- パフォーマンス監視ダッシュボード

### 計画中 📋
- Bluetoothゲームパッド対応
- OTA (Over-The-Air) ファームウェア更新
- クラウド統合
- マルチデバイス同期

## ドキュメント

### Core (ESP32)
- [仕様書](core/spec.md)
- [デュアルコア設計](core/doc/dual_core_design.md)
- [画像マネージャー設計](core/doc/image_manager_design.md)
- [IMU補償設計](core/doc/imu_compensation.md)
- [UDPプロトコル仕様](core/doc/udp_image_protocol.md)
- [実装状況](core/doc/implementation_status.md)
- [クラス図](core/doc/class.md)
- [シーケンス図](core/doc/sequence.md)
- [ステート図](core/doc/state.md)
- [MQTT仕様](core/doc/mqtt.md)

### Server
- [アーキテクチャ](server/docs/architecture.md)
- [MQTTプロトコル仕様](server/docs/mqtt_spec.md)
- [ステート図](server/docs/state_diagram.md)
- [シーケンス図](server/docs/sequence_diagram.md)
- [クラス図](server/docs/class_diagram.md)
- [プロトコル仕様](server/docs/protocol_spec.md)
- [要求仕様書](server/requirements_specification.md)
- [Raspberry Piシステム仕様](server/raspi_system_specification.md)

### その他
- [フレームワークガイド](Flamework.md)
- [Ubuntu移行ガイド](migration_guide_ubuntu.md)

## トラブルシューティング

### ESP32が接続できない
```bash
# シリアルポートの確認
pio device list

# 設定ファイルの確認
cat core/data/config.json

# WiFi設定の更新後、再アップロード
pio run -t uploadfs
pio run -t upload
```

### サーバーが起動しない
```bash
# micro-ROS Agentの状態確認
docker ps

# FastAPIログ確認
journalctl -u isolation-sphere-server -f

# ポートの使用状況確認
sudo netstat -tulpn | grep 9000
```

### WebUIにアクセスできない
```bash
# ファイアウォールの確認
sudo ufw status
sudo ufw allow 9000/tcp

# mDNSサービスの確認
sudo systemctl status avahi-daemon
```

### LEDが点灯しない
```bash
# ESP32シリアルモニターでデバッグ
pio device monitor

# LED電源の確認 (5V, 十分な電流容量)
# GPIOピンの接続確認 (5,6,7,8)
```

## 開発

### コードスタイル

#### C++ (Core)
```bash
# clang-format使用
cd core
find src -name "*.cpp" -o -name "*.h" | xargs clang-format -i
```

#### Python (Server)
```bash
# black + flake8
cd server
black .
flake8 .
```

#### JavaScript (Frontend)
```bash
# ESLint
cd server/frontend
npm run lint
```

### テスト

```bash
# ESP32ユニットテスト
cd core
pio test

# Pythonテスト
cd server
pytest

# フロントエンドテスト
cd server/frontend
npm test
```

## コントリビューション

プロジェクトへの貢献を歓迎します！

1. このリポジトリをフォーク
2. フィーチャーブランチを作成 (`git checkout -b feature/amazing-feature`)
3. 変更をコミット (`git commit -m 'Add amazing feature'`)
4. ブランチにプッシュ (`git push origin feature/amazing-feature`)
5. プルリクエストを作成

## ライセンス

このプロジェクトのライセンスについては、プロジェクトオーナーにお問い合わせください。

## 謝辞

- M5Stack コミュニティ
- FastLED ライブラリ開発者
- ROS2 / micro-ROS チーム
- React および Three.js コミュニティ

## 関連リンク

- [M5Atom S3R 公式ドキュメント](https://docs.m5stack.com/en/core/AtomS3R)
- [FastLED ライブラリ](https://fastled.io/)
- [ROS2 Humble](https://docs.ros.org/en/humble/)
- [micro-ROS](https://micro.ros.org/)
- [React Three Fiber](https://docs.pmnd.rs/react-three-fiber/)

---

**Isolation Sphere** - Making the sphere shine ✨
