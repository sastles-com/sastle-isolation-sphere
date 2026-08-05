> [English](spec.md) · **日本語**

# Isolation Sphere ESP32 ファームウェア仕様書

## 1. 概要
ESP32-S3 (M5Atom S3R) をベースとした球体ディスプレイ制御システムのファームウェア仕様です。
MFT2025プロジェクトをベースに、通信アーキテクチャを最適化しています。
主な機能は、FastLEDによるLED制御、IMUによる姿勢検知、および外部システムとの通信です。

## 2. ハードウェア構成
- **コントローラ**: M5Atom S3R (ESP32-S3)
- **LED**: WS2812 LEDストリップ x 4 (GPIO 5, 6, 7, 8)
  - 構成: [180, 220, 180, 220] (合計 800 LED)
- **IMU**: 内蔵または外部 (BNO055) I2C接続
- **スピーカー**: M5Atom S3R 内蔵 / 外部 I2S

## 3. 通信アーキテクチャ (UDP + MQTT ハイブリッド)
リアルタイム性と信頼性を考慮し、以下のように役割を分担します。

### 3.1 UDP (高速・低遅延)
- **用途**: **映像データ (LED)**
- **PC -> ESP32**:
  - **1フレーム分のJPEG画像データ** をストリーミング配信。
  - ESP32側でJPEGデコードし、LED座標にマッピングして表示。
  - パケットロスは許容（最新フレームを優先）。

### 3.2 MQTT (高信頼・制御)
- **用途**: **IMUデータ**、UI情報、システム制御
- **ブローカー**: Raspberry Pi (Agent)
- **トピック**:
  - `sphere/status`: デバイスのハートビート、エラー情報
  - `sphere/imu`: **IMUクォータニオンデータ (w, x, y, z)**
  - `sphere/ui`: UIコマンド、状態共有
  - `sphere/command`: システムコマンド（再起動、OTA、モード変更など）
  - `sphere/config`: 設定パラメータの更新

### 3.3 ROS2連携 (PC側ブリッジ)
- **Bridge**: UDP/MQTT <-> ROS2
  - MQTT `sphere/imu` -> ROS2 `/isolation_sphere/imu` (`sensor_msgs/Imu`)
  - ROS2 `/isolation_sphere/led` -> UDPパケット (ESP32へ送信)

## 4. データ構造
### Config (`config.json`)
- **Network**: SSID, Password
- **Agent**: IP Address, Port (MQTT Broker / UDP Target)
- **Node Name**: デバイス識別子
- **Debug**:
  - `lcd_enable`: LCD表示のON/OFF (デバッグ用、デフォルト: false)

### LED Layout (`led_layout.csv`)
- フォーマット: `faceID, stripID, stripIndex, x, y, z`
- 各LEDの3次元座標を定義し、テクスチャマッピングに使用。

## 5. 機能要件リスト
- [x] **ファイルシステム**: LittleFS (Config, Layout保存用)
- [x] **ネットワーク**: Wi-Fi接続管理
- [x] **通信機能**:
    - [x] MQTTクライアント (AsyncMqttClient)
    - [ ] UDP通信 (映像受信/IMU送信) - **新規実装**
        - [ ] **ダブルバッファリング**: 受信バッファと描画バッファを分離し、非同期で更新。
- [x] **IMU**: クォータニオン取得と送信
- [x] **LED制御**:
    - [x] FastLED初期化 (4ストリップ)
    - [x] 座標マッピング (Spherical -> UV)
    - [ ] **I2S DMA転送**: 800個のLEDを30fpsで駆動するため、I2S DMA方式を採用 (MFT2025準拠)。
    - [ ] **オンデバイスレンダリング**: 受信したテクスチャ(画像)に対し、最新のIMU値を用いて座標変換を行い、LED色を決定する。
- [ ] **オーディオ**:
    - [ ] 起動音
    - [ ] システムイベント音
- [ ] **その他**:
    - [ ] LCDデバッグ表示 (ConfigでON/OFF切替)

## 6. 検討事項 (Questions & Concerns)
1.  **LEDドライバの最適化**:
    - 現在はRMT方式ですが、800個のLEDを30fpsで駆動するにはI2S DMA方式 (MFT2025採用) への移行が推奨されます。 -> **I2S DMA採用決定**
2.  **メディア機能**:
    - **決定**: ヘッドレス運用が基本だが、デバッグ用にLCDのON/OFFを`config.json`で切り替えられるようにする。
3.  **IMU座標変換**:
    - 現在はESP32内でUV変換を行っていますが、PC側で処理してLED色データのみを送る方式（UDPストリーミング）にすれば、ESP32の負荷を下げられます。 -> **オンデバイスレンダリング採用決定 (スタンドアロンデモ対応のため)**
4.  **ジョイスティック入力**:
    - ESP32で直接受け取ることも検討しましたが、リソース不足の懸念があるため、現時点では **TBD (将来検討)** とします。
