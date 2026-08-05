> **English** · [日本語](SPECIFICATION.ja.md)

# Isolation Sphere Server Requirements Specification v3.0

**⚠️ This document is the latest version. The micro-ROS-related descriptions in the old versions (v2.x) are invalid.**

## 📌 Latest Architecture Documents

For the detailed communication design, refer to the following:
- **[Communication Architecture Design](../docs/architecture/communication_design.md)** - Complete communication specification
- **[ROS2 Removal Plan](../docs/architecture/ros2_removal_plan.md)** - Background of the design change

---

## 1. System Overview

### 1.1 Purpose
Unify communication with the ESP32 to **MQTT + UDP** to achieve a simple, maintainable architecture.

### 1.2 Technology Stack

#### Server
- **OS**: Ubuntu 22.04 LTS
- **Language**: Python 3.10+
- **Framework**: FastAPI
- **Communication**:
  - MQTT (Mosquitto) - control and state management
  - UDP - video streaming
  - WebSocket - UI synchronization

#### Frontend
- **Framework**: React (Vite)
- **UI**: Material-UI, React Three Fiber
- **Communication**: WebSocket

---

## 2. Communication Protocols

### 2.1 MQTT

#### Broker Configuration
- Host: `192.168.49.1`
- Port: `1883`
- Protocol: MQTT v3.1.1

#### Topic Design

**Command topics (Server → ESP32)**
```
sphere/all/command/params      # Parameter change
sphere/all/command/playback    # Playback control
sphere/all/command/led         # LED control
sphere/all/command/system      # System command
```

**State topics (ESP32 → Server)**
```
sphere/{device_id}/state       # Full state (retained)
sphere/{device_id}/imu         # IMU data (10Hz)
sphere/{device_id}/status      # Status
```

### 2.2 UDP

- **Purpose**: Video streaming (Server → ESP32)
- **Destination**: `192.168.49.101:8889`
- **Format**: JPEG (320x160, 10fps)
- **Packet**: Header (8 bytes) + JPEG data

### 2.3 WebSocket

- **Endpoint**: `ws://[server]:9000/ws`
- **Messages**:
  - `STATE_UPDATE` (Server → UI)
  - `SET_PARAMS` (UI → Server)
  - `SET_PLAYBACK` (UI → Server)

---

## 3. Architecture

### 3.1 StateManager (centralized)
- Single source of truth for state
- Processes all commands
- Distributes to MQTT/WebSocket

### 3.2 MQTT Service
- Bidirectional communication with the ESP32
- Coordination with the StateManager

### 3.3 Video Daemon (planned)
- Playlist management
- UDP video distribution

### 3.4 Joystick Daemon (planned)
- USB input acquisition
- MQTT direct publishing

---

## 4. Implementation Status

### ✅ Done
- FastAPI server
- StateManager
- MQTT communication
- WebSocket communication
- React WebUI

### 🚧 In Progress
- Video Daemon

### 📋 Planned
- Joystick Daemon

---

## 5. Reference Documents

- [Communication Design](../docs/architecture/communication_design.md)
- [ROS2 Removal Plan](../docs/architecture/ros2_removal_plan.md)
- [README.md](../README.md)
</content>
