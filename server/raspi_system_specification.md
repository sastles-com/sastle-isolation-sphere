# Isolation Sphere - Raspberry Pi Control System 仕様書

## 文書情報
| 項目 | 内容 |
|------|------|
| 文書名 | Raspberry Pi Control System 仕様書 |
| バージョン | 1.0.0 |
| 作成日 | 2025-08-22 |
| 最終更新 | 2025-08-22 |
| 作成者 | Claude Code |

## 変更履歴
| バージョン | 日付 | 変更内容 | 承認者 |
|-----------|------|----------|--------|
| 1.0.0 | 2025-08-22 | 初版作成 - システム全体仕様 | - |

## 目次
1. [プロジェクト概要](#1-プロジェクト概要)
2. [システム構成](#2-システム構成)
3. [機能要件](#3-機能要件)
4. [非機能要件](#4-非機能要件)
5. [アーキテクチャ設計](#5-アーキテクチャ設計)
6. [技術仕様](#6-技術仕様)
7. [ESP32連携仕様](#7-esp32連携仕様)
8. [セキュリティ要件](#8-セキュリティ要件)
9. [運用要件](#9-運用要件)

## 1. プロジェクト概要

### 1.1 目的
直径110mmの球体ディスプレイ（800個のLED）を制御する中央制御システムを、Raspberry Pi（Ubuntu 22.04）上に構築する。ESP32との連携により、リアルタイム映像ストリーミング、IMU情報に基づく姿勢補正、WebUIによる統合制御を実現する。

### 1.2 システム位置づけ
本システムはIsolation Sphereプロジェクトの制御ハブとして、以下の役割を担う：

- **動画コンテンツ管理**: アップロード・変換・配信・プレイリスト管理
- **デバイス通信制御**: ESP32との高速P2P通信によるリアルタイムデータ交換
- **ユーザーインターフェース**: WebUIによるマルチデバイス操作環境
- **システム管理**: 設定・状態監視・ログ管理・自動起動

### 1.3 主要機能
- ✅ **実装済み機能**:
  - FastAPI Webアプリケーション（ポート9000）
  - mDNS対応マルチデバイスアクセス（スマホ・PC・Mac対応）
  - QRコード生成機能（簡単アクセス用）
  - 動画アップロード・変換機能（320x160, 10fps自動変換）
  - SQLiteデータベースによる動画管理
  - UDP通信システム（udp_communication.py）
  - P2Pアクセスポイント作成（hostapd使用）
  - DHCP サーバー（dnsmasq）による自動IP割当
  - WebSocket リアルタイム通信

- 🔄 **拡張予定機能**:
  - ESP32ゲームパッド連携（Bluetooth経由）
  - 高度なプレイリスト制御
  - パフォーマンス監視ダッシュボード

## 2. システム構成

### 2.1 ハードウェア構成
```
┌─────────────────────────────────────────────────┐
│ Raspberry Pi (Ubuntu 22.04)                    │
├─────────────────────────────────────────────────┤
│ ■ CPUとメモリ: ARM64 4GB+ 推奨                   │
│ ■ ストレージ: microSD 32GB+ (Class 10)         │
│ ■ ネットワーク:                                │
│   - wlp1s0: 無線LANルータ接続 (WebUI用)        │
│   - wlx90de8068da46: USB無線LANアダプター       │
│     (ESP32 P2P専用 192.168.49.1)              │
│ ■ USB: ゲームパッド接続対応                    │
└─────────────────────────────────────────────────┘
```

### 2.2 ネットワーク構成
```
┌──────────────────┐    ┌─────────────────────┐
│   WiFi Router    │    │   P2P Network       │
│   (WebUI Access) │    │  (ESP32 Direct)     │
│                  │    │                     │
│  ┌─────────────┐ │    │  ┌─────────────┐    │
│  │    wlp1s0   │─┼────┼─▶│wlx90de8068da│    │
│  │192.168.1.xx │ │    │  │192.168.49.1 │    │
│  └─────────────┘ │    │  └─────────────┘    │
└──────────────────┘    │                     │
       ▲                │  ┌─────────────┐    │
       │                │  │   ESP32     │    │
   WebUI Users          │  │192.168.49.32│    │
(Smartphones/PCs)       │  └─────────────┘    │
                        └─────────────────────┘
```

### 2.3 ソフトウェア構成
```
┌─────────────────────────────────────────────┐
│             Application Layer               │
├─────────────────────────────────────────────┤
│ FastAPI WebApp │ WebSocket │ Static Files   │
├─────────────────────────────────────────────┤
│           Component Layer                   │
├─────────────────────────────────────────────┤
│ ConfigManager │ VideoProcessor │ P2PManager │
│ UDPCommunication │ PlaylistManager │ ESP32DeviceManager │
├─────────────────────────────────────────────┤
│            Infrastructure Layer             │
├─────────────────────────────────────────────┤
│ SQLite DB │ File System │ Network Stack    │
│ hostapd │ dnsmasq │ systemd │ uvicorn      │
└─────────────────────────────────────────────┘
```

## 3. 機能要件

### 3.1 動画管理機能 (VideoProcessor)

#### 3.1.1 動画アップロード・変換
- **対応形式**: mp4, avi, mov, mkv等の一般的な動画形式
- **自動変換**:
  - 解像度: 320x160ピクセル（固定）
  - フレームレート: 10fps（固定）
  - コーデック: H.264/AVC
  - 音声: 削除（映像のみ）
- **変換品質設定**:
  - CRF値: デフォルト28、設定可能範囲: 18-35
  - プリセット: veryfast（リアルタイム変換優先）
- **サムネイル生成**:
  - 1秒時点からの静止画
  - 解像度: 320x160、フォーマット: JPEG、品質: 80%

#### 3.1.2 動画データベース管理
SQLiteテーブル設計:
```sql
CREATE TABLE videos (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  filename TEXT NOT NULL,
  filepath TEXT NOT NULL,
  original_filename TEXT,
  file_size INTEGER,
  duration REAL,
  uploaded_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  metadata TEXT -- JSON形式でメタデータ保存
);
```

### 3.2 プレイリスト管理機能 (PlaylistManager)

#### 3.2.1 プレイリスト操作
```sql
CREATE TABLE playlists (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  name TEXT NOT NULL,
  description TEXT,
  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE playlist_items (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  playlist_id INTEGER REFERENCES playlists(id),
  video_id INTEGER REFERENCES videos(id),
  order_index INTEGER,
  transition_type TEXT DEFAULT 'fade'
);
```

#### 3.2.2 再生制御
- **再生モード**: sequential（順次）, shuffle（シャッフル）, random（ランダム）
- **リピートモード**: none（なし）, single（単一）, all（全体）
- **高度な制御**: 一時停止、早送り、巻き戻し

### 3.3 ESP32通信機能 (UDPCommunication)

#### 3.3.1 通信プロトコル
- **画像送信**: ポート5000でJPEG画像をESP32に送信
- **IMU受信**: ポート5001でESP32からquaternionデータを受信
- **制御送信**: ポート5002でUI操作コマンドを送信
- **通信形式**: UDP + JSON
- **バッファサイズ**: 65536バイト
- **リトライ回数**: 3回

### 3.4 デバイス管理機能 (ESP32DeviceManager)

#### 3.4.1 デバイス発見・管理
- **自動発見**: P2Pネットワーク内のESP32デバイス自動検出
- **既知デバイス管理**: MACアドレス・IP・デバイス情報の管理
- **接続状態監視**: リアルタイム接続ステータス更新
- **最大接続数**: 5台

### 3.5 P2Pネットワーク管理 (P2PManager)

#### 3.5.1 ネットワーク制御
- **アクセスポイント**: hostapd使用、SSID「ESP32-P2P-Direct」
- **DHCP設定**: dnsmasq使用、IP範囲192.168.49.10-50
- **チャンネル**: 6（設定可能）
- **セキュリティ**: WPA2-PSK

### 3.6 設定管理機能 (ConfigManager)

#### 3.6.1 設定項目
```json
{
  "system": {
    "debug_mode": false,
    "log_level": "info",
    "auto_start": true,
    "backup_retention_days": 30
  },
  "network": {
    "p2p": {
      "ssid": "ESP32-P2P-Direct",
      "password": "isolation-sphere-p2p",
      "p2p_channel": 6,
      "dhcp_range_start": "192.168.49.10",
      "dhcp_range_end": "192.168.49.50"
    },
    "web_app": {
      "web_port": 9000,
      "udp_timeout": 1.0
    }
  },
  "video": {
    "default_resolution": "320x160",
    "default_fps": 10,
    "compression_quality": 80
  }
}
```

## 4. 非機能要件

### 4.1 性能要件
- **WebUI応答時間**: 1秒以内
- **動画変換時間**: アップロード完了から30秒以内
- **UDP通信遅延**: 50ms以内
- **同時接続数**: WebUI 20接続、ESP32 5台
- **稼働率**: 99.5%以上

### 4.2 可用性要件
- **自動起動**: システム再起動時の自動サービス開始
- **エラー復旧**: 通信エラー時の自動リトライ・再接続
- **ログ管理**: 30日間のログ保持・ローテーション

### 4.3 セキュリティ要件
- **ネットワーク**: WPA2-PSK暗号化
- **ファイルアクセス**: アップロード権限制御
- **設定変更**: WebUI認証（今後実装予定）

## 5. アーキテクチャ設計

### 5.1 システムアーキテクチャ
```
┌──────────────────────────────────────────────┐
│                Web Layer                     │
│  ┌─────────┐ ┌─────────┐ ┌─────────────┐    │
│  │FastAPI  │ │WebSocket│ │Static Files │    │
│  │25 APIs  │ │Real-time│ │HTML/CSS/JS  │    │
│  └─────────┘ └─────────┘ └─────────────┘    │
├──────────────────────────────────────────────┤
│               Service Layer                  │
│ ┌──────────────┐  ┌──────────────────────┐  │
│ │ConfigManager │  │VideoProcessor        │  │
│ │- JSON Config │  │- Upload/Convert      │  │
│ │- Validation  │  │- Thumbnail Generate  │  │
│ └──────────────┘  └──────────────────────┘  │
│ ┌──────────────┐  ┌──────────────────────┐  │
│ │UDPCommunication│ │ESP32DeviceManager    │  │
│ │- 3 Port UDP  │  │- Auto Discovery     │  │
│ │- JSON Protocol│  │- Status Management  │  │
│ └──────────────┘  └──────────────────────┘  │
│ ┌──────────────┐  ┌──────────────────────┐  │
│ │PlaylistManager│ │P2PManager            │  │
│ │- SQLite      │  │- hostapd/dnsmasq    │  │
│ │- Playback    │  │- Network Control    │  │
│ └──────────────┘  └──────────────────────┘  │
├──────────────────────────────────────────────┤
│              Infrastructure Layer            │
│ ┌──────────────┐  ┌──────────────────────┐  │
│ │SQLite DB     │  │File System          │  │
│ │- videos      │  │- video_storage/     │  │
│ │- playlists   │  │- logs/              │  │
│ └──────────────┘  └──────────────────────┘  │
│ ┌──────────────┐  ┌──────────────────────┐  │
│ │Network Stack │  │System Services      │  │
│ │- UDP Sockets │  │- systemd           │  │
│ │- WiFi Control│  │- uvicorn           │  │
│ └──────────────┘  └──────────────────────┘  │
└──────────────────────────────────────────────┘
```

### 5.2 データフロー
```
┌─────────────┐    ┌──────────────┐    ┌─────────────┐
│   Web UI    │◄──►│  FastAPI     │◄──►│   ESP32     │
│(Smartphone/ │    │  25 Endpoints│    │(M5AtomS3R)  │
│ PC/Mac)     │    │  WebSocket   │    │             │
└─────────────┘    └──────────────┘    └─────────────┘
       │                    │                   │
       │Video Upload        │UDP Communication  │IMU Data
       ▼                    ▼                   ▼
┌─────────────┐    ┌──────────────┐    ┌─────────────┐
│VideoProcessor    │UDPCommunication   │             │
│- FFmpeg     │    │Port 5000:Image│    │JSON Protocol│
│- 320x160    │    │Port 5001:IMU  │    │30Hz Updates │
│- 10fps      │    │Port 5002:Ctrl│    │             │
└─────────────┘    └──────────────┘    └─────────────┘
       │                    │
       ▼                    ▼
┌─────────────┐    ┌──────────────┐
│SQLite DB    │    │P2P Network   │
│- Videos     │    │192.168.49.x  │
│- Playlists  │    │ESP32-P2P-Direct
└─────────────┘    └──────────────┘
```

## 6. 技術仕様

### 6.1 開発環境
- **OS**: Ubuntu 22.04 LTS
- **Python**: 3.10+
- **パッケージ管理**: uv (Python package manager)
- **Webフレームワーク**: FastAPI 0.100+
- **データベース**: SQLite3
- **非同期処理**: asyncio

### 6.2 依存ライブラリ
```python
# Web Framework
fastapi>=0.100.0
uvicorn[standard]>=0.23.0

# Database & Storage  
sqlite3  # Built-in
aiosqlite>=0.19.0

# Video Processing
opencv-python>=4.8.0
ffmpeg-python>=0.2.0

# Network & Communication
websockets>=11.0
qrcode>=7.4.0

# System Integration
psutil>=5.9.0
```

### 6.3 ディレクトリ構成
```
/home/yakatano/work/isolation-sphere/raspi/project/
├── main.py                    # FastAPIメインアプリケーション
├── config.json               # システム設定
├── requirements.txt          # Python依存関係
├── components/               # コンポーネントモジュール
│   ├── __init__.py
│   ├── config_manager.py
│   ├── udp_communication.py
│   ├── esp32_device_manager.py
│   ├── playlist_manager.py
│   ├── video_processor.py
│   └── p2p_manager.py
├── templates/                # Jinja2テンプレート
│   └── index.html
├── static/                   # 静的ファイル
│   ├── css/
│   ├── js/
│   └── images/
├── video_storage/            # 動画ファイル保存
├── logs/                     # ログファイル
├── backups/                  # 設定バックアップ
└── scripts/                  # 運用スクリプト
    ├── enable_autostart.sh
    └── setup_p2p_pre.sh
```

## 7. ESP32連携仕様

### 7.1 通信プロトコル詳細

#### 7.1.1 画像送信プロトコル (ポート5000)
```json
{
  "type": "image_frame",
  "frame_id": 12345,
  "timestamp": 1703123456789,
  "format": "jpeg",
  "width": 320,
  "height": 160,
  "data_size": 15360,
  "data": "base64_encoded_image_data"
}
```

#### 7.1.2 IMU受信プロトコル (ポート5001)
```json
{
  "type": "imu_data",
  "device_id": "M5AtomS3R-01",
  "timestamp": 1703123456789,
  "quaternion": {
    "w": 0.707,
    "x": 0.0,
    "y": 0.707,
    "z": 0.0
  },
  "accelerometer": {
    "x": 0.12,
    "y": 9.81,
    "z": -0.05
  },
  "gyroscope": {
    "x": 0.001,
    "y": -0.002,
    "z": 0.0003
  }
}
```

#### 7.1.3 制御送信プロトコル (ポート5002)
```json
{
  "type": "control_command",
  "command": "brightness",
  "value": 80,
  "timestamp": 1703123456789
}
```

### 7.2 通信エラー処理
- **タイムアウト**: 1秒
- **リトライ**: 3回
- **フォールバック**: 前回フレームの継続表示

## 8. セキュリティ要件

### 8.1 ネットワークセキュリティ
- **P2P暗号化**: WPA2-PSK
- **パスワード**: "isolation-sphere-p2p"
- **隔離**: P2Pネットワークは他ネットワークから隔離

### 8.2 ファイルセキュリティ
- **アップロード制限**: 最大500MB
- **形式制限**: 動画ファイルのみ
- **保存場所**: 専用ディレクトリに隔離

## 9. 運用要件

### 9.1 起動・停止
```bash
# サービス起動
sudo systemctl start isolation-sphere

# サービス停止  
sudo systemctl stop isolation-sphere

# サービス状態確認
sudo systemctl status isolation-sphere
```

### 9.2 ログ管理
- **ログファイル**: `/path/to/logs/app.log`
- **ローテーション**: 日次、30日保持
- **レベル**: INFO（設定可能）

### 9.3 監視項目
- **ヘルスチェック**: `/health` エンドポイント
- **P2P接続状況**: デバイス数・接続品質
- **リソース使用量**: CPU・メモリ・ディスク使用率
- **通信統計**: UDP送受信レート・エラー率

---

## 付録

### A. 設定ファイル例
完全な設定例は `config.json` を参照

### B. API一覧
詳細なAPI仕様は `api_specification.md` を参照

### C. トラブルシューティング
運用時の問題解決は `deployment_guide.md` を参照

---
*本文書は Isolation Sphere プロジェクトの Raspberry Pi Control System の仕様を定義しています。*