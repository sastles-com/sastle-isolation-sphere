> **English** · [日本語](spec.ja.md)

# Isolation Sphere ESP32 Firmware Specification

## 1. Overview
This is the firmware specification for a spherical display control system based on the ESP32-S3 (M5Atom S3R).
It builds on the MFT2025 project and optimizes the communication architecture.
The main features are LED control via FastLED, attitude detection via IMU, and communication with external systems.

## 2. Hardware Configuration
- **Controller**: M5Atom S3R (ESP32-S3)
- **LED**: WS2812 LED strips x 4 (GPIO 5, 6, 7, 8)
  - Configuration: [180, 220, 180, 220] (800 LEDs total)
- **IMU**: internal or external (BNO055) over I2C
- **Speaker**: M5Atom S3R built-in / external I2S

## 3. Communication Architecture (UDP + MQTT hybrid)
Considering real-time performance and reliability, roles are divided as follows.

### 3.1 UDP (fast, low-latency)
- **Purpose**: **video data (LED)**
- **PC -> ESP32**:
  - Streams **one frame of JPEG image data** at a time.
  - The ESP32 decodes the JPEG, maps it to LED coordinates, and displays it.
  - Packet loss is tolerated (latest frame takes priority).

### 3.2 MQTT (high-reliability, control)
- **Purpose**: **IMU data**, UI information, system control
- **Broker**: Raspberry Pi (Agent)
- **Topics**:
  - `sphere/status`: device heartbeat, error information
  - `sphere/imu`: **IMU quaternion data (w, x, y, z)**
  - `sphere/ui`: UI commands, state sharing
  - `sphere/command`: system commands (reboot, OTA, mode change, etc.)
  - `sphere/config`: configuration parameter updates

### 3.3 ROS2 Integration (PC-side bridge)
- **Bridge**: UDP/MQTT <-> ROS2
  - MQTT `sphere/imu` -> ROS2 `/isolation_sphere/imu` (`sensor_msgs/Imu`)
  - ROS2 `/isolation_sphere/led` -> UDP packet (sent to ESP32)

## 4. Data Structures
### Config (`config.json`)
- **Network**: SSID, Password
- **Agent**: IP Address, Port (MQTT Broker / UDP Target)
- **Node Name**: device identifier
- **Debug**:
  - `lcd_enable`: turn LCD display ON/OFF (for debugging, default: false)

### LED Layout (`led_layouts-5strip.csv`)
- Format: `faceID, stripID, stripIndex, x, y, z`
- Defines the 3D coordinates of each LED, used for texture mapping.

## 5. Functional Requirements List
- [x] **File system**: LittleFS (for storing Config, Layout)
- [x] **Network**: Wi-Fi connection management
- [x] **Communication**:
    - [x] MQTT client (AsyncMqttClient)
    - [ ] UDP communication (video receive / IMU send) - **new implementation**
        - [ ] **Double buffering**: separate the receive buffer from the render buffer and update them asynchronously.
- [x] **IMU**: quaternion acquisition and transmission
- [x] **LED control**:
    - [x] FastLED initialization (4 strips)
    - [x] Coordinate mapping (Spherical -> UV)
    - [ ] **I2S DMA transfer**: to drive 800 LEDs at 30fps, adopt the I2S DMA method (per MFT2025).
    - [ ] **On-device rendering**: apply coordinate transformation to the received texture (image) using the latest IMU values to determine LED colors.
- [ ] **Audio**:
    - [ ] Startup sound
    - [ ] System event sounds
- [ ] **Other**:
    - [ ] LCD debug display (ON/OFF toggle via Config)

## 6. Questions & Concerns
1.  **LED driver optimization**:
    - Currently the RMT method is used, but migrating to the I2S DMA method (adopted in MFT2025) is recommended to drive 800 LEDs at 30fps. -> **Decided to adopt I2S DMA**
2.  **Media features**:
    - **Decision**: headless operation is the default, but the LCD ON/OFF should be switchable via `config.json` for debugging.
3.  **IMU coordinate transformation**:
    - Currently UV transformation is performed inside the ESP32, but processing it on the PC side and sending only LED color data (UDP streaming) would reduce the ESP32's load. -> **Decided to adopt on-device rendering (to support standalone demos)**
4.  **Joystick input**:
    - Receiving it directly on the ESP32 was considered, but due to concerns over insufficient resources, this is **TBD (future consideration)** for now.
</content>
