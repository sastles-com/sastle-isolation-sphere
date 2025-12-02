# MQTT UI制御設計書

最終更新: 2025-12-02

## 概要

Isolation Sphereシステムにおける、複数UI（Web、Joystick、Sphere本体）からの統一的な制御を実現するMQTT設計。

### 核心原則

```
入力方法に依存しない統一状態管理
すべてのUI入力 → MQTT統一コマンド → 全デバイス・全UI同期
```

**重要**: どのUIから操作しても、MQTTに流れる信号は同じ。すべてのUI・デバイスは同一の状態に同期する。

---

## システム構成

### 制御UI一覧

| UI | 役割 | 入力方法 | 出力 |
|----|------|---------|------|
| **Web UI** | プライマリ制御、詳細設定、可視化 | マウス、キーボード | ブラウザ表示、3D可視化 |
| **Joystick** | リアルタイム操作、パラメータ調整 | アナログスティック、ボタン | LEDインジケータ、バイブレーション |
| **Sphere本体** | ジェスチャー入力、単体操作 | シェイク、傾き、タップ | LED、LCD表示 |

### データフロー図

```
┌──────────────────────────────────────────────────────┐
│         任意のUI入力（Web/Joystick/Sphere）            │
└────────────┬─────────────────────────────────────────┘
             │
             ▼ 統一コマンドに変換
  ┌──────────────────────────────────┐
  │   MQTT Broker (192.168.49.1)      │
  │                                   │
  │ sphere/all/command/playback       │
  │   {"action": "play"}              │
  │                                   │
  │ sphere/all/command/params         │
  │   {"brightness": 85}              │
  └────────┬──────────────────────────┘
           │
           │ Subscribe (全員)
           ▼
    ┌──────────────┐
    │  Server      │
    │ StateManager │
    └──────┬───────┘
           │
           ▼ 状態更新 + Publish (retained)
  ┌──────────────────────────────────┐
  │ sphere/all/state                  │
  │ {"playback": {"status": "playing"},│
  │  "params": {"brightness": 85}}   │
  └────────┬──────────────────────────┘
           │
           │ Subscribe (全員)
           ▼
    ┌──────┴──────┬──────────┬──────────┐
    │             │          │          │
    ▼             ▼          ▼          ▼
┌────────┐  ┌────────┐  ┌───────┐  ┌─────────┐
│ESP32-01│  │ESP32-02│  │Web UI │  │Joystick │
│LED更新 │  │LED更新 │  │ボタン  │  │LED      │
│        │  │        │  │表示更新│  │インジケータ│
└────────┘  └────────┘  └───────┘  └─────────┘
     ▲           ▲          ▲          ▲
     └───────────┴──────────┴──────────┘
            すべて同じ状態に同期
```

---

## Topic設計

### コマンドトピック（入力）

すべてのUI入力は以下のトピックに統一される：

```
sphere/all/command/playback      # 全デバイスの再生制御
sphere/all/command/params        # 全デバイスのパラメータ制御
sphere/{device_id}/command/playback   # 個別デバイス制御
sphere/{device_id}/command/params     # 個別デバイスパラメータ
```

### 状態トピック（出力）

すべてのUI・デバイスが購読する真実の単一ソース：

```
sphere/all/state                 # 全体の統一状態（retained）
sphere/{device_id}/state         # 個別デバイス状態（retained）
```

### Topic構成図

```
sphere/
├── all/
│   ├── command/
│   │   ├── playback            # 全デバイス再生制御
│   │   └── params              # 全デバイスパラメータ
│   └── state                   # 全体統一状態 (retained)
│
└── {device_id}/                # 例: sphere001
    ├── command/
    │   ├── playback            # 個別再生制御
    │   └── params              # 個別パラメータ
    ├── state                   # 個別状態 (retained)
    ├── input/                  # デバイスからの入力
    │   ├── shake              # シェイクイベント
    │   └── gesture            # ジェスチャー
    ├── imu                     # IMUデータ
    └── status                  # ステータス
```

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

| Topic Pattern | QoS | Retained | 理由 |
|--------------|-----|----------|------|
| `sphere/all/command/*` | 1 | No | コマンドは確実に届く必要があるが、古いものは不要 |
| `sphere/{id}/command/*` | 1 | No | 同上 |
| `sphere/all/state` | 0 | **Yes** | 新規接続時に最新状態を即座に取得 ✨ |
| `sphere/{id}/state` | 0 | **Yes** | 同上 |
| `sphere/{id}/input/*` | 0 | No | イベントデータ、リアルタイムのみ重要 |
| `sphere/{id}/imu` | 0 | No | リアルタイムデータ、最新のみ |

**Retainedの重要性**:
- 新規接続したUI・デバイスが即座に最新状態を取得
- ネットワーク再接続時に状態が自動復元
- すべてのUIが常に同じ状態を表示

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

### StateManager

```python
class StateManager:
    def __init__(self):
        self._state = {
            "playback": {
                "status": "stopped",
                "playlist": None,
                "track": None,
                "position": 0,
                "duration": 0
            },
            "params": {
                "brightness": 80,
                "speed": 50,
                "hue": 120,
                "saturation": 100
            }
        }
        self._observers = []  # WebSocket connections
        self._mqtt_client = None
        self._seq = 0
    
    async def handle_mqtt_command(self, topic: str, payload: dict):
        """MQTTコマンドを受信して状態を更新"""
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
            self._state['playback']['position'] = 0
        elif action == 'toggle':
            current = self._state['playback']['status']
            self._state['playback']['status'] = (
                'playing' if current != 'playing' else 'paused'
            )
        
        # プレイリスト・トラック情報
        if 'playlist' in payload:
            self._state['playback']['playlist'] = payload['playlist']
        if 'track' in payload:
            self._state['playback']['track'] = payload['track']
        
        # 状態をMQTT・WebSocketに配信
        await self._publish_state()
    
    async def _update_params(self, payload: dict):
        """パラメータを更新（指定されたもののみ）"""
        for key in ['brightness', 'speed', 'hue', 'saturation']:
            if key in payload:
                self._state['params'][key] = payload[key]
        
        await self._publish_state()
    
    async def _publish_state(self):
        """統一状態をMQTT・WebSocketに配信"""
        self._seq += 1
        state_message = {
            **self._state,
            "timestamp": datetime.utcnow().isoformat(),
            "seq": self._seq
        }
        
        # MQTT配信（retained）
        self._mqtt_client.publish(
            'sphere/all/state',
            json.dumps(state_message),
            qos=0,
            retain=True  # 新規接続が最新状態を取得
        )
        
        # WebSocket配信（Web UI用）
        for observer in self._observers:
            try:
                await observer.send_json({
                    "type": "STATE_UPDATE",
                    "payload": state_message
                })
            except:
                pass
```

### MQTTService

```python
class MQTTService:
    def _setup_subscriptions(self):
        """コマンドトピックを購読"""
        self.client.subscribe('sphere/all/command/#')
        self.client.subscribe('sphere/+/command/#')
        self.client.subscribe('sphere/+/input/#')  # デバイス入力
    
    def _on_message(self, client, userdata, msg):
        """MQTTメッセージ受信"""
        topic = msg.topic
        payload = json.loads(msg.payload.decode())
        
        if '/command/' in topic:
            # コマンド処理 → StateManager
            loop = asyncio.new_event_loop()
            loop.run_until_complete(
                self.state_manager.handle_mqtt_command(topic, payload)
            )
            loop.close()
        
        elif '/input/' in topic:
            # デバイス入力をログ・処理
            logger.info(f"Device input: {topic} - {payload}")
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

## 動作シナリオ

### シナリオ1: Joystickで再生開始 → 全UI同期

```
1. Joystick: Aボタン押下
   ↓
2. Joystick → MQTT Publish
   Topic: sphere/all/command/playback
   {"action": "play", "timestamp": "2025-12-02T12:34:56.123Z"}
   ↓
3. Server: コマンド受信
   ↓
4. Server: StateManager.handle_mqtt_command()
   state.playback.status = "playing"
   ↓
5. Server → MQTT Publish (retained)
   Topic: sphere/all/state
   {"playback": {"status": "playing"}, "params": {...}}
   ↓
6. 全員が受信して状態反映:
   ├─ ESP32-01: LED → 再生中表示
   ├─ ESP32-02: LED → 再生中表示
   ├─ Web UI: 再生ボタン → 停止アイコンに変更
   └─ Joystick: LED → 緑色に点灯
   ↓
7. すべてのUI・デバイスが同期完了 ✅
```

### シナリオ2: Sphereシェイク → トグル → Web UI反映

```
1. Sphere: シェイク検知
   ↓
2. Sphere → MQTT Publish
   Topic: sphere/all/command/playback
   {"action": "toggle", "timestamp": "..."}
   ↓
3. Server: 現在の状態を確認
   current_status = "playing"
   ↓
4. Server: トグル実行
   new_status = "paused"
   ↓
5. Server → MQTT Publish
   Topic: sphere/all/state
   {"playback": {"status": "paused"}, ...}
   ↓
6. 全員が受信:
   ├─ Sphere自身: LED → 一時停止表示
   ├─ Web UI: 再生ボタン → 再生アイコンに変更
   └─ Joystick: LED → 黄色に変更
   ↓
7. シェイクした本人も含めて全員同期 ✅
```

### シナリオ3: Web UIスライダー → ESP32の明るさ変更

```
1. Web UI: 明るさスライダーを85に変更
   ↓
2. Web UI → MQTT Publish
   Topic: sphere/all/command/params
   {"brightness": 85, "timestamp": "..."}
   ↓
3. Server: パラメータ更新
   state.params.brightness = 85
   ↓
4. Server → MQTT Publish
   Topic: sphere/all/state
   {"params": {"brightness": 85, ...}}
   ↓
5. 全員が受信:
   ├─ ESP32-01: LED明るさ → 85%
   ├─ ESP32-02: LED明るさ → 85%
   ├─ Web UI: スライダー表示 → 85（確認）
   └─ Joystick: バイブレーション強度 → 85%
   ↓
6. リアルタイムパラメータ同期完了 ✅
```

---

## 競合解決

### 基本原則

**Server（StateManager）が唯一の真実**

- 複数UIから同時にコマンドが来ても、Serverが最終的な状態を決定
- タイムスタンプで順序付け（最新が優先）
- すべてのUIはMQTT stateに従う → 自動的に収束

### 例: 同時操作

```
時刻 12:34:56.100 - Joystick: "play" 送信
時刻 12:34:56.120 - Web UI: "pause" 送信

Server処理:
1. "play" 受信 → 状態を "playing" に
2. 状態配信 → 全員が "playing" になる
3. "pause" 受信（より新しい） → 状態を "paused" に
4. 状態配信 → 全員が "paused" になる

結果: 最新のコマンド（pause）が反映される ✅
```

### UI Locking（将来機能）

特定のUIがデバイスを占有する場合の設計（Phase 4）:

```json
Topic: sphere/sphere001/lock
{
  "locked_by": "web",
  "session_id": "abc123",
  "duration": 60  // 秒、0=無制限
}
```

---

## 実装チェックリスト

### Phase 1: 基本コマンド + 状態同期 ⏳

- [ ] Server: StateManager拡張
  - [ ] handle_mqtt_command()実装
  - [ ] _publish_state() with retained
- [ ] Server: MQTTService拡張
  - [ ] sphere/all/command/# 購読
  - [ ] sphere/+/command/# 購読
- [ ] ESP32: コマンド購読
  - [ ] sphere/all/command/# 購読
  - [ ] sphere/{id}/command/# 購読
- [ ] ESP32: 状態購読・反映
  - [ ] sphere/all/state 購読
  - [ ] LED状態反映ロジック
- [ ] Web UI: MQTT統合
  - [ ] コマンド送信実装
  - [ ] 状態購読・UI反映

### Phase 2: Joystick統合 ⏳

- [ ] Joystick Controller開発
  - [ ] アナログ入力 → MQTTコマンド
  - [ ] ボタン入力 → MQTTコマンド
  - [ ] 状態購読 → LEDフィードバック

### Phase 3: Sphere入力 ⏳

- [ ] ESP32: シェイク検知
  - [ ] IMU閾値設定
  - [ ] シェイク → MQTTコマンド
- [ ] ESP32: ジェスチャー入力
  - [ ] 傾き → パラメータマッピング
  - [ ] input/shake, input/gesture送信

### Phase 4: 高度な機能 ⏳

- [ ] UI Locking機能
- [ ] 同期再生（マスター/スレーブ）
- [ ] コマンド履歴・Undo

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
