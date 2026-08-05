> **English** · [日本語](README.ja.md)

# TimeSync Host Unit Test

A test that verifies the time-calculation logic of `TimeSync` without a physical device or an MQTT broker.
It replaces `millis()` with a shim (`shim/Arduino.h`) and compiles, links, and runs the actual
`core/src/TimeSync.cpp` as-is on the host (g++).

It is placed under `tools/` rather than PlatformIO's `test/` to avoid interference from `pio test`,
which would try to build it for the target (ESP32).

## Running

```sh
cd core
pio run              # Build once to place ArduinoJson into .pio/libdeps (first time only)
bash tools/timesync_test/run.sh
```

It passes if all checks are `[ok]` with `0 failures` (exit code 0).

## Verification Items

1. Initial synchronization and clock interpolation (`syncedNow()` follows elapsed time)
2. EMA smoothing (small jitter is applied at only 1/4)
3. Rejection of single outliers (rejects WiFi jitter spikes)
4. Re-synchronization following consecutive outliers (follows real clock jumps such as a server restart)
5. **Absorbing the millis() 32-bit wrap-around** (most important: `syncedNow()` stays continuous and
   post-wrap beacons are not misjudged as outliers)
6. Rejection of invalid payloads (non-JSON / missing `epoch_ms`)

Design: `core/doc/time_sync_show.md`
