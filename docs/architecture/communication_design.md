# Isolation Sphere 通信・共有アーキテクチャ（確定版）

最終更新: 2025-12-02

## 概要

このドキュメントは、Isolation Sphereシステム全体の通信プロトコルとデータ共有の設計を定義します。

## 設計原則

1. **Single Source of Truth**: `StateManager`が唯一の状態保持者
2. **Protocol by Use Case**: 用途に応じた最適なプロトコル選択
3. **No Over-Engineering**: 必要最小限の技術スタック
4. **ROS2は不使用**: micro-ROS削除、MQTT+UDP+WebSocketで統一

---

## システム全体構成

```
┌──────────────┐         ┌──────────────────┐         ┌───────────┐
│   Web UI     │         │  Server (MiniPC) │         │  ESP32    │
│  (Browser)   │         │  Ubuntu 22.04    │         │ M5AtomS3R │
└──────────────┘         └──────────────────┘         └───────────┘
       │                          │                          │
       │ ┌────────────────────────┤                          │
       │ │  FastAPI Server        │                          │
       │ │  ┌──────────────┐      │                          │
       │ └──┤StateManager  │      │                          │
       │    │(唯一の真実)  │      │                          │
       │    └──────┬───────┘      │                          │
       │           │               │                          │
   ┌───▼───────────▼───────────────▼────────────────────────▼───┐
   │              Communication Layer                            │
   ├─────────────────────────────────────────────────────────────┤
   │  WebSocket     MQTT Broker      UDP Socket                  │
   │  (port 9000)   (port 1883)      (port 8889)                 │
   └─────────────────────────────────────────────────────────────┘
```

---

## 通信プロトコル詳細

### 1. MQTT - 制御・状態・センサーデータ

#### 目的
- 双方向通信
- 信頼性重視（QoS 1）
- Retained message活用

#### ブローカー設定
- **Host**: 192.168.49.1
- **Port**: 1883
- **Protocol**: MQTT v3.1.1
- **Authentication**: Anonymous

#### トピック設計

##### コマンド系（Server → ESP32）

```yaml
sphere/all/command/params:
  direction: Server → ESP32
  qos: 1
  retained: false
  description: パラメータ変更（明るさ、速度、色相等）
  payload: |
    {
      "brightness": 80,    # 0-100
      "speed": 50,         # 0-100
      "hue": 120,          # 0-360
      "saturation": 100    # 0-100
    }

sphere/all/command/playback:
  direction: Server → ESP32
  qos: 1
  retained: false
  description: 動画再生制御
  payload: |
    {
      "action": "play|pause|stop|toggle",
      "playlist": "demo01",      # optional
      "track": "frame_001",      # optional
      "position": 0.5            # optional (0.0-1.0)
    }

sphere/all/command/led:
  direction: Server → ESP32
  qos: 1
  retained: false
  description: LED制御モード変更
  payload: |
    {
      "mode": "sphere|pixels|off",
      "pixels": [                # mode=pixels時のみ
        {"index": 0, "r": 255, "g": 0, "b": 0},
        ...
      ]
    }

sphere/all/command/system:
  direction: Server → ESP32
  qos: 1
  retained: false
  description: システムコマンド
  payload: |
    {
      "action": "restart|calibrate|config_reload"
    }
```

##### 状態系（ESP32 → Server）

```yaml
sphere/{device_id}/state:
  direction: ESP32 → Server
  qos: 1
  retained: true  # 重要！新規接続時に最新状態取得
  description: デバイス完全状態スナップショット
  payload: |
    {
      "params": {"brightness": 80, "speed": 50, "hue": 120, "saturation": 100},
      "playback": {"status": "playing", "playlist": "demo01", "track": "frame_001", "position": 0.5, "duration": 60.0},
      "led": {"mode": "sphere", "pixels": []},
      "system": {"uptime": 12345, "fps": 60, "temp": 42.0, "free_heap": 234567},
      "timestamp": "2025-12-02T13:27:31Z",
      "seq": 42
    }

sphere/{device_id}/imu:
  direction: ESP32 → Server
  qos: 0  # 速度優先、最新データのみ必要
  retained: false
  frequency: 10Hz
  description: IMU姿勢データ（Quaternion）
  payload: |
    {
      "w": 0.7071,
      "x": 0.7071,
      "y": 0.0000,
      "z": 0.0000
    }

sphere/{device_id}/status:
  direction: ESP32 → Server
  qos: 1
  retained: true
  description: デバイスオンライン状態
  payload: |
    {
      "status": "online|offline",
      "uptime": 12345,
      "free_heap": 234567,
      "timestamp": "2025-12-02T13:27:31Z"
    }
```

---

### 2. UDP - 映像ストリーミング

#### 目的
- 高速・低遅延
- 一方向通信（Server → ESP32）
- パケットロス許容（最新フレーム優先）

#### 仕様

```yaml
protocol: UDP
source: Server (Video Daemon)
destination: ESP32 (192.168.49.101:8889)

packet_format:
  header:
    magic: 0x4A504547  # "JPEG" (4 bytes)
    frame_id: uint32   # フレーム連番 (4 bytes)
  payload:
    jpeg_data: bytes   # JPEG圧縮画像データ (max 65499 bytes)
  
  max_packet_size: 65507 bytes (UDP最大)

image_spec:
  resolution: 320x160
  format: JPEG
  quality: 70-90 (adjustable)
  fps: 10
  typical_size: ~10KB/frame

ESP32_implementation:
  receiver: ImageManager::update()
  buffer: Double buffering (PSRAM)
  decoder: TJpg_Decoder
  output: RGB565 → LEDManager
```

#### パケット構造（C言語）

```c
struct UDPImageHeader {
    uint32_t magic;      // 0x4A504547 ("JPEG")
    uint32_t frame_id;   // フレーム連番
} __attribute__((packed));

// 総サイズ = sizeof(UDPImageHeader) + jpeg_data_size
```

---

### 3. WebSocket - UI同期

#### 目的
- リアルタイム双方向通信
- WebUIとServer間の状態同期
- 低遅延（<50ms）

#### エンドポイント
- **URL**: `ws://[server-ip]:9000/ws`
- **Protocol**: WebSocket

#### メッセージ設計

##### Server → WebUI

```yaml
STATE_UPDATE:
  type: "STATE_UPDATE"
  description: 状態更新通知
  payload:
    imu: {w, x, y, z}
    playback: {status, playlist, track, position, duration}
    params: {brightness, speed, hue, saturation}
    led: {mode, pixels}
    system: {fps, temp, uptime}
    timestamp: "2025-12-02T13:27:31Z"
    seq: 42
```

##### WebUI → Server

```yaml
SET_PARAMS:
  type: "SET_PARAMS"
  description: パラメータ変更要求
  payload:
    brightness: 80   # 任意のパラメータ（1つ以上）
    speed: 60
    hue: 180

SET_PLAYBACK:
  type: "SET_PLAYBACK"
  description: 再生制御要求
  payload:
    action: "play|pause|stop|toggle"
    playlist: "demo01"  # optional
    track: "frame_001"  # optional

SET_LED:
  type: "SET_LED"
  description: LED制御要求
  payload:
    mode: "sphere|pixels|off"
    pixels: [...]  # optional
```

---

## コンポーネント設計

### 1. StateManager (Python)

#### 責務
- 状態の保持・更新
- すべての入力ソースからのコマンド処理
- MQTT/WebSocketへの状態配信

#### 状態構造

```python
_state = {
    "imu": {
        "w": 1.0, 
        "x": 0.0, 
        "y": 0.0, 
        "z": 0.0
    },
    "playback": {
        "status": "stopped",  # "playing" | "paused" | "stopped"
        "playlist": None,
        "track": None,
        "position": 0.0,
        "duration": 0.0
    },
    "params": {
        "brightness": 80,
        "speed": 50,
        "hue": 120,
        "saturation": 100
    },
    "led": {
        "mode": "sphere",  # "sphere" | "pixels" | "off"
        "pixels": []
    },
    "system": {
        "fps": 60,
        "temp": 42.0,
        "uptime": 0,
        "free_heap": 0
    },
    "timestamp": "2025-12-02T13:27:31Z",
    "seq": 0
}
```

#### 主要メソッド

```python
async def handle_command(source: str, command: dict):
    """
    コマンド処理統一インターフェース
    
    Args:
        source: "webui" | "joystick" | "mqtt"
        command: {"type": "SET_PARAMS", "payload": {...}}
    """
    # 1. 内部状態更新
    await self._update_state(command)
    
    # 2. MQTT配信（ESP32に反映）
    await self._publish_mqtt(command)
    
    # 3. WebSocket配信（UI更新）
    await self._broadcast_websocket()
```

---

### 2. Video Daemon (Python) - 未実装

#### 責務
- プレイリスト管理
- 動画デコード（OpenCV）
- フレームリサイズ（320x160）
- JPEG圧縮
- UDP送信

#### アーキテクチャ

```python
class VideoDaemon:
    def __init__(self):
        self.mqtt_client = mqtt.Client()
        self.udp_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.current_playlist = None
        self.is_playing = False
        
    def run(self):
        # MQTT Subscribe: sphere/all/command/playback
        self.mqtt_client.subscribe("sphere/all/command/playback")
        self.mqtt_client.on_message = self._on_mqtt_message
        
        while True:
            if self.is_playing:
                frame = self._get_next_frame()
                jpeg = self._encode_jpeg(frame, quality=80)
                self._send_udp(jpeg)
                time.sleep(1/10)  # 10fps
    
    def _send_udp(self, jpeg_data: bytes):
        header = struct.pack("<II", 0x4A504547, self.frame_id)
        packet = header + jpeg_data
        self.udp_socket.sendto(packet, ("192.168.49.101", 8889))
        self.frame_id += 1
```

---

### 3. ESP32 ファームウェア

#### 実装状況
- ✅ MQTTManager: 実装済み
- ✅ ImageManager: UDP受信・デコード実装済み
- ⬜ MQTTコマンドハンドラ: 統合必要

#### 実装例

```cpp
// main.cpp

void setup() {
    // MQTT Subscribe
    mqttManager.subscribe("sphere/all/command/params", onParamsCommand);
    mqttManager.subscribe("sphere/all/command/playback", onPlaybackCommand);
    mqttManager.subscribe("sphere/all/command/led", onLedCommand);
}

void loop() {
    // UDP画像受信・デコード
    if (imageManager.update()) {
        ledManager.displayImage(imageManager);
    }
    
    // IMU送信（10Hz）
    if (millis() - lastIMUTime > 100) {
        publishIMU();
        lastIMUTime = millis();
    }
    
    mqttManager.loop();
}

void onParamsCommand(const char* payload) {
    StaticJsonDocument<256> doc;
    deserializeJson(doc, payload);
    
    if (doc.containsKey("brightness")) {
        ledManager.setBrightness(doc["brightness"]);
    }
    if (doc.containsKey("speed")) {
        // 速度パラメータ処理
    }
    
    // 状態をパブリッシュ（確認）
    publishState();
}
```

---

## データフロー

### シナリオ1: WebUIでBrightness変更

```
[WebUI]
  ↓ WS: {"type":"SET_PARAMS", "payload":{"brightness":80}}
  
[StateManager]
  ├─ state["params"]["brightness"] = 80
  ├─ MQTT Pub: sphere/all/command/params → [ESP32]
  └─ WS Broadcast: STATE_UPDATE → [他のWebUI]
  
[ESP32]
  ↓ MQTT Sub: sphere/all/command/params
  ↓ LEDManager::setBrightness(80)
  ↓ MQTT Pub: sphere/sphere001/state (確認)
  
[StateManager]
  ↓ MQTT Sub: sphere/sphere001/state
  └─ WS Broadcast: STATE_UPDATE (確認反映)
```

---

### シナリオ2: 動画再生開始

```
[WebUI]
  ↓ WS: {"type":"SET_PLAYBACK", "payload":{"action":"play", "playlist":"demo01"}}
  
[StateManager]
  ├─ state["playback"]["status"] = "playing"
  ├─ MQTT Pub: sphere/all/command/playback
  └─ WS Broadcast: STATE_UPDATE
  
[Video Daemon]
  ↓ MQTT Sub: sphere/all/command/playback
  ↓ プレイリスト"demo01"をロード
  ↓ ループ開始: 10fps
      ├─ フレーム取得 → リサイズ(320x160) → JPEG圧縮
      └─ UDP送信: 192.168.49.101:8889
  
[ESP32]
  ↓ UDP受信: ImageManager::update()
  ↓ JPEGデコード → RGB565バッファ (PSRAM)
  ↓ LEDManager::displayImage()
  └─ 800個のLED更新
```

---

### シナリオ3: IMUリアルタイム表示

```
[ESP32] (10Hz)
  ↓ IMU読み取り → Quaternion {w, x, y, z}
  ↓ MQTT Pub: sphere/sphere001/imu
  
[StateManager]
  ↓ MQTT Sub
  ├─ state["imu"] = {w, x, y, z}
  └─ WS Broadcast: STATE_UPDATE
  
[WebUI]
  ↓ Three.js Quaternion適用
  └─ 3D球体回転表示
```

---

## ディレクトリ構成

```
repo/
├── core/                    # ESP32ファームウェア
│   ├── src/
│   │   ├── MQTTManager.*    # ✅ 実装済み
│   │   ├── ImageManager.*   # ✅ UDP受信・デコード実装済み
│   │   ├── LEDManager.*     # ✅ 実装済み
│   │   └── main.cpp         # ⬜ コマンド処理統合必要
│   └── data/
│       └── config.json      # WiFi/MQTT設定
│
├── server/
│   ├── app/
│   │   ├── main.py          # FastAPI + Lifespan
│   │   ├── services/
│   │   │   ├── state_manager.py      # ✅ 基本実装済み
│   │   │   └── mqtt_service.py       # ✅ 実装済み
│   │   └── api/
│   │       └── endpoints/
│   │           └── websocket.py      # ✅ 実装済み
│   │
│   ├── video/               # ⬜ 新規作成必要
│   │   ├── daemon.py        # Video Daemon本体
│   │   ├── playlist.py      # プレイリスト管理
│   │   └── encoder.py       # JPEG圧縮
│   │
│   ├── joystick/            # ⬜ Pending（Phase 2以降）
│   │   └── daemon.py        # PS4コントローラー対応
│   │
│   └── frontend/            # ✅ React実装済み
│
└── docs/
    └── architecture/
        └── communication_design.md  # このドキュメント
```

---

## 実装ロードマップ

### Phase 1: 基盤整備（今週）
- [ ] ROS2コード完全削除
  - `app/core/ros_manager.py`
  - `app/services/ros_bridge.py`
  - `joystick/daemon.py` ROS2部分
- [ ] `state_manager.py` WebSocketメッセージハンドラ統合
- [ ] `websocket.py` シンプル化
- [ ] ESP32側MQTTコマンドハンドラ実装
- [ ] ドキュメント更新（README.md等）

### Phase 2: 映像ストリーミング（来週）
- [ ] `video/daemon.py` 実装
- [ ] プレイリスト管理API
- [ ] OpenCV映像デコード
- [ ] UDP送信実装
- [ ] ESP32側表示確認

### Phase 3: Joystick対応（その後）
- [ ] PS4コントローラー入力取得（evdev）
- [ ] MQTT直接パブリッシュ
- [ ] ボタンマッピング設定

---

## 削除対象（ROS2関連）

以下のコードは削除またはシンプル化する：

1. **`server/app/core/ros_manager.py`** - 完全削除
2. **`server/app/services/ros_bridge.py`** - 完全削除
3. **`server/joystick/daemon.py`** - ROS2部分削除、MQTT直接パブリッシュ版に書き換え
4. **`server/app/main.py`** - ROSManager起動部分削除
5. **`server/app/api/endpoints/websocket.py`** - ros_bridge参照削除

---

## 技術的決定事項

### なぜROS2を使わないのか？

1. **過剰設計**: 本システムはプロセス間通信にROS2の複雑さは不要
2. **MQTT+WebSocketで十分**: 既存プロトコルで要件を満たせる
3. **Joystickの用途**: 単純な入力デバイス、状態監視不要
4. **保守性**: シンプルなアーキテクチャの方が理解・保守が容易

### MQTTのRetained Message活用

- `sphere/all/state`: ESP32の完全状態をretainedで保持
- 利点：
  - Server再起動時に最新状態を自動取得
  - ESP32再起動時の状態復元
  - 新規WebUIクライアント接続時の初期状態同期

---

## 非機能要件

- **遅延**: 
  - 制御コマンド: < 50ms
  - 画像ストリーム: < 100ms
  - IMUデータ: 10Hz (100ms周期)
  
- **安定性**: 
  - 24時間連続稼働
  - MQTT自動再接続
  - UDP パケットロス許容

- **拡張性**:
  - 複数ESP32デバイス対応（device_id分離）
  - プレイリスト拡張可能

---

## 参考資料

- [MQTT v3.1.1 Specification](http://docs.oasis-open.org/mqtt/mqtt/v3.1.1/mqtt-v3.1.1.html)
- [ESP32 ImageManager実装](../../core/src/ImageManager.h)
- [ESP32 MQTTManager実装](../../core/src/MQTTManager.h)
- [Server StateManager実装](../../server/app/services/state_manager.py)

