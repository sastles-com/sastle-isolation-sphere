> **English** · [日本語](README.ja.md)

# Isolation Sphere

**Spherical LED display system** - a 110mm-diameter spherical display fitted with 800 LEDs and its control system

![Project Status](https://img.shields.io/badge/status-active-success.svg)
![Platform](https://img.shields.io/badge/platform-ESP32--S3%20%7C%20Ubuntu-blue.svg)

## Overview

Isolation Sphere is an integrated system consisting of a spherical LED display based on the M5Atom S3R and a control server running on an Ubuntu MiniPC / Raspberry Pi. It provides real-time video display, IMU-based orientation compensation, and control via a WebUI and a physical joystick.

### Key Features

- 🎨 **800 WS2812B LEDs** for a high-density spherical display
- 🎯 **IMU orientation compensation** (BNO055) for a video display that always stays upright
- 🌐 **WebUI control** - operable from a smartphone / tablet / PC
  - Intuitive tab navigation via swipe gestures
  - Mobile optimization (automatic URL-bar hiding, viewport support)
  - 3D sphere visualization using IMU quaternions
- 🎮 **Physical joystick support** - intuitive operation via USB connection
- 🚀 **MQTT + UDP communication** - control and state over MQTT, video optimized over UDP
- 📡 **WebSocket synchronization** - real-time UI updates, IMU data bridge

## System Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                     Isolation Sphere System                  │
└─────────────────────────────────────────────────────────────┘
                              │
        ┌─────────────────────┼─────────────────────┐
        │                     │                     │
        ▼                     ▼                     ▼
┌───────────────┐    ┌────────────────┐    ┌──────────────┐
│   WebUI       │    │     Server     │    │  ESP32 Core  │
│  (Clients)    │    │  Ubuntu/RasPi  │    │  (M5Atom S3R)│
├───────────────┤    ├────────────────┤    ├──────────────┤
│• Smartphone   │───▶│• FastAPI       │───▶│• 800 LEDs    │
│• Tablet       │    │• React UI      │    │• IMU (BNO055)│
│• PC/Mac       │    │• MQTT Broker   │    │• WiFi STA    │
│               │    │• StateManager  │    │• UDP/MQTT    │
│               │    │• Joystick      │    │• PSRAM 8MB   │
└───────────────┘    │  (Optional)    │    └──────────────┘
                     │• Video Daemon  │
                     └────────────────┘
```

## Project Structure

```
isolation-sphere/
├── core/                    # ESP32 firmware (PlatformIO)
│   ├── src/                 # C++ source code
│   │   ├── main.cpp         # Main entry point
│   │   ├── LEDManager.*     # LED control (FastLED, 800 LEDs)
│   │   ├── IMUManager.*     # IMU control (BNO055)
│   │   ├── ImageManager.*   # UDP image reception & decoding
│   │   ├── NetworkManager.* # WiFi / UDP / MQTT communication
│   │   ├── GestureManager.* # Gesture recognition
│   │   ├── SoundManager.*   # Sound control
│   │   └── ConfigManager.*  # Configuration management (JSON)
│   ├── data/                # LittleFS data
│   │   ├── config.json      # WiFi, MQTT configuration
│   │   ├── led_layouts-5strip.csv   # LED coordinate mapping
│   │   └── images/          # Demo image data
│   ├── doc/                 # Documentation
│   │   ├── dual_core_design.md       # Dual-core design
│   │   ├── image_manager_design.md   # Image management design
│   │   ├── imu_compensation.md       # IMU compensation design
│   │   └── udp_image_protocol.md     # UDP protocol specification
│   ├── platformio.ini       # PlatformIO configuration
│   └── README.md            # Core README
│
├── server/                  # Control server (Python/Node.js)
│   ├── app/                 # FastAPI application
│   │   ├── main.py          # Server entry point
│   │   ├── api/             # REST API endpoints
│   │   ├── core/            # Core functionality (configuration management)
│   │   └── services/        # Business logic (StateManager, MQTT)
│   ├── frontend/            # React WebUI (Vite)
│   │   ├── src/
│   │   │   ├── components/  # UI components
│   │   │   ├── contexts/    # React Context
│   │   │   └── pages/       # Page components
│   │   └── package.json
│   ├── joystick/            # Joystick daemon
│   │   ├── daemon.py        # Main daemon
│   │   ├── device_manager.py
│   │   └── mapper.py        # Input mapping
│   ├── scripts/             # Setup scripts
│   │   ├── setup_network.sh # AP configuration
│   │   └── setup_services.sh# systemd configuration
│   ├── video/               # Video streaming daemon (planned)
│   │   └── daemon.py        # UDP video streaming
│   ├── docs/                # Server documentation
│   ├── pyproject.toml       # Python dependencies
│   └── README.md            # Server README
│
└── README.md                # This file
```

## Quick Start

### Requirements

#### Hardware
- **ESP32 device**: M5Atom S3R (ESP32-S3, 8MB Flash, 8MB PSRAM)
- **Server**: Ubuntu 22.04 MiniPC or Raspberry Pi 4+
- **Network**: USB WiFi adapter (for the ESP32-dedicated AP)
- **Optional**: USB joystick

#### Software
- **Core (ESP32)**:
  - PlatformIO Core or PlatformIO IDE
  - Python 3.7+ (for PlatformIO)

- **Server (Ubuntu)**:
  - Ubuntu 22.04 LTS
  - Python 3.10+
  - Node.js 18+
  - ROS2 Humble
  - Docker (for the micro-ROS Agent)

### Installation

#### 1. Build & flash the ESP32 firmware

```bash
# Move to the core directory
cd core

# Build & flash with PlatformIO
pio run -t upload

# Upload the file system (LittleFS)
pio run -t uploadfs
```

For details, see [core/README.md](core/README.md).

#### 2. Set up the server

```bash
# Move to the server directory
cd server

# Install Python dependencies
pip install -e .

# Build the frontend
cd frontend
npm install
npm run build
cd ..

# Network configuration (AP Mode)
sudo scripts/setup_network.sh

# Service configuration (optional)
sudo scripts/setup_services.sh
```

For details, see [server/README.md](server/README.md).

### Running

#### Development mode

```bash
# Terminal 1: Start the MQTT broker
sudo systemctl start mosquitto

# Terminal 2: Start the FastAPI server
cd server
uvicorn app.main:app --host 0.0.0.0 --port 9000 --reload

# Terminal 3: Frontend development server (optional)
cd server/frontend
npm run dev

# Terminal 4: Joystick daemon (optional, planned)
cd server
python -m joystick.daemon
```

#### Production mode

```bash
# Start all services
sudo systemctl start mosquitto
sudo systemctl start isolation-sphere-server
# sudo systemctl start isolation-sphere-joystick  # planned
```

### Access

- **WebUI**: http://[server-ip]:9000 (e.g. http://192.168.1.100:9000)
- **mDNS**: http://[hostname].local:9000 (if configured)
- **ESP32 AP**: SSID `IsolationSphere-Direct`, IP: 192.168.100.1

## Technology Stack

### ESP32 Core
- **Language**: C++17
- **Framework**: Arduino (ESP32-S3)
- **Main libraries**:
  - FastLED - LED control
  - Adafruit_BNO055 - IMU control
  - AsyncMqttClient - MQTT communication
  - JPEGDecoder - image decoding
  - ArduinoJson - JSON processing

### Server Backend
- **Language**: Python 3.10+
- **Framework**: FastAPI (asynchronous web)
- **Communication**:
  - MQTT (paho-mqtt) - control & state management
  - WebSocket - real-time UI synchronization
  - UDP - video streaming (planned)

### Server Frontend
- **Framework**: React 18 (Vite)
- **UI library**: Material-UI (MUI)
- **3D graphics**: React Three Fiber (Three.js)
- **State management**: React Context API
- **Styling**: Emotion (CSS-in-JS)

## Communication Protocols

### MQTT
- **Purpose**: control commands, state management, sensor data
- **Broker**: 192.168.49.1:1883
- **Topics**:
  - `sphere/all/command/*` - control commands (Server → ESP32)
  - `sphere/{id}/imu` - IMU data (ESP32 → Server, 10Hz)
  - `sphere/{id}/state` - device state (ESP32 → Server, retained)

### UDP
- **Purpose**: high-speed image streaming
- **Port**: 8889 (ESP32 reception)
- **Format**: JPEG-compressed images (320x160, 10fps)

### MQTT
- **Purpose**: legacy state monitoring & commands
- **Broker**: 192.168.100.1:1883
- **Topics**:
  - `sphere/[device_id]/command` - command reception
  - `sphere/[device_id]/status` - status transmission
  - `sphere/[device_id]/imu` - IMU data (10Hz)
  - `sphere/[device_id]/response` - command responses

### WebSocket
- **Purpose**: WebUI real-time communication
- **Endpoint**: `ws://[server]:9000/api/ws`
- **Data format**: JSON

## Features

### Implemented ✅
- ESP32 firmware basic features (WiFi, MQTT, UDP)
- LittleFS file system management
- IMU (BNO055) quaternion acquisition
- 800-LED control via FastLED
- UDP image reception & JPEG decoding
- FastAPI web server
- React WebUI (Material-UI + Three.js)
- WebSocket real-time communication
- Joystick daemon
- micro-ROS Agent integration
- Dual-WiFi configuration (WebUI + ESP32-dedicated AP)

### In Development 🚧
- Gesture recognition
- Sound playback
- Playlist management
- Advanced image effects
- Performance monitoring dashboard

### Planned 📋
- Bluetooth gamepad support
- OTA (Over-The-Air) firmware updates
- Cloud integration
- Multi-device synchronization

## Documentation

### Core (ESP32)
- [Specification](core/spec.md)
- [Dual-core design](core/doc/dual_core_design.md)
- [Image manager design](core/doc/image_manager_design.md)
- [IMU compensation design](core/doc/imu_compensation.md)
- [UDP protocol specification](core/doc/udp_image_protocol.md)
- [Implementation status](core/doc/implementation_status.md)
- [Class diagram](core/doc/class.md)
- [Sequence diagram](core/doc/sequence.md)
- [State diagram](core/doc/state.md)
- [MQTT specification](core/doc/mqtt.md)

### Server
- [Architecture](server/docs/architecture.md)
- [MQTT protocol specification](server/docs/mqtt_spec.md)
- [State diagram](server/docs/state_diagram.md)
- [Sequence diagram](server/docs/sequence_diagram.md)
- [Class diagram](server/docs/class_diagram.md)
- [Protocol specification](server/docs/protocol_spec.md)
- [Requirements specification](server/requirements_specification.md)
- [Raspberry Pi system specification](server/raspi_system_specification.md)

### Other
- [Framework guide](Flamework.md)
- [Ubuntu migration guide](migration_guide_ubuntu.md)

## Troubleshooting

### ESP32 cannot connect
```bash
# Check the serial port
pio device list

# Check the configuration file
cat core/data/config.json

# After updating the WiFi configuration, re-upload
pio run -t uploadfs
pio run -t upload
```

### The server does not start
```bash
# Check the MQTT broker status
sudo systemctl status mosquitto

# Check the FastAPI logs
journalctl -u isolation-sphere-server -f

# Check port usage
sudo netstat -tulpn | grep 9000
sudo netstat -tulpn | grep 1883  # MQTT
```

### Cannot access the WebUI
```bash
# Check the firewall
sudo ufw status
sudo ufw allow 9000/tcp

# Check the mDNS service
sudo systemctl status avahi-daemon
```

### The LEDs do not light up
```bash
# Debug with the ESP32 serial monitor
pio device monitor

# Check the LED power supply (5V, sufficient current capacity)
# Check the GPIO pin connections (5,6,7,8)
```

## Development

### Code Style

#### C++ (Core)
```bash
# Use clang-format
cd core
find src -name "*.cpp" -o -name "*.h" | xargs clang-format -i
```

#### Python (Server)
```bash
# black + flake8
cd server
black .
flake8 .
```

#### JavaScript (Frontend)
```bash
# ESLint
cd server/frontend
npm run lint
```

### Testing

```bash
# ESP32 unit tests
cd core
pio test

# Python tests
cd server
pytest

# Frontend tests
cd server/frontend
npm test
```

## Contributing

Contributions to the project are welcome!

1. Fork this repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a pull request

## License

For the license of this project, please contact the project owner.

## Acknowledgments

- The M5Stack community
- The FastLED library developers
- The MQTT / Mosquitto community
- The React and Three.js communities

## Related Links

- [M5Atom S3R official documentation](https://docs.m5stack.com/en/core/AtomS3R)
- [FastLED library](https://fastled.io/)
- [Eclipse Mosquitto](https://mosquitto.org/)
- [MQTT Protocol](https://mqtt.org/)
- [React Three Fiber](https://docs.pmnd.rs/react-three-fiber/)

---

**Isolation Sphere** - Making the sphere shine ✨
