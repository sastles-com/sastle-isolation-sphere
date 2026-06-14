/**
 * @file LCDManager.cpp
 * @brief LCD表示管理クラス実装
 */

#include "LCDManager.h"
#include "common.h"

namespace sastle {

LCDManager::LCDManager() 
    : _config(nullptr)
    , _debugEnabled(false)
    , _initialized(false)
    , _lcdWidth(128)
    , _lcdHeight(128)
    , _rotation(0)
{
}

LCDManager::~LCDManager() {
}

bool LCDManager::begin(ConfigManager* config) {
    if (!config) {
        DEBUG_PRINTLN("[LCDManager] Error: ConfigManager is null");
        return false;
    }
    
    _config = config;
    _debugEnabled = _config->getLCDDebugEnabled();

#if !BOARD_HAS_LCD
    // LCD非搭載ボード: 常に無効・no-op (update() 等は呼ばれない)
    _debugEnabled = false;
    DEBUG_PRINTLN("[LCDManager] No LCD on this board (no-op)");
    return true;
#else
    if (!_debugEnabled) {
        DEBUG_PRINTLN("[LCDManager] LCD debug disabled");
        return true;  // デバッグ無効時も成功扱い
    }

    // M5Unified LCD初期化
    auto cfg = M5.config();
    M5.begin(cfg);
    
    _lcdWidth = M5.Display.width();
    _lcdHeight = M5.Display.height();
    _rotation = 0;  // config.jsonから取得可能

    // 一括転送(pushImage)用バッファ。1画素ずつ writePixel すると 128x128 で
    // ~150ms/フレームかかり描画FPSの律速になっていたため、全画素を組んで
    // 1回の pushImage で転送する。
    _lcdBuf = (uint16_t*)malloc((size_t)_lcdWidth * _lcdHeight * sizeof(uint16_t));

    M5.Display.setRotation(_rotation);
    M5.Display.setBrightness(128);  // 初期輝度50%
    M5.Display.fillScreen(TFT_BLACK);
    
    _initialized = true;
    
    DEBUG_PRINTF("[LCDManager] Initialized: %dx%d, rotation=%d, debug=%s\n",
                 _lcdWidth, _lcdHeight, _rotation, 
                 _debugEnabled ? "ON" : "OFF");
    
    // 起動メッセージ表示
    drawText("LCD Ready", 10, 50, 2, TFT_GREEN);
    delay(1000);
    clear();

    return true;
#endif // BOARD_HAS_LCD
}

void LCDManager::update(ImageManager* imageManager) {
    if (!_initialized || !_debugEnabled || !imageManager || !_lcdBuf) {
        return;
    }

#if BOARD_HAS_LCD
    // スロットル: デバッグ表示は最大 ~10Hz で十分。毎フレーム描くと描画FPSを縛る。
    unsigned long now = millis();
    if (now - _lastUpdateMs < 100) {
        return;
    }
    _lastUpdateMs = now;

    uint16_t imgWidth = imageManager->getWidth();
    uint16_t imgHeight = imageManager->getHeight();

    // 全画素をバッファに組み立て (RAM上、高速) → 1回の pushImage で一括転送。
    // 旧実装は writePixel を 128x128=16384回呼び ~150ms かかっていた。
    for (uint16_t y = 0; y < _lcdHeight; y++) {
        uint16_t srcY = (y * imgHeight) / _lcdHeight;
        uint16_t* row = _lcdBuf + (size_t)y * _lcdWidth;
        for (uint16_t x = 0; x < _lcdWidth; x++) {
            uint16_t srcX = (x * imgWidth) / _lcdWidth;
            uint8_t r, g, b;
            imageManager->getPixel(srcX, srcY, r, g, b);
            row[x] = M5.Display.color565(r, g, b);
        }
    }
    M5.Display.pushImage(0, 0, _lcdWidth, _lcdHeight, _lcdBuf);
#endif // BOARD_HAS_LCD
}

void LCDManager::drawText(const char* text, int16_t x, int16_t y, uint8_t textSize, uint16_t color) {
    if (!_initialized || !_debugEnabled) {
        return;
    }
#if BOARD_HAS_LCD
    M5.Display.setTextSize(textSize);
    M5.Display.setTextColor(color);
    M5.Display.setCursor(x, y);
    M5.Display.print(text);
#endif
}

void LCDManager::clear(uint16_t color) {
    if (!_initialized || !_debugEnabled) {
        return;
    }
#if BOARD_HAS_LCD
    M5.Display.fillScreen(color);
#endif
}

} // namespace sastle
