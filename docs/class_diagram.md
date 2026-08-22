> **English** · [日本語](class_diagram.ja.md)

# Class Diagram and Component Structure

Last updated: 2025-12-02

## Python Server Class Structure

```mermaid
classDiagram
    class FastAPIApp {
        +lifespan()
        +health()
        +serve_frontend()
    }
    
    class ROSManager {
        -_instance
        -node
        -executor
        -thread
        -running
        +start()
        +stop()
        +get_node()
    }
    
    class MQTTService {
        -_instance
        -client
        -state_manager
        -broker_host
        -broker_port
        -device_id
        -is_connected
        +_load_broker_config()
        +_setup_client()
        +_on_connect()
        +_on_message()
        +_handle_imu_data()
        +_handle_status_data()
        +start()
        +stop()
    }
    
    class StateManager {
        -_instance
        -_state
        -_observers
        +get_state()
        +update_state()
        +add_observer()
        +remove_observer()
        -_notify_observers()
    }
    
    class WebSocketRouter {
        +websocket_endpoint()
    }
    
    class APIRouter {
        +get_config()
        +get_playlists()
        +create_playlist()
    }
    
    FastAPIApp --> ROSManager : manages
    FastAPIApp --> MQTTService : manages
    FastAPIApp --> WebSocketRouter : includes
    FastAPIApp --> APIRouter : includes
    
    MQTTService --> StateManager : updates
    WebSocketRouter --> StateManager : subscribes
    
    MQTTService : Singleton
    StateManager : Singleton
    ROSManager : Singleton
```

## React Frontend Component Structure

```mermaid
classDiagram
    class App {
        +render()
    }
    
    class Dashboard {
        -currentTab
        -rotation
        -brightness
        -speed
        -color
        -isPlaying
        +handleSwipeLeft()
        +handleSwipeRight()
        +handleTogglePlay()
        +handleParamChange()
    }
    
    class HoloSphere {
        +render()
    }
    
    class IMUControlledSphere {
        -meshRef
        -quaternionRef
        +useFrame()
        +render()
    }
    
    class WebSocketContext {
        -ws
        -isConnected
        -lastMessage
        +connect()
        +sendMessage()
    }
    
    class PlaylistManager {
        -playlists
        +fetchPlaylists()
        +createPlaylist()
    }
    
    class ConfigEditor {
        -config
        +fetchConfig()
        +saveConfig()
    }
    
    App --> Dashboard : renders
    Dashboard --> HoloSphere : renders
    Dashboard --> PlaylistManager : renders
    Dashboard --> ConfigEditor : renders
    
    HoloSphere --> IMUControlledSphere : renders
    IMUControlledSphere --> WebSocketContext : uses
    Dashboard --> WebSocketContext : uses
```

## ESP32 Class Structure

```mermaid
classDiagram
    class Main {
        +setup()
        +loop()
    }
    
    class ConfigManager {
        -doc
        +loadConfig()
        +saveConfig()
        +getWifiSSID()
        +getMQTTBroker()
        +getDeviceID()
    }
    
    class DeviceManager {
        +initPlatform()
        +initIMU()
        +initLCD()
        +initLED()
        +updateDisplay()
    }
    
    class NetworkManager {
        -mqtt
        -udp
        +begin()
        +connect()
        +publishIMU()
        +publishStatus()
        +handleUDP()
    }
    
    class IMUManager {
        -bno
        +begin()
        +update()
        +getQuaternion()
        +calibrate()
    }
    
    class LEDManager {
        -strips[4]
        -layout
        +begin()
        +setBrightness()
        +displayImage()
        +clear()
    }
    
    class LCDManager {
        +begin()
        +showDebugInfo()
        +showStatus()
        +clear()
    }
    
    Main --> ConfigManager : uses
    Main --> DeviceManager : uses
    Main --> NetworkManager : uses
    
    DeviceManager --> IMUManager : manages
    DeviceManager --> LEDManager : manages
    DeviceManager --> LCDManager : manages
    
    NetworkManager --> IMUManager : reads
    ConfigManager --> NetworkManager : configures
```

## Data Flow

### IMU Quaternion Data Flow

```mermaid
sequenceDiagram
    participant IMU as BNO055 (IMU)
    participant ESP32
    participant MQTT as MQTT Broker
    participant Server as Python Server
    participant WS as WebSocket
    participant Browser as Web Browser
    
    loop 10Hz
        IMU->>ESP32: Read Quaternion
        ESP32->>ESP32: Format JSON: {w,x,y,z}
        ESP32->>MQTT: Publish to sphere/sphere001/imu
        MQTT->>Server: Subscribe sphere/+/imu
        Server->>Server: Parse JSON
        Server->>Server: StateManager.update_state("imu")
        Server->>WS: Broadcast STATE_UPDATE
        WS->>Browser: WebSocket Message
        Browser->>Browser: Apply to Three.js Sphere
    end
```

### WebSocket State Synchronization

```mermaid
sequenceDiagram
    participant Browser
    participant WS as WebSocket
    participant Server
    participant State as StateManager
    
    Browser->>WS: Connect to ws://host:9000/ws
    WS->>Server: WebSocket Handshake
    Server->>State: add_observer(websocket)
    Server->>WS: Send Initial STATE_UPDATE
    WS->>Browser: {type:"STATE_UPDATE", payload:{imu,playback,params}}
    
    Browser->>WS: {type:"SET_PARAMS", payload:{brightness:80}}
    WS->>Server: Receive Message
    Server->>State: update_state("params", ...)
    State->>State: notify_observers()
    State->>WS: Broadcast to all clients
    WS->>Browser: STATE_UPDATE
```

## File Structure

### Server
```
server/
├── app/
│   ├── main.py                 # FastAPI application
│   ├── core/
│   │   ├── config.py          # Settings
│   │   └── ros_manager.py     # ROS2 Manager (Singleton)
│   ├── services/
│   │   ├── mqtt_service.py    # MQTT Client (Singleton)
│   │   └── state_manager.py   # State Manager (Singleton)
│   └── api/
│       ├── router.py          # API Router
│       └── endpoints/
│           ├── websocket.py   # WebSocket endpoint
│           ├── config.py      # Config API
│           └── playlist.py    # Playlist API
└── frontend/
    ├── src/
    │   ├── App.jsx
    │   ├── contexts/
    │   │   └── WebSocketContext.jsx
    │   ├── pages/
    │   │   └── Dashboard.jsx
    │   └── components/
    │       ├── sphere/
    │       │   └── HoloSphere.jsx
    │       ├── playlist/
    │       │   └── PlaylistManager.jsx
    │       └── params/
    │           └── ConfigEditor.jsx
    └── dist/                  # Built files
```

### ESP32
```
core/
├── src/
│   ├── main.cpp               # Main entry point
│   ├── ConfigManager.cpp/h    # JSON config loader
│   ├── DeviceManager.cpp/h    # Platform initialization
│   ├── NetworkManager.cpp/h   # MQTT/UDP handler
│   ├── IMUManager.cpp/h       # BNO055 wrapper
│   ├── LEDManager.cpp/h       # FastLED controller
│   └── LCDManager.cpp/h       # M5Unified display
├── data/                      # LittleFS files
│   ├── config.json            # System config
│   ├── led_layouts-5strip.csv         # LED positions
│   └── images/                # Image files
└── platformio.ini             # Build config
```

## Singleton Pattern

The following classes are implemented using the Singleton pattern:

- **Python Server**
  - `ROSManager`: ROS2 node management
  - `MQTTService`: MQTT connection management
  - `StateManager`: global state management

Reasons:
- A single instance is required across the entire application
- Prevents duplication of resources (network connections, threads)
- Guarantees state consistency

## Asynchronous Processing

### Python Server
- FastAPI: ASGI asynchronous framework
- WebSocket: asynchronous handler
- StateManager: asynchronous notification via `async def update_state()`
- MQTTService: runs on a separate thread, synchronized with `asyncio.new_event_loop()`

### Frontend
- WebSocket: event-driven
- React Hooks: asynchronous updates with `useEffect` and `useState`
- Three.js: animation loop with `useFrame`
