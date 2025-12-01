import asyncio
import json
import time
import sys
import os

# Add parent directory to path to import app modules if needed
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from joystick.device_manager import DeviceManager
from joystick.mapper import InputMapper

# Mock rclpy if not available
try:
    import rclpy
    from rclpy.node import Node
    from std_msgs.msg import String
except ImportError:
    print("WARNING: rclpy not found. Using Mock ROS2 implementation.")
    rclpy = None
    import os
    class Node:
        def __init__(self, name):
            self.name = name
        def create_publisher(self, msg_type, topic, qos):
            return MockPublisher(topic)
        def destroy_node(self):
            pass
    class MockPublisher:
        def __init__(self, topic):
            self.topic = topic
            self.path = f"/tmp/ros_mock_{topic.replace('/', '_')}"
        def publish(self, msg):
            try:
                with open(self.path, 'w') as f:
                    f.write(msg.data)
                print(f"[DAEMON] Published to {self.topic}: {msg.data}")
            except Exception as e:
                print(f"Mock Publish Error: {e}")
    class String:
        def __init__(self):
            self.data = ""

class JoystickDaemon:
    def __init__(self):
        self.device_manager = DeviceManager()
        self.mapper = InputMapper()
        self.running = False
        self.node = None
        self.pub = None

    def start(self):
        print("Starting Joystick Daemon...")
        
        # Init ROS2
        if rclpy:
            rclpy.init()
            self.node = Node("joystick_daemon")
        else:
            self.node = Node("joystick_daemon")
            
        self.pub = self.node.create_publisher(String, '/isolation_sphere/ui/control', 10)
        self.running = True
        
        try:
            asyncio.run(self.main_loop())
        except KeyboardInterrupt:
            print("Stopping Daemon...")
        finally:
            self.stop()

    async def main_loop(self):
        while self.running:
            devices = self.device_manager.scan_devices()
            if not devices:
                print("No devices found. Waiting...")
                await asyncio.sleep(3)
                continue

            # For simplicity, just pick the first device
            device_path = devices[0]
            device = self.device_manager.get_device(device_path)
            
            if device:
                print(f"Connected to {device.name} at {device_path}")
                try:
                    # Read loop
                    # Note: evdev read_loop is blocking or async iterator.
                    # Here we assume async iterator or we need to run it in executor.
                    # For Mock, it's a generator.
                    if hasattr(device, 'async_read_loop'):
                         async for event in device.async_read_loop():
                            self.process_event(event)
                    else:
                        # Fallback for mock or blocking
                        for event in device.read_loop():
                            self.process_event(event)
                            await asyncio.sleep(0.01) # Yield
                            
                except Exception as e:
                    print(f"Device error: {e}")
                    await asyncio.sleep(1)

    def process_event(self, event):
        result = self.mapper.map_event(event)
        if result:
            cmd_type, payload = result
            msg = {
                "type": cmd_type,
                "payload": payload,
                "source": "joystick"
            }
            ros_msg = String()
            ros_msg.data = json.dumps(msg)
            self.pub.publish(ros_msg)

    def stop(self):
        self.running = False
        if self.node:
            self.node.destroy_node()
        if rclpy and rclpy.ok():
            rclpy.shutdown()

if __name__ == "__main__":
    daemon = JoystickDaemon()
    daemon.start()
