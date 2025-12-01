import json
try:
    from std_msgs.msg import String
except ImportError:
    class String:
        def __init__(self):
            self.data = ""
from app.core.ros_manager import ROSManager
from app.services.state_manager import StateManager

class ROSBridge:
    def __init__(self):
        self.ros_manager = ROSManager()
        self.state_manager = StateManager()
        self.node = self.ros_manager.get_node()
        
        # Publishers
        self.control_pub = self.node.create_publisher(String, '/isolation_sphere/ui/control', 10)
        
        # Subscribers
        self.status_sub = self.node.create_subscription(
            String, 
            '/isolation_sphere/ui/status', 
            self._status_callback, 
            10
        )
        
        # Subscribe to control topic to sync Joystick/Other inputs
        self.control_sub = self.node.create_subscription(
            String,
            '/isolation_sphere/ui/control',
            self._control_callback,
            10
        )
        
        # Capture event loop for thread-safe callbacks
        import asyncio
        try:
            self.loop = asyncio.get_running_loop()
        except RuntimeError:
            self.loop = asyncio.get_event_loop()

    async def handle_frontend_message(self, message: dict):
        """
        Handle messages from Frontend (WebSocket) and publish to ROS2
        """
        msg_type = message.get("type")
        payload = message.get("payload")
        
        ros_msg = String()
        ros_msg.data = json.dumps(message)
        
        self.control_pub.publish(ros_msg)
        
        # Optimistic UI update (optional)
        if msg_type == "SET_PARAMS":
            await self.state_manager.update_state("params", payload)

    def _control_callback(self, msg):
        """
        Handle control messages from ROS2 (Joystick, etc)
        """
        try:
            data = json.loads(msg.data)
            # Bridge to async StateManager on main loop
            import asyncio
            asyncio.run_coroutine_threadsafe(
                self._process_control_update(data),
                self.loop
            )
        except Exception as e:
            print(f"Error parsing ROS control: {e}")

    async def _process_control_update(self, data: dict):
        """
        Async handler for control updates
        """
        msg_type = data.get("type")
        payload = data.get("payload")
        
        if msg_type == "SET_PARAMS":
            await self.state_manager.update_state("params", payload)
        elif msg_type == "SET_PLAYBACK":
            await self.state_manager.update_state("playback", payload)
        elif msg_type == "SET_OFFSET":
            await self.state_manager.update_state("imu", payload) # Map offset to IMU for now? Or separate?

    def _status_callback(self, msg):
        """
        Handle messages from ROS2 and update StateManager
        """
        try:
            data = json.loads(msg.data)
            # Bridge to async StateManager
            import asyncio
            asyncio.run_coroutine_threadsafe(
                self._process_status_update(data),
                self.loop
            )
        except Exception as e:
            print(f"Error parsing ROS status: {e}")

    async def _process_status_update(self, data: dict):
        # Assuming status updates map directly to state
        for key, value in data.items():
            await self.state_manager.update_state(key, value)
