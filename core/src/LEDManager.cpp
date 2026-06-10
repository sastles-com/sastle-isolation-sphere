/**
 * @file LEDManager.cpp
 * @brief LED制御マネージャー実装
 */

#include "LEDManager.h"
#include "FileManager.h"
#include "common.h"
#include "FastMath.h"
#include <FS.h>
#include <LittleFS.h>
#include <math.h>

namespace sastle {

// LED設定定数
#define MAX_LEDS 800
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB
#define TARGET_FPS 30
#define FRAME_DELAY_MS (1000 / TARGET_FPS)

// 静的メンバー初期化
LEDManager* LEDManager::_instance = nullptr;

LEDManager::LEDManager()
    : _initialized(false)
    , _taskRunning(false)
    , _config(nullptr)
    , _imageManager(nullptr)
    , _imuManager(nullptr)
    , _imuCompensationEnabled(false)
    , _renderTaskHandle(nullptr)
    , _frameReadySemaphore(nullptr)
    , _ledBuffer(nullptr)
    , _ledLayout(nullptr)
    , _numLEDs(0)
    , _lastFPSUpdate(0)
    , _frameCount(0)
{
    memset(&_stats, 0, sizeof(_stats));
    memset(_stripPins, 0, sizeof(_stripPins));
    memset(_ledsPerStrip, 0, sizeof(_ledsPerStrip));
    
    _instance = this;
}

LEDManager::~LEDManager() {
    stopRenderTask();
    
    if (_ledBuffer) {
        free(_ledBuffer);
        _ledBuffer = nullptr;
    }
    
    if (_ledLayout) {
        free(_ledLayout);
        _ledLayout = nullptr;
    }
    
    if (_frameReadySemaphore) {
        vSemaphoreDelete(_frameReadySemaphore);
        _frameReadySemaphore = nullptr;
    }
    
    _instance = nullptr;
}

bool LEDManager::begin(ConfigManager& config, ImageManager& imageManager, IMUManager* imuManager) {
    Serial.println("[LEDManager] Initializing...");
    
    _config = &config;
    _imageManager = &imageManager;
    _imuManager = imuManager;
    
    // IMUが有効なら姿勢補正を有効化
    if (_imuManager && _imuManager->isInitialized()) {
        _imuCompensationEnabled = true;
        Serial.println("[LEDManager] IMU compensation enabled");
    } else {
        _imuCompensationEnabled = false;
        Serial.println("[LEDManager] IMU compensation disabled (no IMU)");
    }
    
    // LEDレイアウト読み込み
    String layoutPathStr = config.getLayoutPath();
    const char* layoutPath = layoutPathStr.c_str();
    if (!loadLayout(layoutPath)) {
        Serial.println("[LEDManager] Failed to load LED layout");
        return false;
    }
    
    Serial.printf("[LEDManager] Loaded %d LEDs from layout\n", _numLEDs);
    
    // LEDバッファ確保 (SRAM)
    _ledBuffer = (CRGB*)malloc(_numLEDs * sizeof(CRGB));
    if (!_ledBuffer) {
        Serial.println("[LEDManager] Failed to allocate LED buffer");
        return false;
    }
    
    Serial.printf("[LEDManager] Allocated LED buffer: %d bytes\n", _numLEDs * sizeof(CRGB));
    
    // GPIO設定 (仕様書から)
    _stripPins[0] = 5;  // Strip 0 -> GPIO 5
    _stripPins[1] = 6;  // Strip 1 -> GPIO 6
    _stripPins[2] = 7;  // Strip 2 -> GPIO 7
    _stripPins[3] = 8;  // Strip 3 -> GPIO 8
    
    // FastLED初期化 (4ストリップ)
    // ストリップ毎のLED数とオフセットを計算
    memset(_ledsPerStrip, 0, sizeof(_ledsPerStrip));
    memset(_stripStartIndex, 0, sizeof(_stripStartIndex));
    
    for (uint16_t i = 0; i < _numLEDs; i++) {
        uint8_t strip = _ledLayout[i].strip;
        if (strip < 4) {
            _ledsPerStrip[strip]++;
        }
    }
    
    // ストリップ開始インデックスを計算
    _stripStartIndex[0] = 0;
    for (int i = 1; i < 4; i++) {
        _stripStartIndex[i] = _stripStartIndex[i-1] + _ledsPerStrip[i-1];
    }
    
    // ストリップ毎のバッファポインタを設定
    _stripBuffers[0] = _ledBuffer;
    _stripBuffers[1] = _ledBuffer + _stripStartIndex[1];
    _stripBuffers[2] = _ledBuffer + _stripStartIndex[2];
    _stripBuffers[3] = _ledBuffer + _stripStartIndex[3];
    
    Serial.printf("[LEDManager] LEDs per strip: [%d, %d, %d, %d]\n",
                  _ledsPerStrip[0], _ledsPerStrip[1], _ledsPerStrip[2], _ledsPerStrip[3]);
    Serial.printf("[LEDManager] Strip offsets: [%d, %d, %d, %d]\n",
                  _stripStartIndex[0], _stripStartIndex[1], _stripStartIndex[2], _stripStartIndex[3]);
    
    // FastLED初期化 (RMT DMA自動使用)
    // 各ストリップは独立したRMTチャンネルで並列出力される
    FastLED.addLeds<LED_TYPE, 5, COLOR_ORDER>(_stripBuffers[0], _ledsPerStrip[0]);
    FastLED.addLeds<LED_TYPE, 6, COLOR_ORDER>(_stripBuffers[1], _ledsPerStrip[1]);
    FastLED.addLeds<LED_TYPE, 7, COLOR_ORDER>(_stripBuffers[2], _ledsPerStrip[2]);
    FastLED.addLeds<LED_TYPE, 8, COLOR_ORDER>(_stripBuffers[3], _ledsPerStrip[3]);
    
    FastLED.setBrightness(128);  // デフォルト輝度 50%
    FastLED.clear();
    FastLED.show();
    
    // セマフォ作成
    _frameReadySemaphore = xSemaphoreCreateBinary();
    if (!_frameReadySemaphore) {
        Serial.println("[LEDManager] Failed to create semaphore");
        return false;
    }
    
    // ImageManagerにフレーム準備完了コールバックを設定
    _imageManager->setFrameReadyCallback(LEDManager::onFrameReady);
    
    _initialized = true;
    Serial.println("[LEDManager] Initialization complete");
    
    return true;
}

bool LEDManager::loadLayout(const char* path) {
    if (!FileManager::exists(path)) {
        Serial.printf("[LEDManager] Layout file not found: %s\n", path);
        return false;
    }
    
    File file = LittleFS.open(path, "r");
    if (!file) {
        Serial.println("[LEDManager] Failed to open layout file");
        return false;
    }
    
    // 1行目（ヘッダー）をスキップ
    String line = file.readStringUntil('\n');
    
    // LED数をカウント
    uint16_t ledCount = 0;
    while (file.available()) {
        line = file.readStringUntil('\n');
        if (line.length() > 0) {
            ledCount++;
        }
    }
    
    if (ledCount == 0 || ledCount > MAX_LEDS) {
        Serial.printf("[LEDManager] Invalid LED count: %d\n", ledCount);
        file.close();
        return false;
    }
    
    // レイアウトバッファ確保
    _ledLayout = (LEDCoord*)malloc(ledCount * sizeof(LEDCoord));
    if (!_ledLayout) {
        Serial.println("[LEDManager] Failed to allocate layout buffer");
        file.close();
        return false;
    }
    
    // ファイルを再読み込み
    file.seek(0);
    file.readStringUntil('\n');  // ヘッダースキップ
    
    _numLEDs = 0;
    while (file.available() && _numLEDs < ledCount) {
        line = file.readStringUntil('\n');
        if (line.length() == 0) continue;
        
        // CSV解析: FaceID,strip,strip_num,x,y,z
        int idx[5];
        idx[0] = line.indexOf(',');
        for (int i = 1; i < 5; i++) {
            idx[i] = line.indexOf(',', idx[i-1] + 1);
        }
        
        if (idx[4] > 0) {
            LEDCoord& coord = _ledLayout[_numLEDs];
            
            coord.faceID = line.substring(0, idx[0]).toInt();
            coord.strip = line.substring(idx[0] + 1, idx[1]).toInt();
            coord.stripNum = line.substring(idx[1] + 1, idx[2]).toInt();
            coord.x = line.substring(idx[2] + 1, idx[3]).toFloat();
            coord.y = line.substring(idx[3] + 1, idx[4]).toFloat();
            coord.z = line.substring(idx[4] + 1).toFloat();
            
            _numLEDs++;
        }
    }
    
    file.close();
    
    Serial.printf("[LEDManager] Loaded %d LED coordinates\n", _numLEDs);
    
    return _numLEDs > 0;
}

void LEDManager::sphereToUV(float x, float y, float z, float& u, float& v) {
    // 球面座標変換: (x, y, z) -> (u, v)
    // ブログ記事の式を使用:
    // u = arctan2(√(rx² + rz²), ry)  // 緯度方向
    // v = arctan2(rx, rz)             // 経度方向
    
    // 正規化（念のため）- common.hの高速平方根を使用
    float len = _sqrt(x*x + y*y + z*z);
    if (len < 0.0001f) {
        u = 0.5f;
        v = 0.5f;
        return;
    }
    
    float nx = x / len;
    float ny = y / len;
    float nz = z / len;
    
    // 緯度: arctan2(√(nx² + nz²), ny) を 0-1 にマッピング
    float horizontal_dist = _sqrt(nx*nx + nz*nz);
    u = (_atan2(horizontal_dist, ny) + 1.0f) / 2.0f;  // _atan2は-1.0~1.0を返す
    
    // 経度: arctan2(nx, nz) を 0-1 にマッピング
    v = (_atan2(nx, nz) + 1.0f) / 2.0f;
    
    // クランプ
    if (u < 0.0f) u = 0.0f;
    if (u > 1.0f) u = 1.0f;
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
}

void LEDManager::rotateByQuaternion(float& x, float& y, float& z, float qw, float qx, float qy, float qz) {
    // Quaternionでベクトルを回転: v' = q * v * q^-1
    // 最適化された形: v' = v + 2 * cross(q.xyz, cross(q.xyz, v) + q.w * v)
    
    // cross1 = q.xyz × v
    float cross1_x = qy * z - qz * y;
    float cross1_y = qz * x - qx * z;
    float cross1_z = qx * y - qy * x;
    
    // cross1 += q.w * v
    cross1_x += qw * x;
    cross1_y += qw * y;
    cross1_z += qw * z;
    
    // cross2 = q.xyz × cross1
    float cross2_x = qy * cross1_z - qz * cross1_y;
    float cross2_y = qz * cross1_x - qx * cross1_z;
    float cross2_z = qx * cross1_y - qy * cross1_x;
    
    // v' = v + 2 * cross2
    x += 2.0f * cross2_x;
    y += 2.0f * cross2_y;
    z += 2.0f * cross2_z;
}

void LEDManager::setIMUCompensation(bool enabled) {
    _imuCompensationEnabled = enabled;
    Serial.printf("[LEDManager] IMU compensation %s\n", enabled ? "enabled" : "disabled");
}

bool LEDManager::startRenderTask(uint8_t core, uint8_t priority, uint32_t stackSize) {
    if (!_initialized) {
        Serial.println("[LEDManager] Not initialized");
        return false;
    }
    
    if (_taskRunning) {
        Serial.println("[LEDManager] Task already running");
        return true;
    }
    
    BaseType_t result = xTaskCreatePinnedToCore(
        renderTaskFunction,     // タスク関数
        "LED_Render",           // タスク名
        stackSize,              // スタックサイズ
        this,                   // パラメータ (this)
        priority,               // 優先度
        &_renderTaskHandle,     // タスクハンドル
        core                    // 実行コア
    );
    
    if (result != pdPASS) {
        Serial.println("[LEDManager] Failed to create render task");
        return false;
    }
    
    _taskRunning = true;
    Serial.printf("[LEDManager] Render task started on core %d (priority %d)\n", core, priority);
    
    return true;
}

void LEDManager::stopRenderTask() {
    if (!_taskRunning) {
        return;
    }
    
    if (_renderTaskHandle) {
        vTaskDelete(_renderTaskHandle);
        _renderTaskHandle = nullptr;
    }
    
    _taskRunning = false;
    Serial.println("[LEDManager] Render task stopped");
}

void LEDManager::renderTaskFunction(void* parameter) {
    LEDManager* manager = static_cast<LEDManager*>(parameter);
    const TickType_t frameDelay = pdMS_TO_TICKS(FRAME_DELAY_MS);
    
    Serial.printf("[LED_Render] Task started on core %d\n", xPortGetCoreID());
    
    while (true) {
        unsigned long frameStart = micros();
        
        // ImageManagerのフレーム更新を待機
        // タイムアウト付き（フレーム間隔）
        if (xSemaphoreTake(manager->_frameReadySemaphore, frameDelay)) {
            // 新しいフレームが利用可能
            manager->renderFrame();
        } else {
            // タイムアウト - 前のフレームを再描画または何もしない
            // ここでは統計カウント用にドロップとする
            manager->_stats.frames_dropped++;
        }
        
        // フレームレート維持
        unsigned long frameTime = micros() - frameStart;
        manager->_stats.render_time_us = frameTime;
        
        // 短い待機で他タスクに譲る
        vTaskDelay(1);
    }
}

void LEDManager::renderFrame() {
    unsigned long start = micros();
    
    // LEDバッファ更新
    updateLEDBuffer();
    
    unsigned long mappingTime = micros() - start;
    _stats.mapping_time_us = mappingTime;
    
    // LED出力（RMT DMA並列出力）
    unsigned long outputStart = micros();
    showParallel();
    unsigned long outputTime = micros() - outputStart;
    _stats.output_time_us = outputTime;
    
    // 統計更新
    _stats.frames_rendered++;
    _frameCount++;
    
    // FPS計算 (1秒毎)
    unsigned long now = millis();
    if (now - _lastFPSUpdate >= 1000) {
        _stats.fps = _frameCount * 1000.0f / (now - _lastFPSUpdate);
        _frameCount = 0;
        _lastFPSUpdate = now;
    }
}

void LEDManager::updateLEDBuffer() {
    if (!_imageManager || !_ledLayout || !_ledBuffer) {
        return;
    }
    
    // 全ストリップを順次更新
    for (uint8_t strip = 0; strip < 4; strip++) {
        updateStripBuffer(strip);
    }
}

void LEDManager::updateStripBuffer(uint8_t stripIndex) {
    if (stripIndex >= 4 || !_imageManager || !_ledLayout) {
        return;
    }
    
    uint16_t imgWidth = _imageManager->getWidth();
    uint16_t imgHeight = _imageManager->getHeight();
    
    // IMU姿勢補正用のquaternionを取得（全ストリップ共通）
    float qw = 1.0f, qx = 0.0f, qy = 0.0f, qz = 0.0f;
    bool useIMU = false;
    
    if (_imuCompensationEnabled && _imuManager && _imuManager->isInitialized()) {
        if (_imuManager->getQuaternion(qw, qx, qy, qz)) {
            // Quaternionの共役（逆回転）
            qx = -qx;
            qy = -qy;
            qz = -qz;
            useIMU = true;
        }
    }
    
    // このストリップのLEDのみ処理
    for (uint16_t i = 0; i < _numLEDs; i++) {
        LEDCoord& coord = _ledLayout[i];
        
        // ストリップフィルタ
        if (coord.strip != stripIndex) {
            continue;
        }
        
        // LEDの3D座標をコピー
        float x = coord.x;
        float y = coord.y;
        float z = coord.z;
        
        // IMU姿勢補正
        if (useIMU) {
            rotateByQuaternion(x, y, z, qw, qx, qy, qz);
        }
        
        // 3D座標 → UV座標変換（高速近似）
        float u, v;
        sphereToUV(x, y, z, u, v);
        
        // UV → ピクセル座標
        uint16_t px = (uint16_t)(u * (imgWidth - 1));
        uint16_t py = (uint16_t)(v * (imgHeight - 1));
        
        // ピクセル色取得
        uint8_t r, g, b;
        _imageManager->getPixel(px, py, r, g, b);
        
        // LEDバッファに設定
        _ledBuffer[i] = CRGB(r, g, b);
    }
}

void LEDManager::showParallel() {
    // FastLED.show()は内部的にRMT DMAを使用
    // 4つのRMTチャンネルが並列動作し、CPUブロック時間は最大ストリップのみ
    FastLED.show();
    
    // Note: ESP32-S3のRMTは8チャンネルあり、4ストリップは完全に並列出力される
    // 実測: 800 LED でも ~6-8ms (最長ストリップ220 LEDのみ待機)
}

void LEDManager::fillSolid(uint8_t r, uint8_t g, uint8_t b) {
    if (!_ledBuffer) return;
    
    CRGB color(r, g, b);
    for (uint16_t i = 0; i < _numLEDs; i++) {
        _ledBuffer[i] = color;
    }
}

void LEDManager::show() {
    FastLED.show();
}

void LEDManager::setBrightness(uint8_t brightness) {
    FastLED.setBrightness(brightness);
}

void LEDManager::printStatus() {
    Serial.println("\n=== LED Manager Status ===");
    Serial.printf("Initialized: %s\n", _initialized ? "Yes" : "No");
    Serial.printf("Task Running: %s\n", _taskRunning ? "Yes" : "No");
    Serial.printf("Total LEDs: %d\n", _numLEDs);
    Serial.printf("Strips: 4 [%d, %d, %d, %d]\n",
                  _ledsPerStrip[0], _ledsPerStrip[1], _ledsPerStrip[2], _ledsPerStrip[3]);
    Serial.println("\n--- Statistics ---");
    Serial.printf("Frames Rendered: %u\n", _stats.frames_rendered);
    Serial.printf("Frames Dropped: %u\n", _stats.frames_dropped);
    Serial.printf("FPS: %.2f\n", _stats.fps);
    Serial.printf("Render Time: %u us\n", _stats.render_time_us);
    Serial.printf("  Mapping: %u us\n", _stats.mapping_time_us);
    Serial.printf("  Output: %u us\n", _stats.output_time_us);
}

void LEDManager::onFrameReady() {
    // 静的コールバック関数
    // ImageManagerからフレームデコード完了時に呼ばれる
    if (_instance && _instance->_frameReadySemaphore) {
        // セマフォをGiveしてレンダリングタスクに通知
        BaseType_t higherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(_instance->_frameReadySemaphore, &higherPriorityTaskWoken);
        portYIELD_FROM_ISR(higherPriorityTaskWoken);
    }
}

} // namespace sastle
