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
- **Broker**: A central MQTT broker (e.g., Mosquitto) runs on the MiniPC (192.168.49.1:1883).
- **Key Topics**:
    - `sphere/{device_id}/command`: `FastAPI Server` publishes commands (originating from joystick or UI) for the ESP32 to execute.
    - `sphere/{device_id}/status`: The `ESP32` publishes online/offline status.
    - `sphere/{device_id}/imu`: The `ESP32` publishes IMU sensor data (quaternion) at 10Hz.
    - `sphere/{device_id}/gesture`: The `ESP32` publishes gesture detection events.
    - `sphere/{device_id}/response`: The `ESP32` publishes command responses.
- **Payload Format**: 
    - Commands: Plain text strings
    - Responses/Data: JSON (UTF-8 encoded)

## 4. UDP (Server -> ESP32 Video Streaming) — チャンク分割JPEG

映像フレーム(320x160 JPEG)を ESP32 へ一方向ストリーミングする。**唯一の正となる仕様**で、
送信側 `server/scripts/stream_to_sphere.py` と受信側 `core/src/ImageManager.{h,cpp}`
(`UDPChunkHeader` / `decodeOneFrame`) はこれに従うこと。両者を変更する際は必ず同時に。

- **送信先**: デバイスの static_ip (config.json `sphere.static_ip`) : UDPポート (`wifi.udp_port`, 既定 8889)。
- **なぜチャンク分割か**: 1フレームのJPEGは数KB〜十数KBで MTU(~1500B)を超え IP断片化する。
  ESP32 の lwIP は断片化UDPを受信できない(WiFiUDP/AsyncUDPとも実機で不可と確認)。
  そこで **アプリ層で MTU内チャンクに分割**し、受信側が `frame_id` 単位で再構成する。
- **1チャンク = 16Bヘッダ + JPEGデータ(<=1400B)**。ヘッダは little-endian:

  | オフセット | 型 | フィールド | 説明 |
  |---|---|---|---|
  | 0 | u32 | `magic` | `0x4A504547` ("JPEG") |
  | 4 | u32 | `frame_id` | フレーム連番。変化で新フレーム開始 |
  | 8 | u16 | `chunk_index` | チャンク番号 (0..count-1) |
  | 10 | u16 | `chunk_count` | このフレームの総チャンク数 (<=46) |
  | 12 | u16 | `chunk_size` | このチャンクのJPEGバイト数 (<=1400) |
  | 14 | u16 | `reserved` | 0 |

- **再構成**: 受信側は `chunk_index * 1400` のオフセットでJPEGを組み立て、`chunk_count` 個
  揃ったらデコード。`frame_id` が変わると未完フレームは破棄(ロス耐性)。

> 注: 旧仕様にあった ROS2 / 生RGB565 は不使用(ROS2はサーバーから削除済み, §2は歴史的記述)。

## 5. HTTP
- **Purpose**: Serving the static Web UI assets (HTML, JS, CSS) and providing RESTful API endpoints for one-off actions.
- **Server**: `FastAPI Server`.

## 6. WebSocket
- **Purpose**: Real-time, bidirectional communication between the Web UI and the `FastAPI Server` for things like status updates.
- **Server**: `FastAPI Server`.
- **Message Format**: JSON.
