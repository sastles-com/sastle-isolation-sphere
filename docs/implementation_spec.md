# 実装仕様書 (Implementation Specification)

最終更新: 2025-12-02

## 1. システム概要

Isolation Sphereは、ESP32で制御される球体LEDディスプレイと、Pythonバックエンド、Webフロントエンドで構成されるリアルタイム姿勢可視化システムです。

### 主要機能
1. IMUセンサー（BNO055）による姿勢検知
2. MQTT経由でのリアルタイムデータ送信
3. Webブラウザ上での3D球体可視化
4. WebSocketによる双方向通信
5. REST APIによる設定・制御

## 2. ESP32 実装詳細

### 2.1 ハードウェア
- **MCU**: ESP32-S3 (M5Atom S3R)
- **RAM**: 8MB PSRAM
- **Flash**: LittleFS (設定・画像保存)
- **IMU**: BNO055 (I2C接続)
- **LED**: WS2812B x 800個 (4ストリップ)
- **Display**: 128x128 LCD

### 2.2 主要コンポーネント

#### ConfigManager
```cpp
class ConfigManager {
public:
    bool loadConfig(const char* path = "/config.json");
    bool saveConfig(const char* path = "/config.json");
    
    String getWifiSSID();
    String getMQTTBroker();
    int getMQTTPort();
    String getDeviceID();
    
private:
    JsonDocument doc;
};
```

**実装ポイント:**
- ArduinoJsonでJSON解析
- LittleFSからconfig.json読み込み
- デフォルト値のフォールバック

#### IMUManager
```cpp
class IMUManager {
public:
    bool begin();
    bool update();
    bool getQuaternion(float &w, float &x, float &y, float &z);
    
private:
    Adafruit_BNO055 bno;
};
```

**実装ポイント:**
- BNO055から絶対方位モードでQuaternion取得
- キャリブレーション状態の確認
- エラーハンドリング

#### NetworkManager (MQTT)
```cpp
void loop() {
    if (millis() - lastIMUPublish > 100) { // 10Hz
        float w, x, y, z;
        if (imu.getQuaternion(w, x, y, z)) {
            char payload[128];
            snprintf(payload, sizeof(payload), 
                     "{\"w\":%.4f,\"x\":%.4f,\"y\":%.4f,\"z\":%.4f}",
                     w, x, y, z);
            mqtt.publish("sphere/sphere001/imu", payload, false);
        }
        lastIMUPublish = millis();
    }
}
```

**送信フォーマット:**
```json
{"w":0.9999,"x":0.0001,"y":0.0002,"z":-0.0003}
```

### 2.3 設定ファイル (config.json)

```json
{
  "wifi": {
    "SSID": "ESP32-P2P-Direct",
    "password": "isolation-sphere-p2p",
    "broker": "192.168.49.1",
    "mqtt_port": 1883
  },
  "sphere": {
    "id": "sphere001",
    "mac": "F0:9E:9E:32:67:D0",
    "features": {
      "IMU": "BNO055",
      "LED": true,
      "LCD": {
        "width": 128,
        "height": 128,
        "debug": true
      }
    }
  }
}
```

## 3. Python Server 実装詳細

### 3.1 MQTTService

```python
class MQTTService:
    def _load_broker_config(self):
        """config.jsonからブローカーアドレスを読み込み"""
        config_paths = [
            "../core/data/config.json",
            "data/config.json",
            "../data/config.json"
        ]
        for path in config_paths:
            if os.path.exists(path):
                with open(path, 'r') as f:
                    config = json.load(f)
                    broker = config.get("wifi", {}).get("broker", "localhost")
                    return broker
        return "localhost"
    
    def _handle_imu_data(self, payload):
        """IMUデータ処理（ESP32フォーマット対応）"""
        if "w" in payload and "x" in payload:
            # ESP32フォーマット: {"w":...,"x":...}
            quat = payload
        elif "quaternion" in payload:
            # 代替フォーマット: {"quaternion":{...}}
            quat = payload["quaternion"]
        
        # MQTTコールバックは別スレッドで実行されるため
        # asyncio.new_event_loop()で同期的に処理
        loop = asyncio.new_event_loop()
        loop.run_until_complete(
            self.state_manager.update_state("imu", {
                "w": quat.get("w", 1.0),
                "x": quat.get("x", 0.0),
                "y": quat.get("y", 0.0),
                "z": quat.get("z", 0.0)
            })
        )
        loop.close()
```

**実装の課題と解決策:**

**課題1**: MQTTコールバックは別スレッドで実行される  
**解決**: `asyncio.new_event_loop()`で新しいイベントループを作成

**課題2**: ESP32は直接`{w,x,y,z}`を送信  
**解決**: 両方のフォーマットに対応

### 3.2 StateManager

```python
class StateManager:
    def __init__(self):
        self._state = {
            "imu": {"w": 1.0, "x": 0.0, "y": 0.0, "z": 0.0},
            "playback": {"isPlaying": False},
            "params": {"brightness": 80, "speed": 50, "hue": 120},
            "system": {"fps": 60, "temp": 42.0}
        }
        self._observers = []  # WebSocket connections
    
    async def update_state(self, key: str, value: Any):
        self._state[key].update(value)
        await self._notify_observers()
    
    async def _notify_observers(self):
        message = {"type": "STATE_UPDATE", "payload": self._state}
        for observer in self._observers:
            await observer.send_json(message)
```

### 3.3 WebSocket Endpoint

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
            # クライアントからのコマンド処理
    except WebSocketDisconnect:
        state_manager.remove_observer(websocket)
```

### 3.4 API Router構成

```python
# main.py
app.include_router(websocket.router, tags=["websocket"])  # /ws
app.include_router(api_router, prefix="/api")  # /api/*

# api/router.py
api_router.include_router(config.router, prefix="/config")    # /api/config
api_router.include_router(playlist.router, prefix="/playlist") # /api/playlist
```

**重要**: WebSocketは `/ws` でプレフィックスなし、REST APIは `/api` プレフィックス付き

## 4. Web Frontend 実装詳細

### 4.1 WebSocketContext

```javascript
const WebSocketProvider = ({ children }) => {
    const connect = useCallback(() => {
        const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
        const host = window.location.hostname;
        const port = window.location.port || '9000';
        const url = `${protocol}//${host}:${port}/ws`;
        
        ws.current = new WebSocket(url);
        
        ws.current.onmessage = (event) => {
            const data = JSON.parse(event.data);
            setLastMessage(data);
        };
    }, []);
};
```

**実装ポイント:**
- `window.location.port`で動的にポート取得
- 自動再接続（3秒後）
- 状態管理は`lastMessage`で保持

### 4.2 HoloSphere (Three.js)

```javascript
const IMUControlledSphere = () => {
    const meshRef = useRef();
    const quaternionRef = useRef(new THREE.Quaternion(0, 0, 0, 1));
    const { lastMessage } = useWebSocket();
    
    useEffect(() => {
        if (lastMessage?.type === 'STATE_UPDATE' && lastMessage.payload?.imu) {
            const { w, x, y, z } = lastMessage.payload.imu;
            // Three.jsはx,y,z,w順
            quaternionRef.current.set(x, y, z, w);
        }
    }, [lastMessage]);
    
    useFrame(() => {
        if (meshRef.current) {
            meshRef.current.quaternion.copy(quaternionRef.current);
        }
    });
    
    return (
        <Sphere args={[1, 32, 32]} ref={meshRef} scale={2.2}>
            <meshStandardMaterial
                color="#00F0FF"
                wireframe={true}
                emissive="#00F0FF"
                emissiveIntensity={0.8}
            />
        </Sphere>
    );
};
```

**実装ポイント:**
- WebSocketからIMU状態を受信
- `useFrame`で毎フレーム球体に適用
- Three.js Quaternion順序: `(x, y, z, w)` ※ESP32は`(w, x, y, z)`

### 4.3 API呼び出し

```javascript
// 相対パス使用（動的にホスト・ポート対応）
const API_URL = '/api/playlist/playlists';

const fetchPlaylists = async () => {
    const response = await fetch(API_URL);
    const data = await response.json();
    setPlaylists(data);
};
```

**重要**: ハードコードされたURL（`http://localhost:8000`）は使用しない

## 5. データフォーマット仕様

### 5.1 MQTT Payload

#### IMU Data
```json
{
  "w": 0.9999,
  "x": 0.0001,
  "y": 0.0002,
  "z": -0.0003
}
```

#### Status Data
```json
{
  "status": "online",
  "timestamp": "2025-12-02T12:34:56Z",
  "uptime": 3600,
  "free_heap": 150000
}
```

### 5.2 WebSocket Messages

#### Server → Client (STATE_UPDATE)
```json
{
  "type": "STATE_UPDATE",
  "payload": {
    "imu": {
      "w": 0.9999,
      "x": 0.0001,
      "y": 0.0002,
      "z": -0.0003
    },
    "playback": {
      "isPlaying": false,
      "track": "None"
    },
    "params": {
      "brightness": 80,
      "speed": 50,
      "hue": 120
    },
    "system": {
      "fps": 60,
      "temp": 42.0
    }
  }
}
```

#### Client → Server (SET_PARAMS)
```json
{
  "type": "SET_PARAMS",
  "payload": {
    "brightness": 90,
    "hue": 180
  }
}
```

#### Client → Server (SET_PLAYBACK)
```json
{
  "type": "SET_PLAYBACK",
  "payload": {
    "action": "toggle"
  }
}
```

**Supported actions**: `"play"`, `"pause"`, `"stop"`, `"toggle"`

## 6. ビルド・デプロイ

### 6.1 ESP32
```bash
cd core
pio run -e atoms3r_bno055          # ビルド
pio run -e atoms3r_bno055 -t upload # アップロード
pio run -e atoms3r_bno055 -t uploadfs # LittleFS書き込み
```

### 6.2 Python Server
```bash
cd server
python3 -m uvicorn app.main:app --reload --host 0.0.0.0 --port 9000
```

### 6.3 Frontend
```bash
cd server/frontend
npm install
npm run build  # dist/ に出力
```

## 7. トラブルシューティング

### MQTT接続エラー
**症状**: `MQTT client not available`  
**原因**: paho-mqtt未インストール  
**解決**: `pip install paho-mqtt` (system-site-packages使用)

### WebSocket接続エラー
**症状**: ポート8000に接続しようとする  
**原因**: ブラウザキャッシュ  
**解決**: Ctrl+Shift+R で完全リロード

### 球体が動かない
**症状**: IMUデータ受信しているが球体が静止  
**原因**: asyncio event loop エラー  
**解決**: `asyncio.new_event_loop()` 使用（実装済み）

## 8. パフォーマンス

- **IMU送信頻度**: 10Hz (100ms間隔)
- **WebSocket遅延**: <50ms (LAN環境)
- **3D描画**: 60FPS (Three.js)
- **MQTT QoS**: 0 (最新データ優先)

## 9. セキュリティ考慮事項

- WiFiパスワードはconfig.jsonに平文保存（改善予定）
- MQTT認証なし（ローカルネットワーク前提）
- CORS: 開発環境では全許可（`allow_origins=["*"]`）
