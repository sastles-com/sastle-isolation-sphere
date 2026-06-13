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
#include "CommandHandler.h"
#include "MqttTopics.h"
#include "RemoteLog.h"
#include "OtaManager.h"

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
CommandHandler commandHandler;
OtaManager ota;

unsigned long lastIMUPublish = 0;
const unsigned long IMU_PUBLISH_INTERVAL = 100; // 100ms = 10Hz
unsigned long lastIMULog = 0;
const unsigned long IMU_LOG_INTERVAL = 3000; // 3秒に1回ログ出力
unsigned long lastStatePublish = 0;
const unsigned long STATE_PUBLISH_INTERVAL = 5000; // 5秒に1回状態パブリッシュ

// MQTTメッセージ受信コールバック
void mqttCallback(char* topic, uint8_t* payload, unsigned int length) {
    sastle::Log.printf("\n[MQTT] ← Message on topic: %s\n", topic);
    
    // CommandHandlerに処理を委譲
    if (strstr(topic, "/command/") != NULL) {
        commandHandler.handleMessage(topic, payload, length);
    } else {
        // その他のトピック（例: sphere/all/state）
        char message[512];
        int len = (length < sizeof(message) - 1) ? length : sizeof(message) - 1;
        memcpy(message, payload, len);
        message[len] = '\0';
        sastle::Log.printf("[MQTT] Unhandled topic: %s\n", message);
    }
}

void setup() {
    // シリアル初期化
    Serial.begin(115200);
    delay(2000);  // シリアルモニタ接続待ち

    // tee ロガーを初期化 (MQTT 接続前のログはバッファされ、接続後に送出される)
    sastle::Log.begin(&mqtt, topics::kDeviceLog);

    sastle::Log.println("\n\n=== M5Atom S3R Network Test ===");
    
    // PSRAM初期化確認
    if (psramFound()) {
        sastle::Log.printf("PSRAM found: %d bytes\n", ESP.getPsramSize());
        sastle::Log.printf("Free PSRAM: %d bytes\n", ESP.getFreePsram());
    } else {
        sastle::Log.println("PSRAM not found");
    }
    
    // LittleFS初期化
    sastle::Log.println("\n=== Initializing LittleFS ===");
    if (!FileManager::begin()) {
        sastle::Log.println("FileManager initialization FAILED!");
        while(1) delay(1000);
    }
    
    // ConfigManager初期化とロード
    sastle::Log.println("\n=== Loading Configuration ===");
    if (!config.loadConfig("/config.json")) {
        sastle::Log.println("Failed to load config!");
        while(1) delay(1000);
    }
    
    // 設定内容を表示
    config.printConfig();
    
    // NetworkManager初期化
    if (!network.begin(config)) {
        sastle::Log.println("Network initialization FAILED!");
        while(1) delay(1000);
    }
    
    // ネットワークステータス表示
    network.printStatus();
    
    // UDP開始（configから取得したポートでリッスン）
    uint16_t udpPort = config.getUDPPort();
    if (!network.beginUDP(udpPort)) {
        sastle::Log.println("Failed to start UDP!");
    } else {
        sastle::Log.println("UDP ready for communication");
    }
    
    // MQTT初期化
    mqtt.setCallback(mqttCallback);
    if (!mqtt.begin(config)) {
        sastle::Log.println("MQTT initialization failed (will retry)");
    }
    mqtt.printStatus();
    
    // MQTTトピックをサブスクライブ
    mqtt.subscribe(topics::kCommandParams);
    mqtt.subscribe(topics::kCommandPlayback);
    mqtt.subscribe(topics::kCommandLed);
    mqtt.subscribe(topics::kCommandSystem);
    sastle::Log.println("MQTT topics subscribed");
    
    // CommandHandler初期化 (LEDManager初期化後に移動)
    
    // Sound初期化
    if (!sound.begin(config)) {
        sastle::Log.println("Sound initialization failed (continuing without sound)");
    } else {
        // 起動音を再生
        sound.playEffect(SoundEffect::STARTUP);
    }
    
    // IMU初期化
    if (!imuSensor.begin(config)) {
        sastle::Log.println("IMU initialization failed (continuing without IMU)");
    } else {
        imuSensor.printStatus();
    }
    
    // ジェスチャー初期化 (サウンドフィードバック付き)
    if (imuSensor.isInitialized()) {
        if (!gesture.begin(imuSensor, mqtt, &sound)) {
            sastle::Log.println("Gesture initialization failed");
        }
    } else {
        sastle::Log.println("Gesture disabled (IMU not available)");
    }
    
    // ImageManager初期化
    if (!imageManager.begin(config, network)) {
        sastle::Log.println("ImageManager initialization failed (continuing without image)");
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
            sastle::Log.println("LEDManager initialization failed (continuing without LED)");
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
                sastle::Log.println("Failed to start LED render task");
            }
        }
    } else {
        sastle::Log.println("LEDManager disabled (ImageManager not available)");
    }
    
    // CommandHandler初期化
    if (!commandHandler.begin(&ledManager, &config)) {
        sastle::Log.println("CommandHandler initialization failed");
    }

    // OTA (espota) 初期化: WiFi 接続済みなので無線書き込みを受け付ける。
    // OTA 開始時はレンダリングタスクを停止する (要件: 更新中は描画停止で可)。
    ota.begin(&ledManager);

    sastle::Log.println("\n=== Setup Complete ===");
}

// 定期的にIMUデータをMQTT送信 (10Hz)
static void publishImuIfDue(unsigned long now) {
    if (now - lastIMUPublish < IMU_PUBLISH_INTERVAL) {
        return;
    }
    lastIMUPublish = now;

    float w, x, y, z;
    if (imuSensor.getQuaternion(w, x, y, z)) {
        char payload[128];
        snprintf(payload, sizeof(payload),
                 "{\"w\":%.4f,\"x\":%.4f,\"y\":%.4f,\"z\":%.4f}",
                 w, x, y, z);
        mqtt.publish(topics::kDeviceImu, payload, false);

        // 3秒に1回だけシリアルログ出力
        if (now - lastIMULog >= IMU_LOG_INTERVAL) {
            lastIMULog = now;
            sastle::Log.printf("[IMU] Quaternion: w=%.3f x=%.3f y=%.3f z=%.3f\n", w, x, y, z);
        }
    }
}

// 定期的に状態をパブリッシュ (retained, 5秒間隔)
static void publishStateIfDue(unsigned long now) {
    if (now - lastStatePublish < STATE_PUBLISH_INTERVAL) {
        return;
    }
    lastStatePublish = now;

    char stateBuffer[512];
    if (commandHandler.getState(stateBuffer, sizeof(stateBuffer))) {
        mqtt.publish(topics::kDeviceState, stateBuffer, true); // retained = true
        sastle::Log.println("[MQTT] → Published state (retained)");
    }
}

void loop() {
    // OTA 要求を最優先で処理。書き込みセッション中は handle() が転送完了まで
    // ブロックするため、描画・配信は自然に停止する (要件どおり)。
    ota.handle();

    // MQTT処理 (keep-alive & メッセージ受信)
    mqtt.loop();

    // 退避済みデバッグログを MQTT へフラッシュ
    sastle::Log.loop();

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
    unsigned long now = millis();
    if (imuSensor.isInitialized()) {
        imuSensor.update();

        // ジェスチャー検出更新
        gesture.update();

        publishImuIfDue(now);
    }

    // 状態パブリッシュはIMUの有無にかかわらず実施 (retained)
    publishStateIfDue(now);


    // UDP受信チェック
    int packetSize = network.parsePacket();
    if (packetSize) {
        uint8_t buffer[256];
        int len = network.read(buffer, sizeof(buffer) - 1);
        if (len > 0) {
            buffer[len] = 0;
            sastle::Log.printf("[%lu] Received UDP packet from %s:%d\n", 
                         millis(),
                         network.remoteIP().toString().c_str(), 
                         network.remotePort());
            sastle::Log.printf("  Data (%d bytes): %s\n", len, (char*)buffer);
        }
    }
    delay(10);
}

