# Isolation Server Project Guide

This document serves as the primary entry point for developers and AI agents to understand, build, test, and continue development of the Isolation Server project.

## 1. Project Overview & Architecture
The project consists of an ESP32 firmware (M5Atom S3R) and a Python backend server.
Before starting any work, **review the following design documents**:

- **Class Diagram**: `esp32/docs/class_diagram.md` (System structure & Dependency Injection)
- **State Machine**: `esp32/docs/state_diagram.md` (Network & System states)
- **Sequence Diagram**: `esp32/docs/sequence_diagram.md` (Startup & Provisioning flow)



### Conversation Guidelines
- 思考は英語で，私との会話は日本語で
- 常に日本語で会話する
- **実装前の承認:** テストの作成とコミットが完了した後、実際の実装コードに着手する前には、**必ずユーザーの承認を得ること**。


### Test-Driven Development (TDD)

- 原則としてテスト駆動開発（TDD）で進める
- 期待される入出力に基づき、まずテストを作成する
- 実装コードは書かず、テストのみを用意する
- テストを実行し、失敗を確認する
- テストが正しいことを確認できた段階でコミットする
- その後、テストをパスさせる実装を進める
- 実装中はテストを変更せず、コードを修正し続ける
- すべてのテストが通過するまで繰り返す
- テストはcpp，pythonで作成する場合はクラスとして実装することでカプセル化し，本実装時にはそのクラスを使って実装する

### classベースの機能実装
- config, buzzer, LCD, IMUなどタスクで分かれているものはクラス化できるか検討
- ユニットテストはクラス単位で作成し，検証する
- プログラム本体はこれらのクラスを組み合わせて実装するスタイル


## 2. Development Status (Current Context)
As of the last update, the project is in the **TDD Implementation Phase**.

- **Completed**:
    - `ConfigManager`: JSON parsing, default values.
    - `DeviceManager`: Platform initialization (M5, PSRAM, LittleFS) and component delegation. **Refactored for Dependency Injection.**
    - `esp32/data`: Configuration files (`config.json`, `led_layout.csv`) are prepared and uploaded.
- **In Progress / Next Steps**:
    - **IMU Implementation**: Needs `test_imu` implementation (I2C init, Sensor check).
    - **LEDController**: Needs FastLED initialization tests.
    - **NetworkManager**: Needs full implementation including MQTT/UDP logic.

**Reference Artifacts**:
- `task.md`: Detailed checklist of remaining tasks.
- `test_plan.md`: **The Source of Truth for specifications.** All tests must verify these criteria.
- `walkthrough.md`: Evidence of passing tests (DeviceManager).

---

## 3. ESP32 Firmware Development

### Prerequisites
- PlatformIO Core (CLI)

### Testing Strategy (TDD)
We use **PlatformIO Native Environment** for logic testing and mocking hardware.

1.  **Edit `test_plan.md`**: Define the spec and acceptance criteria FIRST.
2.  **Create Mocks**: Use `esp32/test/mocks/` to mock hardware dependencies (e.g., `MockIMU.h`, `MockArduino.cpp`).
3.  **Implement Test**: Write tests in `esp32/test/test_xxx/`.
4.  **Run Test**:
    ```bash
    cd esp32
    pio test -e native -f test_device  # Example
    ```
5.  **Implement Code**: Modify `src/` to pass the test.

### Key Commands
Run these from the `esp32/` directory.

#### Build & Upload
```bash
# Build Firmware
pio run -e atoms3r_bno055

# Upload Firmware
pio run -e atoms3r_bno055 -t upload

# Upload Filesystem (LittleFS) - REQUIRED for config.json
pio run -e atoms3r_bno055 -t uploadfs
```

#### Unit Tests (Native)
```bash
# Run All Native Tests
pio test -e native

# Run Specific Test Suite (Recommended)
pio test -e native -f test_device
pio test -e native -f test_config_manager
```

---

## 4. Server Development

### Prerequisites
- Python 3.10+
- Docker (optional, for MQTT broker)

### Commands
Run these from the `server/` directory.

#### Setup & Run
```bash
# Install Dependencies
bash scripts/setup_services.sh

# Run Server (Hot Reload)
uvicorn app.main:app --reload --host 0.0.0.0 --port 8000
```

#### Network Setup
Configures the server as a Wi-Fi Access Point (SSID: `ESP32-P2P-Direct`).
```bash
sudo bash scripts/setup_network.sh
```
