> **English** · [日本語](system_architecture.ja.md)

# Isolation Sphere System Architecture

Last updated: 2025-12-02

## Overview

Isolation Sphere is a real-time IMU orientation visualization system composed of an ESP32-based spherical display and a Python backend server.

## System Architecture

```
┌─────────────────┐      MQTT        ┌──────────────────┐
│   ESP32 Device  │◄────────────────►│  MQTT Broker     │
│   (M5Atom S3R)  │  (192.168.49.1)  │  (mosquitto)     │
│                 │                   └──────────────────┘
│  - IMU (BNO055) │                            │
│  - LED (800)    │                            │
│  - LCD Display  │                            │
└─────────────────┘                            │
                                               │ MQTT Subscribe
                                               ▼
                                    ┌──────────────────────┐
                                    │  Python Server       │
                                    │  (FastAPI)           │
                                    │                      │
                                    │  - MQTT Service      │
                                    │  - State Manager     │
                                    │  - WebSocket Server  │
                                    └──────────────────────┘
                                               │
                                               │ WebSocket
                                               ▼
                                    ┌──────────────────────┐
                                    │  Web Frontend        │
                                    │  (React + Three.js)  │
                                    │                      │
                                    │  - HoloSphere (3D)   │
                                    │  - Dashboard UI      │
                                    └──────────────────────┘
```

## Data Flow

### Flow of IMU Orientation Data

1. **ESP32 (sender side)**
   - Acquires orientation data from the IMU sensor (BNO055)
   - Converts to quaternion format: `{w, x, y, z}`
   - Publishes to MQTT topic `sphere/sphere001/imu`
   - Publish rate: approx. 10 Hz (continuous)

2. **MQTT Broker**
   - Broker address: `192.168.49.1:1883`
   - Topic: `sphere/sphere001/imu`
   - QoS: 0 (latest data prioritized)

3. **Python Server (relay processing)**
   - **MQTTService**: subscribes from the broker
   - **Data format**: `{"w":0.707,"x":0.707,"y":0.0,"z":0.0}`
   - **StateManager**: stores the state
   - **WebSocket**: distributes to connected clients

4. **Web Frontend (display)**
   - **WebSocketContext**: real-time reception
   - **HoloSphere Component**: applies the quaternion
   - **Three.js**: rotates the 3D sphere in real time

## Communication Protocols

### MQTT Topics

| Topic | Direction | Format | Description |
|-------|-----------|--------|-------------|
| `sphere/{id}/imu` | ESP32 → Server | `{"w":float,"x":float,"y":float,"z":float}` | IMU orientation data |
| `sphere/{id}/status` | ESP32 → Server | `{"status":string,"timestamp":string}` | Device state |

### WebSocket Messages

| Type | Direction | Payload | Description |
|------|-----------|---------|-------------|
| `STATE_UPDATE` | Server → Client | `{"imu":{...},"playback":{...},"params":{...}}` | State update |
| `SET_PLAYBACK` | Client → Server | `{"isPlaying":bool}` | Playback control |
| `SET_PARAMS` | Client → Server | `{"brightness":int,"speed":int,"hue":int}` | Parameter configuration |

### REST API Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/config` | GET | Get system configuration |
| `/api/playlist/playlists` | GET | List playlists |
| `/api/playlist/playlists` | POST | Create playlist |
| `/ws` | WebSocket | Real-time communication |
| `/health` | GET | Health check |

## Configuration Files

### config.json (shared configuration)

Location: `core/data/config.json` (shared by ESP32 and Server)

```json
{
  "wifi": {
    "SSID": "ESP32-P2P-Direct",
    "password": "isolation-sphere-p2p",
    "broker": "192.168.49.1",
    "mqtt_port": 1883
  },
  "sphere": {
    "id": "sphere001",
    "mac": "F0:9E:9E:32:67:D0",
    "features": {
      "IMU": "BNO055",
      "LED": true
    }
  }
}
```

## Technology Stack

### ESP32 Firmware
- **Platform**: ESP32-S3 (M5Atom S3R)
- **Framework**: Arduino
- **Libraries**:
  - PubSubClient (MQTT)
  - Adafruit BNO055 (IMU)
  - FastLED (LED control)
  - M5Unified (Display)
  - ArduinoJson (configuration management)

### Python Server
- **Framework**: FastAPI 
- **Libraries**:
  - paho-mqtt (MQTT client)
  - uvicorn (ASGI server)
- **Characteristics**:
  - Asynchronous processing
  - WebSocket support
  - Auto-reload (during development)

### Web Frontend
- **Framework**: React
- **Libraries**:
  - Three.js + @react-three/fiber (3D rendering)
  - Material-UI (UI components)
  - react-swipeable (gestures)
- **Build**: Vite

## Deployment

### Starting the Server
```bash
cd server
python3 -m uvicorn app.main:app --reload --host 0.0.0.0 --port 9000
```

### Building the Frontend
```bash
cd server/frontend
npm run build
```

The build artifacts are generated in `server/frontend/dist/`, and FastAPI serves them as static files.

## Network Configuration

- **WiFi SSID**: ESP32-P2P-Direct
- **Server IP**: 192.168.49.1 (WiFi access point)
- **MQTT port**: 1883
- **HTTP port**: 9000
- **WebSocket port**: 9000 (same port)

## Implemented Features

### ESP32
- ✅ IMU sensor initialization and quaternion acquisition
- ✅ MQTT connection and data transmission
- ✅ LED control (FastLED)
- ✅ LCD display (debug information)
- ✅ Configuration file loading (config.json)

### Python Server
- ✅ MQTT connection and subscription
- ✅ WebSocket real-time distribution
- ✅ REST API (config, playlist)
- ✅ StateManager (state management)
- ✅ Reading the broker address from config.json
- ✅ ESP32 format support
- ✅ asyncio event loop support

### Web Frontend
- ✅ WebSocket connection (dynamic port)
- ✅ 3D sphere display (Three.js)
- ✅ IMU quaternion application
- ✅ Real-time orientation synchronization
- ✅ Dashboard UI
- ✅ Responsive design
- ✅ Swipe navigation

## Future Work

- [ ] LED video streaming (UDP)
- [ ] Playlist playback feature
- [ ] Changing MQTT settings from the configuration screen
- [ ] Performance optimization
- [ ] Enhanced error handling
</content>
</invoke>
