# Sequence Diagram (Server)

This document illustrates the sequence of interactions for key use cases in the new architecture.

## 1. Joystick Control to ESP32

This diagram shows how physical joystick movements are translated into commands for the ESP32.

```mermaid
sequenceDiagram
    participant JoyDevice as Physical Joystick
    participant JoyDaemon as Joystick Daemon
    participant ROS2
    participant FastAPI
    participant MQTT
    participant ESP32

    JoyDevice->>JoyDaemon: User moves joystick
    activate JoyDaemon
    JoyDaemon->>ROS2: Publish /joy_data
    deactivate JoyDaemon
    
    ROS2->>FastAPI: Forward /joy_data message
    activate FastAPI
    FastAPI->>FastAPI: Translate ROS2 msg to MQTT payload
    FastAPI->>MQTT: Publish /esp32/command
    deactivate FastAPI

    MQTT->>ESP32: Forward /esp32/command
    activate ESP32
    ESP32->>ESP32: Execute command (e.g., move motors)
    deactivate ESP32
```

## 2. Web UI Initiates Video Streaming

This diagram shows how a user action on the web interface starts the video stream to the ESP32.

```mermaid
sequenceDiagram
    participant WebUI
    participant FastAPI
    participant ROS2
    participant VideoDaemon as Video Streaming Daemon
    participant ESP32

    WebUI->>FastAPI: HTTP Request: POST /video/play
    activate FastAPI
    FastAPI->>ROS2: Publish /video_control
    deactivate FastAPI

    ROS2->>VideoDaemon: Forward /video_control message
    activate VideoDaemon
    VideoDaemon->>VideoDaemon: Start reading video file
    loop For each frame in video
        VideoDaemon->>ESP32: Send frame via UDP
    end
    deactivate VideoDaemon
```

## 3. ESP32 Status Update to Web UI

This diagram shows how the ESP32's status is reported back to the user.

```mermaid
sequenceDiagram
    participant ESP32
    participant MQTT
    participant FastAPI
    participant WebUI

    ESP32->>MQTT: Publish /esp32/status
    
    MQTT->>FastAPI: Forward /esp32/status message
    activate FastAPI
    FastAPI->>WebUI: Push status via WebSocket
    deactivate FastAPI
```
