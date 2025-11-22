# Isolation Sphere Server (MiniPC) 要求仕様書

## 変更履歴
| バージョン | 日付 | 変更内容 | 承認者 |
|-----------|------|----------|--------|
| 2.5.0 | 2025/11/21 | UI詳細確定 (Arwes + Neon Dials) | - |
| 2.4.0 | 2025/11/21 | UIデザイン方針確定 (Arwes + 3D Sphere) | - |
| 2.3.0 | 2025/11/21 | 技術スタック確定 (FastAPI, React, Python Joystick) | - |
| 2.2.0 | 2025/11/21 | Joystick Daemon追加・UI情報詳細化 | - |
| 2.1.0 | 2025/11/21 | 画像データのJPEG圧縮・全通信micro-ROS化の方針反映 | - |
| 2.0.0 | 2025/11/21 | MiniPC移行・micro-ROS導入に伴う全面改訂 | - |

## 1. プロジェクト概要

### 1.1 目的
Isolation Sphereの制御機能をUbuntu MiniPC（以下「Server」）に集約し、ESP32との通信を**micro-ROS (XRCE-DDS)** に統一する。
WebUIと物理ジョイスティック（Joystick Daemon）の双方から、統一されたインターフェースでシステムを制御可能にする。

### 1.2 システム構成図
```mermaid
graph TD
    subgraph "Server (MiniPC / Ubuntu 22.04)"
        WLAN0[内蔵WiFi (wlan0)] -- "WebUI Access" --> Router[外部ルーター]
        USBWiFi[USB WiFi (wlx...)] -- "AP Mode (192.168.100.1)" --> ESP32Network
        
        subgraph "Software Stack"
            WebApp[FastAPI Web App]
            ReactApp[React Frontend]
            JoyDaemon[Joystick Daemon (Python)]
            ROS2[ROS2 Humble Node]
            uROSAgent[micro-ROS Agent]
            
            ReactApp -- "WebSocket" --> WebApp
            WebApp <--> ROS2
            JoyDaemon <--> ROS2
            ROS2 <--> uROSAgent
        end
    end

    subgraph "Clients"
        User[User (Smartphone/Tablet)] -- "HTTP/WebSocket" --> WLAN0
    end

    subgraph "Isolation Sphere Devices"
        ESP32[ESP32-S3 (Sphere)] -- "micro-ROS (XRCE-DDS)" --> USBWiFi
    end
```

### 1.3 開発・運用環境
- **Hardware**: MiniPC (Ubuntu 22.04 LTS)
- **Network**:
    - `wlan0`: インターネット/LAN接続用 (DHCP Client)
    - `USB WiFi`: 専用AP構築用 (Static IP: 192.168.100.1)
- **Software**:
    - **OS**: Ubuntu 22.04 LTS
    - **Middleware**: ROS2 Humble, micro-ROS Agent
    - **Container**: Docker (推奨: micro-ROS Agent環境の分離)

### 1.4 技術スタック (確定)
- **Server Backend**:
    -   **Language**: Python 3.10+
    -   **Framework**: FastAPI (非同期処理、WebSocket対応)
    -   **ROS2 Interface**: `rclpy`
- **Server Frontend**:
    -   **Framework**: React (Vite)
    -   **UI Library**: **Arwes** (Cyberpunk/Sci-Fi Theme)
    -   **3D Graphics**: **React Three Fiber** (3D Sphere Control)
    -   **Components**: `react-knob-headless` (Custom Neon Dials)
    -   **Style**: CSS Modules / Styled Components
    -   **Communication**: WebSocket (リアルタイム状態同期)
- **Joystick Daemon**:
    -   **Language**: Python 3.10+
    -   **Library**: `evdev` (入力制御), `rclpy` (ROS2通信)

## 2. 機能要件

### 2.1 ネットワーク機能 (Dual WiFi)
Serverは2つのネットワークインターフェースを管理する。

#### 2.1.1 外部接続 (wlan0)
- **役割**: ユーザーインターフェース（WebUI）へのアクセス提供。
- **構成**: 既存の家庭内/スタジオ内ルーターに接続。
- **サービス**: Webサーバー (Port 8000/9000), SSH。

#### 2.1.2 デバイス接続 (USB WiFi)
- **役割**: Isolation Sphereデバイス（ESP32）との専用通信。
- **構成**: アクセスポイント (AP) モード。
- **設定**:
    - **SSID**: `IsolationSphere-Direct` (config.json準拠)
    - **IP**: `192.168.100.1` (Gateway/Server)
    - **Subnet**: `192.168.100.0/24`
    - **DHCP**: `192.168.100.100` ~ (ESP32用)

### 2.2 通信システム (All micro-ROS)
ESP32との通信はすべて **micro-ROS (XRCE-DDS)** で統一する。

#### 2.2.1 共有データ・Topic設計
以下の情報をROS2 Topic経由で共有し、WebUIとJoystick Daemonの双方から制御可能にする。

1.  **フレーム画像 (JPEG圧縮)**
    -   **Topic**: `/isolation_sphere/image/compressed`
    -   **Type**: `sensor_msgs/CompressedImage`
    -   **Pub**: Server (Video Processor)
    -   **Sub**: ESP32
    -   **Spec**: 320x160, 10fps, JPEG Quality調整可

2.  **UI情報 (制御・状態)**
    -   **Topic**: `/isolation_sphere/ui/control` (Pub: WebUI/JoyDaemon, Sub: Server/ESP32)
    -   **Topic**: `/isolation_sphere/ui/status` (Pub: Server/ESP32, Sub: WebUI/JoyDaemon)
    -   **内容詳細**:
        -   **動画機能**: 再生(Play), 停止(Stop), 早送り(FW), 巻き戻し(REV), シーク
        -   **プレイリスト**: 作成, 編集, 選択, ループ設定
        -   **System**: Config更新, 接続確認(Ping/Health), 再起動
        -   **Sphere管理**:
            -   IMU状態 (Quaternion)
            -   オフセット調整 (Calibration)
            -   色・明るさ (Color/Brightness)
            -   パターン投影 (Test Pattern)

### 2.3 サーバー機能構成

#### 2.3.1 WebUI (FastAPI + React)
- **Backend (FastAPI)**:
    -   REST API: 動画管理、設定管理。
    -   WebSocket: フロントエンドへのリアルタイム状態プッシュ（ROS2 Topicの中継）。
    -   Static Files: Reactビルド成果物の配信。
- **Frontend (React)**:
    -   **Design Theme**: **Arwes Cyberpunk** (Sci-Fi Hologram)。
    -   **3D Interface**: `react-three-fiber` を使用した3D球体コントローラー。
        -   **Virtual Trackball**: スマホ画面上で球体を転がすような直感的な操作。
    -   **Custom UI Elements**:
        -   **Neon Dials**: `react-knob-headless` を使用し、発光するリング状のダイヤルを実装。
        -   **Sound Effects**: 操作時のSF効果音 (Arwes標準機能)。
    -   **Real-time Sync**: WebSocket経由でJoystick操作やIMU状態を即座に反映。

#### 2.3.2 Joystick Daemon (Python)
- **役割**: 物理コントローラーによる直感的な操作の提供。
- **構成**: 独立したデーモンプロセスとして動作。
- **機能**:
    -   物理ジョイスティック/ダイアル入力の監視 (`evdev`)。
    -   入力イベントのROS2 Topic (`/isolation_sphere/ui/control`) への変換・Publish。
    -   状態Topic (`/isolation_sphere/ui/status`) のSubscribeによるLED/Hapticフィードバック（あれば）。

#### 2.3.3 動画処理・配信
- **機能**: 動画ファイルのデコード、リサイズ(320x160)、JPEG圧縮、ROS2 Publish。
- **性能**: 10fps安定配信。帯域適応型の品質調整。

## 3. 非機能要件
- **安定性**: 24時間連続稼働。micro-ROS Agentの自動再接続機能。
- **遅延**: 制御コマンド < 50ms。画像ストリーム < 100ms。
- **並行性**: WebUIとJoystickからの同時操作の競合解決（Last-Winまたは排他制御）。

## 4. 開発ロードマップ
1.  **Server構築**: Ubuntu設定、Dual WiFi化 (`hostapd`, `netplan`).
2.  **micro-ROS環境**: DockerでのAgent立ち上げ、ESP32とのPing疎通。
3.  **基本通信**: IMUデータのROS2 Topic化。
4.  **画像転送実装**: `sensor_msgs/CompressedImage` を用いたJPEG配信の実装とパフォーマンステスト。
5.  **Joystick Daemon実装**: 入力デバイスの読み取りとROS2連携。
6.  **WebUI統合**: FastAPIとROS2ノードの連携、Reactフロントエンド実装 (Arwes + R3F + Dials)。
 (Arwes + R3F)。