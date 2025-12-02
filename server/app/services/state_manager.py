import asyncio
import json
import logging
from typing import Dict, Any, List, Optional
from datetime import datetime

logger = logging.getLogger(__name__)

class StateManager:
    """
    StateManager - 唯一の状態管理者
    
    責務:
    - コマンド受信・処理
    - 状態更新
    - MQTT/WebSocketへの状態配信
    
    原則:
    - すべてのUI/デバイスからのコマンドを受け付ける
    - 状態決定権はStateManagerのみが持つ
    - 変更された状態はMQTT(retained) + WebSocketで配信
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
            
        # 初期状態
        self._state: Dict[str, Any] = {
            "imu": {"w": 1.0, "x": 0.0, "y": 0.0, "z": 0.0},
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
                "pixels": []  # [{index: int, r: int, g: int, b: int}, ...]
            },
            "system": {
                "fps": 60,
                "temp": 42.0
            },
            "timestamp": datetime.utcnow().isoformat() + "Z",
            "seq": 0
        }
        
        self._observers: List[Any] = []  # WebSocket connections
        self._mqtt_client: Optional[Any] = None
        self._seq = 0
        self._initialized = True
        
        logger.info("StateManager initialized")

    def set_mqtt_client(self, mqtt_client):
        """MQTTクライアントを設定（MQTTServiceから呼ばれる）"""
        self._mqtt_client = mqtt_client
        logger.info("MQTT client set in StateManager")
    
    def get_state(self) -> Dict[str, Any]:
        """現在の状態を取得"""
        return self._state

    async def update_state(self, key: str, value: Any):
        """状態を更新（IMUなど直接更新用）"""
        if key in self._state and isinstance(self._state[key], dict) and isinstance(value, dict):
            self._state[key].update(value)
        else:
            self._state[key] = value
            
        await self._notify_observers()
    
    async def handle_command(self, topic: str, payload: Dict[str, Any]):
        """
        MQTTコマンドを処理
        
        Args:
            topic: MQTTトピック（例: sphere/all/command/playback）
            payload: コマンドペイロード
        """
        try:
            logger.info(f"Handling command from {topic}: {payload}")
            
            # トピックからコマンドタイプを抽出
            # sphere/all/command/playback → playback
            # sphere/all/command/params → params
            parts = topic.split('/')
            if len(parts) >= 4 and parts[2] == 'command':
                command_type = parts[3]
                
                if command_type == 'playback':
                    await self._update_playback(payload)
                elif command_type == 'params':
                    await self._update_params(payload)
                elif command_type == 'led':
                    await self._update_led(payload)
                else:
                    logger.warning(f"Unknown command type: {command_type}")
            else:
                logger.warning(f"Invalid command topic format: {topic}")
                
        except Exception as e:
            logger.error(f"Error handling command: {e}", exc_info=True)
    
    async def _update_playback(self, payload: Dict[str, Any]):
        """
        Playbackコマンドを処理
        
        コマンド例:
        - {"action": "play", "playlist": "morning", "track": "demo01"}
        - {"action": "pause"}
        - {"action": "stop"}
        - {"action": "toggle"}
        """
        action = payload.get("action")
        current_status = self._state["playback"]["status"]
        
        if action == "play":
            self._state["playback"]["status"] = "playing"
            if "playlist" in payload:
                self._state["playback"]["playlist"] = payload["playlist"]
            if "track" in payload:
                self._state["playback"]["track"] = payload["track"]
        
        elif action == "pause":
            if current_status == "playing":
                self._state["playback"]["status"] = "paused"
        
        elif action == "stop":
            self._state["playback"]["status"] = "stopped"
            self._state["playback"]["position"] = 0.0
        
        elif action == "toggle":
            if current_status == "playing":
                self._state["playback"]["status"] = "paused"
            elif current_status in ["paused", "stopped"]:
                self._state["playback"]["status"] = "playing"
        
        else:
            logger.warning(f"Unknown playback action: {action}")
            return
        
        logger.info(f"Playback updated: {self._state['playback']}")
        await self._publish_state()
    
    async def _update_params(self, payload: Dict[str, Any]):
        """
        パラメータコマンドを処理
        
        コマンド例:
        - {"brightness": 85}
        - {"speed": 60, "hue": 180}
        - {"brightness": 100, "saturation": 80}
        """
        logger.info(f"Params command received: {payload}")
        updated = False
        
        for key in ["brightness", "speed", "hue", "saturation"]:
            if key in payload:
                value = payload[key]
                # 範囲チェック
                if key in ["brightness", "speed", "saturation"]:
                    value = max(0, min(100, value))
                elif key == "hue":
                    value = max(0, min(360, value))
                
                logger.info(f"Setting {key} to {value}")
                self._state["params"][key] = value
                updated = True
        
        if updated:
            logger.info(f"Params state updated: {self._state['params']}")
            await self._publish_state()
        else:
            logger.warning(f"No valid params in payload: {payload}")
    
    async def _update_led(self, payload: Dict[str, Any]):
        """
        LEDコマンドを処理
        
        コマンド例:
        - {"mode": "sphere"}
        - {"mode": "pixels", "pixels": [{"index": 0, "r": 255, "g": 0, "b": 0}, ...]}
        - {"mode": "off"}
        """
        logger.info(f"LED command received: {payload}")
        
        if "mode" in payload:
            mode = payload["mode"]
            
            if mode not in ["sphere", "pixels", "off"]:
                logger.warning(f"Invalid LED mode: {mode}")
                return
            
            logger.info(f"Setting LED mode to: {mode}")
            self._state["led"]["mode"] = mode
            
            # pixelsモードの場合、ピクセルデータを更新
            if mode == "pixels" and "pixels" in payload:
                pixels = payload["pixels"]
                logger.info(f"Processing {len(pixels) if isinstance(pixels, list) else 0} pixel data")
                # バリデーション
                if isinstance(pixels, list):
                    validated_pixels = []
                    for pixel in pixels:
                        if isinstance(pixel, dict) and all(k in pixel for k in ["index", "r", "g", "b"]):
                            validated_pixels.append({
                                "index": int(pixel["index"]),
                                "r": max(0, min(255, int(pixel["r"]))),
                                "g": max(0, min(255, int(pixel["g"]))),
                                "b": max(0, min(255, int(pixel["b"])))
                            })
                    self._state["led"]["pixels"] = validated_pixels
                    logger.info(f"Validated {len(validated_pixels)} pixels")
            elif mode != "pixels":
                # sphere/offモードの場合はpixelsをクリア
                self._state["led"]["pixels"] = []
                logger.info(f"Cleared pixel data for mode: {mode}")
            
            logger.info(f"LED state updated: mode={self._state['led']['mode']}, pixels_count={len(self._state['led']['pixels'])}")
            await self._publish_state()
        else:
            logger.warning(f"No mode in LED payload: {payload}")
    
    async def handle_websocket_message(self, data: dict):
        """
        WebSocketメッセージを処理してMQTT送信
        
        Args:
            data: {"type": "SET_PARAMS", "payload": {...}}
        """
        msg_type = data.get("type")
        payload = data.get("payload", {})
        
        logger.info(f"[WebSocket] Processing message: type={msg_type}")
        
        if msg_type == "SET_PARAMS":
            await self._update_params(payload)
        
        elif msg_type == "SET_PLAYBACK":
            await self._update_playback(payload)
        
        elif msg_type == "SET_LED":
            await self._update_led(payload)
        
        else:
            logger.warning(f"Unknown WebSocket message type: {msg_type}")
    
    async def _publish_state(self):
        """
        状態をMQTT(retained) + WebSocketで配信
        
        MQTT: sphere/all/state (retained)
        WebSocket: STATE_UPDATE message
        """
        # シーケンス番号とタイムスタンプを更新
        self._seq += 1
        self._state["seq"] = self._seq
        self._state["timestamp"] = datetime.utcnow().isoformat() + "Z"
        
        # MQTT配信（retained）
        if self._mqtt_client:
            try:
                state_json = json.dumps(self._state)
                self._mqtt_client.publish(
                    "sphere/all/state",
                    state_json,
                    qos=1,
                    retain=True  # 重要: 新規接続時に最新状態を自動取得
                )
                logger.debug(f"Published state to MQTT (seq={self._seq})")
            except Exception as e:
                logger.error(f"Failed to publish state to MQTT: {e}")
        
        # WebSocket配信
        await self._notify_observers()

    def add_observer(self, observer):
        """WebSocket接続を追加"""
        self._observers.append(observer)
        logger.debug(f"Observer added. Total: {len(self._observers)}")

    def remove_observer(self, observer):
        """WebSocket接続を削除"""
        if observer in self._observers:
            self._observers.remove(observer)
            logger.debug(f"Observer removed. Total: {len(self._observers)}")

    async def _notify_observers(self):
        """WebSocketクライアントに状態を配信"""
        message = {"type": "STATE_UPDATE", "payload": self._state}
        
        disconnected = []
        for observer in self._observers:
            try:
                await observer.send_json(message)
            except Exception as e:
                logger.warning(f"Failed to send to observer: {e}")
                disconnected.append(observer)
        
        # 切断されたクライアントを削除
        for observer in disconnected:
            self.remove_observer(observer)
