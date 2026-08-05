> **English** · [日本語](README.ja.md)

# Isolation Sphere Documentation

Last updated: 2025-12-02

This directory contains the technical documentation for the Isolation Sphere project.

## Document List

### System Design

| Document | Description |
|-------------|------|
| [system_architecture.md](./system_architecture.md) | Overall system architecture, data flow, technology stack |
| [class_diagram.md](./class_diagram.md) | Class diagrams, component structure, Mermaid diagrams |
| [implementation_spec.md](./implementation_spec.md) | Implementation specification, code examples, troubleshooting |

### Project-Specific

- **ESP32 specification**: `../core/spec.md` - firmware specification
- **Server design**: `../server/docs/` - individual server documents

## Quick Links

### For New Developers

1. [Understand the overall system](./system_architecture.md#overview)
2. [Review the data flow](./class_diagram.md#data-flow)
3. [Set up the development environment](./implementation_spec.md#6-build-and-deploy)

### For Implementers

- [MQTT data format](./implementation_spec.md#51-mqtt-payload)
- [WebSocket message specification](./implementation_spec.md#52-websocket-messages)
- [API Endpoints](./system_architecture.md#rest-api-endpoints)

### Troubleshooting

- [Common problems and solutions](./implementation_spec.md#7-troubleshooting)

## System Overview Diagram

```
ESP32 (IMU) --[MQTT]--> Python Server --[WebSocket]--> Web Browser
                            ↓
                     State Manager
```

### Main Components

1. **ESP32 Device**
   - IMU orientation detection (BNO055)
   - Data transmission via MQTT
   - LED control (800 LEDs)

2. **Python Server**
   - MQTT reception
   - WebSocket distribution
   - REST API provision

3. **Web Frontend**
   - Three.js 3D visualization
   - Real-time orientation synchronization
   - React UI

## Implementation Status

### Completed Features ✅

- ESP32 IMU data acquisition and transmission
- MQTT communication
- WebSocket real-time distribution
- 3D sphere orientation visualization
- Dashboard UI
- config.json configuration management

### Not Yet Implemented ⏳

- LED video streaming (UDP)
- Playlist playback
- Changing MQTT settings from the configuration screen

## Technology Stack

- **ESP32**: Arduino, PubSubClient, BNO055, FastLED
- **Server**: Python, FastAPI, paho-mqtt, uvicorn
- **Frontend**: React, Three.js, Material-UI, Vite

## Development Guidelines

### Branch Strategy
- `main`: stable version
- feature branches: feature development

### Commit Messages
```
feat: add new feature
fix: bug fix
docs: documentation update
refactor: refactoring
```

### Testing
- ESP32: unit tests in the PlatformIO Native environment
- Server: API tests with pytest
- Frontend: component tests with Vitest

## Contacts & Resources

- **GitHub**: https://github.com/sastles-com/sastle-isolation-sphere
- **Project root**: `<repo-root>/sastle-isolation-sphere`

## Change History

| Date | Contents |
|------|------|
| 2025-12-02 | First edition of documentation created, IMU integration completed |
| 2025-12-01 | Project structure established |
