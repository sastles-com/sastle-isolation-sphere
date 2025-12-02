# MQTT Message Specification

This document defines the MQTT topics and payloads for communication between the ESP32 and the server.

## 1. Topic Namespace
All topics follow the pattern: `sphere/{device_id}/{category}`
- **device_id**: Device identifier (e.g., `sphere001`)
- **category**: Message category (e.g., `status`, `imu`, `command`)

## 2. Connection Settings
- **Broker**: 192.168.49.1
- **Port**: 1883
- **Protocol**: MQTT v3.1.1
- **Authentication**: Anonymous (no auth)
- **Keep Alive**: 15 seconds
- **Default QoS**: 0 (At most once)

## 3. Server-to-ESP32 (S2C)

### `sphere/{device_id}/command`
- **Direction**: `Server` -> `ESP32`
- **Publisher**: `FastAPI Server` (forwarding commands from Joystick or Web UI)
- **Description**: Sends action commands to the ESP32.
- **QoS**: 0
- **Retain**: No
- **Payload Format**: Plain text command strings

#### Supported Commands:

**status** - Device status query
```
status
```
Response: Published to `sphere/{device_id}/response`

**restart** - Device restart
```
restart
```
Action: ESP32 software reset

**set_brightness** - Set LED brightness (0-255)
```
set_brightness:128
```

**show_image** - Display specific image
```
show_image:/images/demo01/frame_001.jpg
```

**set_mode** - Change display mode
```
set_mode:rotate
```
Values: `static` | `rotate` | `animate`

## 4. ESP32-to-Server (C2S)

### `sphere/{device_id}/status`
- **Direction**: `ESP32` -> `Server`
- **Publisher**: `ESP32 Device`
- **Subscriber**: `FastAPI Server` (for bridging to Web UI)
- **Description**: Device online/offline status
- **QoS**: 0
- **Retain**: No
- **Payload Example**:
    ```json
    {
      "status": "online",
      "timestamp": 1234567890
    }
    ```

**Timing**:
- On MQTT connect: `status: "online"`
- On graceful shutdown: `status: "offline"`

### `sphere/{device_id}/imu`
- **Direction**: `ESP32` -> `Server`
- **Publisher**: `ESP32 Device`
- **Description**: IMU sensor data (quaternion)
- **QoS**: 0
- **Retain**: No
- **Update Rate**: 10Hz (100ms interval)
- **Payload Example**:
    ```json
    {
      "w": 0.7042,
      "x": 0.3469,
      "y": 0.6196,
      "z": 0.0000
    }
    ```

**Data Specification**:
- `w`: Quaternion w component (scalar)
- `x`, `y`, `z`: Quaternion vector components
- Range: -1.0 to 1.0
- Precision: 4 decimal places

### `sphere/{device_id}/response`
- **Direction**: `ESP32` -> `Server`
- **Publisher**: `ESP32 Device`
- **Description**: Response to commands
- **QoS**: 0
- **Retain**: No
- **Payload Example**:
    ```json
    {
      "command": "status",
      "result": "OK",
      "data": {
        "device": "sphere001",
        "uptime": 12345,
        "free_heap": 234567,
        "imu_calibration": {
          "sys": 3,
          "gyro": 3,
          "accel": 3,
          "mag": 3
        }
      }
    }
    ```

### `sphere/{device_id}/gesture`
- **Direction**: `ESP32` -> `Server`
- **Publisher**: `ESP32 Device`
- **Description**: Gesture event notifications
- **QoS**: 0
- **Retain**: No
- **Payload Example (Triple Shake)**:
    ```json
    {
      "event": "triple_shake",
      "timestamp": 1234567890
    }
    ```
- **Payload Example (Rotation)**:
    ```json
    {
      "event": "rotation",
      "axis": "roll",
      "direction": "positive",
      "angle": 52.3,
      "action": "next_image"
    }
    ```

**Actions**: `next_image` | `prev_image` | `brightness_up` | `brightness_down` | `next_mode` | `prev_mode`

### `sphere/{device_id}/ui_mode`
- **Direction**: `ESP32` -> `Server`
- **Publisher**: `ESP32 Device`
- **Description**: UI mode state changes
- **QoS**: 0
- **Retain**: No
- **Payload Example**:
    ```json
    {
      "mode": "active",
      "timeout": 10000
    }
    ```

**Modes**: `normal` | `active` | `selecting`
