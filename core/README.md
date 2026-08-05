> **English** · [日本語](README.ja.md)

# Sastle Isolation Sphere

An 800-LED spherical display project based on the M5Atom S3R

## Project Structure

```
sastle-isolation-sphere/
├── core/           # Main firmware
│   ├── src/        # Source code
│   ├── data/       # LittleFS data (images, configuration)
│   ├── doc/        # Documentation
│   └── platformio.ini
└── README.md
```

## Core - Firmware

### Hardware
- **MCU**: M5Atom S3R (ESP32-S3, 240MHz, 8MB Flash, 8MB PSRAM)
- **LED**: 800 WS2812B (4 strips: GPIO 5,6,7,8)
- **IMU**: BNO055 (I2C: GPIO2=SDA, GPIO1=SCL)
- **WiFi**: 192.168.49.101 (static IP)

### Features
- ✅ LittleFS file system
- ✅ JSON configuration management
- ✅ WiFi connection (static IP)
- ✅ UDP reception (port 8889)
- ✅ MQTT communication (192.168.49.1:1883)
- ✅ BNO055 IMU sensor (quaternion, Euler angles, acceleration, gyro)
- ⏳ Gesture input (planned)
- ⏳ LED control (planned)
- ⏳ Image display (planned)

### Documentation
- [Class diagram](doc/class.md)
- [MQTT specification](doc/mqtt.md)
- [Sequence diagram](doc/sequence.md)
- [State diagram](doc/state.md)
- [Dual-core design](doc/dual_core_design.md)
- [Image manager design](doc/image_manager_design.md)
- [IMU compensation design](doc/imu_compensation.md)
- [UDP protocol specification](doc/udp_image_protocol.md)
- [Implementation status](doc/implementation_status.md)
- [Specification](spec.md)

### Build & Flash

```bash
cd core
pio run -t upload
```

### MQTT Topics

#### Subscribe (command reception)
- `sphere/sphere001/command`

#### Publish (data transmission)
- `sphere/sphere001/status` - device state
- `sphere/sphere001/imu` - IMU data (10Hz)
- `sphere/sphere001/response` - command response
- `sphere/sphere001/gesture` - gesture event (planned)
- `sphere/sphere001/ui_mode` - UI mode state (planned)

## License

TBD
