> [English](README.md) · **日本語**

# Isolation Sphere Server

Ubuntu MiniPC/Raspberry Pi上で動作するIsolation Sphereプロジェクトの制御サーバー

## 概要

このサーバーは、Isolation Sphere（800個のLEDを搭載した直径110mmの球体LEDディスプレイ）の集中制御を提供します。ESP32デバイスとの通信管理、WebUIによる制御、物理ジョイスティック入力のサポートを行います。

## システムアーキテクチャ

```
┌─────────────────────────────────────────────────────┐
│ Server (Ubuntu 22.04 LTS)                          │
├─────────────────────────────────────────────────────┤
│                                                     │
│  ┌─────────────┐  ┌──────────────┐  ┌───────────┐ │
│  │  FastAPI    │  │  Joystick    │  │  Video    │ │
│  │  Server     │  │  Daemon      │  │  Daemon   │ │
│  │  (Port 9000)│  │  (Python)    │  │  (Python) │ │
│  └──────┬──────┘  └──────┬───────┘  └─────┬─────┘ │
│         │                │                 │        │
│  ┌──────┴────────────────┴─────────────────┴─────┐ │
│  │         micro-ROS Agent (UDP 8888)            │ │
│  │         ROS2 Humble Message Bus               │ │
│  └───────────────────────────────────────────────┘ │
│                                                     │
│  ネットワークインターフェース:                        │
│  • wlan0: 外部ルーター (WebUIアクセス)              │
│  • USB WiFi: APモード 192.168.100.1 (ESP32)       │
└─────────────────────────────────────────────────────┘
         │                              │
    WebUIアクセス                   ESP32デバイス
  (スマホ/PC)                  (micro-ROS via UDP)
```

## 機能

### コアサービス
- **FastAPI Webアプリケーション** (ポート 9000)
  - システム制御用RESTful APIエンドポイント
  - リアルタイム通信用WebSocketサポート
  - ROS2/MQTTブリッジ機能
  - Reactフロントエンドの提供

- **React フロントエンド** (Viteベース)
  - Material-UIコンポーネントを使用したモダンUI
  - React Three Fiberを使用した3D可視化（IMUクォータニオン制御）
  - WebSocketによるリアルタイム制御
  - モバイル/タブレット/デスクトップ対応のレスポンシブデザイン
  - スワイプジェスチャーによる直感的なタブナビゲーション
  - モバイル最適化（URLバー自動非表示、viewport対応）
  - 上下移動ボタンと縦フリックによる垂直タブ切り替え

- **ジョイスティックデーモン**
  - `evdev`経由での物理USBジョイスティックサポート
  - ROS2トピックへのジョイスティック状態パブリッシュ
  - クロスプラットフォーム入力マッピング

- **ビデオストリーミングデーモン**
  - UDP経由でESP32へビデオコンテンツをストリーミング
  - ROS2制御による再生
  - 複数のビデオフォーマットサポート

### ネットワーク構成
- **デュアルWiFi設定**:
  - `wlan0`: WebUIアクセス用外部ネットワーク
  - `USB WiFiアダプター`: ESP32専用AP (192.168.100.1/24)
- **mDNSサポート**: ホスト名による簡単アクセス
- **DHCPサーバー**: ESP32デバイスへの自動IP割り当て

### 通信プロトコル
- **micro-ROS (XRCE-DDS)**: ESP32との主要通信
- **MQTT**: ステータス/コマンドメッセージング、IMUクォータニオンデータ受信
- **WebSocket**: WebUIのリアルタイム更新、IMUデータのブリッジ
- **UDP**: 高スループットビデオストリーミング

## 前提条件

- Ubuntu 22.04 LTS (MiniPC または Ubuntu搭載Raspberry Pi)
- Python 3.10+
- Node.js 18+ および npm
- ROS2 Humble
- Docker (micro-ROS agent用)
- USB WiFiアダプター (ESP32専用AP用)

## インストール

### 1. システム依存関係のインストール

```bash
# ROS2 Humble
sudo apt update
sudo apt install ros-humble-desktop python3-colcon-common-extensions

# Network tools
sudo apt install hostapd dnsmasq

# Python development
sudo apt install python3-pip python3-venv

# Node.js (if not installed)
curl -fsSL https://deb.nodesource.com/setup_18.x | sudo -E bash -
sudo apt install nodejs
```

### 2. Python依存関係のインストール

```bash
cd server
pip install -e .
# または uv を使用 (推奨):
# pip install uv
# uv pip install -e .
```

### 3. フロントエンドのビルド

```bash
cd server/frontend
npm install
npm run build
```

### 4. ネットワークのセットアップ (APモード)

```bash
cd server/scripts
sudo ./setup_network.sh
```

これにより、USB WiFiアダプターがESP32デバイス用のアクセスポイントとして設定されます。

### 5. サービスのセットアップ (オプション)

```bash
cd server/scripts
sudo ./setup_services.sh
```

これにより、起動時の自動起動用systemdサービスが設定されます。

## サーバーの実行

### 開発モード

```bash
# ターミナル 1: micro-ROS agentの起動
cd server/docker
docker-compose up

# ターミナル 2: FastAPIサーバーの起動
cd server
uvicorn app.main:app --host 0.0.0.0 --port 9000 --reload

# ターミナル 3: フロントエンド開発サーバーの起動 (オプション)
cd server/frontend
npm run dev

# ターミナル 4: ジョイスティックデーモンの起動 (ジョイスティック接続時)
cd server
python -m joystick.daemon
```

### 本番モード

```bash
# すべてのサービスを起動
sudo systemctl start isolation-sphere-server
sudo systemctl start isolation-sphere-joystick
sudo systemctl start micro-ros-agent
```

## プロジェクト構造

```
server/
├── app/                    # FastAPIアプリケーション
│   ├── main.py            # アプリケーションエントリポイント
│   ├── api/               # APIルート
│   ├── core/              # コア機能
│   └── services/          # ビジネスロジックサービス
├── frontend/              # Reactフロントエンド
│   ├── src/
│   │   ├── components/    # Reactコンポーネント
│   │   ├── contexts/      # Reactコンテキスト
│   │   └── pages/         # ページコンポーネント
│   └── public/            # 静的アセット
├── joystick/              # ジョイスティックデーモン
│   ├── daemon.py          # メインデーモン
│   ├── device_manager.py  # デバイス管理
│   └── mapper.py          # 入力マッピング
├── scripts/               # セットアップおよびユーティリティスクリプト
├── docs/                  # ドキュメント
│   ├── architecture.md    # システムアーキテクチャ
│   ├── mqtt_spec.md       # MQTTプロトコル仕様
│   └── ...
├── docker/                # Docker設定
│   └── docker-compose.yml # micro-ROS agent
├── pyproject.toml         # Pythonプロジェクト設定
└── README.md              # このファイル
```

## APIエンドポイント

- `GET /health` - ヘルスチェック
- `GET /api/config` - システム設定の取得
- `POST /api/config` - 設定の更新
- `GET /api/playlist` - プレイリストの取得
- `POST /api/playlist` - プレイリストの更新
- `WebSocket /api/ws` - リアルタイム通信

## 設定

設定は以下を通じて管理されます:
- `app/core/config.py` - アプリケーション設定
- 環境変数
- `core/data/config.json` - ESP32共有設定

## ドキュメント

- [システムアーキテクチャ](docs/architecture.md)
- [MQTTプロトコル仕様](docs/mqtt_spec.md)
- [ステート図](docs/state_diagram.md)
- [シーケンス図](docs/sequence_diagram.md)
- [要求仕様書](requirements_specification.md)
- [Raspberry Piシステム仕様](raspi_system_specification.md)

## 開発

### テストの実行

```bash
# Pythonテスト
pytest

# フロントエンドテスト
cd frontend
npm test
```

### コードスタイル

```bash
# Python
black .
flake8 .

# フロントエンド
cd frontend
npm run lint
```

## トラブルシューティング

### micro-ROS agentが接続しない
- Dockerコンテナが実行中か確認: `docker ps`
- ファイアウォールを確認: `sudo ufw allow 8888/udp`
- ESP32のネットワーク接続を確認

### WebUIにアクセスできない
- FastAPIがポート9000で実行中か確認
- ファイアウォールがポート9000を許可しているか確認
- mDNSサービスが実行中か確認

### ジョイスティックが検出されない
- デバイスのパーミッションを確認: `ls -l /dev/input/`
- ユーザーをinputグループに追加: `sudo usermod -a -G input $USER`
- デーモンログを確認

## ライセンス

ライセンス情報はプロジェクトルートを参照してください。

## 貢献者

貢献者情報はプロジェクトルートを参照してください。
