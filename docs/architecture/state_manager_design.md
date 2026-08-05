> **English** · [日本語](state_manager_design.ja.md)

# StateManager Architecture Design Document

Last updated: 2025-12-02

## Overview

StateManager functions as the **sole state manager** in the Isolation Sphere system.

**Important**: It is implemented not as a new daemon but as a **singleton service within the existing FastAPI server**.

---

## System Architecture

### Daemon Composition (Unchanged)

```
Entire system:
├── FastAPI Server (existing) :9000
│   ├── HTTP API
│   ├── WebSocket (/ws)
│   ├── StateManager ← integrated here (no new daemon)
│   └── MQTTService
├── MQTT Broker (mosquitto) :1883
└── Frontend (static file serving)
```

### Process Composition

```
┌─────────────────────────────────────────────────────────┐
│ FastAPI Server (uvicorn) - single process               │
│                                                         │
│  main.py                                                │
│    ├── StateManager (Singleton)                         │
│    ├── MQTTService (Singleton)                          │
│    ├── WebSocket endpoints                              │
│    └── HTTP API endpoints                               │
└─────────────────────────────────────────────────────────┘
```

---

## Responsibilities

| Component | Responsibility | Process |
|---------------|------|---------|
| **FastAPI Server** | HTTP API, WebSocket, startup management | uvicorn |
| **StateManager** | State management, command processing, state distribution | Within FastAPI |
| **MQTTService** | MQTT communication, command subscription | Within FastAPI |
| **MQTT Broker** | Messaging | mosquitto |

---

## Sequence Diagram

See `docs/mqtt_ui_control.md` for the detailed sequence diagram.

### Summary: Data Flow

```
UI → Command → MQTT → StateManager → State → MQTT/WebSocket → everyone synchronized
```

---

## Implementation Checklist

### Phase 1: StateManager Integration

- [ ] Extend StateManager (`app/services/state_manager.py`)
- [ ] Extend MQTTService (`app/services/mqtt_service.py`)
- [ ] Modify main.py (coordination at startup)
- [ ] Modify WebSocket endpoints

### Phase 2: ESP32 Implementation

- [ ] Subscribe to sphere/all/state
- [ ] Reflect LED state

### Phase 3: Web UI Implementation

- [ ] Subscribe to state and update UI
- [ ] Send commands

---

## Summary

- **No new daemon needed** ✅
- **Self-contained within the existing FastAPI server** ✅
- **No infrastructure changes** ✅
