> **English** · [日本語](requirements_specification.ja.md)

# Isolation Sphere Server (MiniPC) Requirements Specification

## Change History
| Version | Date | Change description | Approver |
|-----------|------|----------|--------|
| 3.0.0 | 2025/12/02 | Removed ROS2, unified on MQTT+UDP+WebSocket | - |
| 2.5.0 | 2025/11/21 | UI details finalized (Arwes + Neon Dials) | - |
| 2.4.0 | 2025/11/21 | UI design direction finalized (Arwes + 3D Sphere) | - |
| 2.3.0 | 2025/11/21 | Technology stack finalized (FastAPI, React, Python Joystick) | - |
| 2.2.0 | 2025/11/21 | Added Joystick Daemon, detailed UI information | - |
| 2.1.0 | 2025/11/21 | JPEG compression of image data, communication method review | - |
| 2.0.0 | 2025/11/21 | Full revision for MiniPC migration | - |

## 1. Project Overview

### 1.1 Purpose
Consolidate the control functions of the Isolation Sphere onto an Ubuntu MiniPC (hereafter "Server"), and unify communication with ESP32 on **MQTT (control/status) + UDP (video)**.
Enable control of the system through a unified interface from both the WebUI and a physical joystick (optional).

### 1.2 System Architecture Diagram
```mermaid
graph TD
    subgraph "Server (MiniPC / Ubuntu 22.04)"
        WLAN0[Built-in WiFi (wlan0)] -- "WebUI Access" --> Router[External Router]
        USBWiFi[USB WiFi (wlx...)] -- "AP Mode (192.168.100.1)" --> ESP32Network
        
        subgraph "Software Stack"
            WebApp[FastAPI Web App]
            ReactApp[React Frontend]
            JoyDaemon[Joystick Daemon (Python)]
            ROS2[ROS2 Humble Node]
            uROSAgent[micro-ROS Agent]
            
            ReactApp -- "WebSocket" --> WebApp
            WebApp <--> ROS2
            JoyDaemon <--> ROS2
            ROS2 <--> uROSAgent
        end
    end

    subgraph "Clients"
        User[User (Smartphone/Tablet)] -- "HTTP/WebSocket" --> WLAN0
    end

    subgraph "Isolation Sphere Devices"
        ESP32[ESP32-S3 (Sphere)] -- "micro-ROS (XRCE-DDS)" --> USBWiFi
    end
```

### 1.3 Development/Operation Environment
- **Hardware**: MiniPC (Ubuntu 22.04 LTS)
- **Network**:
    - `wlan0`: for Internet/LAN connection (DHCP Client)
    - `USB WiFi`: for building the dedicated AP (Static IP: 192.168.100.1)
- **Software**:
    - **OS**: Ubuntu 22.04 LTS
    - **Middleware**: ROS2 Humble, micro-ROS Agent
    - **Container**: Docker (recommended: isolation of the micro-ROS Agent environment)

### 1.4 Technology Stack (Finalized)
- **Server Backend**:
    -   **Language**: Python 3.10+
    -   **Framework**: FastAPI (asynchronous processing, WebSocket support)
    -   **ROS2 Interface**: `rclpy`
- **Server Frontend**:
    -   **Framework**: React (Vite)
    -   **UI Library**: **Arwes** (Cyberpunk/Sci-Fi Theme)
    -   **3D Graphics**: **React Three Fiber** (3D Sphere Control)
    -   **Components**: `react-knob-headless` (Custom Neon Dials)
    -   **Style**: CSS Modules / Styled Components
    -   **Communication**: WebSocket (real-time status synchronization)
- **Joystick Daemon**:
    -   **Language**: Python 3.10+
    -   **Library**: `evdev` (input control), `rclpy` (ROS2 communication)

## 2. Functional Requirements

### 2.1 Network Feature (Dual WiFi)
The Server manages two network interfaces.

#### 2.1.1 External Connection (wlan0)
- **Role**: provide access to the user interface (WebUI).
- **Configuration**: connect to an existing home/studio router.
- **Services**: web server (Port 8000/9000), SSH.

#### 2.1.2 Device Connection (USB WiFi)
- **Role**: dedicated communication with the Isolation Sphere device (ESP32).
- **Configuration**: access point (AP) mode.
- **Settings**:
    - **SSID**: `IsolationSphere-Direct` (per config.json)
    - **IP**: `192.168.100.1` (Gateway/Server)
    - **Subnet**: `192.168.100.0/24`
    - **DHCP**: `192.168.100.100` ~ (for ESP32)

### 2.2 Communication System (All micro-ROS)
All communication with ESP32 is unified on **micro-ROS (XRCE-DDS)**.

#### 2.2.1 Shared Data / Topic Design
The following information is shared via ROS2 Topics, enabling control from both the WebUI and the Joystick Daemon.

1.  **Frame image (JPEG compression)**
    -   **Topic**: `/isolation_sphere/image/compressed`
    -   **Type**: `sensor_msgs/CompressedImage`
    -   **Pub**: Server (Video Processor)
    -   **Sub**: ESP32
    -   **Spec**: 320x160, 10fps, JPEG Quality adjustable

2.  **UI information (control/status)**
    -   **Topic**: `/isolation_sphere/ui/control` (Pub: WebUI/JoyDaemon, Sub: Server/ESP32)
    -   **Topic**: `/isolation_sphere/ui/status` (Pub: Server/ESP32, Sub: WebUI/JoyDaemon)
    -   **Content details**:
        -   **Video functions**: Play, Stop, fast-forward (FW), rewind (REV), seek
        -   **Playlist**: create, edit, select, loop settings
        -   **System**: Config update, connection check (Ping/Health), reboot
        -   **Sphere management**:
            -   IMU status (Quaternion)
            -   offset adjustment (Calibration)
            -   color/brightness (Color/Brightness)
            -   pattern projection (Test Pattern)

### 2.3 Server Feature Architecture

#### 2.3.1 WebUI (FastAPI + React)
- **Backend (FastAPI)**:
    -   REST API: video management, configuration management.
    -   WebSocket: real-time status push to the frontend (relaying ROS2 Topics).
    -   Static Files: serving React build artifacts.
- **Frontend (React)**:
    -   **Design Theme**: **Arwes Cyberpunk** (Sci-Fi Hologram).
    -   **3D Interface**: 3D sphere controller using `react-three-fiber`.
        -   **Virtual Trackball**: intuitive operation like rolling the sphere on the smartphone screen.
    -   **Custom UI Elements**:
        -   **Neon Dials**: implemented using `react-knob-headless` as glowing ring-shaped dials.
        -   **Sound Effects**: sci-fi sound effects on operation (Arwes standard feature).
    -   **Real-time Sync**: immediately reflect joystick operations and IMU status via WebSocket.

#### 2.3.2 Joystick Daemon (Python)
- **Role**: provide intuitive operation via a physical controller.
- **Configuration**: runs as an independent daemon process.
- **Features**:
    -   Monitoring of physical joystick/dial input (`evdev`).
    -   Conversion and Publish of input events to a ROS2 Topic (`/isolation_sphere/ui/control`).
    -   LED/Haptic feedback (if any) via Subscribe to the status Topic (`/isolation_sphere/ui/status`).

#### 2.3.3 Video Processing/Distribution
- **Features**: decoding of video files, resize (320x160), JPEG compression, ROS2 Publish.
- **Performance**: stable distribution at 10fps. Bandwidth-adaptive quality adjustment.

## 3. Non-Functional Requirements
- **Stability**: 24-hour continuous operation. Automatic reconnection feature of the micro-ROS Agent.
- **Latency**: control commands < 50ms. Image stream < 100ms.
- **Concurrency**: conflict resolution for simultaneous operations from WebUI and Joystick (Last-Win or exclusive control).

## 4. Development Roadmap
1.  **Server setup**: Ubuntu configuration, Dual WiFi setup (`hostapd`, `netplan`).
2.  **micro-ROS environment**: bringing up the Agent with Docker, Ping connectivity with ESP32.
3.  **Basic communication**: converting IMU data to a ROS2 Topic.
4.  **Image transfer implementation**: implementation of JPEG distribution using `sensor_msgs/CompressedImage` and performance testing.
5.  **Joystick Daemon implementation**: reading input devices and ROS2 integration.
6.  **WebUI integration**: integration of FastAPI and ROS2 node, React frontend implementation (Arwes + R3F + Dials).
 (Arwes + R3F).
