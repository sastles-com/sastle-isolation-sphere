#ifndef BOARD_ATOMS3R_H
#define BOARD_ATOMS3R_H

// ============================================================
//  M5AtomS3R (ESP32-S3) ボード定義
//  デバイス個別の物理ハード設定 (ピン・有無) をここに集約する。
//  変更時は配線図 (FPC-isolation-sphere/kiban/core-M5atom-FPC) と
//  core/doc/ の仕様書との整合を確認すること。
// ============================================================

#include <stdint.h>

// --- ボード機能フラグ (プリプロセッサ: M5固有コードの有効/無効に使用) ---
#define BOARD_NAME       "M5AtomS3R"
#define BOARD_HAS_LCD    1   // 内蔵 128x128 LCD (M5Unified 経由)
#define BOARD_HAS_BUZZER 1   // 圧電スピーカー LS1 (G39)

namespace sastle {

// --- LED ---
// FastLED の addLeds<> はピンがテンプレート引数のため、実行時値にできない。
// (config.json には出せず、コンパイル時定数として持つ必要がある)
constexpr uint8_t  kLedPin0 = 5;   // Strip 0 -> GPIO 5
constexpr uint8_t  kLedPin1 = 6;   // Strip 1 -> GPIO 6
constexpr uint8_t  kLedPin2 = 7;   // Strip 2 -> GPIO 7
constexpr uint8_t  kLedPin3 = 8;   // Strip 3 -> GPIO 8
constexpr uint8_t  kNumStrips = 4;
constexpr uint16_t kMaxLeds = 800;
constexpr uint8_t  kTargetFps = 30;
constexpr uint32_t kFrameDelayMs = 1000 / kTargetFps;
constexpr uint8_t  kLedDefaultBrightness = 128;  // デフォルト輝度 50%

// --- IMU (BNO055) I2C ---
constexpr uint8_t kImuI2cSda = 2;  // Grove SDA
constexpr uint8_t kImuI2cScl = 1;  // Grove SCL

// --- ブザー ---
constexpr uint8_t kBuzzerGpio = 39;  // G39 -> LS1 SPEAKER

} // namespace sastle

#endif /* BOARD_ATOMS3R_H */
