/**
 * @file CommandHandler.h
 * @brief MQTT Command Handler
 * @author sastle-com
 * @date 2025-12-02
 */

#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include "LEDManager.h"
#include "ConfigManager.h"

namespace sastle {

/**
 * @class CommandHandler
 * @brief MQTTコマンドを処理するハンドラクラス
 * 
 * 設計仕様 (docs/architecture/communication_design.md) に基づき、
 * 以下のトピックを処理します：
 * - sphere/all/command/params
 * - sphere/all/command/playback
 * - sphere/all/command/led
 * - sphere/all/command/system
 */
class CommandHandler {
public:
    CommandHandler();
    
    /**
     * @brief 初期化
     * @param ledManager LEDManager参照
     * @param config ConfigManager参照
     * @return true 初期化成功
     */
    bool begin(LEDManager* ledManager, ConfigManager* config);
    
    /**
     * @brief MQTTメッセージを処理
     * @param topic トピック名
     * @param payload ペイロード (JSON文字列)
     * @param length ペイロード長
     * @return true 処理成功, false 処理失敗
     */
    bool handleMessage(const char* topic, const uint8_t* payload, unsigned int length);
    
    /**
     * @brief 現在の状態を取得（JSON形式）
     * @param buffer 出力バッファ
     * @param bufferSize バッファサイズ
     * @return true 成功, false 失敗
     */
    bool getState(char* buffer, size_t bufferSize);
    
private:
    LEDManager* _ledManager;
    ConfigManager* _config;
    
    // 現在の状態
    struct {
        uint8_t brightness;  // 0-100
        uint8_t speed;       // 0-100
        uint16_t hue;        // 0-360
        uint8_t saturation;  // 0-100
    } _params;
    
    struct {
        String status;       // "stopped", "playing", "paused"
        String playlist;
        String track;
        float position;
        float duration;
    } _playback;
    
    struct {
        String mode;         // "sphere", "pixels", "off"
        // pixels配列は大きいのでここでは保持しない
    } _led;
    
    /**
     * @brief パラメータコマンド処理
     * @param payload JSONペイロード
     * @return true 成功
     */
    bool _handleParams(const char* payload);
    
    /**
     * @brief 再生制御コマンド処理
     * @param payload JSONペイロード
     * @return true 成功
     */
    bool _handlePlayback(const char* payload);
    
    /**
     * @brief LED制御コマンド処理
     * @param payload JSONペイロード
     * @return true 成功
     */
    bool _handleLed(const char* payload);
    
    /**
     * @brief システムコマンド処理
     * @param payload JSONペイロード
     * @return true 成功
     */
    bool _handleSystem(const char* payload);
    
    /**
     * @brief トピックからコマンドタイプを抽出
     * @param topic MQTTトピック
     * @return コマンドタイプ ("params", "playback", "led", "system", "")
     */
    String _extractCommandType(const char* topic);
};

} // namespace sastle
