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
        self.state_manager = StateManager()
        self.broker_host = "192.168.100.1"  # Default from config
        self.broker_port = 1883
        self.device_id = "sphere001"  # Default device ID
        self.is_connected = False
        self._initialized = True
        
        if MQTT_AVAILABLE:
            self._setup_client()
        else:
            logger.warning("MQTT client not available. Using mock data.")

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
            
            # Handle IMU data
            if topic.endswith("/imu"):
                self._handle_imu_data(payload)
            # Handle status data
            elif topic.endswith("/status"):
                self._handle_status_data(payload)
                
        except json.JSONDecodeError as e:
            logger.error(f"Failed to decode MQTT message: {e}")
        except Exception as e:
            logger.error(f"Error handling MQTT message: {e}")

    def _handle_imu_data(self, payload):
        """Handle IMU quaternion data"""
        try:
            # Expected format: {"quaternion": {"w": 1.0, "x": 0.0, "y": 0.0, "z": 0.0}, ...}
            if "quaternion" in payload:
                quat = payload["quaternion"]
                # Update state asynchronously
                asyncio.create_task(
                    self.state_manager.update_state("imu", {
                        "w": quat.get("w", 1.0),
                        "x": quat.get("x", 0.0),
                        "y": quat.get("y", 0.0),
                        "z": quat.get("z", 0.0)
                    })
                )
                logger.debug(f"Updated IMU quaternion: {quat}")
        except Exception as e:
            logger.error(f"Error handling IMU data: {e}")

    def _handle_status_data(self, payload):
        """Handle device status data"""
        try:
            # Update system state with device status
            asyncio.create_task(
                self.state_manager.update_state("system", {
                    **self.state_manager.get_state().get("system", {}),
                    "device_status": payload.get("status", "unknown"),
                    "last_update": payload.get("timestamp", "")
                })
            )
        except Exception as e:
            logger.error(f"Error handling status data: {e}")

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
