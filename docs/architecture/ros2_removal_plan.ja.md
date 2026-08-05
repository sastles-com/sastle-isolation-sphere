> [English](ros2_removal_plan.md) · **日本語**

# ROS2削除計画

作成日: 2025-12-02

## 背景

当初、micro-ROS/ROS2を使用する設計だったが、以下の理由により削除を決定：

1. **過剰設計**: プロセス間通信にROS2の複雑さは不要
2. **MQTT+WebSocketで十分**: 既存プロトコルで要件を満たせる
3. **Joystickの用途**: 単純な入力→MQTT送信のみ、状態監視不要
4. **保守性**: シンプルなアーキテクチャの方が理解・保守が容易

---

## 削除対象ファイル

### 1. `server/app/core/ros_manager.py`
- **現状**: ROS2初期化とモック実装
- **対応**: **完全削除**

### 2. `server/app/services/ros_bridge.py`
- **現状**: ROS2トピックとWebSocket/MQTTのブリッジ
- **対応**: **完全削除**

### 3. `server/joystick/daemon.py`
- **現状**: ROS2トピックにジョイスティック入力をパブリッシュ
- **対応**: **シンプル化** - MQTT直接パブリッシュ版に書き換え

---

## 修正対象ファイル

### 1. `server/app/main.py`

#### Before
```python
from app.core.ros_manager import ROSManager

@asynccontextmanager
async def lifespan(app: FastAPI):
    loop = asyncio.get_event_loop()
    state_manager = StateManager()
    ros_manager = ROSManager()
    ros_manager.start()
    mqtt_service = MQTTService()
    mqtt_service.state_manager = state_manager
    mqtt_service.set_event_loop(loop)
    state_manager.set_mqtt_client(mqtt_service.client)
    mqtt_service.start()
    app.state.state_manager = state_manager
    app.state.mqtt_service = mqtt_service
    yield
    mqtt_service.stop()
    ros_manager.stop()
```

#### After
```python
@asynccontextmanager
async def lifespan(app: FastAPI):
    loop = asyncio.get_event_loop()
    
    state_manager = StateManager()
    mqtt_service = MQTTService()
    
    mqtt_service.state_manager = state_manager
    mqtt_service.set_event_loop(loop)
    state_manager.set_mqtt_client(mqtt_service.client)
    
    mqtt_service.start()
    
    app.state.state_manager = state_manager
    app.state.mqtt_service = mqtt_service
    
    yield
    
    mqtt_service.stop()
```

---

### 2. `server/app/api/endpoints/websocket.py`

#### Before
```python
from app.services.ros_bridge import ROSBridge

router = APIRouter()
state_manager = StateManager()
ros_bridge = None

@router.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    global ros_bridge
    if ros_bridge is None:
        ros_bridge = ROSBridge()
    
    await websocket.accept()
    state_manager.add_observer(websocket)
    await websocket.send_json({"type": "STATE_UPDATE", "payload": state_manager.get_state()})
    
    try:
        while True:
            data = await websocket.receive_json()
            # ...
            await ros_bridge.handle_frontend_message(data)
    except WebSocketDisconnect:
        state_manager.remove_observer(websocket)
```

#### After
```python
router = APIRouter()
state_manager = StateManager()

@router.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    await websocket.accept()
    state_manager.add_observer(websocket)
    
    # 初期状態送信
    await websocket.send_json({
        "type": "STATE_UPDATE", 
        "payload": state_manager.get_state()
    })
    
    try:
        while True:
            data = await websocket.receive_json()
            print(f"[WebSocket] Received: {data}")
            
            # StateManagerに処理を委譲
            await state_manager.handle_websocket_message(data)
            
    except WebSocketDisconnect:
        state_manager.remove_observer(websocket)
```

---

### 3. `server/app/services/state_manager.py`

#### 追加メソッド

```python
async def handle_websocket_message(self, data: dict):
    """
    WebSocketメッセージを処理してMQTT送信
    
    Args:
        data: {"type": "SET_PARAMS", "payload": {...}}
    """
    msg_type = data.get("type")
    payload = data.get("payload", {})
    
    if msg_type == "SET_PARAMS":
        await self._update_params(payload)
    
    elif msg_type == "SET_PLAYBACK":
        await self._update_playback(payload)
    
    elif msg_type == "SET_LED":
        await self._update_led(payload)
    
    else:
        logger.warning(f"Unknown message type: {msg_type}")
```

---

### 4. `server/joystick/daemon.py`

#### Before（ROS2使用）
```python
import rclpy
from rclpy.node import Node
from std_msgs.msg import String

class JoystickDaemon:
    def __init__(self):
        rclpy.init()
        self.node = Node("joystick_daemon")
        self.pub = self.node.create_publisher(String, '/isolation_sphere/ui/control', 10)
    
    def publish_event(self, event):
        ros_msg = String()
        ros_msg.data = json.dumps(event)
        self.pub.publish(ros_msg)
```

#### After（MQTT直接パブリッシュ）
```python
import paho.mqtt.client as mqtt
import evdev
import json

class JoystickDaemon:
    def __init__(self, broker_host="192.168.49.1", broker_port=1883):
        self.mqtt_client = mqtt.Client()
        self.mqtt_client.connect(broker_host, broker_port)
        self.mqtt_client.loop_start()
        
        self.device = None
    
    def run(self):
        # ジョイスティックデバイス検索
        devices = [evdev.InputDevice(path) for path in evdev.list_devices()]
        # PS4コントローラーを検索
        for device in devices:
            if "Sony" in device.name or "Wireless Controller" in device.name:
                self.device = device
                break
        
        if not self.device:
            print("Joystick not found!")
            return
        
        print(f"Connected to: {self.device.name}")
        
        # イベントループ
        for event in self.device.read_loop():
            self.handle_event(event)
    
    def handle_event(self, event):
        if event.type == evdev.ecodes.EV_KEY:
            # ボタン押下
            if event.code == evdev.ecodes.BTN_A and event.value == 1:
                # 再生トグル
                self.publish_command("playback", {"action": "toggle"})
            
            elif event.code == evdev.ecodes.BTN_X and event.value == 1:
                # 次のトラック
                self.publish_command("playback", {"action": "next"})
    
    def publish_command(self, command_type, payload):
        topic = f"sphere/all/command/{command_type}"
        message = json.dumps(payload)
        self.mqtt_client.publish(topic, message)
        print(f"[Joystick] Published to {topic}: {message}")

if __name__ == "__main__":
    daemon = JoystickDaemon()
    daemon.run()
```

---

## 削除手順

### Step 1: ファイル削除
```bash
cd <repo-root>/server

# ROS2関連ファイル削除
rm app/core/ros_manager.py
rm app/services/ros_bridge.py
```

### Step 2: インポート削除
```bash
# main.pyからROSManager削除
# websocket.pyからROSBridge削除
```

### Step 3: Joystick Daemon書き換え
```bash
# joystick/daemon.py を上記のMQTT版に書き換え
```

### Step 4: requirements更新
```bash
# pyproject.toml または requirements.txt から rclpy 削除
```

### Step 5: 動作確認
```bash
# サーバー起動
uvicorn app.main:app --reload --host 0.0.0.0 --port 9000

# WebSocketテスト
# ブラウザでUIを開いて動作確認
```

---

## 影響範囲チェックリスト

- [ ] `app/main.py` - ROSManager起動削除
- [ ] `app/api/endpoints/websocket.py` - ROSBridge削除
- [ ] `app/services/state_manager.py` - `handle_websocket_message`追加
- [ ] `joystick/daemon.py` - MQTT直接パブリッシュ版に書き換え
- [ ] `app/core/ros_manager.py` - ファイル削除
- [ ] `app/services/ros_bridge.py` - ファイル削除
- [ ] `pyproject.toml` - rclpy依存削除
- [ ] `README.md` - ROS2関連記述削除
- [ ] `server/requirements_specification.md` - micro-ROS記述削除

---

## テスト項目

### 1. WebSocket通信テスト
- [ ] WebUI接続確認
- [ ] パラメータ変更（brightness等）が反映される
- [ ] ESP32からのIMUデータがリアルタイム表示される

### 2. MQTT通信テスト
- [ ] `sphere/all/command/*` がESP32に届く
- [ ] `sphere/{id}/imu` がServerに届く
- [ ] `sphere/{id}/state` がretainedで保持される

### 3. Joystick通信テスト（実装後）
- [ ] ボタン押下でMQTTコマンドが送信される
- [ ] コマンドがESP32に反映される
- [ ] WebUIに状態変更が表示される

---

## ロールバック手順

万が一問題が発生した場合：

```bash
# gitで以前のコミットに戻る
git log --oneline
git revert <commit-hash>
```

---

## 完了条件

- [ ] ROS2関連コードが完全に削除されている
- [ ] サーバーがROS2なしで起動する
- [ ] WebSocket通信が正常に動作する
- [ ] MQTT通信が正常に動作する
- [ ] ドキュメントが更新されている

