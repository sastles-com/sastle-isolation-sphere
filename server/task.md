# Server Development Tasks

This document outlines the tasks for the server-side components based on the defined architecture.

## Phase 1: Core Service Implementation

### 1. Joystick Daemon
- [ ] Create a Python script to read data from a USB joystick (e.g., using `evdev`).
- [ ] Implement as a ROS2 node.
- [ ] Define a custom ROS2 message for joystick state (`JoyState.msg`).
- [ ] Publish joystick data to the `/joy_data` ROS2 topic.

### 2. Video Streaming Daemon
- [ ] Create a Python script to be the daemon.
- [ ] Implement as a ROS2 node that subscribes to the `/video_control` topic.
- [ ] Implement logic to read a video file using OpenCV.
- [ ] Implement UDP socket programming to stream video frames to the ESP32.
- [ ] Define a ROS2 message for video control commands (`VideoControl.msg`).

### 3. FastAPI Server
- [ ] **ROS2/MQTT Integration**:
    - [ ] Implement ROS2 node initialization within the FastAPI application lifecycle.
    - [ ] Implement an MQTT client (e.g., using `gmqtt`) that connects to the broker.
- [ ] **Bridge Implementation**:
    - [ ] Create a ROS2 subscriber for `/joy_data`.
    - [ ] In the subscriber callback, convert ROS2 messages to a defined JSON payload and publish via MQTT to `/esp32/command`.
    - [ ] Create an MQTT subscriber for `/esp32/status`.
    - [ ] In the subscriber callback, forward status data to the Web UI via WebSocket.
- [ ] **API & Control Endpoints**:
    - [ ] Create a WebSocket endpoint for the Web UI to connect to.
    - [ ] Create HTTP endpoints to trigger video playback (e.g., `/video/play`), which will publish messages to the `/video_control` ROS2 topic.

### 4. Overall System
- [ ] Define common ROS2 message types (`.msg` files) in a shared package.
- [ ] Create `launch.py` files to start all the daemons and nodes together.
- [ ] Write documentation for topic names, message formats, and API endpoints.
