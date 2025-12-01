import asyncio
from typing import Dict, Any, List

class StateManager:
    _instance = None
    
    def __new__(cls):
        if cls._instance is None:
            cls._instance = super(StateManager, cls).__new__(cls)
            cls._instance._initialized = False
        return cls._instance

    def __init__(self):
        if self._initialized:
            return
            
        self._state: Dict[str, Any] = {
            "imu": {"w": 1.0, "x": 0.0, "y": 0.0, "z": 0.0},
            "playback": {"isPlaying": False, "track": "None"},
            "params": {"brightness": 80, "speed": 50, "hue": 120},
            "system": {"fps": 60, "temp": 42.0}
        }
        self._observers: List[Any] = [] # List of WebSocket queues or callbacks
        self._initialized = True

    def get_state(self) -> Dict[str, Any]:
        return self._state

    async def update_state(self, key: str, value: Any):
        # Deep merge or simple update depending on key
        if key in self._state and isinstance(self._state[key], dict) and isinstance(value, dict):
            self._state[key].update(value)
        else:
            self._state[key] = value
            
        await self._notify_observers()

    def add_observer(self, observer):
        self._observers.append(observer)

    def remove_observer(self, observer):
        if observer in self._observers:
            self._observers.remove(observer)

    async def _notify_observers(self):
        # Broadcast full state or diff to all connected clients
        # For simplicity, sending full state for now
        message = {"type": "STATE_UPDATE", "payload": self._state}
        for observer in self._observers:
            try:
                await observer.send_json(message)
            except Exception:
                # Handle disconnected clients
                pass
