# Tasks

- [x] Analyze MFT2025 reference code <!-- id: 0 -->
    - [x] Explore MFT2025/isolation-sphere <!-- id: 1 -->
    - [x] Explore MFT2025/SPHERE_neon <!-- id: 2 -->
    - [x] Identify reusable components and logic <!-- id: 3 -->
- [x] Design ESP32 features for isolation-server <!-- id: 4 -->
    - [x] Update spec.md with findings <!-- id: 5 -->
    - [x] Create implementation plan <!-- id: 6 -->

## Phase 1: Filesystem & Configuration (Priority)
- [x] **Test Filesystem (LittleFS)** <!-- id: 26 -->
    - [x] Create test for LittleFS mount/read/write (ESP32) <!-- id: 27 -->
    - [x] Verify `data` folder upload (partition check) <!-- id: 28 -->
- [x] **Test ConfigManager** <!-- id: 29 -->
    - [x] Create Native test for JSON/CSV parsing logic (`test_config_manager`) <!-- id: 30 -->
    - [x] Create ESP32 integration test for loading config from LittleFS <!-- id: 31 -->

## Phase 2: Core Logic (Native)
- [x] **Test LED Mapping** <!-- id: 32 -->
    - [x] Verify `sphericalToUV` logic (Native) <!-- id: 33 -->
- [x] **Test Double Buffering** <!-- id: 34 -->
    - [x] Verify buffer swap and thread safety (Native) <!-- id: 35 -->

## Phase 3: Network & Communication
- [ ] **Test Network Connectivity** <!-- id: 36 -->
    - [ ] Verify WiFi connection using Config <!-- id: 37 -->
    - [ ] Verify MQTT connection and Pub/Sub <!-- id: 38 -->
- [ ] **Implement & Test UDP** <!-- id: 14 -->
    - [ ] Create UDPAdapter for ESP32 <!-- id: 15 -->
    - [ ] Implement Image reception logic (JPEG) with Double Buffering <!-- id: 16 -->
    - [ ] Implement JPEG decoding & mapping logic <!-- id: 21 -->
    - [ ] Verify UDP reception and decoding (ESP32) <!-- id: 39 -->

## Phase 4: Hardware & Integration
- [ ] **Implement & Test LED Driver** <!-- id: 22 -->
    - [ ] Implement I2S DMA support for FastLED <!-- id: 23 -->
    - [ ] Implement On-device Rendering (IMU rotation + Texture Mapping) <!-- id: 24 -->
    - [ ] Verify LED output performance (30fps check) <!-- id: 40 -->
- [ ] **System Integration** <!-- id: 41 -->
    - [ ] Implement LCD Debug Toggle <!-- id: 25 -->
    - [ ] Full pipeline test (PC -> UDP -> ESP32 -> LED) <!-- id: 42 -->

## Phase 5: Server-side
- [ ] **Server-side Implementation** <!-- id: 17 -->
    - [ ] Create ROS2 Bridge (MQTT/UDP <-> ROS2) <!-- id: 18 -->
    - [ ] Implement IMU subscriber (MQTT) <!-- id: 19 -->
    - [ ] Implement LED publisher (UDP) <!-- id: 20 -->
