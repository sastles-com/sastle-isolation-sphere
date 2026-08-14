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
 *
 * NOTE: M5Stack公式ドキュメントにより AtomS3R 底面拡張ピンは G5/G6/G7/G8/G38/G39 の
 * 6本のみと確認。
 *
 * NOTE (2026-08-15): core-M5atom-FPC の ESP32-01 コネクタ (J17) が
 * GPIO008,007,006,005,038 の降順で配線されており、mother-ring 側 (J9: GPIO01..05
 * 昇順、LINE01..05へ直結) と逆順に嵌合する。実機の LINE01〜05 (シルク印刷) を
 * MODE_STRIP_ID で色確認した結果、LINE01=G8, LINE02=G7, LINE03=G6, LINE04=G5,
 * LINE05=G38 と確定 (5本目は反転の影響なし)。下記 LED_TEST_PIN0..3 はこの実態に
 * 合わせて反転済み — 素直な G5/G6/G7/G8 の昇順ではない点に注意。
 */

#include <Arduino.h>
#include <M5Unified.h>
#include <FastLED.h>

// --- 検証構成 (V2 確定仕様) ---
// core-M5atom-FPC のネット GPIO01..GPIO05 = 5本のLEDデータ線。
// AtomS3R 底面ソケット経由。物理ピン対応の仮説: G5/G6/G7/G8/G38 (G39予備=LS1スピーカー)。
#ifndef LED_TEST_PIN0
#define LED_TEST_PIN0 8    // LINE01 (実配線: G8)
#endif
#ifndef LED_TEST_PIN1
#define LED_TEST_PIN1 7    // LINE02 (実配線: G7)
#endif
#ifndef LED_TEST_PIN2
#define LED_TEST_PIN2 6    // LINE03 (実配線: G6)
#endif
#ifndef LED_TEST_PIN3
#define LED_TEST_PIN3 5    // LINE04 (実配線: G5)
#endif
#ifndef LED_TEST_PIN4
#define LED_TEST_PIN4 38   // LINE05 (実配線: G38)
#endif

#ifndef LED_TEST_LEDS_PER_STRIP
#define LED_TEST_LEDS_PER_STRIP 160
#endif

// MODE_BLOCK_CHASE で走らせるブロックの幅 (連続点灯するLED数)
#ifndef LED_TEST_BLOCK_WIDTH
#define LED_TEST_BLOCK_WIDTH 5
#endif

constexpr uint8_t kNumStrips = 5;
constexpr uint16_t kLedsPerStrip = LED_TEST_LEDS_PER_STRIP;
constexpr uint16_t kTotalLeds = kNumStrips * kLedsPerStrip;
constexpr uint16_t kBlockWidth = LED_TEST_BLOCK_WIDTH;

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
    MODE_BLOCK_CHASE,    // 各ストリップを識別色の5連ピクセルが走る (色分け+チェーン順序)
    MODE_RAINBOW,        // 全体レインボースクロール (描画なめらかさ)
    MODE_WHITE_LIMITED,  // 輝度制限つき全白 (電流測定用)
    MODE_FPS_BENCH,      // 最速 show() ループ (FPS計測)
    MODE_COUNT
};

static uint8_t mode = MODE_STRIP_ID;
static uint32_t frameCount = 0;
static uint32_t lastReportMs = 0;
static uint32_t showTimeAccumUs = 0;
// TestMode と同じ並びを保つこと (report() が mode で添字する)
static const char* kModeNames[] = {"STRIP_ID", "CHASE", "BLOCK_CHASE",
                                   "RAINBOW", "WHITE_64", "FPS_BENCH"};
static_assert(sizeof(kModeNames) / sizeof(kModeNames[0]) == MODE_COUNT,
              "kModeNames と TestMode の要素数が一致していない");

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
        case MODE_BLOCK_CHASE: {
            // 各ストリップを、そのストリップの識別色で塗った kBlockWidth 連ピクセルの
            // ブロックが DIN 側から順に走る。ストリップ間で色が違うので、どの線が
            // どのカセットに繋がっているかと、チェーン順序を同時に確認できる。
            //
            //   ●●●●●○○○○  →  ○●●●●●○○○  →  ○○●●●●●○○  → ...
            //
            // 末尾に達したブロックは折り返して先頭から出る (ストリップ境界は跨がない)。
            fill_solid(leds, kTotalLeds, CRGB::Black);
            const uint16_t head = frame % kLedsPerStrip;
            for (uint8_t s = 0; s < kNumStrips; s++) {
                const CRGB color = stripIdColor(s);
                CRGB* strip = leds + s * kLedsPerStrip;
                for (uint16_t i = 0; i < kBlockWidth; i++) {
                    strip[(head + i) % kLedsPerStrip] = color;
                }
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
