/**
 * @file CommandHandler.cpp
 * @brief MQTT Command Handler Implementation
 */

#include "CommandHandler.h"
#include "RemoteLog.h"
#include "MQTTManager.h"   // kMqttBufferSize
#include "SoundManager.h"

// main.cpp のグローバルインスタンス。ブザー配線ピンの実機診断 (action:"sound_pin_test")
// のためだけに直接参照する。
extern sastle::SoundManager sound;

namespace sastle {

// led コマンド用 JSON ドキュメント容量 (ヒープ確保)。
// pixels 1要素 {"index":799,"r":255,"g":255,"b":255} は JSON 文字列で約39B に対し、
// ArduinoJson v6 のメモリプールでは JSON_OBJECT_SIZE(4)=約72B を占める。受信上限
// (kMqttBufferSize) 全量が pixels のとき約52要素 → 約4.5KB 必要になるため、
// 溢れない容量を確保する。足りないと DeserializationError::NoMemory で丸ごと捨てる。
constexpr size_t kLedDocCapacity = kMqttBufferSize * 4;

// mode:"test" 突入時に強制する低輝度 (0-255)。点灯/配線確認が目的で眩しさ・電流を
// 抑えるため暗めに固定する。sphere/pixels へ戻す際は params の brightness を再適用する。
constexpr uint8_t kTestBrightness = 24;  // 約10%

// JSONペイロードの deserialize + エラーログの共通処理
// ドキュメントサイズ/確保先はコマンドごとに異なる (led は pixels 配列があるため
// ヒープ確保の DynamicJsonDocument) ので、基底の JsonDocument& で受ける
static bool parseJsonPayload(const char* payload, JsonDocument& doc) {
    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
        // MQTT へも出す: パース失敗はリモートから見えないと切り分けできない
        Log.printf("[CommandHandler] JSON parse error: %s\n", error.c_str());
        return false;
    }
    return true;
}

CommandHandler::CommandHandler()
    : _ledManager(nullptr)
    , _config(nullptr) {
    // デフォルト値設定
    _params.brightness = 50;
    _params.speed = 50;
    _params.hue = 120;
    _params.saturation = 100;
    
    _playback.status = "stopped";
    _playback.position = 0.0f;
    _playback.duration = 0.0f;
    
    _led.mode = "sphere";
    _led.axisIndicator = false;
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
    // ペイロードを文字列に変換。受信バッファと同容量にしないと led コマンドの
    // pixels 配列が途中で切れ、JSON パースが丸ごと失敗する。
    // 2KB をスタックに積むと呼び出し元 (loop タスク) を圧迫するため static。
    // 呼び出しは mqttCallback 経由の loop タスクのみなので再入はしない。
    static char message[kMqttBufferSize];
    int len = (length < sizeof(message) - 1) ? length : sizeof(message) - 1;
    memcpy(message, payload, len);
    message[len] = '\0';
    
    Serial.printf("[CommandHandler] Topic: %s\n", topic);
    Serial.printf("[CommandHandler] Payload: %s\n", message);
    
    // コマンドタイプを抽出
    String cmdType = _extractCommandType(topic);
    Log.printf("[CmdDbg] topic=%s cmdType=%s\n", topic, cmdType.c_str());

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
    if (!parseJsonPayload(payload, doc)) {
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
    if (!parseJsonPayload(payload, doc)) {
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

uint16_t CommandHandler::_applyPixels(JsonArrayConst pixels) {
    if (!_ledManager) {
        return 0;
    }

    uint16_t applied = 0;
    uint16_t rejected = 0;

    for (JsonObjectConst px : pixels) {
        // index 必須。r/g/b は省略時 0 (= 消灯) として扱う
        if (!px.containsKey("index")) {
            ++rejected;
            continue;
        }

        const int index = px["index"].as<int>();
        if (index < 0 || index > UINT16_MAX) {
            ++rejected;
            continue;
        }

        const uint8_t r = px["r"] | 0;
        const uint8_t g = px["g"] | 0;
        const uint8_t b = px["b"] | 0;

        if (_ledManager->setPixel((uint16_t)index, r, g, b)) {
            ++applied;
        } else {
            ++rejected;  // LED数を超える index
        }
    }

    // 出力は Core1 の描画タスクが行う。ここから FastLED を呼ぶと RMT ドライバを
    // 描画タスクと同時に叩いて競合するため、タスクが止まっている場合のみ直接出す。
    if (applied > 0 && !_ledManager->isRunning()) {
        _ledManager->show();
    }
    if (rejected > 0) {
        Log.printf("  → %u pixel(s) rejected (missing/out-of-range index)\n", rejected);
    }

    return applied;
}

bool CommandHandler::_handleLed(const char* payload) {
    // pixels 配列を載せるため、他コマンドより大きめのドキュメントをヒープに確保する。
    // 実際の上限は MQTT 受信バッファ (MQTTManager::begin の setBufferSize) 側で決まる。
    DynamicJsonDocument doc(kLedDocCapacity);
    if (!parseJsonPayload(payload, doc)) {
        return false;
    }

    Log.println("[CommandHandler] === LED CONTROL ===");

    bool handled = false;

    if (doc.containsKey("mode")) {
        String mode = doc["mode"].as<String>();
        _led.mode = mode;
        Log.printf("  mode: %s\n", mode.c_str());

        if (mode == "off") {
            // 消灯。Manual に切り替えないと次フレームの updateLEDBuffer() で塗り戻される。
            if (_ledManager) {
                _ledManager->setOutputMode(LEDManager::OutputMode::Manual);
                _ledManager->fillSolid(0, 0, 0);
                if (!_ledManager->isRunning()) {
                    _ledManager->show();
                }
                Log.println("  → LEDs turned off");
            }
        } else if (mode == "pixels") {
            // ピクセル個別制御。updateLEDBuffer() による毎フレーム上書きを止める。
            if (_ledManager) {
                _restoreBrightnessFromParams();
                _ledManager->setOutputMode(LEDManager::OutputMode::Manual);
                Log.println("  → Pixel mode");
            }
        } else if (mode == "test") {
            // 点灯/配線確認モード。IMU/画像を通さず strip index から自前描画する。
            // 眩しさ・電流対策で低輝度を強制する。
            if (_ledManager) {
                String pattern = doc["pattern"] | "chase";
                uint8_t width = (uint8_t)(doc["width"] | 5);
                LEDManager::TestPattern tp = (pattern == "strip_id")
                                                 ? LEDManager::TestPattern::StripId
                                                 : LEDManager::TestPattern::Chase;
                _ledManager->setTestPattern(tp, width);
                _ledManager->setBrightness(kTestBrightness);
                _ledManager->setOutputMode(LEDManager::OutputMode::Test);
                Log.printf("  → Test mode (pattern=%s width=%u)\n", pattern.c_str(), width);
            }
        } else if (mode == "sphere") {
            // 球体モード（デフォルト）。画像+IMU からのマッピングを再開する。
            if (_ledManager) {
                _restoreBrightnessFromParams();
                _ledManager->setOutputMode(LEDManager::OutputMode::Sphere);
            }
            Log.println("  → Sphere mode");
        }
        handled = true;
    }

    // pixels 配列は mode と同一メッセージでも、pixels モード中の逐次更新でも受け付ける
    if (doc.containsKey("pixels") && _led.mode == "pixels") {
        JsonArrayConst pixels = doc["pixels"].as<JsonArrayConst>();
        if (pixels.isNull()) {
            Log.println("  → pixels is not an array, ignored");
        } else {
            const uint16_t applied = _applyPixels(pixels);
            Log.printf("  → %u pixel(s) applied\n", applied);
            handled = true;
        }
    } else if (doc.containsKey("pixels")) {
        Log.printf("  → pixels ignored (mode is \"%s\", not \"pixels\")\n", _led.mode.c_str());
    }

    // XYZ軸インジケータ (mode とは独立に切替可能)
    if (doc.containsKey("axis")) {
        bool on = doc["axis"].as<bool>();
        _led.axisIndicator = on;
        if (_ledManager) {
            _ledManager->setAxisIndicator(on);
        }
        Serial.printf("  → Axis indicator: %s\n", on ? "ON" : "OFF");
        handled = true;
    }

    return handled;
}

void CommandHandler::_restoreBrightnessFromParams() {
    // test モードで下げた輝度を params の値へ戻す (_handleParams と同じ換算)。
    if (_ledManager) {
        uint8_t ledBrightness = map(_params.brightness, 0, 100, 0, 255);
        _ledManager->setBrightness(ledBrightness);
    }
}

bool CommandHandler::_handleSystem(const char* payload) {
    StaticJsonDocument<128> doc;
    if (!parseJsonPayload(payload, doc)) {
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
            
        } else if (action == "sound_pin_test") {
            // ブザー配線ピンの実機診断: 底面露出6ピンを順に鳴らす。
            // N番目のピンはビープを (N+1) 回鳴らすので、実際に音が出た回数を
            // 数えればどのGPIOが本物のスピーカー配線か特定できる。
            static const uint8_t kCandidatePins[] = {5, 6, 7, 8, 38, 39};
            constexpr size_t kNumCandidates = sizeof(kCandidatePins) / sizeof(kCandidatePins[0]);
            Log.println("[SoundTest] Pin scan start: 1st pin=1 beep, 2nd=2 beeps, ... (GPIO 5,6,7,8,38,39)");
            for (size_t i = 0; i < kNumCandidates && _config; i++) {
                uint8_t pin = kCandidatePins[i];
                Log.printf("[SoundTest] GPIO%u -> %u beep(s)\n", pin, (unsigned)(i + 1));
                sound.end();
                sound.begin(*_config, pin);
                for (size_t b = 0; b <= i; b++) {
                    sound.playEffect(SoundEffect::BEEP);
                    delay(150);
                }
                delay(700);
            }
            // デフォルトのブザーピンに戻す
            sound.end();
            if (_config) {
                sound.begin(*_config);
            }
            Log.println("[SoundTest] Done. Restored default buzzer pin.");

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
    led["axis"] = _led.axisIndicator;
    
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
