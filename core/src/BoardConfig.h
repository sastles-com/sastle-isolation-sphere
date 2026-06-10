#ifndef __BOARD_CONFIG_H__
#define __BOARD_CONFIG_H__

#include <stdint.h>

// ハードウェア構成定数の集約ヘッダー。
// 値はすべて従来ソース内に散在していたハードコード値と同一。
// 変更する場合は配線・仕様書(core/doc/)との整合を確認すること。

namespace sastle {

// --- LED ---
// FastLED.addLeds<> のピンはテンプレート引数のため実行時値にできない。
// 個別の constexpr として定義し、テンプレート引数と _stripPins[] の両方で参照する。
constexpr uint8_t kLedPin0 = 5;   // Strip 0 -> GPIO 5
constexpr uint8_t kLedPin1 = 6;   // Strip 1 -> GPIO 6
constexpr uint8_t kLedPin2 = 7;   // Strip 2 -> GPIO 7
constexpr uint8_t kLedPin3 = 8;   // Strip 3 -> GPIO 8
constexpr uint8_t kNumStrips = 4;
constexpr uint16_t kMaxLeds = 800;
constexpr uint8_t kTargetFps = 30;
constexpr uint32_t kFrameDelayMs = 1000 / kTargetFps;
constexpr uint8_t kLedDefaultBrightness = 128;  // デフォルト輝度 50%

// --- IMU (BNO055) ---
constexpr uint8_t kImuI2cSda = 2;
constexpr uint8_t kImuI2cScl = 1;

// --- WiFi ---
constexpr int kWifiConnectRetries = 40;     // 40回 × 250ms = 最大10秒
constexpr uint32_t kWifiRetryDelayMs = 250;

} // namespace sastle

#endif /* __BOARD_CONFIG_H__ */
