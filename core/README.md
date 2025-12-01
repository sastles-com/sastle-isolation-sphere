# Sastle Isolation Sphere

M5Atom S3Rベースの800 LED球体ディスプレイプロジェクト

## プロジェクト構成

```
sastle-isolation-sphere/
├── core/           # メインファームウェア
│   ├── src/        # ソースコード
│   ├── data/       # LittleFS データ (画像、設定)
│   ├── doc/        # ドキュメント
│   └── platformio.ini
└── README.md
```

## Core - ファームウェア

### ハードウェア
- **MCU**: M5Atom S3R (ESP32-S3, 240MHz, 8MB Flash, 8MB PSRAM)
- **LED**: 800個 WS2812B (4ストリップ: GPIO 5,6,7,8)
- **IMU**: BNO055 (I2C: GPIO2=SDA, GPIO1=SCL)
- **WiFi**: 192.168.49.101 (静的IP)

### 機能
- ✅ LittleFS ファイルシステム
- ✅ JSON設定管理
- ✅ WiFi接続 (静的IP)
- ✅ UDP受信 (ポート8889)
- ✅ MQTT通信 (192.168.49.1:1883)
- ✅ BNO055 IMUセンサー (クォータニオン、オイラー角、加速度、ジャイロ)
- ⏳ ジェスチャー入力 (実装予定)
- ⏳ LED制御 (実装予定)
- ⏳ 画像表示 (実装予定)

### ドキュメント
- [クラス図](doc/class.md)
- [MQTT仕様](doc/mqtt.md)
- [シーケンス図](doc/sequence.md)
- [ステート図](doc/state.md)
- [デュアルコア設計](doc/dual_core_design.md)
- [画像マネージャー設計](doc/image_manager_design.md)
- [IMU補償設計](doc/imu_compensation.md)
- [UDPプロトコル仕様](doc/udp_image_protocol.md)
- [実装状況](doc/implementation_status.md)
- [仕様書](spec.md)

### ビルド & フラッシュ

```bash
cd core
pio run -t upload
```

### MQTTトピック

#### Subscribe (コマンド受信)
- `sphere/sphere001/command`

#### Publish (データ送信)
- `sphere/sphere001/status` - デバイス状態
- `sphere/sphere001/imu` - IMUデータ (10Hz)
- `sphere/sphere001/response` - コマンド応答
- `sphere/sphere001/gesture` - ジェスチャーイベント (予定)
- `sphere/sphere001/ui_mode` - UIモード状態 (予定)

## ライセンス

TBD
