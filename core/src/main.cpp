#include <Arduino.h>
#include <WiFi.h>
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
#include "TimeSync.h"

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
TimeSync timeSync;

unsigned long lastIMUPublish = 0;
const unsigned long IMU_PUBLISH_INTERVAL = 100; // 100ms = 10Hz
unsigned long lastIMULog = 0;
const unsigned long IMU_LOG_INTERVAL = 3000; // 3秒に1回ログ出力
unsigned long lastStatePublish = 0;
const unsigned long STATE_PUBLISH_INTERVAL = 5000; // 5秒に1回状態パブリッシュ
unsigned long lastPerfLog = 0;
const unsigned long PERF_LOG_INTERVAL = 2000; // 2秒に1回 性能計測ログ
unsigned long lastTimeSyncLog = 0;
const unsigned long TIMESYNC_LOG_INTERVAL = 5000; // 5秒に1回 時刻同期状態ログ

// MQTTメッセージ受信コールバック
void mqttCallback(char* topic, uint8_t* payload, unsigned int length) {
    // 重要: PubSubClient は送受信で同一バッファを使う。コールバック内で
    // publish (= sastle::Log のMQTT送出やコマンド応答) を行うと、受信中の
    // topic/payload が上書き破損する。先にローカルへ退避してから処理する。
    char topicCopy[128];
    strncpy(topicCopy, topic, sizeof(topicCopy) - 1);
    topicCopy[sizeof(topicCopy) - 1] = '\0';

    // 受信バッファと同容量 (led コマンドの pixels 配列で最大になる)。
    // 2KB をコールバックのスタックに積むと loop タスク (8KB) を圧迫するため static。
    // mqtt.loop() 経由でこのコールバックが呼ばれるのは loop タスクのみなので再入はしない。
    static char payloadCopy[sastle::kMqttBufferSize];
    unsigned int len = (length < sizeof(payloadCopy) - 1) ? length : sizeof(payloadCopy) - 1;
    memcpy(payloadCopy, payload, len);
    payloadCopy[len] = '\0';

    // 時刻同期ビーコン (sphere/all/clock) は TimeSync へ。1秒周期で頻繁に来るため
    // command 判定より先に捌き、ログも出さない (ログ洪水を避ける)。
    if (strcmp(topicCopy, sastle::topics::kAllClock) == 0) {
        timeSync.onClockMessage(payloadCopy);
        return;
    }

    sastle::Log.printf("\n[MQTT] ← Message on topic: %s\n", topicCopy);

    // CommandHandlerに処理を委譲
    if (strstr(topicCopy, "/command/") != NULL) {
        commandHandler.handleMessage(topicCopy, (uint8_t*)payloadCopy, len);
    } else {
        // その他のトピック（例: sphere/all/state）
        sastle::Log.printf("[MQTT] Unhandled topic payload: %s\n", payloadCopy);
    }
}

// Grove I2C バスを走査して検出アドレスを MQTT ログに出す (診断用)。
// BNO055 は ADR ピンにより 0x28 (L) / 0x29 (H) に応答する。
static void scanI2cBus(uint8_t sda, uint8_t scl) {
    Wire.begin(sda, scl, 400000);
    delay(50);

    sastle::Log.printf("\n[I2C] Scanning bus (SDA=GPIO%u, SCL=GPIO%u)\n", sda, scl);

    int found = 0;
    bool bnoFound = false;
    for (uint8_t addr = 0x08; addr <= 0x77; ++addr) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            ++found;
            const bool isBno = (addr == 0x28 || addr == 0x29);
            if (isBno) {
                bnoFound = true;
            }
            sastle::Log.printf("[I2C]   found 0x%02X%s\n", addr, isBno ? "  <- BNO055" : "");
            delay(2);
        }
    }

    sastle::Log.printf("[I2C] Scan done: %d device(s), BNO055(0x28/0x29)=%s\n",
                       found, bnoFound ? "PRESENT" : "ABSENT");
}

void setup() {
    // シリアル初期化
    Serial.begin(115200);

    // Sound初期化を最優先で行い、起動音を即再生する。
    // 理由: WiFi/MQTT/LCD初期化が終わるまでLED表示は数秒かかるため、
    // 電源スイッチが正しくONになったかをすぐ確認できるよう、
    // 他の初期化より前に音でフィードバックする。
    bool soundReady = sound.begin(config);
    if (soundReady) {
        sound.playEffect(SoundEffect::STARTUP);
    }

    delay(2000);  // シリアルモニタ接続待ち

    // tee ロガーを初期化 (MQTT 接続前のログはバッファされ、接続後に送出される)
    // 宛先トピックは接続後に sphere/<id>/log へ動的解決される
    sastle::Log.begin(&mqtt, "log");

    if (!soundReady) {
        sastle::Log.println("Sound initialization failed (continuing without sound)");
    }

    sastle::Log.println("\n\n=== M5Atom S3R Network Test ===");

    // 直前のリセット理由を記録 (勝手な再起動の原因切り分け用)
    // 1=POWERON 3=SW 4=PANIC 5/6=WDT 7=TASK_WDT 9=BROWNOUT (esp_reset_reason_t)
    {
        esp_reset_reason_t rr = esp_reset_reason();
        const char* rrName;
        switch (rr) {
            case ESP_RST_POWERON:  rrName = "POWERON"; break;
            case ESP_RST_SW:       rrName = "SW_RESTART"; break;
            case ESP_RST_PANIC:    rrName = "PANIC"; break;
            case ESP_RST_INT_WDT:  rrName = "INT_WDT"; break;
            case ESP_RST_TASK_WDT: rrName = "TASK_WDT"; break;
            case ESP_RST_WDT:      rrName = "WDT"; break;
            case ESP_RST_BROWNOUT: rrName = "BROWNOUT"; break;
            case ESP_RST_DEEPSLEEP:rrName = "DEEPSLEEP"; break;
            default:               rrName = "OTHER"; break;
        }
        sastle::Log.printf("[BOOT] Reset reason: %d (%s)\n", (int)rr, rrName);
    }
    
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
    mqtt.subscribe(topics::kAllClock);        // 時刻同期ビーコン
    sastle::Log.println("MQTT topics subscribed");
    
    // CommandHandler初期化 (LEDManager初期化後に移動)

    // IMU初期化前に I2C バスを走査 (BNO055 の有無を切り分けるため)
    scanI2cBus(kImuI2cSda, kImuI2cScl);

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

            // 起動オープニングパターン (config でスキップ可)
            if (config.getOpeningActionEnabled()) {
                ledManager.playOpening(config.getOpeningActionDurationMs());
            } else {
                ledManager.fillSolid(0, 0, 0);
                ledManager.show();
            }

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

    // デコードタスクを Core0 で開始 (レンダリングは Core1)。
    // UDP受信+JPEGデコードを描画と並列実行する。
    if (imageManager.isInitialized()) {
        imageManager.startDecodeTask(0, 1, 8192);
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
        mqtt.publishDevice("imu", payload, false);

        // 3秒に1回だけシリアルログ出力 (診断用に gyro/accel/キャリブ状態も出す)
        if (now - lastIMULog >= IMU_LOG_INTERVAL) {
            lastIMULog = now;
            float gx, gy, gz, ax, ay, az;
            imuSensor.getGyro(gx, gy, gz);
            imuSensor.getAccel(ax, ay, az);
            uint8_t cs, cg, ca, cm;
            imuSensor.getCalibration(cs, cg, ca, cm);
            sastle::Log.printf(
                "[IMU] q=(%.3f,%.3f,%.3f,%.3f) gyro=(%.2f,%.2f,%.2f) acc=(%.1f,%.1f,%.1f) cal=%u%u%u%u\n",
                w, x, y, z, gx, gy, gz, ax, ay, az, cs, cg, ca, cm);
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
        mqtt.publishDevice("state", stateBuffer, true); // retained = true
        sastle::Log.println("[MQTT] → Published state (retained)");
    }
}

// 性能計測ログ (2秒間隔): マッピング/出力/デコード時間と FPS を MQTT ログへ
static void publishPerfStatsIfDue(unsigned long now) {
    if (now - lastPerfLog < PERF_LOG_INTERVAL) {
        return;
    }
    lastPerfLog = now;

    LEDStats led = ledManager.getStats();
    ImageStats img = imageManager.getStats();
    sastle::Log.printf(
        "[PERF] render_fps=%.1f map=%luus out=%luus | img_fps=%.1f decode=%luus jpeg=%uB recv=%lu hits=%lu drop=%lu | heap=%u\n",
        led.fps, (unsigned long)led.mapping_time_us, (unsigned long)led.output_time_us,
        img.fps, (unsigned long)img.decode_time_us, (unsigned)img.last_jpeg_size,
        (unsigned long)img.frames_received, (unsigned long)imageManager.getParseHits(),
        (unsigned long)imageManager.getDropped(), (unsigned)ESP.getFreeHeap());
}

// 時刻同期状態ログ (5秒間隔): 同期可否・offset・syncedNow を確認する検証用
static void publishTimeSyncIfDue(unsigned long now) {
    if (now - lastTimeSyncLog < TIMESYNC_LOG_INTERVAL) {
        return;
    }
    lastTimeSyncLog = now;

    if (timeSync.isSynced()) {
        int64_t nowMs = timeSync.syncedNow();
        sastle::Log.printf("[TIMESYNC] synced now=%lld ms offset=%lld ms seq=%lu\n",
                           (long long)nowMs, (long long)timeSync.offset(),
                           (unsigned long)timeSync.lastSeq());
    } else {
        sastle::Log.println("[TIMESYNC] not synced yet (no clock beacon received)");
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

    // UDP受信+デコードは Core0 のデコードタスクで実行 (loop からは呼ばない)。
    // レンダリングは Core1 のレンダリングタスク。decode∥render で並列化。

    // LCD更新 (デバッグモード時のみ)。
    // 映像フレームが一定時間届いていれば映像を表示、途絶えていれば
    // device 生存情報(STANDBY ステータス画面)へ自動で切り替える。
    // これにより停止時の焼き付き解消と「動いてないが生きている」表示を両立。
    if (lcdManager.isDebugEnabled()) {
        static uint32_t s_lastFrames = 0;
        static unsigned long s_lastFrameMs = 0;
        unsigned long n = millis();
        uint32_t fr = imageManager.getStats().frames_received;
        if (fr != s_lastFrames) {
            s_lastFrames = fr;
            s_lastFrameMs = n;
        }
        // 映像未受信なら起動3秒後にSTANDBY、受信後は途切れ1.5秒でSTANDBY
        bool idle = (s_lastFrames == 0) ? (n > 3000) : ((n - s_lastFrameMs) > 1500);
        if (idle) {
            LcdStatus st;
            st.uptime_s = n / 1000;
            static String ipStr;
            ipStr = WiFi.localIP().toString();
            st.ip = ipStr.c_str();
            st.rssi = (int)WiFi.RSSI();
            st.free_heap = ESP.getFreeHeap();
            st.fps = ledManager.getStats().fps;
            float qw, qx, qy, qz;
            st.imu_ok = imuSensor.getQuaternion(qw, qx, qy, qz);
            st.qw = qw; st.qx = qx; st.qy = qy; st.qz = qz;
            lcdManager.drawStatus(st);
        } else {
            lcdManager.update(&imageManager);
        }
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

    // 性能計測ログ
    publishPerfStatsIfDue(now);

    // 時刻同期状態ログ (検証用)
    publishTimeSyncIfDue(now);

    // 映像UDP受信+デコードは Core0 のデコードタスクが担当 (loop では扱わない)
    delay(10);
}

