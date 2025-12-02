# Server Development Tasks

This document outlines the tasks for the server-side components based on the simplified MQTT+WebSocket architecture.

## Phase 1: Core Service Implementation (Completed ✅)

### 1. StateManager
- ✅ Centralized state management
- ✅ MQTT command processing
- ✅ WebSocket message handling
- ✅ State broadcasting

### 2. MQTT Service
- ✅ MQTT client implementation
- ✅ ESP32 connection handling
- ✅ Topic subscription (imu, status, state)
- ✅ Command publishing

### 3. FastAPI Server
- ✅ WebSocket endpoint
- ✅ REST API endpoints
- ✅ Frontend static file serving
- ✅ StateManager integration

## Phase 2: Video Streaming (In Progress)

### 1. Video Daemon
- [ ] MQTT subscription for playback commands
- [ ] Playlist management
- [ ] Video decoding (OpenCV)
- [ ] Frame resizing (320x160)
- [ ] JPEG compression
- [ ] UDP transmission to ESP32

### 2. Playlist API
- [ ] Create/Read/Update/Delete playlists
- [ ] Track management
- [ ] Playback control endpoints

## Phase 3: Joystick Support (Pending)

### 1. Joystick Daemon
- [ ] USB device detection (evdev)
- [ ] PS4 controller support
- [ ] Button mapping configuration
- [ ] Direct MQTT publishing (no ROS2)

## Documentation
- ✅ Communication design specification
- ✅ ROS2 removal plan
- ✅ Architecture documentation updated
