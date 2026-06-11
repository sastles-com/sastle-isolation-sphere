"""
MQTT Service for receiving IMU data from ESP32 devices
"""
import asyncio
import json
import os
import threading
from typing import Optional
import logging

try:
    import paho.mqtt.client as mqtt
    MQTT_AVAILABLE = True
except ImportError:
    MQTT_AVAILABLE = False
    logging.warning("paho-mqtt not installed. MQTT functionality disabled.")

from app.core.config import (
    CONFIG_SEARCH_PATHS,
    MQTT_BROKER_PORT,
    MQTT_CLIENT_ID,
    MQTT_COMMAND_TOPIC_WILDCARD,
    MQTT_DEVICE_ID,
)
from app.services.state_manager import StateManager

logger = logging.getLogger(__name__)


class MQTTService:
    """
    MQTT Client Service for subscribing to ESP32 IMU data
    """
    _instance = None
    _instance_lock = threading.Lock()

    def __new__(cls):
        if cls._instance is None:
            with cls._instance_lock:
                if cls._instance is None:
                    instance = super(MQTTService, cls).__new__(cls)
                    instance._initialized = False
                    cls._instance = instance
        return cls._instance

    def __init__(self):
        if self._initialized:
            return
            
        self.client: Optional[mqtt.Client] = None
        self.state_manager: Optional[StateManager] = None  # 外部からセット
        self._loop = None  # set_event_loop() でセットされる

        # Load config from config.json
        self.broker_host = self._load_broker_config()
        self.broker_port = MQTT_BROKER_PORT
        self.device_id = MQTT_DEVICE_ID
        self.is_connected = False
        self._initialized = True
        
        if MQTT_AVAILABLE:
            self._setup_client()
        else:
            logger.warning("MQTT client not available. Using mock data.")
    
    def _load_broker_config(self):
        """Load MQTT broker address.

        優先順位: 環境変数 SPHERE_MQTT_BROKER > config.json(wifi.broker) > localhost。
        env override はローカル検証やデプロイ時に config.json を書き換えず
        ブローカーを差し替えるために使う。
        """
        env_broker = os.environ.get("SPHERE_MQTT_BROKER")
        if env_broker:
            logger.info(f"Using MQTT broker from SPHERE_MQTT_BROKER env: {env_broker}")
            return env_broker
        try:
            for path in CONFIG_SEARCH_PATHS:
                if os.path.exists(path):
                    with open(path, 'r') as f:
                        config = json.load(f)
                        broker = config.get("wifi", {}).get("broker", "localhost")
                        logger.info(f"Loaded MQTT broker from config: {broker}")
                        return broker
            logger.warning("Config file not found, using localhost")
            return "localhost"
        except Exception as e:
            logger.error(f"Failed to load config: {e}, using localhost")
            return "localhost"

    def _setup_client(self):
        """Initialize MQTT client"""
        self.client = mqtt.Client(client_id=MQTT_CLIENT_ID)
        self.client.on_connect = self._on_connect
        self.client.on_disconnect = self._on_disconnect
        self.client.on_message = self._on_message
        
    def _on_connect(self, client, userdata, flags, rc):
        """Callback when connected to MQTT broker"""
        if rc == 0:
            logger.info(f"Connected to MQTT broker at {self.broker_host}:{self.broker_port}")
            self.is_connected = True
            
            # Subscribe to IMU topic
            topic = f"sphere/{self.device_id}/imu"
            client.subscribe(topic)
            logger.info(f"Subscribed to topic: {topic}")
            
            # Subscribe to status topic
            status_topic = f"sphere/{self.device_id}/status"
            client.subscribe(status_topic)
            logger.info(f"Subscribed to topic: {status_topic}")
            
            # Subscribe to command topics (wildcard for all command types)
            command_topic = MQTT_COMMAND_TOPIC_WILDCARD
            client.subscribe(command_topic)
            logger.info(f"Subscribed to command topic: {command_topic}")
        else:
            logger.error(f"Failed to connect to MQTT broker. Return code: {rc}")
            self.is_connected = False

    def _on_disconnect(self, client, userdata, rc):
        """Callback when disconnected from MQTT broker"""
        logger.warning(f"Disconnected from MQTT broker. Return code: {rc}")
        self.is_connected = False
        
    def _on_message(self, client, userdata, msg):
        """Callback when message received from MQTT broker"""
        try:
            topic = msg.topic
            payload = json.loads(msg.payload.decode())
            
            logger.debug(f"Received MQTT message on {topic}: {payload}")
            
            # Handle command topics
            if "/command/" in topic:
                self._handle_command(topic, payload)
            # Handle IMU data
            elif topic.endswith("/imu"):
                self._handle_imu_data(payload)
            # Handle status data
            elif topic.endswith("/status"):
                self._handle_status_data(payload)
                
        except json.JSONDecodeError as e:
            logger.error(f"Failed to decode MQTT message: {e}")
        except Exception as e:
            logger.error(f"Error handling MQTT message: {e}")

    def _submit_coroutine(self, coro, context: str) -> bool:
        """MQTTスレッドからイベントループにコルーチンを投入する共通処理"""
        if self._loop:
            asyncio.run_coroutine_threadsafe(coro, self._loop)
            return True
        coro.close()
        logger.warning(f"Event loop not set, cannot {context}")
        return False

    def _handle_command(self, topic: str, payload: dict):
        """Handle command messages - delegate to StateManager"""
        logger.info(f"[MQTT] Received command on {topic}: {payload}")

        if self.state_manager:
            try:
                # MQTT callback runs in different thread, use run_coroutine_threadsafe
                if self._submit_coroutine(
                    self.state_manager.handle_command(topic, payload),
                    "handle command"
                ):
                    logger.info(f"[MQTT] Command delegated to StateManager")
            except Exception as e:
                logger.error(f"Error delegating command to StateManager: {e}")
        else:
            logger.warning("StateManager not set, cannot handle command")

    def _handle_imu_data(self, payload):
        """Handle IMU quaternion data"""
        if not self.state_manager:
            logger.warning("StateManager not set, skipping IMU update")
            return

        try:
            # ESP32 sends format: {"w": 1.0, "x": 0.0, "y": 0.0, "z": 0.0}
            # (without "quaternion" wrapper)
            quat = None
            if "w" in payload and "x" in payload:
                quat = payload
            elif "quaternion" in payload:
                # Alternative format with wrapper
                quat = payload["quaternion"]

            if quat:
                # MQTT callback runs in different thread, use run_coroutine_threadsafe
                if self._submit_coroutine(
                    self.state_manager.update_state("imu", {
                        "w": quat.get("w", 1.0),
                        "x": quat.get("x", 0.0),
                        "y": quat.get("y", 0.0),
                        "z": quat.get("z", 0.0)
                    }),
                    "update IMU"
                ):
                    logger.debug(f"Updated IMU quaternion: {quat}")
        except Exception as e:
            logger.error(f"Error handling IMU data: {e}")

    def _handle_status_data(self, payload):
        """Handle device status data"""
        # TODO: MQTTスレッド内で新しいイベントループを生成しており、
        # メインループ上の observers/_state と競合しうる。
        # _submit_coroutine() に統一すべきだが挙動が変わるため別タスクで対応。
        if not self.state_manager:
            logger.warning("StateManager not set, skipping status update")
            return

        try:
            # Update system state with device status
            try:
                loop = asyncio.get_event_loop()
            except RuntimeError:
                loop = asyncio.new_event_loop()
                asyncio.set_event_loop(loop)
            
            if loop.is_running():
                asyncio.create_task(
                    self.state_manager.update_state("system", {
                        **self.state_manager.get_state().get("system", {}),
                        "device_status": payload.get("status", "unknown"),
                        "last_update": payload.get("timestamp", "")
                    })
                )
            else:
                loop.run_until_complete(
                    self.state_manager.update_state("system", {
                        **self.state_manager.get_state().get("system", {}),
                        "device_status": payload.get("status", "unknown"),
                        "last_update": payload.get("timestamp", "")
                    })
                )
        except Exception as e:
            logger.error(f"Error handling status data: {e}")

    def set_event_loop(self, loop):
        """Set event loop for async operations (called from main.py)"""
        self._loop = loop
        logger.info("Event loop set in MQTTService")
    
    def start(self):
        """Start MQTT client connection"""
        if not MQTT_AVAILABLE:
            logger.warning("MQTT not available, skipping connection")
            return
            
        if self.client:
            try:
                logger.info(f"Connecting to MQTT broker at {self.broker_host}:{self.broker_port}")
                self.client.connect_async(self.broker_host, self.broker_port, keepalive=60)
                self.client.loop_start()
            except Exception as e:
                logger.error(f"Failed to start MQTT client: {e}")

    def stop(self):
        """Stop MQTT client connection"""
        if self.client:
            logger.info("Stopping MQTT client")
            self.client.loop_stop()
            self.client.disconnect()
            self.is_connected = False

    def configure(self, broker_host: str, broker_port: int = 1883, device_id: str = "sphere001"):
        """Configure MQTT connection parameters"""
        self.broker_host = broker_host
        self.broker_port = broker_port
        self.device_id = device_id
        logger.info(f"MQTT configured: {broker_host}:{broker_port}, device: {device_id}")
