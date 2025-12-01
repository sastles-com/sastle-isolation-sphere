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
}

void LCDManager::update(ImageManager* imageManager) {
    if (!_initialized || !_debugEnabled || !imageManager) {
        return;
    }
    
    uint16_t imgWidth = imageManager->getWidth();
    uint16_t imgHeight = imageManager->getHeight();
    
    // 画像データをLCDに描画
    // ImageManagerのRGB565データを直接転送
    // スケーリング: 320x160 → 128x128 (アスペクト比無視で全画面表示)
    
    M5.Display.startWrite();
    
    for (uint16_t y = 0; y < _lcdHeight; y++) {
        for (uint16_t x = 0; x < _lcdWidth; x++) {
            // LCDの座標を画像座標にマッピング
            uint16_t srcX = (x * imgWidth) / _lcdWidth;
            uint16_t srcY = (y * imgHeight) / _lcdHeight;
            
            // RGB565ピクセル取得
            uint8_t r, g, b;
            imageManager->getPixel(srcX, srcY, r, g, b);
            
            // RGB888 → RGB565変換
            uint16_t color = M5.Display.color565(r, g, b);
            M5.Display.writePixel(x, y, color);
        }
    }
    
    M5.Display.endWrite();
}

void LCDManager::drawText(const char* text, int16_t x, int16_t y, uint8_t textSize, uint16_t color) {
    if (!_initialized || !_debugEnabled) {
        return;
    }
    
    M5.Display.setTextSize(textSize);
    M5.Display.setTextColor(color);
    M5.Display.setCursor(x, y);
    M5.Display.print(text);
}

void LCDManager::clear(uint16_t color) {
    if (!_initialized || !_debugEnabled) {
        return;
    }
    
    M5.Display.fillScreen(color);
}

} // namespace sastle
