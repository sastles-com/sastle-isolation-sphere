"""
MQTT Service for receiving IMU data from ESP32 devices
"""
import asyncio
import json
from typing import Optional
import logging

try:
    import paho.mqtt.client as mqtt
    MQTT_AVAILABLE = True
except ImportError:
    MQTT_AVAILABLE = False
    logging.warning("paho-mqtt not installed. MQTT functionality disabled.")

from app.services.state_manager import StateManager

logger = logging.getLogger(__name__)


class MQTTService:
    """
    MQTT Client Service for subscribing to ESP32 IMU data
    """
    _instance = None
    
    def __new__(cls):
        if cls._instance is None:
            cls._instance = super(MQTTService, cls).__new__(cls)
            cls._instance._initialized = False
        return cls._instance

    def __init__(self):
        if self._initialized:
            return
            
        self.client: Optional[mqtt.Client] = None
        self.state_manager: Optional[StateManager] = None  # 外部からセット
        
        # Load config from config.json
        self.broker_host = self._load_broker_config()
        self.broker_port = 1883
        self.device_id = "sphere001"  # Default device ID
        self.is_connected = False
        self._initialized = True
        
        if MQTT_AVAILABLE:
            self._setup_client()
        else:
            logger.warning("MQTT client not available. Using mock data.")
    
    def _load_broker_config(self):
        """Load MQTT broker address from config.json"""
        try:
            import json
            import os
            # Try to load from shared data directory (server is run from server/ dir)
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
                        logger.info(f"Loaded MQTT broker from config: {broker}")
                        return broker
            logger.warning("Config file not found, using localhost")
            return "localhost"
        except Exception as e:
            logger.error(f"Failed to load config: {e}, using localhost")
            return "localhost"

    def _setup_client(self):
        """Initialize MQTT client"""
        self.client = mqtt.Client(client_id="isolation-server")
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
            command_topic = "sphere/all/command/#"
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

    def _handle_command(self, topic: str, payload: dict):
        """Handle command messages - delegate to StateManager"""
        logger.info(f"[MQTT] Received command on {topic}: {payload}")
        
        if self.state_manager:
            try:
                # MQTT callback runs in different thread, use run_coroutine_threadsafe
                import asyncio
                if hasattr(self, '_loop') and self._loop:
                    asyncio.run_coroutine_threadsafe(
                        self.state_manager.handle_command(topic, payload),
                        self._loop
                    )
                    logger.info(f"[MQTT] Command delegated to StateManager")
                else:
                    logger.warning("Event loop not set, cannot handle command")
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
                import asyncio
                if hasattr(self, '_loop') and self._loop:
                    asyncio.run_coroutine_threadsafe(
                        self.state_manager.update_state("imu", {
                            "w": quat.get("w", 1.0),
                            "x": quat.get("x", 0.0),
                            "y": quat.get("y", 0.0),
                            "z": quat.get("z", 0.0)
                        }),
                        self._loop
                    )
                    logger.debug(f"Updated IMU quaternion: {quat}")
                else:
                    logger.warning("Event loop not set, cannot update IMU")
        except Exception as e:
            logger.error(f"Error handling IMU data: {e}")

    def _handle_status_data(self, payload):
        """Handle device status data"""
        if not self.state_manager:
            logger.warning("StateManager not set, skipping status update")
            return
            
        try:
            # Update system state with device status
            import asyncio
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
