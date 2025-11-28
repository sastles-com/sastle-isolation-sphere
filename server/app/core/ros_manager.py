import threading
import time
import sys
from app.core.config import get_settings

# Mock rclpy if not available (for local dev on Mac)
try:
    import rclpy
    from rclpy.node import Node
except ImportError:
    print("WARNING: rclpy not found. Using Mock ROS2 implementation.")
    rclpy = None
    import os
    class Node:
        def __init__(self, name):
            self.name = name
        def create_publisher(self, msg_type, topic, qos):
            return MockPublisher(topic)
        def create_subscription(self, msg_type, topic, callback, qos):
            return MockSubscriber(topic, callback)
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
                print(f"[MOCK ROS] Published to {self.topic}")
            except Exception as e:
                print(f"Mock Publish Error: {e}")

    class MockSubscriber:
        def __init__(self, topic, callback):
            self.topic = topic
            self.callback = callback
            self.path = f"/tmp/ros_mock_{topic.replace('/', '_')}"
            self.running = True
            self.thread = threading.Thread(target=self._poll, daemon=True)
            self.thread.start()
            
        def _poll(self):
            last_mtime = 0
            while self.running:
                if os.path.exists(self.path):
                    try:
                        mtime = os.path.getmtime(self.path)
                        if mtime > last_mtime:
                            last_mtime = mtime
                            with open(self.path, 'r') as f:
                                data = f.read()
                            if data:
                                msg = type('String', (), {'data': data})()
                                self.callback(msg)
                    except Exception as e:
                        print(f"Mock Poll Error: {e}")
                time.sleep(0.1)

settings = get_settings()

class ROSManager:
    _instance = None
    _lock = threading.Lock()

    def __new__(cls):
        with cls._lock:
            if cls._instance is None:
                cls._instance = super(ROSManager, cls).__new__(cls)
                cls._instance._initialized = False
        return cls._instance

    def __init__(self):
        if self._initialized:
            return
        
        self.node = None
        self.executor = None
        self.thread = None
        self.running = False
        self._initialized = True

    def start(self):
        if self.running:
            return

        print("Initializing ROS2...")
        if rclpy:
            rclpy.init()
            self.node = Node(settings.NODE_NAME)
            self.running = True
            self.thread = threading.Thread(target=self._spin, daemon=True)
            self.thread.start()
            print("ROS2 Node started.")
        else:
            self.node = Node(settings.NODE_NAME)
            self.running = True
            print("MOCK ROS2 Node started.")

    def _spin(self):
        if not rclpy:
            return
        try:
            rclpy.spin(self.node)
        except Exception as e:
            print(f"ROS2 Spin Error: {e}")
        finally:
            if rclpy.ok():
                rclpy.shutdown()

    def stop(self):
        self.running = False
        if self.node:
            self.node.destroy_node()
        if rclpy and rclpy.ok():
            rclpy.shutdown()
        if self.thread:
            self.thread.join()
        print("ROS2 Node stopped.")

    def get_node(self):
        return self.node
