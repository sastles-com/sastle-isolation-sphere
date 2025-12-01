# UDP Image Protocol Specification

## 概要
Isolation SphereプロジェクトでPC/ROS2からESP32へJPEG画像をリアルタイム転送するためのUDPプロトコル仕様。

## プロトコルバージョン
- Version: 1.0
- Date: 2025-12-01

## パケットフォーマット

### シングルパケット方式 (v1.0)

```
┌─────────────────────────────────────────┐
│ Header (8 bytes)                        │
├─────────────────────────────────────────┤
│ Magic Number    : uint32_t (4 bytes)    │
│ Frame ID        : uint32_t (4 bytes)    │
├─────────────────────────────────────────┤
│ JPEG Binary Data (N bytes)              │
│ ...                                     │
│ ...                                     │
└─────────────────────────────────────────┘
```

### フィールド定義

#### Header (8 bytes)

| Field | Offset | Size | Type | Value | Description |
|-------|--------|------|------|-------|-------------|
| Magic Number | 0 | 4 | uint32_t | 0x4A504547 | マジックナンバー "JPEG" (リトルエンディアン) |
| Frame ID | 4 | 4 | uint32_t | 0 ~ 0xFFFFFFFF | フレーム連番 (送信側で管理) |

#### JPEG Data

| Field | Offset | Size | Description |
|-------|--------|------|-------------|
| JPEG Binary | 8 | Variable | 標準JPEG形式のバイナリデータ |

### パケットサイズ制限

- **最小サイズ**: 8 bytes (ヘッダーのみ、データなし)
- **推奨最大サイズ**: 65,507 bytes (UDP理論上限)
- **実用サイズ**: 10,000 ~ 30,000 bytes (320x160 JPEG at quality 70-90)

## 画像仕様

### 推奨パラメータ

| Parameter | Value | Note |
|-----------|-------|------|
| Width | 320 pixels | config.jsonで設定可能 |
| Height | 160 pixels | config.jsonで設定可能 |
| Format | JPEG | Baseline JPEG |
| Color Space | RGB / YCbCr | 標準JPEG |
| Quality | 70-90 | 品質と転送量のバランス |

### JPEG圧縮率の目安

| Quality | File Size | FPS @ 1Gbps | FPS @ 100Mbps |
|---------|-----------|-------------|---------------|
| 50 | ~8 KB | 15,625 | 1,562 |
| 70 | ~15 KB | 8,333 | 833 |
| 90 | ~30 KB | 4,166 | 416 |
| 95 | ~50 KB | 2,500 | 250 |

**WiFi環境での推奨**: Quality 70-80, 30fps可能

## 通信パラメータ

### ESP32側設定

```json
{
  "wifi": {
    "SSID": "ESP32-P2P-Direct",
    "static_ip": "192.168.49.101",
    "udp_port": 8889
  },
  "image": {
    "width": 320,
    "height": 160,
    "format": "RGB565",
    "type": "JPEG"
  }
}
```

### PC側設定

```python
ESP32_IP = "192.168.49.101"
ESP32_PORT = 8889
```

## データフロー

```
┌──────────┐                 ┌──────────┐                 ┌──────────┐
│ Camera / │                 │  WiFi    │                 │  ESP32   │
│ Image    │  ────UDP───>    │ Network  │  ────UDP───>    │ LwIP     │
│ Source   │  (JPEG+Header)  │          │  (Fragments)    │ Stack    │
└──────────┘                 └──────────┘                 └──────────┘
                                                                 │
                                                                 ▼
                                                          ┌──────────┐
                                                          │ UDP      │
                                                          │ Buffer   │
                                                          │ (PSRAM)  │
                                                          └──────────┘
                                                                 │
                                                                 ▼
                                                          ┌──────────┐
                                                          │ TJpg     │
                                                          │ Decoder  │
                                                          └──────────┘
                                                                 │
                                                                 ▼
                                                          ┌──────────┐
                                                          │ RGB565   │
                                                          │ Buffer   │
                                                          │ (Double) │
                                                          └──────────┘
```

## エラーハンドリング

### パケット検証

ESP32側で以下の検証を実施:

1. **パケットサイズチェック**
   - 最小サイズ (8 bytes) 以上
   - 最大サイズ (65,507 bytes) 以下

2. **マジックナンバー検証**
   - `0x4A504547` ("JPEG") であること

3. **JPEG整合性チェック**
   - TJpg_Decoderで画像サイズを取得
   - 設定された幅・高さと一致することを確認

### エラー時の動作

- **パケットドロップ**: フレームをスキップし、次のフレームを待つ
- **デコードエラー**: エラーカウンタを増やし、バッファを更新しない
- **統計情報**: `frames_dropped`, `decode_errors` で記録

### パケットロス対策

- **最新フレーム優先**: 古いフレームはバッファリングせず破棄
- **リカバリー不要**: 次のフレームで自動復帰
- **統計モニタリング**: FPS、ドロップ率を監視

## 送信側実装例

### Python (OpenCV)

```python
import cv2
import socket
import struct

MAGIC = 0x4A504547
ESP32_IP = "192.168.49.101"
ESP32_PORT = 8889

def send_frame(jpeg_data, frame_id):
    header = struct.pack('<II', MAGIC, frame_id)
    packet = header + jpeg_data
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.sendto(packet, (ESP32_IP, ESP32_PORT))
    sock.close()

# カメラキャプチャ
cap = cv2.VideoCapture(0)
frame_id = 0

while True:
    ret, frame = cap.read()
    frame = cv2.resize(frame, (320, 160))
    _, jpeg = cv2.imencode('.jpg', frame, [cv2.IMWRITE_JPEG_QUALITY, 80])
    send_frame(jpeg.tobytes(), frame_id)
    frame_id += 1
```

### ROS2 (C++)

```cpp
#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/image.hpp>
#include <cv_bridge/cv_bridge.h>
#include <opencv2/opencv.hpp>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

struct UDPImageHeader {
    uint32_t magic = 0x4A504547;
    uint32_t frame_id;
} __attribute__((packed));

void send_jpeg_udp(const std::vector<uint8_t>& jpeg_data, 
                   uint32_t frame_id,
                   const std::string& ip, 
                   uint16_t port) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);
    
    UDPImageHeader header;
    header.frame_id = frame_id;
    
    std::vector<uint8_t> packet;
    packet.resize(sizeof(header) + jpeg_data.size());
    memcpy(packet.data(), &header, sizeof(header));
    memcpy(packet.data() + sizeof(header), jpeg_data.data(), jpeg_data.size());
    
    sendto(sock, packet.data(), packet.size(), 0, 
           (struct sockaddr*)&addr, sizeof(addr));
    
    close(sock);
}
```

## 将来の拡張 (v2.0以降)

### マルチパケット方式

大きな画像 (> 64KB) を複数パケットに分割:

```
┌─────────────────────────────────────────┐
│ Header (16 bytes)                       │
├─────────────────────────────────────────┤
│ Magic           : 0x4A504547            │
│ Frame ID        : uint32_t              │
│ Chunk Index     : uint16_t              │
│ Total Chunks    : uint16_t              │
│ Chunk Size      : uint16_t              │
│ Reserved        : uint16_t              │
├─────────────────────────────────────────┤
│ JPEG Chunk Data (最大1400 bytes)        │
└─────────────────────────────────────────┘
```

### 追加機能候補

- **フレームスキップ制御**: PC側からフレームレート調整
- **画質動的調整**: ネットワーク状況に応じてJPEG品質を変更
- **ACK/NACK**: 重要フレームの再送制御
- **圧縮形式拡張**: H.264, WebP対応

## 参考資料

- [UDP Packet Structure](https://en.wikipedia.org/wiki/User_Datagram_Protocol)
- [JPEG File Interchange Format](https://www.w3.org/Graphics/JPEG/itu-t81.pdf)
- [TJpg_Decoder Library](https://github.com/Bodmer/TJpg_Decoder)
- [ESP32 LwIP Stack](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/lwip.html)
