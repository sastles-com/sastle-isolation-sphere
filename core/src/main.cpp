#include <Arduino.h>
#include "common.h"
#include "FileManager.h"
#include "ConfigManager.h"
#include "NetworkManager.h"
#include "MQTTManager.h"
#include "IMUManager.h"

using namespace sastle;

ConfigManager config;
NetworkManager network;
MQTTManager mqtt;
IMUManager imuSensor;

unsigned long lastIMUPublish = 0;
const unsigned long IMU_PUBLISH_INTERVAL = 100; // 100ms = 10Hz

// MQTTメッセージ受信コールバック
void mqttCallback(char* topic, uint8_t* payload, unsigned int length) {
    Serial.printf("[MQTT] Message arrived [%s]: ", topic);
    
    // ペイロードを文字列として表示
    char message[256];
    int len = (length < sizeof(message) - 1) ? length : sizeof(message) - 1;
    memcpy(message, payload, len);
    message[len] = '\0';
    Serial.println(message);
    
    // トピック別処理の例
    if (strstr(topic, "/command") != NULL) {
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
    
    // IMU初期化
    if (!imuSensor.begin(config)) {
        Serial.println("IMU initialization failed (continuing without IMU)");
    } else {
        imuSensor.printStatus();
    }
    
    Serial.println("\n=== Setup Complete ===");
}

void loop() {
    // MQTT処理 (keep-alive & メッセージ受信)
    mqtt.loop();
    
    // IMU更新
    if (imuSensor.isInitialized()) {
        imuSensor.update();
        
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

