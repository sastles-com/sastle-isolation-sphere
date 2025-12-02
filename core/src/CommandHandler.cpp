/**
 * @file CommandHandler.cpp
 * @brief MQTT Command Handler Implementation
 */

#include "CommandHandler.h"

namespace sastle {

CommandHandler::CommandHandler()
    : _ledManager(nullptr)
    , _config(nullptr) {
    // デフォルト値設定
    _params.brightness = 80;
    _params.speed = 50;
    _params.hue = 120;
    _params.saturation = 100;
    
    _playback.status = "stopped";
    _playback.position = 0.0f;
    _playback.duration = 0.0f;
    
    _led.mode = "sphere";
}

bool CommandHandler::begin(LEDManager* ledManager, ConfigManager* config) {
    _ledManager = ledManager;
    _config = config;
    
    // 初期brightness適用
    if (_ledManager) {
        uint8_t ledBrightness = map(_params.brightness, 0, 100, 0, 255);
        _ledManager->setBrightness(ledBrightness);
    }
    
    Serial.println("[CommandHandler] Initialized");
    return true;
}

bool CommandHandler::handleMessage(const char* topic, const uint8_t* payload, unsigned int length) {
    // ペイロードを文字列に変換
    char message[512];
    int len = (length < sizeof(message) - 1) ? length : sizeof(message) - 1;
    memcpy(message, payload, len);
    message[len] = '\0';
    
    Serial.printf("[CommandHandler] Topic: %s\n", topic);
    Serial.printf("[CommandHandler] Payload: %s\n", message);
    
    // コマンドタイプを抽出
    String cmdType = _extractCommandType(topic);
    
    if (cmdType == "params") {
        return _handleParams(message);
    } else if (cmdType == "playback") {
        return _handlePlayback(message);
    } else if (cmdType == "led") {
        return _handleLed(message);
    } else if (cmdType == "system") {
        return _handleSystem(message);
    } else {
        Serial.printf("[CommandHandler] Unknown command type: %s\n", cmdType.c_str());
        return false;
    }
}

bool CommandHandler::_handleParams(const char* payload) {
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (error) {
        Serial.printf("[CommandHandler] JSON parse error: %s\n", error.c_str());
        return false;
    }
    
    Serial.println("[CommandHandler] === PARAMS UPDATE ===");
    
    bool updated = false;
    
    // brightness
    if (doc.containsKey("brightness")) {
        _params.brightness = doc["brightness"];
        Serial.printf("  brightness: %d%%\n", _params.brightness);
        
        if (_ledManager) {
            uint8_t ledBrightness = map(_params.brightness, 0, 100, 0, 255);
            _ledManager->setBrightness(ledBrightness);
            Serial.printf("  → LED brightness set to %d/255\n", ledBrightness);
        }
        updated = true;
    }
    
    // speed
    if (doc.containsKey("speed")) {
        _params.speed = doc["speed"];
        Serial.printf("  speed: %d%%\n", _params.speed);
        updated = true;
    }
    
    // hue
    if (doc.containsKey("hue")) {
        _params.hue = doc["hue"];
        Serial.printf("  hue: %d°\n", _params.hue);
        updated = true;
    }
    
    // saturation
    if (doc.containsKey("saturation")) {
        _params.saturation = doc["saturation"];
        Serial.printf("  saturation: %d%%\n", _params.saturation);
        updated = true;
    }
    
    if (updated) {
        Serial.println("[CommandHandler] Params updated successfully");
    }
    
    return updated;
}

bool CommandHandler::_handlePlayback(const char* payload) {
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (error) {
        Serial.printf("[CommandHandler] JSON parse error: %s\n", error.c_str());
        return false;
    }
    
    Serial.println("[CommandHandler] === PLAYBACK CONTROL ===");
    
    if (doc.containsKey("action")) {
        String action = doc["action"].as<String>();
        Serial.printf("  action: %s\n", action.c_str());
        
        if (action == "play") {
            _playback.status = "playing";
            if (doc.containsKey("playlist")) {
                _playback.playlist = doc["playlist"].as<String>();
            }
            if (doc.containsKey("track")) {
                _playback.track = doc["track"].as<String>();
            }
            Serial.println("  → Playback started");
            
        } else if (action == "pause") {
            _playback.status = "paused";
            Serial.println("  → Playback paused");
            
        } else if (action == "stop") {
            _playback.status = "stopped";
            _playback.position = 0.0f;
            Serial.println("  → Playback stopped");
            
        } else if (action == "toggle") {
            if (_playback.status == "playing") {
                _playback.status = "paused";
            } else {
                _playback.status = "playing";
            }
            Serial.printf("  → Playback toggled to: %s\n", _playback.status.c_str());
        }
        
        return true;
    }
    
    return false;
}

bool CommandHandler::_handleLed(const char* payload) {
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (error) {
        Serial.printf("[CommandHandler] JSON parse error: %s\n", error.c_str());
        return false;
    }
    
    Serial.println("[CommandHandler] === LED CONTROL ===");
    
    if (doc.containsKey("mode")) {
        String mode = doc["mode"].as<String>();
        _led.mode = mode;
        Serial.printf("  mode: %s\n", mode.c_str());
        
        if (mode == "off") {
            // LEDを消灯
            if (_ledManager) {
                _ledManager->fillSolid(0, 0, 0);
                _ledManager->show();
                Serial.println("  → LEDs turned off");
            }
        } else if (mode == "pixels") {
            // ピクセル個別制御
            Serial.println("  → Pixel mode (not implemented yet)");
            // TODO: pixels配列を処理
        } else if (mode == "sphere") {
            // 球体モード（デフォルト）
            Serial.println("  → Sphere mode");
        }
        
        return true;
    }
    
    return false;
}

bool CommandHandler::_handleSystem(const char* payload) {
    StaticJsonDocument<128> doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (error) {
        Serial.printf("[CommandHandler] JSON parse error: %s\n", error.c_str());
        return false;
    }
    
    Serial.println("[CommandHandler] === SYSTEM COMMAND ===");
    
    if (doc.containsKey("action")) {
        String action = doc["action"].as<String>();
        Serial.printf("  action: %s\n", action.c_str());
        
        if (action == "restart") {
            Serial.println("  → Restarting in 1 second...");
            delay(1000);
            ESP.restart();
            
        } else if (action == "calibrate") {
            Serial.println("  → IMU calibration (not implemented)");
            // TODO: IMU calibration
            
        } else if (action == "config_reload") {
            Serial.println("  → Config reload (not implemented)");
            // TODO: Config reload
        }
        
        return true;
    }
    
    return false;
}

String CommandHandler::_extractCommandType(const char* topic) {
    // sphere/all/command/params → "params"
    // sphere/all/command/playback → "playback"
    
    String topicStr = String(topic);
    int lastSlash = topicStr.lastIndexOf('/');
    
    if (lastSlash > 0) {
        return topicStr.substring(lastSlash + 1);
    }
    
    return "";
}

bool CommandHandler::getState(char* buffer, size_t bufferSize) {
    StaticJsonDocument<512> doc;
    
    // params
    JsonObject params = doc.createNestedObject("params");
    params["brightness"] = _params.brightness;
    params["speed"] = _params.speed;
    params["hue"] = _params.hue;
    params["saturation"] = _params.saturation;
    
    // playback
    JsonObject playback = doc.createNestedObject("playback");
    playback["status"] = _playback.status;
    playback["playlist"] = _playback.playlist;
    playback["track"] = _playback.track;
    playback["position"] = _playback.position;
    playback["duration"] = _playback.duration;
    
    // led
    JsonObject led = doc.createNestedObject("led");
    led["mode"] = _led.mode;
    
    // system
    JsonObject system = doc.createNestedObject("system");
    system["uptime"] = millis() / 1000;
    system["fps"] = 60; // TODO: 実際のFPS取得
    system["temp"] = 0.0; // TODO: 温度センサー
    system["free_heap"] = ESP.getFreeHeap();
    
    // timestamp
    doc["timestamp"] = ""; // TODO: NTP時刻
    doc["seq"] = 0; // TODO: シーケンス番号
    
    // JSONをバッファにシリアライズ
    size_t len = serializeJson(doc, buffer, bufferSize);
    
    return len > 0;
}

} // namespace sastle
