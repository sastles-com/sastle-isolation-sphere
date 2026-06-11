/**
 * @file led_drive_test_main.cpp
 * @brief M5AtomS3R LED駆動検証ファームウェア (V2ハードウェア構成)
 *
 * Isolation Sphere V2 の確定仕様「5ストリップ × 160 LED = 800 LED」を
 * M5AtomS3R (ESP32-S3) + FastLED で駆動できるかをベンチ検証する。
 * 本体ファームウェア (env:atoms3r) とは独立した env:led_drive_test でビルドする。
 *
 *   pio run -e led_drive_test -t upload
 *
 * 検証項目:
 *   1. 5本のデータ出力が全て動作するか (ESP32-S3 の RMT TXは4ch、5本目の挙動確認)
 *   2. 160 LED/ストリップのチェーン順序・数珠繋ぎが正しいか
 *   3. show() の所要時間と実効FPS (目標: 30fps以上)
 *   4. 輝度制限下での全点灯 (ポゴピン定格 1.5A/pin を考慮した上限運用の確認)
 *
 * 操作: AtomS3R 本体ボタンでテストパターン切替。LCDとシリアルに状態表示。
 *
 * ピン割当はビルドフラグで上書き可能 (デフォルトは core-M5atom-FPC 基板の
 * ネット GPIO01..GPIO05 に対する想定値。基板実配線との照合は要確認):
 *   -D LED_TEST_PIN0=5 ... -D LED_TEST_PIN4=38
 */

#include <Arduino.h>
#include <M5Unified.h>
#include <FastLED.h>

// --- 検証構成 (V2 確定仕様) ---
// core-M5atom-FPC のネット GPIO01..GPIO05 = 5本のLEDデータ線。
// AtomS3R 底面ソケット経由。物理ピン対応の仮説: G5/G6/G7/G8/G38 (G39予備)。
#ifndef LED_TEST_PIN0
#define LED_TEST_PIN0 5
#endif
#ifndef LED_TEST_PIN1
#define LED_TEST_PIN1 6
#endif
#ifndef LED_TEST_PIN2
#define LED_TEST_PIN2 7
#endif
#ifndef LED_TEST_PIN3
#define LED_TEST_PIN3 8
#endif
#ifndef LED_TEST_PIN4
#define LED_TEST_PIN4 38
#endif

#ifndef LED_TEST_LEDS_PER_STRIP
#define LED_TEST_LEDS_PER_STRIP 160
#endif

constexpr uint8_t kNumStrips = 5;
constexpr uint16_t kLedsPerStrip = LED_TEST_LEDS_PER_STRIP;
constexpr uint16_t kTotalLeds = kNumStrips * kLedsPerStrip;

// WS2812C-2020 は WS2812B と同タイミングで駆動可能
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB

// ポゴピン 1.5A/pin (5V-GND 各2本/カセット) を考慮した安全上限。
// ベンチ電源でも 800 LED 全白@255 は ~48A になるため絶対に上げないこと。
constexpr uint8_t kMaxBrightness = 64;  // 25%

static CRGB leds[kTotalLeds];
static const uint8_t kStripPins[kNumStrips] = {
    LED_TEST_PIN0, LED_TEST_PIN1, LED_TEST_PIN2, LED_TEST_PIN3, LED_TEST_PIN4};

enum TestMode : uint8_t {
    MODE_STRIP_ID = 0,   // ストリップ識別色 (0=R 1=G 2=B 3=Y 4=M)
    MODE_CHASE,          // 各ストリップを白1ピクセルが走る (チェーン順序確認)
    MODE_RAINBOW,        // 全体レインボースクロール (描画なめらかさ)
    MODE_WHITE_LIMITED,  // 輝度制限つき全白 (電流測定用)
    MODE_FPS_BENCH,      // 最速 show() ループ (FPS計測)
    MODE_COUNT
};

static uint8_t mode = MODE_STRIP_ID;
static uint32_t frameCount = 0;
static uint32_t lastReportMs = 0;
static uint32_t showTimeAccumUs = 0;
static const char* kModeNames[] = {"STRIP_ID", "CHASE", "RAINBOW", "WHITE_64", "FPS_BENCH"};

static CRGB stripIdColor(uint8_t strip) {
    switch (strip) {
        case 0: return CRGB::Red;
        case 1: return CRGB::Green;
        case 2: return CRGB::Blue;
        case 3: return CRGB::Yellow;
        default: return CRGB::Magenta;
    }
}

static void renderFrame(uint32_t frame) {
    switch (mode) {
        case MODE_STRIP_ID:
            for (uint8_t s = 0; s < kNumStrips; s++) {
                fill_solid(leds + s * kLedsPerStrip, kLedsPerStrip, stripIdColor(s));
            }
            break;
        case MODE_CHASE: {
            fill_solid(leds, kTotalLeds, CRGB::Black);
            uint16_t pos = frame % kLedsPerStrip;
            for (uint8_t s = 0; s < kNumStrips; s++) {
                leds[s * kLedsPerStrip + pos] = CRGB::White;
                // 先頭LEDをストリップ識別色で常時点灯 (DIN側の目印)
                if (pos != 0) leds[s * kLedsPerStrip] = stripIdColor(s);
            }
            break;
        }
        case MODE_RAINBOW:
            for (uint8_t s = 0; s < kNumStrips; s++) {
                fill_rainbow(leds + s * kLedsPerStrip, kLedsPerStrip,
                             (frame + s * 51) & 0xFF, 256 / kLedsPerStrip);
            }
            break;
        case MODE_WHITE_LIMITED:
            fill_solid(leds, kTotalLeds, CRGB::White);
            break;
        case MODE_FPS_BENCH:
            // 最小限の描画変化 (全黒 + 1ピクセル) で show() 性能のみ測る
            fill_solid(leds, kTotalLeds, CRGB::Black);
            leds[frame % kTotalLeds] = CRGB::White;
            break;
    }
}

static void report() {
    uint32_t now = millis();
    uint32_t elapsed = now - lastReportMs;
    if (elapsed < 1000) return;

    float fps = frameCount * 1000.0f / elapsed;
    float avgShowMs = frameCount ? (showTimeAccumUs / 1000.0f / frameCount) : 0;

    Serial.printf("[LED-TEST] mode=%s fps=%.1f show_avg=%.2fms strips=%d x %d leds (pins %d,%d,%d,%d,%d)\n",
                  kModeNames[mode], fps, avgShowMs, kNumStrips, kLedsPerStrip,
                  kStripPins[0], kStripPins[1], kStripPins[2], kStripPins[3], kStripPins[4]);

    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setCursor(0, 4);
    M5.Display.printf("%s\n", kModeNames[mode]);
    M5.Display.printf("FPS : %.1f\n", fps);
    M5.Display.printf("show: %.2fms\n", avgShowMs);
    M5.Display.printf("5x%d LEDs\n", kLedsPerStrip);
    M5.Display.printf("pins %d %d %d\n     %d %d", kStripPins[0], kStripPins[1],
                      kStripPins[2], kStripPins[3], kStripPins[4]);

    frameCount = 0;
    showTimeAccumUs = 0;
    lastReportMs = now;
}

void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);
    Serial.begin(115200);
    delay(1000);

    Serial.println("=== Isolation Sphere V2 LED drive test (M5AtomS3R) ===");
    Serial.printf("strips=%d leds/strip=%d total=%d brightness_cap=%d/255\n",
                  kNumStrips, kLedsPerStrip, kTotalLeds, kMaxBrightness);

    // FastLED のピンはテンプレート引数のため個別に列挙する
    FastLED.addLeds<LED_TYPE, LED_TEST_PIN0, COLOR_ORDER>(leds + 0 * kLedsPerStrip, kLedsPerStrip);
    FastLED.addLeds<LED_TYPE, LED_TEST_PIN1, COLOR_ORDER>(leds + 1 * kLedsPerStrip, kLedsPerStrip);
    FastLED.addLeds<LED_TYPE, LED_TEST_PIN2, COLOR_ORDER>(leds + 2 * kLedsPerStrip, kLedsPerStrip);
    FastLED.addLeds<LED_TYPE, LED_TEST_PIN3, COLOR_ORDER>(leds + 3 * kLedsPerStrip, kLedsPerStrip);
    FastLED.addLeds<LED_TYPE, LED_TEST_PIN4, COLOR_ORDER>(leds + 4 * kLedsPerStrip, kLedsPerStrip);
    FastLED.setBrightness(kMaxBrightness);
    FastLED.clear(true);

    lastReportMs = millis();
}

void loop() {
    M5.update();
    if (M5.BtnA.wasPressed()) {
        mode = (mode + 1) % MODE_COUNT;
        frameCount = 0;
        showTimeAccumUs = 0;
        lastReportMs = millis();
        Serial.printf("[LED-TEST] mode -> %s\n", kModeNames[mode]);
    }

    static uint32_t frame = 0;
    renderFrame(frame++);

    uint32_t t0 = micros();
    FastLED.show();
    showTimeAccumUs += micros() - t0;
    frameCount++;

    report();

    // ベンチモード以外は 30fps 相当に間引く (本番ファームの目標レート)
    if (mode != MODE_FPS_BENCH) {
        delay(33);
    }
}
