> **English** · [日本語](README_verify_comm.ja.md)

# Server ⇔ Device Communication Verification

You can verify the server's MQTT communication contract even without a physical ESP32 on hand.
A Python device simulator reproduces the MQTT behavior of the ESP32 firmware (core/), and it
communicates bidirectionally with a local MQTT broker + FastAPI server to determine pass/fail.

## Running

```bash
bash server/scripts/verify_server_comm.sh
```

It automatically starts, verifies, and stops mosquitto and the FastAPI server. Prerequisites:

- `brew install mosquitto`
- `paho-mqtt`, `websockets`, `fastapi`, `uvicorn` in `server/.venv`

## MQTT Contract Being Verified

Confirms the correspondence between `core/src/MqttTopics.h` and `server/app/core/config.py`.

| Direction | Topic | Payload |
|---|---|---|
| Device → Server | `sphere/sphere001/imu` | `{"w","x","y","z"}` quaternion |
| Device → Server | `sphere/sphere001/status` | `"online"`/`"offline"` (retained) |
| Server → Device | `sphere/all/command/params` | `{"brightness":..}` etc. |
| Server → Device | `sphere/all/state` | full state (retained, state broadcast) |

## Verification Cases (4)

1. **Server→Device command propagation**: a UI client publishes `command/params {"brightness":73}`
   → the device sim receives it (the firmware's command reception path).
2. **Server state re-distribution**: the server processes the above command and re-distributes
   `sphere/all/state` as retained, confirming that `params.brightness=73` is reflected.
3. **Device→Server IMU ingestion**: the device sim publishes `imu` → the server ingests it into
   the StateManager, confirming that it is reflected in the `STATE_UPDATE` of WebSocket `/ws`.
4. **status online**: the device sim publishes a retained `online`.

## Overriding the Broker

The server can override the broker via the environment variable `SPHERE_MQTT_BROKER`
(allowing local verification or deployment to another environment without editing `config.json`).
When unspecified, it is resolved in the order `config.json`'s `wifi.broker` → `localhost`.

```bash
SPHERE_MQTT_BROKER=localhost .venv/bin/python -m uvicorn app.main:app
```

## Verification on a Physical ESP32 (Next Stage)

This harness is for verifying the server-side contract. To confirm integration with a physical device:

1. Set `wifi.broker` in `core/data/config.json` to the real broker IP
2. Flash the firmware and connect the AtomS3R to WiFi
3. Stop this harness's device sim and observe the physical device's `sphere/sphere001/imu`
   distribution and `sphere/all/command/#` reception across the broker
   (`mosquitto_sub -t 'sphere/#' -v` can intercept all topics)
```
