# Brightness同期テスト手順

## 概要
WebUIのブライトネススライダーとESP32の状態が、MQTTとStateManagerを通じて双方向同期される機能のテストです。

## システム構成

```
WebUI (Browser) ─────┐
                     │
                     ├─> FastAPI Server ──> StateManager ──> MQTT Broker
                     │                           │               │
                     └──────> WebSocket <────────┘               │
                                                                 │
ESP32 <──────────────────────────────────────────────────────────┘
```

## 前提条件

1. **MQTTブローカー起動** (192.168.49.1:1883)
2. **StateManager起動** (FastAPI server内)
3. **ESP32起動** (sphere001として接続)
4. **WebUIアクセス** (http://100.88.207.117:9000)

## テスト手順

### 1. 初期状態確認

#### MQTT State監視
```bash
mosquitto_sub -h 192.168.49.1 -t "sphere/all/state" -v
```

期待される出力（初期状態）:
```json
sphere/all/state {
  "imu": {"w": 1.0, "x": 0.0, "y": 0.0, "z": 0.0},
  "playback": {"status": "stopped", ...},
  "params": {
    "brightness": 80,
    "speed": 50,
    "hue": 120,
    "saturation": 100
  },
  ...
}
```

#### ESP32シリアルモニタ確認
- 接続時に`Subscribed to: sphere/all/state`が表示されることを確認

---

### 2. WebUIからの変更テスト

#### 操作
1. WebUIにアクセス
2. **SPHERE CONTROL**パネルを開く
3. **GLOBAL BRIGHTNESS**スライダーを動かす（例: 85%）

#### 期待される動作

**A. StateManagerログ** (サーバー側)
```
[INFO] Params command received: {'brightness': 85}
[INFO] Setting brightness to 85
[INFO] Params state updated: {'brightness': 85, 'speed': 50, 'hue': 120, 'saturation': 100}
[DEBUG] Published state to MQTT (seq=1)
```

**B. MQTT State更新** (`mosquitto_sub`で確認)
```json
{
  "params": {
    "brightness": 85,  // ← 更新された
    ...
  },
  "timestamp": "2025-12-02T14:30:00.123Z",
  "seq": 1
}
```

**C. ESP32シリアルモニタ**
```
[MQTT] Message arrived [sphere/all/state]: {...}

=== STATE UPDATE ===
  Brightness: 85%
==================
```

**D. WebUIスライダー**
- 他のタブで開いているWebUIでも85%に同期される（WebSocket経由）

---

### 3. MQTT経由での変更テスト

#### 操作
別ターミナルでMQTTコマンド送信:
```bash
mosquitto_pub -h 192.168.49.1 -t "sphere/all/command/params" \
  -m '{"brightness": 50}'
```

#### 期待される動作

**A. StateManagerログ**
```
[INFO] Handling command from sphere/all/command/params: {'brightness': 50}
[INFO] Params command received: {'brightness': 50}
[INFO] Setting brightness to 50
[INFO] Params state updated: {'brightness': 50, ...}
[DEBUG] Published state to MQTT (seq=2)
```

**B. ESP32シリアルモニタ**
```
=== STATE UPDATE ===
  Brightness: 50%
==================
```

**C. WebUIスライダー**
- **自動的に50%の位置に移動する**（WebSocket STATE_UPDATEで同期）

---

### 4. 複数デバイス同時テスト

#### 準備
- 複数のブラウザタブでWebUIを開く
- ESP32も接続

#### 操作
いずれかのWebUIでブライトネスを変更（例: 70%）

#### 期待される動作
- **全てのWebUIタブ**でスライダーが70%に同期
- **ESP32ログ**にも`Brightness: 70%`が表示
- **mosquitto_sub**で状態更新が確認できる

---

## 実装詳細

### フロントエンド (SphereControl.jsx)

```javascript
// WebSocket経由で状態更新を受信
useEffect(() => {
    if (lastMessage && lastMessage.type === 'STATE_UPDATE') {
        const state = lastMessage.payload;
        if (state.params && state.params.brightness !== undefined) {
            setBrightness(state.params.brightness);  // スライダー同期
        }
    }
}, [lastMessage]);

// スライダー変更時にMQTTコマンド送信
const handleBrightnessChange = async (value) => {
    setBrightness(value);  // UI即時更新
    
    await fetch('/api/command/params', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ brightness: value }),
    });
};
```

### バックエンド (StateManager)

```python
async def _update_params(self, payload: Dict[str, Any]):
    """パラメータ更新とMQTT配信"""
    if "brightness" in payload:
        value = max(0, min(100, payload["brightness"]))  // 範囲制限
        self._state["params"]["brightness"] = value
        await self._publish_state()  # MQTT + WebSocketで配信
```

### ESP32 (main.cpp)

```cpp
// MQTT接続時にトピック購読
subscribe("sphere/all/state");

// メッセージ受信時にbrightness抽出
void mqttCallback(char* topic, uint8_t* payload, unsigned int length) {
    if (strstr(topic, "sphere/all/state") != NULL) {
        // JSON簡易パース
        const char* brightnessPtr = strstr(message, "\"brightness\":");
        if (brightnessPtr) {
            int brightness = atoi(brightnessPtr + 13);
            Serial.printf("  Brightness: %d%%\n", brightness);
            // TODO: ledManager.setBrightness(brightness);
        }
    }
}
```

---

## トラブルシューティング

### WebUIでスライダーが動かない
- **確認**: ブラウザのコンソールでエラーチェック
- **確認**: WebSocket接続状態 (`isConnected: true`)
- **修正**: サーバー再起動後、ブラウザリロード

### ESP32にログが出ない
- **確認**: `mosquitto_sub -t "sphere/all/state"`で配信を確認
- **確認**: ESP32のMQTT接続状態 (`mqtt.isConnected()`)
- **修正**: ESP32再起動、MQTT再接続

### スライダーを動かしてもStateが更新されない
- **確認**: サーバーログで`Params command received`を確認
- **確認**: `/api/command/params`のHTTPレスポンスコード
- **修正**: StateManagerの初期化状態を確認

---

## 次のステップ

1. **ArduinoJson統合** - ESP32でJSONパースを正式実装
2. **LEDManager連携** - `setBrightness()`実装
3. **他のパラメータ同期** - speed, hue, saturation
4. **Playback同期** - play/pause/stopボタン

