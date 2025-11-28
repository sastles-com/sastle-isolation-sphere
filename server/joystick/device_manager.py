import asyncio
import sys
import time

# Mock evdev for Mac/Windows
try:
    import evdev
    from evdev import ecodes
except ImportError:
    evdev = None
    ecodes = None
    print("WARNING: evdev not found. Using Mock Device Manager.")

class DeviceManager:
    def __init__(self):
        self.devices = {}
        
    def scan_devices(self):
        """
        Scans for available input devices.
        Returns a list of device paths.
        """
        if not evdev:
            # Mock behavior: Return a dummy device if none exists
            if not self.devices:
                return ["/dev/input/event0"]
            return list(self.devices.keys())
            
        return [evdev.InputDevice(path).path for path in evdev.list_devices()]

    def get_device(self, path):
        """
        Returns an InputDevice instance for the given path.
        """
        if path in self.devices:
            return self.devices[path]
            
        if not evdev:
            # Create a mock device
            device = MockInputDevice(path)
            self.devices[path] = device
            return device
            
        try:
            device = evdev.InputDevice(path)
            self.devices[path] = device
            return device
        except Exception as e:
            print(f"Error opening device {path}: {e}")
            return None

class MockInputDevice:
    def __init__(self, path):
        self.path = path
        self.name = "Mock Gamepad"
        self.fd = -1
        
    def read_loop(self):
        # Simulate events for testing
        # In a real mock, we might read from keyboard or just sleep
        while True:
            time.sleep(1)
            yield MockEvent(1, 304, 1) # Button A Press (example)

class MockEvent:
    def __init__(self, type, code, value):
        self.type = type
        self.code = code
        self.value = value
        self.timestamp = time.time()
