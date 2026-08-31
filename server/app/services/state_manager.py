import asyncio
import json
import logging
import threading
import time
from typing import Dict, Any, List, Optional
from datetime import datetime

from app.core.config import (
    DEVICE_OFFLINE_TIMEOUT_SEC,
    DEVICE_PRESENCE_SWEEP_SEC,
    DEVICE_TARGET_ALL,
    MQTT_COMMAND_TOPIC_PREFIX,
    MQTT_DEVICE_COMMAND_PREFIX_FMT,
    MQTT_STATE_TOPIC,
)

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
    _instance_lock = threading.Lock()

    def __new__(cls):
        if cls._instance is None:
            with cls._instance_lock:
                if cls._instance is None:
                    instance = super(StateManager, cls).__new__(cls)
                    instance._initialized = False
                    cls._instance = instance
        return cls._instance

    def __init__(self):
        if self._initialized:
            return
            
        # 初期状態
        self._state: Dict[str, Any] = {
            "imu": {"w": 1.0, "x": 0.0, "y": 0.0, "z": 0.0},
            # device_id ごとのIMU等 (複数sphere対応、WebUIでの選択用)。
            # 例: {"sphere001": {"imu": {...}, "last_seen": 1756...}, ...}
            # 一度 publish してきた core は落ちても残る (最後の姿勢を保持するため)。
            # 現在生きているかは last_seen ベースの "online" を見ること。
            "devices": {},
            # config.json の spheres[] から作るデバイスレジストリ (set_spheres)。
            # オフラインの core も含む全一覧で、WebUI の操作対象セレクタの選択肢になる。
            "spheres": [],
            # 現在の操作対象 core の id ("all" = 全 core にブロードキャスト)。
            "target": DEVICE_TARGET_ALL,
            # 死活判定を通ったオンライン core の id 一覧。配信直前に
            # _refresh_presence() が devices[].last_seen から毎回作り直す。
            # UI のインジケータはこの配列だけを見ればよい (時刻の突き合わせ不要)。
            "online": [],
            "playback": {
                "status": "stopped",  # "playing" | "paused" | "stopped"
                "playlist": None,
                "track": None,
                "position": 0.0,
                "duration": 0.0
            },
            "params": {
                "brightness": 50,
                "speed": 50,
                "hue": 120,
                "saturation": 100
            },
            "led": {
                "mode": "sphere",  # "sphere" | "pixels" | "off"
                "axis": False,  # XYZ軸オーバーレイ
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

    def set_spheres(self, spheres: List[Dict[str, Any]]):
        """config.json の spheres[] をデバイスレジストリとして取り込む (起動時)。

        _state は MQTT (retained) と WebSocket の両方に毎回まるごと載るため、
        ここでは UI が必要な最小項目だけに絞る (ファーム側の受信バッファは
        sastle::kMqttBufferSize = 2048B で、超えたメッセージは捨てられる)。
        LCD 等を含む全項目は GET /api/config/spheres で取得できる。
        """
        self._state["spheres"] = [
            {"id": s.get("id"), "static_ip": s.get("static_ip")}
            for s in (spheres or []) if s.get("id")
        ]
        logger.info(f"sphere registry: {[s['id'] for s in self._state['spheres']]}")

    def set_initial_target(self, device_id: str):
        """操作対象 core の初期値を設定する (起動時、config の active_sphere)。"""
        if device_id:
            self._state["target"] = device_id
            logger.info(f"initial command target: {device_id}")

    async def set_target(self, device_id: str):
        """操作対象 core を切り替え、WebSocket/MQTT に新しい状態を配信する。"""
        self._state["target"] = device_id
        logger.info(f"command target -> {device_id}")
        await self._publish_state()

    def get_target(self) -> str:
        return self._state.get("target") or DEVICE_TARGET_ALL

    # ===== core の死活判定 =====
    # core からの publish (IMU/status/log) が届くたびに last_seen を打ち、
    # DEVICE_OFFLINE_TIMEOUT_SEC 以上途切れた core をオフラインと見なす。
    # MQTT の LWT に頼らないのは、電源を抜かれた場合ブローカーが keepalive の
    # 満了を待つまで (既定で最大1.5倍) offline を出さず、検知が遅いため。

    def touch_device(self, device_id: str):
        """core から何か届いたことを記録する (死活判定用)。

        MQTT の受信スレッドから同期的に呼ばれる。dict への代入だけなので
        イベントループへ投入せずに済ませている (配信は次の notify に乗る)。
        """
        if not device_id:
            return
        devices = self._state.setdefault("devices", {})
        devices.setdefault(device_id, {})["last_seen"] = time.time()

    def online_ids(self) -> List[str]:
        """死活判定を通った core の id 一覧 (登録順ではなく id 昇順)。"""
        now = time.time()
        return sorted(
            device_id
            for device_id, entry in (self._state.get("devices") or {}).items()
            if now - (entry.get("last_seen") or 0) < DEVICE_OFFLINE_TIMEOUT_SEC
        )

    def _refresh_presence(self):
        """配信直前に "online" を作り直す。

        last_seen は「最後に届いた時刻」なので、時間が経つだけで online から
        外れる。配信のたびに再計算しないと、静かになった core が居残る。
        """
        self._state["online"] = self.online_ids()

    async def presence_sweep_loop(self, interval: float = DEVICE_PRESENCE_SWEEP_SEC,
                                  on_change=None):
        """online 集合の変化を監視して配信する asyncio タスク。

        core が完全に黙った場合は他の更新契機が無く、_notify_observers() が
        呼ばれないままオンライン表示が残り続ける。そのための定期チェック。
        変化が無ければ配信しないので、平常時のトラフィックは増えない。

        Args:
            on_change: online 集合が変わったとき online_ids を渡して呼ぶ同期関数。
                映像 (UDP) の宛先から落ちた core を外すために使う (main.py で接続)。
                StateManager に ConfigService/VideoStreamer を持たせたくないので
                コールバックで外に出す。
        """
        last = None
        while True:
            try:
                await asyncio.sleep(interval)
                current = self.online_ids()
                if current != last:
                    if last is not None:
                        gone = set(last) - set(current)
                        came = set(current) - set(last)
                        if gone:
                            logger.info(f"core offline: {sorted(gone)}")
                        if came:
                            logger.info(f"core online: {sorted(came)}")
                    last = current
                    if on_change:
                        try:
                            on_change(current)
                        except Exception as e:
                            logger.error(f"presence on_change failed: {e}")
                    await self._notify_observers()
            except asyncio.CancelledError:
                raise
            except Exception as e:  # 監視タスクは落とさない
                logger.error(f"presence sweep error: {e}")

    def set_initial_params(self, params: Dict[str, Any]):
        """起動時に config の既定パラメータを反映する（クライアント接続前の初期化用）。"""
        if isinstance(params, dict):
            self._state["params"].update({k: v for k, v in params.items() if k in self._state["params"]})
            logger.info(f"initial params applied from config: {self._state['params']}")
    
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

    async def update_device_imu(self, device_id: str, quat: Dict[str, Any]):
        """デバイス別のIMUクォータニオンを更新する (複数sphere対応)。

        既存の単一値 "imu" も後方互換のため最新受信値で更新する
        (WebUI がまだデバイス未選択の場合のフォールバック用)。
        """
        devices = self._state.setdefault("devices", {})
        entry = devices.setdefault(device_id, {})
        entry["imu"] = quat
        entry["last_seen"] = time.time()
        self._state["imu"] = quat
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

        # 軸オーバーレイ (mode と独立に切替可能)。UIの XYZ 選択状態の同期に使う
        if "axis" in payload:
            self._state["led"]["axis"] = bool(payload["axis"])

        if "mode" in payload:
            mode = payload["mode"]
            
            if mode not in ["sphere", "pixels", "off", "test"]:
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
        # 宛先 core。WebUI が選択中の id を毎メッセージに載せる (未指定ならサーバー既定)
        device = data.get("device") or self.get_target()

        logger.info(f"[WebSocket] Processing message: type={msg_type} device={device}")

        # WSメッセージ種別 → デバイスコマンドトピックの suffix
        COMMAND_SUFFIX = {"SET_PARAMS": "params", "SET_PLAYBACK": "playback", "SET_LED": "led"}

        if msg_type == "SET_PARAMS":
            await self._update_params(payload)
        elif msg_type == "SET_PLAYBACK":
            await self._update_playback(payload)
        elif msg_type == "SET_LED":
            await self._update_led(payload)
        else:
            logger.warning(f"Unknown WebSocket message type: {msg_type}")
            return

        # サーバー状態更新だけでなく、デバイスのコマンドトピックへも転送する。
        # (これが無いとUI操作が実機に届かない)
        suffix = COMMAND_SUFFIX.get(msg_type)
        if suffix:
            self._publish_command(suffix, payload, device)

    @staticmethod
    def _command_prefix(device: Optional[str]) -> str:
        """宛先 core に応じたコマンドトピックの prefix を返す。

        "all"/未指定 → sphere/all/command (全 core が購読)
        それ以外     → sphere/<id>/command (その core だけが購読)
        """
        if not device or device == DEVICE_TARGET_ALL:
            return MQTT_COMMAND_TOPIC_PREFIX
        return MQTT_DEVICE_COMMAND_PREFIX_FMT.format(device_id=device)

    def command_prefix(self, device: Optional[str] = None) -> str:
        """現在の操作対象 (または指定 core) 宛のコマンドトピック prefix を返す。

        MQTT を直接叩く REST エンドポイント (api/endpoints/command.py) からも
        同じ宛先解決を使うための公開版。
        """
        return self._command_prefix(device or self.get_target())

    def _publish_command(self, suffix: str, payload: Dict[str, Any], device: Optional[str] = None):
        """コマンドトピック <prefix>/<suffix> へ publish する。

        Args:
            suffix: params / playback / led / system
            device: 宛先 core の id。None なら現在の操作対象 (self.get_target())
        """
        if not self._mqtt_client:
            return
        topic = f"{self._command_prefix(device or self.get_target())}/{suffix}"
        try:
            self._mqtt_client.publish(topic, json.dumps(payload), qos=1)
            logger.debug(f"Command -> {topic}: {payload}")
        except Exception as e:
            logger.error(f"Failed to publish command to {topic}: {e}")
    
    async def _publish_state(self):
        """
        状態をMQTT(retained) + WebSocketで配信
        
        MQTT: sphere/all/state (retained)
        WebSocket: STATE_UPDATE message
        """
        # シーケンス番号とタイムスタンプを更新
        self._refresh_presence()
        self._seq += 1
        self._state["seq"] = self._seq
        self._state["timestamp"] = datetime.utcnow().isoformat() + "Z"
        
        # MQTT配信（retained）
        if self._mqtt_client:
            try:
                state_json = json.dumps(self._state)
                self._mqtt_client.publish(
                    MQTT_STATE_TOPIC,
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
        self._refresh_presence()
        await self._broadcast({"type": "STATE_UPDATE", "payload": self._state})

    async def _broadcast(self, message: Dict[str, Any]):
        """全 WebSocket クライアントへ任意のメッセージを配信する共通処理"""
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

    async def broadcast_log(self, line: str):
        """デバイスのデバッグログ1行を配信する (状態スナップショットには保持しない)"""
        await self._broadcast({"type": "LOG_LINE", "payload": {"line": line}})

    async def broadcast_frame_preview(self, jpeg_b64: str, w: int, h: int, seq: int):
        """再生中の映像プレビュー1枚(base64 JPEG)を配信する。

        UI 側デジタルツイン(LED 800点のライブ発色)用。LOG_LINE と同じ流儀で
        _state には保持せず、STATE_UPDATE とは別経路で流す (retained 不要・高頻度)。
        """
        await self._broadcast({
            "type": "FRAME_PREVIEW",
            "payload": {"jpeg_b64": jpeg_b64, "w": w, "h": h, "seq": seq},
        })
