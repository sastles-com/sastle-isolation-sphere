# MQTT Message Specification

This document defines the MQTT topics and payloads for communication between the ESP32 and the server.

## 1. Topic Namespace
To prevent collisions, all topics are prefixed with `isolation-server/`. The `{device_id}` allows for multi-device support, but a default can be used for single-device setups.

## 2. Server-to-ESP32 (S2C)

### `isolation-server/esp32/{device_id}/command`
- **Direction**: `Server` -> `ESP32`
- **Publisher**: `FastAPI Server` (forwarding commands from Joystick or Web UI)
- **Description**: Sends action commands to the ESP32.
- **QoS**: 1 (At Least Once)
- **Retain**: No
- **Payload Example (from Joystick)**:
    ```json
    {
      "timestamp": "2025-11-29T10:00:00Z",
      "source": "joystick",
      "action": "set_motor_speed",
      "payload": {
        "left_motor": 0.8,
        "right_motor": -0.8
      }
    }
    ```
- **Payload Example (from Web UI)**:
    ```json
    {
      "timestamp": "2025-11-29T10:01:00Z",
      "source": "web_ui",
      "action": "set_led_pattern",
      "payload": {
        "pattern": "rainbow",
        "speed": 50
      }
    }
    ```

## 3. ESP32-to-Server (C2S)

### `isolation-server/esp32/{device_id}/status`
- **Direction**: `ESP32` -> `Server`
- **Publisher**: `ESP32 Device`
- **Subscriber**: `FastAPI Server` (for bridging to Web UI)
- **Description**: Periodically publishes the device's health and state.
- **QoS**: 0 (Best Effort)
- **Retain**: Yes (Allows new connections to get the last known status immediately)
- **Payload Example**:
    ```json
    {
      "timestamp": "2025-11-29T10:00:01Z",
      "device_id": "esp32-main",
      "battery_voltage": 4.1,
      "wifi_rssi": -62,
      "state": "playing_video",
      "uptime_seconds": 3600
    }
    ```
