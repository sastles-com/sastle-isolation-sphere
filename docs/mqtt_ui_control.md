# MQTT UI制御設計書（コマンド・ステート分離型）

最終更新: 2025-12-02

## 概要

Isolation Sphereシステムにおける、複数UI（Web、Joystick、Sphere本体）からの統一的な制御を実現するMQTT設計。

## アーキテクチャパターン: コマンド・ステート分離型

### 核心原則

```
コマンド層（入力）: 誰でも送信可能
      ↓
StateManager（調停者）: 唯一の状態決定者
      ↓
ステート層（出力）: StateManagerのみが配信
      ↓
全デバイス・全UIが同期
```

**設計思想**:
1. **コマンドとステートを完全分離**: 入力（command）と出力（state）は別のトピック
2. **StateManagerが唯一の真実**: 状態の決定権はStateManagerのみ
3. **Retained Stateで自動同期**: 新規接続・再接続時に最新状態を自動取得
4. **入力源は無関係**: どのUIから操作しても同じコマンド形式
5. **全デバイス・全UIがStateに従う** ⭐: 
   - 自分が操作したコマンドであっても、必ずStateを経由して反映
   - ボタン、スライダー、LEDなどの状態は常に `sphere/all/state` と一致
   - 他のUIが操作しても自動的に同期される

### 状態同期の原則（最重要）

```
┌──────────────────────────────────────────────────────────┐
│ 原則: すべてのUI・デバイスは自分の入力を無視する          │
└──────────────────────────────────────────────────────────┘

例: Web UIでPlayボタンをクリック

❌ 間違った実装:
  1. ボタンクリック
  2. ボタンを即座に「再生中」表示に変更  ← NG!
  3. MQTTコマンド送信
  
✅ 正しい実装:
  1. ボタンクリック
  2. MQTTコマンド送信のみ
  3. sphere/all/state を受信
  4. Stateに基づいてボタン表示を更新  ← OK!

理由:
- 自分の操作であっても、StateManagerを経由した状態のみを信頼
- これにより、他のUIの操作と完全に同期
- UIロジックがシンプルになる（State購読だけ実装すればよい）
```

### データの流れ（重要）

```
┌─────────┐
│ Web UI  │ ボタンクリック
└────┬────┘
     │
     ▼ Publish command
sphere/all/command/playback
     │
     ▼ StateManager処理
     │
     ▼ Publish state (retained)
sphere/all/state
     │
     ├─────────┬─────────┬─────────┐
     ▼         ▼         ▼         ▼
┌─────────┐ ┌──────┐ ┌──────┐ ┌──────┐
│ Web UI  │ │ESP32 │ │ESP32 │ │Joy-  │
│         │ │ -01  │ │ -02  │ │stick │
└─────────┘ └──────┘ └──────┘ └──────┘
     │
     ▼ State購読
ボタン表示を更新  ← 自分が押したボタンもStateから更新！
```

**ポイント**:
- Web UIは自分が押したボタンでも、Stateを待ってから表示更新
- これにより、他のUIが操作した場合も同じロジックで同期
- すべてのUIが「Stateの表示装置」として機能

---

## システム構成

### 制御UI一覧

| UI | 役割 | 入力方法 | 出力 |
|----|------|---------|------|
| **Web UI** | プライマリ制御、詳細設定、可視化 | マウス、キーボード | ブラウザ表示、3D可視化 |
| **Joystick** | リアルタイム操作、パラメータ調整 | アナログスティック、ボタン | LEDインジケータ、バイブレーション |
| **Sphere本体** | ジェスチャー入力、単体操作 | シェイク、傾き、タップ | LED、LCD表示 |

### システムアーキテクチャ図

```
┌─────────────────────────────────────────────────────────────┐
│          入力層: コマンド（複数のPublisher）                   │
│                                                             │
│  Web UI ────┐    Joystick ────┐    Sphere ────┐            │
│             │                  │                │            │
│             ▼                  ▼                ▼            │
│       sphere/all/command/playback                           │
│       sphere/all/command/params                             │
│       sphere/{id}/command/*                                 │
└───────────────────────┬─────────────────────────────────────┘
                        │ Subscribe
                        ▼
┌─────────────────────────────────────────────────────────────┐
│                  調停層: StateManager                        │
│                                                             │
│  1. コマンド受信                                             │
│  2. 状態遷移処理（play/pause/stop）                          │
│  3. パラメータ更新（brightness/speed/hue）                   │
│  4. 整合性チェック                                           │
│  5. ステート配信                                             │
└───────────────────────┬─────────────────────────────────────┘
                        │ Publish (retained, QoS 0)
                        ▼
┌─────────────────────────────────────────────────────────────┐
│          出力層: ステート（単一のPublisher）                   │
│                                                             │
│              sphere/all/state (retained)                    │
│              {"playback": {...}, "params": {...}}           │
└───────────────────────┬─────────────────────────────────────┘
                        │ Subscribe (全員)
         ┌──────────────┼──────────────┬──────────────┐
         │              │              │              │
         ▼              ▼              ▼              ▼
    ┌────────┐    ┌────────┐    ┌────────┐    ┌─────────┐
    │ESP32-01│    │ESP32-02│    │ Web UI │    │Joystick │
    │LED反映 │    │LED反映 │    │UI更新  │    │LED更新  │
    └────────┘    └────────┘    └────────┘    └─────────┘
         ▲              ▲              ▲              ▲
         └──────────────┴──────────────┴──────────────┘
                   全員が同一状態に同期
```

### データフロー

```
任意のUI → コマンド送信 (sphere/all/command/*)
                ↓
         StateManager受信
                ↓
         状態遷移・更新処理
                ↓
   sphere/all/state配信 (retained)
                ↓
    全デバイス・全UIが自動同期
```

---

## Topic設計（コマンド・ステート分離）

### 基本原則

| 層 | Topic | Publisher | Subscriber | Retained | QoS |
|----|-------|-----------|------------|----------|-----|
| **入力** | `sphere/all/command/*` | 複数（Web/Joystick/Sphere） | StateManager | No | 1 |
| **出力** | `sphere/all/state` | StateManager**のみ** | 全員 | **Yes** | 0 |

### コマンドトピック（入力層）

**Publisher**: 任意のUI・デバイス  
**Subscriber**: StateManager

```
sphere/all/command/playback      # 全デバイスの再生制御
sphere/all/command/params        # 全デバイスのパラメータ制御
sphere/{device_id}/command/playback   # 個別デバイス制御（将来拡張）
sphere/{device_id}/command/params     # 個別パラメータ（将来拡張）
```

**特徴**:
- 複数のPublisherが存在
- **Retainedなし**: コマンドは一度きり
- **QoS 1**: 確実に届く必要がある
- StateManagerのみがSubscribe

### ステートトピック（出力層）

**Publisher**: StateManager**のみ**  
**Subscriber**: 全デバイス・全UI

```
sphere/all/state                 # 唯一の真実（retained）
```

**特徴**:
- **単一Publisher**: StateManagerのみが配信
- **Retained**: 新規接続時に最新状態を自動取得
- **QoS 0**: 最新のみ重要、配信遅延より速度優先
- 全員がSubscribe

### Topic階層構造

```
sphere/
├── all/
│   ├── command/               ← 入力層（複数Publisher）
│   │   ├── playback          
│   │   └── params            
│   │
│   └── state                  ← 出力層（StateManagerのみ）★
│
└── {device_id}/               
    ├── command/               ← 将来拡張用
    │   ├── playback
    │   └── params
    │
    ├── imu                    ← センサーデータ（StateManager管理外・直接配信）
    └── status                 ← デバイスステータス（既存）
```

**重要な設計判断**:
- `sphere/all/state` はStateManagerのみが配信する唯一の真実
- **IMUデータは例外**: 高頻度（10Hz以上）のため、StateManagerを経由せず直接WebSocketに配信
  - `sphere/{device_id}/imu` は状態管理の対象外
  - MQTTブローカー → WebSocket Manager → フロントエンドの直接ルート

---

## メッセージフォーマット

### コマンドメッセージ（統一フォーマット）

どのUIから送信されても同じ構造：

#### Playback Command
```json
{
  "action": "play" | "pause" | "stop" | "toggle",
  "playlist": "morning",        // optional
  "track": "demo01",            // optional
  "timestamp": "2025-12-02T12:34:56.123Z",
  "source_info": {              // メタ情報のみ、処理に影響しない
    "type": "web" | "joystick" | "sphere",
    "id": "session-abc" | "joy001" | "sphere001"
  }
}
```

**action値**:
- `play`: 再生開始
- `pause`: 一時停止
- `stop`: 停止（位置リセット）
- `toggle`: 現在の状態を反転（playing ↔ paused）

#### Params Command
```json
{
  "brightness": 85,             // 0-100, optional
  "speed": 60,                  // 0-100, optional
  "hue": 120,                   // 0-360, optional
  "saturation": 100,            // 0-100, optional
  "timestamp": "2025-12-02T12:34:56.123Z",
  "source_info": {
    "type": "joystick",
    "id": "joy001"
  }
}
```

**パラメータ**:
- 指定したパラメータのみ更新
- 未指定のパラメータは変更されない
- すべてoptional

### 状態メッセージ（統一フォーマット）

Serverが配信する唯一の真実：

```json
{
  "playback": {
    "status": "playing" | "paused" | "stopped",
    "playlist": "morning",
    "track": "demo01",
    "position": 45.3,           // 秒
    "duration": 120.0           // 秒
  },
  "params": {
    "brightness": 85,
    "speed": 60,
    "hue": 120,
    "saturation": 100
  },
  "timestamp": "2025-12-02T12:34:56.123Z",
  "seq": 12345                  // シーケンス番号（順序保証）
}
```

### デバイス入力メッセージ（Sphere本体から）

```json
// シェイクイベント
Topic: sphere/sphere001/input/shake
{
  "type": "shake",
  "intensity": 0.8,             // 0.0 - 1.0
  "direction": "horizontal" | "vertical",
  "timestamp": "2025-12-02T12:34:56.123Z"
}

// ジェスチャーイベント
Topic: sphere/sphere001/input/gesture
{
  "type": "tilt",
  "angle": 45.0,
  "axis": "x" | "y" | "z",
  "timestamp": "2025-12-02T12:34:56.123Z"
}
```

---

## QoS・Retained設定

| Topic Pattern | QoS | Retained | Publisher | Subscriber | 理由 |
|--------------|-----|----------|-----------|------------|------|
| `sphere/all/command/*` | 1 | No | 複数（UI/デバイス） | StateManager | コマンドは確実に届く、古いものは不要 |
| `sphere/all/state` | 0 | **Yes** | **StateManagerのみ** | 全員 | 最新状態のみ重要、新規接続時に自動取得 ✨ |
| `sphere/{id}/imu` | 0 | No | ESP32 | Server/UI | リアルタイムデータ、最新のみ |
| `sphere/{id}/status` | 0 | Yes | ESP32 | Server/UI | デバイスステータス、接続時に取得 |

### 設定理由の詳細

**コマンド層 (QoS 1, Retained: No)**:
- **QoS 1**: コマンドは確実にStateManagerに届く必要がある
- **Retainedなし**: 古いコマンドを再実行すると意図しない動作
- **例**: `{"action": "play"}` を再接続時に再実行すると二重再生

**ステート層 (QoS 0, Retained: Yes)**:
- **QoS 0**: 最新の状態のみ重要、途中のパケットロスは許容
- **Retained**: 新規接続時に最新状態を自動取得 ← **最重要**
- **例**: Web UI再接続時に最新の再生状態・パラメータが即座に反映

### Retainedの効果

```
シナリオ: Web UIを再読み込み

Retainedなしの場合:
1. Web UI再接続
2. 状態が分からない（デフォルト値で表示）
3. StateManagerが次回配信するまで待機（最大1秒）
4. ボタン表示が間違っている可能性

Retainedありの場合:
1. Web UI再接続
2. MQTT Brokerから最新のsphere/all/stateを即座に受信
3. 正しい状態で即座に表示 ✅
```

---

## 各UI実装ガイド

### Web UI

#### コマンド送信

```javascript
// 再生ボタンクリック
const handlePlayClick = () => {
  // MQTTに統一コマンド送信
  mqttClient.publish('sphere/all/command/playback', JSON.stringify({
    action: 'play',
    timestamp: new Date().toISOString(),
    source_info: { type: 'web', id: sessionId }
  }));
  
  // ボタン状態は更新しない（MQTT状態を待つ）
};

// 明るさスライダー
const handleBrightnessChange = (value) => {
  mqttClient.publish('sphere/all/command/params', JSON.stringify({
    brightness: value,
    timestamp: new Date().toISOString(),
    source_info: { type: 'web', id: sessionId }
  }));
};
```

#### 状態購読

```javascript
// MQTT状態を購読
mqttClient.subscribe('sphere/all/state');

mqttClient.on('message', (topic, message) => {
  if (topic === 'sphere/all/state') {
    const state = JSON.parse(message);
    
    // UIを状態に合わせる
    setIsPlaying(state.playback.status === 'playing');
    setBrightness(state.params.brightness);
    setSpeed(state.params.speed);
    setHue(state.params.hue);
  }
});
```

**重要原則**:
- ❌ ボタンクリック → 即座にUI更新 しない
- ✅ MQTT state受信 → UI更新
- ✅ 他のUIからの操作も自動反映

---

### Joystick Controller

#### アナログスティック入力

```python
# 右スティック → 明るさ制御
def on_joystick_axis_change(axis, value):
    if axis == 'right_x':  # -1.0 ~ 1.0
        brightness = int((value + 1.0) * 50)  # 0 ~ 100に変換
        
        mqtt_client.publish('sphere/all/command/params', json.dumps({
            'brightness': brightness,
            'timestamp': datetime.utcnow().isoformat(),
            'source_info': {'type': 'joystick', 'id': 'joy001'}
        }))
```

#### ボタン入力

```python
# Aボタン → 再生/一時停止トグル
def on_button_press(button):
    if button == 'A':
        mqtt_client.publish('sphere/all/command/playback', json.dumps({
            'action': 'toggle',
            'timestamp': datetime.utcnow().isoformat(),
            'source_info': {'type': 'joystick', 'id': 'joy001'}
        }))
```

#### 状態反映（LEDインジケータ）

```python
# MQTT状態を購読
def on_mqtt_state_message(topic, payload):
    state = json.loads(payload)
    
    # LEDインジケータを更新
    if state['playback']['status'] == 'playing':
        joystick.set_led('play', color='green')
    elif state['playback']['status'] == 'paused':
        joystick.set_led('play', color='yellow')
    else:
        joystick.set_led('play', color='red')
    
    # 明るさフィードバック（バイブレーション）
    joystick.set_rumble(state['params']['brightness'] / 100.0)

mqtt_client.subscribe('sphere/all/state')
mqtt_client.on_message = on_mqtt_state_message
```

---

### Sphere本体（ESP32）

#### シェイク入力

```cpp
void loop() {
  // シェイク検知
  if (imu.detectShake()) {
    // MQTTに統一コマンド送信
    char payload[256];
    snprintf(payload, sizeof(payload),
      "{\"action\":\"toggle\",\"timestamp\":\"%s\","
      "\"source_info\":{\"type\":\"sphere\",\"id\":\"%s\"}}",
      getCurrentTimestamp().c_str(),
      deviceId.c_str()
    );
    mqtt.publish("sphere/all/command/playback", payload);
    
    // LED状態は更新しない（MQTT stateを待つ）
  }
}
```

#### ジェスチャー入力

```cpp
void handleGesture() {
  float tiltAngle = imu.getTiltAngle();
  
  if (abs(tiltAngle) > 30.0) {
    // 傾きをパラメータ変更にマッピング
    int brightness = map(tiltAngle, -90, 90, 0, 100);
    
    char payload[256];
    snprintf(payload, sizeof(payload),
      "{\"brightness\":%d,\"timestamp\":\"%s\","
      "\"source_info\":{\"type\":\"sphere\",\"id\":\"%s\"}}",
      brightness,
      getCurrentTimestamp().c_str(),
      deviceId.c_str()
    );
    mqtt.publish("sphere/all/command/params", payload);
  }
}
```

#### 状態購読とLED反映

```cpp
void setupMQTT() {
  mqtt.subscribe("sphere/all/state");
  mqtt.subscribe("sphere/all/command/#");       // 全体コマンド
  mqtt.subscribe("sphere/sphere001/command/#"); // 個別コマンド
}

void onMQTTMessage(char* topic, byte* payload, unsigned int length) {
  JsonDocument state;
  deserializeJson(state, payload, length);
  
  if (strcmp(topic, "sphere/all/state") == 0) {
    // LED状態を反映
    const char* status = state["playback"]["status"];
    if (strcmp(status, "playing") == 0) {
      ledManager.setIndicator(LED_PLAYING);
    } else if (strcmp(status, "paused") == 0) {
      ledManager.setIndicator(LED_PAUSED);
    } else {
      ledManager.setIndicator(LED_STOPPED);
    }
    
    // パラメータ反映
    ledManager.setBrightness(state["params"]["brightness"]);
    ledManager.setSpeed(state["params"]["speed"]);
    ledManager.setHue(state["params"]["hue"]);
  }
}
```

---

## Server実装（Python）

### StateManager（完全実装）

```python
from typing import Dict, Any, List
from datetime import datetime
import json
import asyncio

class StateManager:
    """
    StateManager: 唯一の状態管理者
    
    責務:
    1. MQTTコマンドを受信して状態遷移
    2. sphere/all/state を配信（retained）
    3. WebSocketで接続中のUIに配信
    """
    
    _instance = None
    
    def __new__(cls):
        if cls._instance is None:
            cls._instance = super(StateManager, cls).__new__(cls)
            cls._instance._initialized = False
        return cls._instance
    
    def __init__(self):
        if self._initialized:
            return
        
        # 状態の初期値
        self._state = {
            "playback": {
                "status": "stopped",      # playing | paused | stopped
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
            "system": {
                "fps": 60,
                "temp": 42.0
            }
        }
        
        self._observers = []      # WebSocket connections
        self._mqtt_client = None  # MQTTServiceから設定される
        self._seq = 0
        self._initialized = True
    
    def set_mqtt_client(self, mqtt_client):
        """MQTTServiceから呼ばれる"""
        self._mqtt_client = mqtt_client
    
    async def handle_mqtt_command(self, topic: str, payload: dict):
        """
        MQTTコマンドを受信して状態を更新
        
        Args:
            topic: sphere/all/command/playback など
            payload: {"action": "play"} など
        """
        if 'playback' in topic:
            await self._update_playback(payload)
        elif 'params' in topic:
            await self._update_params(payload)
    
    async def _update_playback(self, payload: dict):
        """再生状態を更新"""
        action = payload.get('action')
        
        if action == 'play':
            self._state['playback']['status'] = 'playing'
        elif action == 'pause':
            self._state['playback']['status'] = 'paused'
        elif action == 'stop':
            self._state['playback']['status'] = 'stopped'
            self._state['playback']['position'] = 0.0
        elif action == 'toggle':
            # 現在の状態を反転
            current = self._state['playback']['status']
            self._state['playback']['status'] = (
                'playing' if current != 'playing' else 'paused'
            )
        
        # オプション情報
        if 'playlist' in payload:
            self._state['playback']['playlist'] = payload['playlist']
        if 'track' in payload:
            self._state['playback']['track'] = payload['track']
        
        # 状態を配信
        await self._publish_state()
    
    async def _update_params(self, payload: dict):
        """
        パラメータを更新
        
        指定されたパラメータのみ更新（部分更新）
        """
        for key in ['brightness', 'speed', 'hue', 'saturation']:
            if key in payload:
                self._state['params'][key] = payload[key]
        
        await self._publish_state()
    
    async def _publish_state(self):
        """
        状態をMQTT・WebSocketに配信
        
        MQTT: sphere/all/state (retained)
        WebSocket: STATE_UPDATE
        """
        self._seq += 1
        
        state_message = {
            **self._state,
            "timestamp": datetime.utcnow().isoformat(),
            "seq": self._seq
        }
        
        # MQTT配信（retained）
        if self._mqtt_client:
            self._mqtt_client.publish(
                'sphere/all/state',
                json.dumps(state_message),
                qos=0,
                retain=True  # ← 最重要: 新規接続が最新状態を取得
            )
        
        # WebSocket配信
        await self._notify_observers(state_message)
    
    def get_state(self) -> Dict[str, Any]:
        """現在の状態を取得"""
        return self._state
    
    def add_observer(self, observer):
        """WebSocket接続を追加"""
        self._observers.append(observer)
    
    def remove_observer(self, observer):
        """WebSocket接続を削除"""
        if observer in self._observers:
            self._observers.remove(observer)
    
    async def _notify_observers(self, state_message: dict):
        """WebSocketで接続中のUIに配信"""
        message = {"type": "STATE_UPDATE", "payload": state_message}
        
        for observer in self._observers:
            try:
                await observer.send_json(message)
            except Exception:
                # 切断されたクライアントは無視
                pass
```

### MQTTService（コマンド購読専用）

```python
import paho.mqtt.client as mqtt
import json
import asyncio
from .state_manager import StateManager

class MQTTService:
    """
    MQTTService: MQTTとの通信を管理
    
    購読:
    - sphere/all/command/#  ← コマンドのみ
    
    配信:
    - StateManagerに委譲（sphere/all/state）
    """
    
    _instance = None
    
    def __new__(cls):
        if cls._instance is None:
            cls._instance = super(MQTTService, cls).__new__(cls)
        return cls._instance
    
    def __init__(self):
        self.client = None
        self.state_manager = StateManager()
        self.broker_host = "192.168.49.1"
        self.broker_port = 1883
    
    def _setup_client(self):
        """MQTTクライアント設定"""
        self.client = mqtt.Client()
        self.client.on_connect = self._on_connect
        self.client.on_message = self._on_message
        
        # StateManagerにMQTTクライアントを渡す
        self.state_manager.set_mqtt_client(self.client)
    
    def _on_connect(self, client, userdata, flags, rc):
        """接続時のコールバック"""
        if rc == 0:
            print(f"Connected to MQTT Broker: {self.broker_host}")
            
            # コマンドトピックのみ購読
            self.client.subscribe('sphere/all/command/#', qos=1)
            self.client.subscribe('sphere/+/command/#', qos=1)
            
            print("Subscribed to command topics")
        else:
            print(f"Failed to connect, return code {rc}")
    
    def _on_message(self, client, userdata, msg):
        """
        MQTTメッセージ受信
        
        コマンドのみ処理、ステートは配信のみで受信しない
        """
        topic = msg.topic
        
        try:
            payload = json.loads(msg.payload.decode())
        except json.JSONDecodeError:
            print(f"Invalid JSON: {msg.payload}")
            return
        
        # コマンド処理
        if '/command/' in topic:
            # StateManagerに処理を委譲
            # asyncio.new_event_loop() で同期的に実行
            loop = asyncio.new_event_loop()
            loop.run_until_complete(
                self.state_manager.handle_mqtt_command(topic, payload)
            )
            loop.close()
    
    def start(self):
        """MQTTサービス開始"""
        self._setup_client()
        self.client.connect(self.broker_host, self.broker_port, 60)
        self.client.loop_start()
    
    def stop(self):
        """MQTTサービス停止"""
        if self.client:
            self.client.loop_stop()
            self.client.disconnect()
```

### WebSocket Bridge

```python
@router.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    await websocket.accept()
    state_manager.add_observer(websocket)
    
    # 初期状態を送信
    await websocket.send_json({
        "type": "STATE_UPDATE",
        "payload": state_manager.get_state()
    })
    
    try:
        while True:
            data = await websocket.receive_json()
            
            # WebSocketコマンドをMQTTに変換
            if data['type'] == 'SET_PLAYBACK':
                mqtt_service.publish('sphere/all/command/playback', {
                    'action': data['payload']['action'],
                    'timestamp': datetime.utcnow().isoformat(),
                    'source_info': {'type': 'web', 'id': 'websocket'}
                })
            
            elif data['type'] == 'SET_PARAMS':
                mqtt_service.publish('sphere/all/command/params', {
                    **data['payload'],
                    'timestamp': datetime.utcnow().isoformat(),
                    'source_info': {'type': 'web', 'id': 'websocket'}
                })
    
    except WebSocketDisconnect:
        state_manager.remove_observer(websocket)
```

---

## 動作シナリオ（コマンド・ステート分離型）

### シナリオ1: Joystickで再生開始

```
┌─────────────────────────────────────────────────────┐
│ 1. Joystick: Aボタン押下                             │
└───────────────────┬─────────────────────────────────┘
                    │
                    ▼ Publish (QoS 1)
         ┌──────────────────────────┐
         │ sphere/all/command/      │
         │   playback               │
         │ {"action": "play"}       │
         └──────────┬───────────────┘
                    │
                    ▼ Subscribe
         ┌──────────────────────────┐
         │ StateManager             │
         │  - action受信             │
         │  - status = "playing"    │
         └──────────┬───────────────┘
                    │
                    ▼ Publish (QoS 0, retained)
         ┌──────────────────────────┐
         │ sphere/all/state         │
         │ {"playback": {           │
         │   "status": "playing"    │
         │ }}                       │
         └──────────┬───────────────┘
                    │
        ┌───────────┼───────────┬───────────┐
        │           │           │           │
        ▼           ▼           ▼           ▼
   ┌────────┐ ┌────────┐ ┌────────┐ ┌─────────┐
   │ESP32-01│ │ESP32-02│ │ Web UI │ │Joystick │
   │LED点灯 │ │LED点灯 │ │再生表示│ │LED緑   │
   └────────┘ └────────┘ └────────┘ └─────────┘
```

**重要ポイント**:
- JoystickはコマンドをPublishするだけ
- StateManagerが状態を決定
- ステート配信で全員が同期（**Joystick自身も含む**）
- **Joystick自身のLEDもStateを受信してから点灯** ✨

**フロー詳細（Joystickの視点）**:
```
1. ユーザーがAボタン押下
2. Joystick: MQTTコマンド送信
   ↓
   この時点ではJoystick自身のLEDはまだ変わらない
   ↓
3. StateManager: コマンド受信・処理
4. StateManager: sphere/all/state 配信
   ↓
5. Joystick: sphere/all/state 受信
6. Joystick: LEDを緑に点灯  ← ここで初めて自分のLEDを更新！
```

この実装により:
- **自分が押したボタンでも、他人が押したボタンでも同じLED動作**
- コードがシンプル（State購読ロジックだけでOK）
- 完全な状態同期を保証

### シナリオ2: Sphereシェイク → トグル

```
┌─────────────────────────────────────────────────────┐
│ 1. Sphere: シェイク検知                              │
└───────────────────┬─────────────────────────────────┘
                    │
                    ▼ Publish
         sphere/all/command/playback
         {"action": "toggle"}
                    │
                    ▼
         ┌──────────────────────────┐
         │ StateManager             │
         │  - 現在: "playing"        │
         │  - 判断: toggle → "paused"│
         └──────────┬───────────────┘
                    │
                    ▼ Publish (retained)
         sphere/all/state
         {"playback": {"status": "paused"}}
                    │
        ┌───────────┼───────────┬───────────┐
        │           │           │           │
        ▼           ▼           ▼           ▼
   ┌────────┐ ┌────────┐ ┌────────┐ ┌─────────┐
   │Sphere  │ │ESP32-02│ │ Web UI │ │Joystick │
   │LED変化 │ │LED変化 │ │一時停止│ │LED黄   │
   └────────┘ └────────┘ └────────┘ └─────────┘
```

**重要ポイント**:
- Sphere自身もステートをSubscribe
- **自分が送ったシェイクコマンドの結果も、ステート経由で受け取る**
- これにより状態の一貫性が保証される

**なぜこれが重要か**:
```
悪い実装例:
  シェイク検知 → LED即座に変更 → MQTTコマンド送信
  問題: StateManagerが拒否した場合、LEDだけ変わってしまう
  
良い実装:
  シェイク検知 → MQTTコマンド送信のみ
  State受信 → LED変更
  結果: StateManagerの判断に完全に従う
```

**実装例（Sphere）**:
```cpp
void loop() {
  // シェイク検知
  if (imu.detectShake()) {
    // ✅ 正しい: コマンド送信のみ
    publishCommand("playback", "{\"action\":\"toggle\"}");
    
    // ❌ 間違い: ここでLED変更しない
    // ledManager.toggle(); ← これはしない！
  }
  
  // MQTTメッセージ処理
  mqtt.loop();
}

// State受信時のコールバック
void onStateReceived(JsonDocument& state) {
  // ✅ 正しい: Stateに基づいてLED更新
  const char* status = state["playback"]["status"];
  if (strcmp(status, "playing") == 0) {
    ledManager.setPlaying();
  } else {
    ledManager.setPaused();
  }
}
```

### シナリオ3: 複数UIから同時操作

```
時刻: 12:34:56.100
┌──────────────┐
│ Joystick:    │ → sphere/all/command/playback
│ "play"       │    {"action": "play", "timestamp": "...100"}
└──────────────┘

時刻: 12:34:56.120  
┌──────────────┐
│ Web UI:      │ → sphere/all/command/playback
│ "pause"      │    {"action": "pause", "timestamp": "...120"}
└──────────────┘

         ↓ StateManagerが順次処理
         
処理1 (12:34:56.100):
  StateManager: status = "playing"
  → sphere/all/state {"status": "playing"} 配信
  
処理2 (12:34:56.120):
  StateManager: status = "paused"
  → sphere/all/state {"status": "paused"} 配信

最終結果:
  全員が "paused" に同期 ✅
```

**競合解決**:
- StateManagerが順次処理
- 最新のコマンドが最終的に反映される
- タイムスタンプで順序保証（オプション）

---

## 競合解決

### 基本方針: StateManagerが唯一の決定者

**原則**:
- StateManagerが唯一の状態決定権を持つ
- 複数のコマンドが来ても順次処理
- 最終的な状態は `sphere/all/state` のみが真実

### パターン1: 時系列順の処理

```python
# StateManager内部
async def handle_mqtt_command(self, topic: str, payload: dict):
    # コマンドは受信順に処理される
    # 非同期だが、awaitで順序保証
    
    if 'playback' in topic:
        await self._update_playback(payload)  # ← ここで待機
    
    # 次のコマンド処理まで待つ
```

**結果**: 最後のコマンドが最終状態になる

### パターン2: タイムスタンプによる順序保証（オプション）

```python
async def handle_mqtt_command(self, topic: str, payload: dict):
    # タイムスタンプをチェック
    incoming_ts = payload.get('timestamp', '')
    last_ts = self._state.get('last_command_timestamp', '')
    
    if incoming_ts < last_ts:
        # 古いコマンドは無視
        return
    
    self._state['last_command_timestamp'] = incoming_ts
    
    # 処理継続
    if 'playback' in topic:
        await self._update_playback(payload)
```

**メリット**: ネットワーク遅延による順序逆転を防ぐ

### パターン3: UI Locking（Phase 4: 将来機能）

特定のUIが占有する場合:

```json
// ロック取得
Topic: sphere/sphere001/lock
{
  "locked_by": "web",
  "session_id": "abc123",
  "duration": 60
}

// 他のUIからのコマンドは拒否
StateManager: コマンド受信 → ロック確認 → 拒否
```

**現時点では不要**: 通常の運用では順次処理で十分

---

## 推奨アーキテクチャまとめ

### Topic構造（最終版）

```
入力層（複数Publisher）:
  sphere/all/command/playback    # QoS 1, No Retained
  sphere/all/command/params      # QoS 1, No Retained
  
調停層（StateManager）:
  ← コマンド受信
  ← 状態遷移・更新
  → ステート配信
  
出力層（単一Publisher: StateManager）:
  sphere/all/state               # QoS 0, Retained ★
```

### 責務分離

| コンポーネント | 責務 | MQTTアクション |
|---------------|------|---------------|
| **UI/デバイス** | コマンド送信、ステート受信 | Publish command, Subscribe state |
| **StateManager** | 状態管理、ステート配信 | Subscribe command, Publish state |
| **WebSocket** | Web UIへの配信 | - |

### データフロー（簡潔版）

```
UI → コマンド → StateManager → ステート → 全員同期
```

### 実装チェックリスト

#### Phase 1: 基本実装 ✅

- [x] StateManager
  - [x] handle_mqtt_command()
  - [x] _update_playback()
  - [x] _update_params()
  - [x] _publish_state() with retained
  - [x] set_mqtt_client()
  
- [x] MQTTService
  - [x] コマンドトピック購読
  - [x] StateManager連携
  
- [ ] ESP32
  - [ ] sphere/all/state 購読
  - [ ] LED状態反映
  
- [ ] Web UI
  - [ ] コマンド送信
  - [ ] ステート購読・UI更新

#### Phase 2: UI統合

- [ ] Joystick Controller
- [ ] Sphere入力（シェイク/ジェスチャー）

#### Phase 3: 高度な機能

- [ ] タイムスタンプ順序保証
- [ ] UI Locking
- [ ] コマンド履歴

---

## トラブルシューティング

### 問題: UI操作が反映されない

**確認事項**:
1. MQTT Brokerが起動しているか（`mosquitto -v`）
2. Serverが`sphere/all/command/#`を購読しているか
3. Retained flagが設定されているか（`sphere/all/state`）

**デバッグ**:
```bash
# コマンドを手動送信
mosquitto_pub -h 192.168.49.1 -t "sphere/all/command/playback" \
  -m '{"action":"play","timestamp":"2025-12-02T12:00:00Z"}'

# 状態を確認
mosquitto_sub -h 192.168.49.1 -t "sphere/all/state" -v
```

### 問題: UIの状態がバラバラ

**原因**: Retained flagが設定されていない

**解決**:
```python
# Server側でretained=True確認
mqtt_client.publish('sphere/all/state', payload, retain=True)
```

### 問題: Web UIとESP32の状態が同期しない

**原因**: 両方が同じトピックを購読していない

**確認**:
```cpp
// ESP32
mqtt.subscribe("sphere/all/state");

// Web UI
mqttClient.subscribe('sphere/all/state');
```

---

## 参考資料

- [システムアーキテクチャ](./system_architecture.md)
- [MQTT仕様（core/spec.md）](../core/spec.md)
- [クラス図](./class_diagram.md)

---

## 付録: メッセージ例

### 完全な操作シーケンス

```json
// 1. Web UIで再生開始
Topic: sphere/all/command/playback
{
  "action": "play",
  "playlist": "morning",
  "timestamp": "2025-12-02T12:00:00.100Z",
  "source_info": {"type": "web", "id": "session-abc"}
}

// 2. Server状態配信
Topic: sphere/all/state (retained)
{
  "playback": {
    "status": "playing",
    "playlist": "morning",
    "track": "demo01",
    "position": 0,
    "duration": 120.0
  },
  "params": {
    "brightness": 80,
    "speed": 50,
    "hue": 120,
    "saturation": 100
  },
  "timestamp": "2025-12-02T12:00:00.120Z",
  "seq": 1001
}

// 3. Joystickで明るさ変更
Topic: sphere/all/command/params
{
  "brightness": 90,
  "timestamp": "2025-12-02T12:00:05.200Z",
  "source_info": {"type": "joystick", "id": "joy001"}
}

// 4. Server状態配信（brightnessのみ変更）
Topic: sphere/all/state (retained)
{
  "playback": {
    "status": "playing",
    "playlist": "morning",
    "track": "demo01",
    "position": 5.1,
    "duration": 120.0
  },
  "params": {
    "brightness": 90,  // 変更された
    "speed": 50,
    "hue": 120,
    "saturation": 100
  },
  "timestamp": "2025-12-02T12:00:05.220Z",
  "seq": 1002
}

// 5. Sphereシェイクでトグル
Topic: sphere/all/command/playback
{
  "action": "toggle",
  "timestamp": "2025-12-02T12:00:10.300Z",
  "source_info": {"type": "sphere", "id": "sphere001"}
}

// 6. Server状態配信（paused）
Topic: sphere/all/state (retained)
{
  "playback": {
    "status": "paused",  // playing → paused
    "playlist": "morning",
    "track": "demo01",
    "position": 10.2,
    "duration": 120.0
  },
  "params": {
    "brightness": 90,
    "speed": 50,
    "hue": 120,
    "saturation": 100
  },
  "timestamp": "2025-12-02T12:00:10.320Z",
  "seq": 1003
}
```

すべてのUI・デバイスが同じ状態に同期される ✅
