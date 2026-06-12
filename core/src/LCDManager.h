/**
 * @file LCDManager.h
 * @brief LCD表示管理クラス
 * 
 * M5AtomS3の内蔵LCD(128x128)に受信画像を表示する
 * ConfigManagerのLCDデバッグフラグで表示ON/OFFを制御
 */

#ifndef LCDMANAGER_H
#define LCDMANAGER_H

#include <Arduino.h>
#include "BoardConfig.h"
#if BOARD_HAS_LCD
#include <M5Unified.h>
#else
// LCD非搭載ボード: 関数シグネチャ維持のための色定数フォールバック (RGB565)
#ifndef TFT_WHITE
#define TFT_WHITE 0xFFFF
#endif
#ifndef TFT_BLACK
#define TFT_BLACK 0x0000
#endif
#ifndef TFT_GREEN
#define TFT_GREEN 0x07E0
#endif
#endif
#include "ConfigManager.h"
#include "ImageManager.h"

namespace sastle {

/**
 * @class LCDManager
 * @brief LCD表示管理
 * 
 * sphere.features.LCD.debugフラグでデバッグ表示を制御
 * ON時は受信した画像をLCDに表示
 */
class LCDManager {
public:
    /**
     * @brief コンストラクタ
     */
    LCDManager();
    
    /**
     * @brief デストラクタ
     */
    ~LCDManager();
    
    /**
     * @brief LCD初期化
     * @param config ConfigManagerインスタンス
     * @return true 初期化成功, false 失敗
     */
    bool begin(ConfigManager* config);
    
    /**
     * @brief LCD表示更新
     * @param imageManager 画像データソース
     * 
     * getLCDDebugEnabled()がtrueの場合のみ描画を実行
     */
    void update(ImageManager* imageManager);
    
    /**
     * @brief デバッグテキスト表示
     * @param text 表示するテキスト
     * @param x X座標
     * @param y Y座標
     * @param textSize テキストサイズ (1-7)
     * @param color 色 (RGB565)
     */
    void drawText(const char* text, int16_t x, int16_t y, uint8_t textSize = 2, uint16_t color = TFT_WHITE);
    
    /**
     * @brief 画面クリア
     * @param color 背景色 (RGB565)
     */
    void clear(uint16_t color = TFT_BLACK);
    
    /**
     * @brief デバッグ表示が有効かチェック
     * @return true 有効, false 無効
     */
    bool isDebugEnabled() const { return _debugEnabled; }

private:
    ConfigManager* _config;      ///< ConfigManager参照
    bool _debugEnabled;          ///< デバッグ表示フラグ
    bool _initialized;           ///< 初期化完了フラグ
    
    uint16_t _lcdWidth;          ///< LCD幅
    uint16_t _lcdHeight;         ///< LCD高さ
    uint8_t _rotation;           ///< LCD回転角度 (0/1/2/3)
};

} // namespace sastle

#endif // LCDMANAGER_H
