> [English](class_diagram.md) · **日本語**

# Class Diagram (Server)

This document provides a high-level conceptual class diagram for the server-side components.

## Diagram

```mermaid
classDiagram
    direction LR

    class FastAPIApp {
        <<entrypoint>>
        + api_router
        + websocket_manager
        + ros2_node
        + mqtt_client
        + start()
        + stop()
    }

    class JoystickDaemon {
        <<ros2_node>>
        + ros2_publisher
        + read_joystick()
        + run()
    }

    class VideoStreamingDaemon {
        <<ros2_node>>
        + ros2_subscriber
        + udp_socket
        + stream_video(file_path)
        + run()
    }

    class Ros2Node {
        <<library>>
        + create_publisher(topic)
        + create_subscription(topic, callback)
        + spin()
    }
    
    class MqttClient {
        <<library>>
        + connect()
        + publish(topic, payload)
        + subscribe(topic, callback)
    }

    FastAPIApp --|> Ros2Node : uses
    FastAPIApp --|> MqttClient : uses
    JoystickDaemon --|> Ros2Node : uses
    VideoStreamingDaemon --|> Ros2Node : uses

    FastAPIApp ..> JoystickDaemon : (indirectly via ROS2)
    note for FastAPIApp "Subscribes to /joy_data"
    
    FastAPIApp ..> VideoStreamingDaemon : (indirectly via ROS2)
    note for FastAPIApp "Publishes to /video_control"

    FastAPIApp ..> MqttClient : (Bridge)
    note for FastAPIApp "Publishes to /esp32/command\nSubscribes to /esp32/status"

    JoystickDaemon ..> Ros2Node : (Pub/Sub)
    note for JoystickDaemon "Publishes to /joy_data"

    VideoStreamingDaemon ..> Ros2Node : (Pub/Sub)
    note for VideoStreamingDaemon "Subscribes to /video_control"

```

## Description of Components

- **FastAPIApp**: The main application process. It initializes and manages both a ROS2 node and an MQTT client to act as the central bridge. It exposes the HTTP/WebSocket interface to the outside world.
- **JoystickDaemon**: A dedicated ROS2 node process that reads from a physical joystick and publishes the state to a ROS2 topic.
- **VideoStreamingDaemon**: A dedicated ROS2 node process that listens for commands on a ROS2 topic and, in response, reads a video file and streams it over UDP.
- **Ros2Node / MqttClient**: These represent the libraries or client instances used by the application daemons to interact with the respective middleware.
