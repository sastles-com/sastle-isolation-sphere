> **English** · [日本語](README.ja.md)

# Isolation Sphere Server

The control server for the Isolation Sphere project, running on an Ubuntu MiniPC / Raspberry Pi

## Overview

This server provides centralized control of the Isolation Sphere (a 110mm-diameter spherical LED display fitted with 800 LEDs). It manages communication with the ESP32 device, control via the WebUI, and support for physical joystick input.

## System Architecture

```
┌─────────────────────────────────────────────────────┐
│ Server (Ubuntu 22.04 LTS)                          │
├─────────────────────────────────────────────────────┤
│                                                     │
│  ┌─────────────┐  ┌──────────────┐  ┌───────────┐ │
│  │  FastAPI    │  │  Joystick    │  │  Video    │ │
│  │  Server     │  │  Daemon      │  │  Daemon   │ │
│  │  (Port 9000)│  │  (Python)    │  │  (Python) │ │
│  └──────┬──────┘  └──────┬───────┘  └─────┬─────┘ │
│         │                │                 │        │
│  ┌──────┴────────────────┴─────────────────┴─────┐ │
│  │         micro-ROS Agent (UDP 8888)            │ │
│  │         ROS2 Humble Message Bus               │ │
│  └───────────────────────────────────────────────┘ │
│                                                     │
│  Network interfaces:                                │
│  • wlan0: external router (WebUI access)            │
│  • USB WiFi: AP mode 192.168.100.1 (ESP32)         │
└─────────────────────────────────────────────────────┘
         │                              │
    WebUI access                   ESP32 device
  (smartphone/PC)              (micro-ROS via UDP)
```

## Features

### Core Services
- **FastAPI web application** (port 9000)
  - RESTful API endpoints for system control
  - WebSocket support for real-time communication
  - ROS2/MQTT bridge functionality
  - Serving the React frontend

- **React frontend** (Vite-based)
  - Modern UI using Material-UI components
  - 3D visualization using React Three Fiber (IMU quaternion control)
  - Real-time control via WebSocket
  - Responsive design for mobile/tablet/desktop
  - Intuitive tab navigation via swipe gestures
  - Mobile optimization (automatic URL-bar hiding, viewport support)
  - Vertical tab switching via up/down buttons and vertical flicks

- **Joystick daemon**
  - Physical USB joystick support via `evdev`
  - Publishing joystick state to ROS2 topics
  - Cross-platform input mapping

- **Video streaming daemon**
  - Streaming video content to the ESP32 over UDP
  - Playback controlled via ROS2
  - Support for multiple video formats

### Network Configuration
- **Dual-WiFi configuration**:
  - `wlan0`: external network for WebUI access
  - `USB WiFi adapter`: ESP32-dedicated AP (192.168.100.1/24)
- **mDNS support**: easy access by hostname
- **DHCP server**: automatic IP assignment to ESP32 devices

### Communication Protocols
- **micro-ROS (XRCE-DDS)**: primary communication with the ESP32
- **MQTT**: status/command messaging, receiving IMU quaternion data
- **WebSocket**: real-time WebUI updates, bridging IMU data
- **UDP**: high-throughput video streaming

## Prerequisites

- Ubuntu 22.04 LTS (MiniPC or Raspberry Pi running Ubuntu)
- Python 3.10+
- Node.js 18+ and npm
- ROS2 Humble
- Docker (for the micro-ROS agent)
- USB WiFi adapter (for the ESP32-dedicated AP)

## Installation

### 1. Install system dependencies

```bash
# ROS2 Humble
sudo apt update
sudo apt install ros-humble-desktop python3-colcon-common-extensions

# Network tools
sudo apt install hostapd dnsmasq

# Python development
sudo apt install python3-pip python3-venv

# Node.js (if not installed)
curl -fsSL https://deb.nodesource.com/setup_18.x | sudo -E bash -
sudo apt install nodejs
```

### 2. Install Python dependencies

```bash
cd server
pip install -e .
# Or use uv (recommended):
# pip install uv
# uv pip install -e .
```

### 3. Build the frontend

```bash
cd server/frontend
npm install
npm run build
```

### 4. Set up the network (AP mode)

```bash
cd server/scripts
sudo ./setup_network.sh
```

This configures the USB WiFi adapter as an access point for the ESP32 device.

### 5. Set up services (optional)

```bash
cd server/scripts
sudo ./setup_services.sh
```

This configures systemd services for automatic startup at boot.

## Running the Server

### Development mode

```bash
# Terminal 1: Start the micro-ROS agent
cd server/docker
docker-compose up

# Terminal 2: Start the FastAPI server
cd server
uvicorn app.main:app --host 0.0.0.0 --port 9000 --reload

# Terminal 3: Start the frontend development server (optional)
cd server/frontend
npm run dev

# Terminal 4: Start the joystick daemon (when a joystick is connected)
cd server
python -m joystick.daemon
```

### Production mode

```bash
# Start all services
sudo systemctl start isolation-sphere-server
sudo systemctl start isolation-sphere-joystick
sudo systemctl start micro-ros-agent
```

## Project Structure

```
server/
├── app/                    # FastAPI application
│   ├── main.py            # Application entry point
│   ├── api/               # API routes
│   ├── core/              # Core functionality
│   └── services/          # Business logic services
├── frontend/              # React frontend
│   ├── src/
│   │   ├── components/    # React components
│   │   ├── contexts/      # React contexts
│   │   └── pages/         # Page components
│   └── public/            # Static assets
├── joystick/              # Joystick daemon
│   ├── daemon.py          # Main daemon
│   ├── device_manager.py  # Device management
│   └── mapper.py          # Input mapping
├── scripts/               # Setup and utility scripts
├── docs/                  # Documentation
│   ├── architecture.md    # System architecture
│   ├── mqtt_spec.md       # MQTT protocol specification
│   └── ...
├── docker/                # Docker configuration
│   └── docker-compose.yml # micro-ROS agent
├── pyproject.toml         # Python project configuration
└── README.md              # This file
```

## API Endpoints

- `GET /health` - health check
- `GET /api/config` - get system configuration
- `POST /api/config` - update configuration
- `GET /api/playlist` - get the playlist
- `POST /api/playlist` - update the playlist
- `WebSocket /api/ws` - real-time communication

## Configuration

Configuration is managed through:
- `app/core/config.py` - application configuration
- Environment variables
- `core/data/config.json` - ESP32 shared configuration

## Documentation

- [System architecture](docs/architecture.md)
- [MQTT protocol specification](docs/mqtt_spec.md)
- [State diagram](docs/state_diagram.md)
- [Sequence diagram](docs/sequence_diagram.md)
- [Requirements specification](requirements_specification.md)
- [Raspberry Pi system specification](raspi_system_specification.md)

## Development

### Running tests

```bash
# Python tests
pytest

# Frontend tests
cd frontend
npm test
```

### Code Style

```bash
# Python
black .
flake8 .

# Frontend
cd frontend
npm run lint
```

## Troubleshooting

### The micro-ROS agent does not connect
- Check that the Docker container is running: `docker ps`
- Check the firewall: `sudo ufw allow 8888/udp`
- Check the ESP32's network connection

### Cannot access the WebUI
- Check that FastAPI is running on port 9000
- Check that the firewall allows port 9000
- Check that the mDNS service is running

### The joystick is not detected
- Check the device permissions: `ls -l /dev/input/`
- Add the user to the input group: `sudo usermod -a -G input $USER`
- Check the daemon logs

## License

For license information, see the project root.

## Contributors

For contributor information, see the project root.
