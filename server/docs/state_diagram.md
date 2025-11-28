# State Diagram (Server)

This document illustrates the possible states and transitions for key components.

## 1. Video Streaming Daemon States

This diagram shows the lifecycle of the `VideoStreamingDaemon` in response to ROS2 commands.

```mermaid
stateDiagram-v2
    direction LR
    
    [*] --> Idle : Daemon starts

    Idle --> Streaming : Receives ROS2 command {"action": "play", ...}
    Streaming --> Stopped : Receives ROS2 command {"action": "stop"}
    Streaming --> Idle : Video finishes or error occurs
    
    Stopped --> Streaming : Receives ROS2 command {"action": "play", ...}
    Stopped --> Idle : (Optional: after a timeout or explicit reset command)
```

## 2. ESP32 State (Conceptual)

This diagram shows the conceptual state of the ESP32 device as controlled by the server.

```mermaid
stateDiagram-v2
    direction LR

    [*] --> Idle

    Idle --> Command_Mode : Receives Joystick command (MQTT)
    Command_Mode --> Idle : Joystick returns to neutral

    Idle --> Video_Mode : Receives Video frame (UDP)
    Video_Mode --> Idle : Video stream ends or timeout

    Command_Mode --> Video_Mode : Video stream starts (UDP overrides)
    Video_Mode --> Command_Mode : Joystick command received (MQTT overrides)

```
