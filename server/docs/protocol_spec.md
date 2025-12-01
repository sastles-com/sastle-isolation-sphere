# Protocol Specification

This document details the communication protocols used within the server and between the server and external devices.

## 1. Overview
The system utilizes a hybrid communication model:
- **ROS2**: For internal, structured communication between server-side daemons.
- **MQTT**: For bidirectional, low-bandwidth command and status messages between the server and the ESP32.
- **UDP**: For high-throughput, one-way data streaming from the server to the ESP32.
- **HTTP/WebSocket**: For all communication with the Web UI.

## 2. ROS2 (Internal Daemon Communication)
- **Purpose**: To provide a robust, type-safe, and debuggable backbone for inter-process communication on the MiniPC.
- **Key Topics**:
    - `/joy_data`: Publishes joystick state from the `Joystick Daemon`. Consumed by `FastAPI Server`.
    - `/video_control`: Publishes video control commands from `FastAPI Server`. Consumed by `Video Streaming Daemon`.
- **Message Types**: Custom `.msg` files will be used to ensure data consistency (e.g., `JoyState.msg`, `VideoControl.msg`).

## 3. MQTT (Server-ESP32 Communication)
- **Purpose**: Reliable, low-latency messaging for commands and status updates.
- **Broker**: A central MQTT broker (e.g., Mosquitto) runs on the MiniPC.
- **Key Topics**:
    - `/esp32/command`: `FastAPI Server` publishes commands (originating from joystick or UI) for the ESP32 to execute.
    - `/esp32/status`: The `ESP32` publishes its current state, sensor readings, or acknowledgements.
- **Payload Format**: JSON (UTF-8 encoded).

## 4. UDP (Server -> ESP32 Video Streaming)
- **Purpose**: High-speed, one-way transfer of video frames. A connectionless protocol is chosen to prioritize throughput over reliability for video data.
- **Sender**: `Video Streaming Daemon`.
- **Receiver**: `ESP32`.
- **Payload Format**: Raw or lightly-encoded binary image data (e.g., RGB565 bitmaps).

## 5. HTTP
- **Purpose**: Serving the static Web UI assets (HTML, JS, CSS) and providing RESTful API endpoints for one-off actions.
- **Server**: `FastAPI Server`.

## 6. WebSocket
- **Purpose**: Real-time, bidirectional communication between the Web UI and the `FastAPI Server` for things like status updates.
- **Server**: `FastAPI Server`.
- **Message Format**: JSON.
