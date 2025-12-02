#include <Arduino.h>
#include "common.h"
#include "FileManager.h"
#include "ConfigManager.h"
#include "NetworkManager.h"
#include "MQTTManager.h"
#include "IMUManager.h"
#include "GestureManager.h"
#include "SoundManager.h"
#include "ImageManager.h"
#include "LEDManager.h"
#include "LCDManager.h"

using namespace sastle;

// グローバルデバッグフラグ (common.hでextern宣言)
bool g_debugEnabled = false;

ConfigManager config;
NetworkManager network;
MQTTManager mqtt;
IMUManager imuSensor;
GestureManager gesture;
SoundManager sound;
ImageManager imageManager;
LEDManager ledManager;
LCDManager lcdManager;

unsigned long lastIMUPublish = 0;
const unsigned long IMU_PUBLISH_INTERVAL = 100; // 100ms = 10Hz
unsigned long lastIMULog = 0;
const unsigned long IMU_LOG_INTERVAL = 3000; // 3秒に1回ログ出力

// MQTTメッセージ受信コールバック
void mqttCallback(char* topic, uint8_t* payload, unsigned int length) {
    DEBUG_PRINTF("[MQTT] Message arrived [%s]: ", topic);
    
    // ペイロードを文字列として表示
    char message[512];  // Increased buffer for state messages
    int len = (length < sizeof(message) - 1) ? length : sizeof(message) - 1;
    memcpy(message, payload, len);
    message[len] = '\0';
    DEBUG_PRINTLN(message);
    
    // トピック別処理
    if (strstr(topic, "sphere/all/state") != NULL) {
        // StateManagerからの状態更新
        Serial.println("\n=== STATE UPDATE ===");
        
        // JSONパース (簡易版 - ArduinoJsonを使う方が良い)
        // brightnessを抽出
        const char* brightnessKey = "\"brightness\":";
        const char* brightnessPtr = strstr(message, brightnessKey);
        if (brightnessPtr) {
            int brightness = atoi(brightnessPtr + strlen(brightnessKey));
            Serial.printf("  Brightness: %d%%\n", brightness);
            
            // LEDManagerにbrightness設定を適用 (0-100% → 0-255)
            uint8_t ledBrightness = map(brightness, 0, 100, 0, 255);
            ledManager.setBrightness(ledBrightness);
            Serial.printf("  LED Brightness set to: %d/255\n", ledBrightness);
        }
        
        // その他のパラメータも同様にパース可能
        Serial.println("==================\n");
        
    } else if (strstr(topic, "/command") != NULL) {
        // コマンド処理
        if (strcmp(message, "status") == 0) {
            mqtt.publish("sphere/sphere001/response", "OK", false);
        } else if (strcmp(message, "restart") == 0) {
            Serial.println("Restart command received!");
            ESP.restart();
        }
    }
}

void setup() {
    // シリアル初期化
    Serial.begin(115200);
    delay(2000);  // シリアルモニタ接続待ち
    
    Serial.println("\n\n=== M5Atom S3R Network Test ===");
    
    // PSRAM初期化確認
    if (psramFound()) {
        Serial.printf("PSRAM found: %d bytes\n", ESP.getPsramSize());
        Serial.printf("Free PSRAM: %d bytes\n", ESP.getFreePsram());
    } else {
        Serial.println("PSRAM not found");
    }
    
    // LittleFS初期化
    Serial.println("\n=== Initializing LittleFS ===");
    if (!FileManager::begin()) {
        Serial.println("FileManager initialization FAILED!");
        while(1) delay(1000);
    }
    
    // ConfigManager初期化とロード
    Serial.println("\n=== Loading Configuration ===");
    if (!config.loadConfig("/config.json")) {
        Serial.println("Failed to load config!");
        while(1) delay(1000);
    }
    
    // 設定内容を表示
    config.printConfig();
    
    // NetworkManager初期化
    if (!network.begin(config)) {
        Serial.println("Network initialization FAILED!");
        while(1) delay(1000);
    }
    
    // ネットワークステータス表示
    network.printStatus();
    
    // UDP開始（configから取得したポートでリッスン）
    uint16_t udpPort = config.getUDPPort();
    if (!network.beginUDP(udpPort)) {
        Serial.println("Failed to start UDP!");
    } else {
        Serial.println("UDP ready for communication");
    }
    
    // MQTT初期化
    mqtt.setCallback(mqttCallback);
    if (!mqtt.begin(config)) {
        Serial.println("MQTT initialization failed (will retry)");
    }
    mqtt.printStatus();
    
    // Sound初期化
    if (!sound.begin(config)) {
        Serial.println("Sound initialization failed (continuing without sound)");
    } else {
        // 起動音を再生
        sound.playEffect(SoundEffect::STARTUP);
    }
    
    // IMU初期化
    if (!imuSensor.begin(config)) {
        Serial.println("IMU initialization failed (continuing without IMU)");
    } else {
        imuSensor.printStatus();
    }
    
    // ジェスチャー初期化 (サウンドフィードバック付き)
    if (imuSensor.isInitialized()) {
        if (!gesture.begin(imuSensor, mqtt, &sound)) {
            Serial.println("Gesture initialization failed");
        }
    } else {
        Serial.println("Gesture disabled (IMU not available)");
    }
    
    // ImageManager初期化
    if (!imageManager.begin(config, network)) {
        Serial.println("ImageManager initialization failed (continuing without image)");
    } else {
        imageManager.printStats();
    }
    
    // LCDManager初期化 (デバッグモード)
    if (!lcdManager.begin(&config)) {
        DEBUG_PRINTLN("[Setup] LCDManager initialization failed");
    }
    
    // LEDManager初期化 (IMUManager連携)
    if (imageManager.isInitialized()) {
        IMUManager* imuPtr = imuSensor.isInitialized() ? &imuSensor : nullptr;
        if (!ledManager.begin(config, imageManager, imuPtr)) {
            Serial.println("LEDManager initialization failed (continuing without LED)");
        } else {
            ledManager.printStatus();
            
            // 起動時LEDテスト: 赤→緑→青
            ledManager.fillSolid(255, 0, 0);
            ledManager.show();
            delay(500);
            ledManager.fillSolid(0, 255, 0);
            ledManager.show();
            delay(500);
            ledManager.fillSolid(0, 0, 255);
            ledManager.show();
            delay(500);
            ledManager.fillSolid(0, 0, 0);
            ledManager.show();
            
            // レンダリングタスク開始 (Core 1)
            if (!ledManager.startRenderTask(1, 2, 8192)) {
                Serial.println("Failed to start LED render task");
            }
        }
    } else {
        Serial.println("LEDManager disabled (ImageManager not available)");
    }
    
    Serial.println("\n=== Setup Complete ===");
}

void loop() {
    // MQTT処理 (keep-alive & メッセージ受信)
    mqtt.loop();
    
    // ImageManager更新 (UDP画像受信・デコード)
    bool newFrameReceived = false;
    if (imageManager.isInitialized()) {
        if (imageManager.update()) {
            newFrameReceived = true;
            // 新フレームデコード完了 → LEDManagerに通知
            // Note: LEDManager内部でセマフォ経由で通知される
        }
    }
    
    // LCD更新 (デバッグモード時のみ)
    if (lcdManager.isDebugEnabled() && newFrameReceived) {
        lcdManager.update(&imageManager);
    }
    
    // IMU更新
    if (imuSensor.isInitialized()) {
        imuSensor.update();
        
        // ジェスチャー検出更新
        gesture.update();
        
        // 定期的にIMUデータをMQTT送信
        unsigned long now = millis();
        if (now - lastIMUPublish >= IMU_PUBLISH_INTERVAL) {
            lastIMUPublish = now;
            
            float w, x, y, z;
            if (imuSensor.getQuaternion(w, x, y, z)) {
                char payload[128];
                snprintf(payload, sizeof(payload), 
                         "{\"w\":%.4f,\"x\":%.4f,\"y\":%.4f,\"z\":%.4f}",
                         w, x, y, z);
                mqtt.publish("sphere/sphere001/imu", payload, false);
                
                // 3秒に1回だけシリアルログ出力
                if (now - lastIMULog >= IMU_LOG_INTERVAL) {
                    lastIMULog = now;
                    Serial.printf("[IMU] Quaternion: w=%.3f x=%.3f y=%.3f z=%.3f\n", w, x, y, z);
                }
            }
        }
    }
    
    // UDP受信チェック
    int packetSize = network.parsePacket();
    if (packetSize) {
        uint8_t buffer[256];
        int len = network.read(buffer, sizeof(buffer) - 1);
        if (len > 0) {
            buffer[len] = 0;
            Serial.printf("[%lu] Received UDP packet from %s:%d\n", 
                         millis(),
                         network.remoteIP().toString().c_str(), 
                         network.remotePort());
            Serial.printf("  Data (%d bytes): %s\n", len, (char*)buffer);
        }
    }
    delay(10);
}

