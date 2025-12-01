# ESP32-S3 デュアルコア タスク分散設計

## 概要
ESP32-S3の2つのCPUコア (PRO_CPU / APP_CPU) を効率的に活用し、
リアルタイム画像処理とLED描画を並列実行する設計。

## コアアーキテクチャ

### ESP32-S3 コア仕様
- **PRO_CPU (Core 0)**: Protocol CPU - システム制御、通信処理
- **APP_CPU (Core 1)**: Application CPU - アプリケーションタスク
- **両コア**: 240MHz動作、独立したキャッシュ
- **共有リソース**: PSRAM, Flash, 周辺機器

## タスク分散戦略

```
┌──────────────────────────────────────────────────────┐
│               ESP32-S3 Dual Core                     │
├───────────────────────┬──────────────────────────────┤
│   PRO_CPU (Core 0)    │    APP_CPU (Core 1)          │
│   システム制御         │    描画・出力                 │
├───────────────────────┼──────────────────────────────┤
│ ● Arduino setup/loop  │ ● LED描画タスク              │
│ ● WiFi通信            │ ● 画像→LED変換              │
│ ● UDP受信             │ ● WS2812出力 (RMT/I2S)      │
│ ● MQTT通信            │ ● 座標マッピング             │
│ ● IMU読み取り         │                              │
│ ● ジェスチャー検出     │                              │
│ ● JPEG デコード       │                              │
│ ● サウンド出力         │                              │
│ ● システム管理         │                              │
└───────────────────────┴──────────────────────────────┘
            ↓                         ↑
      ┌─────────────────────────────────┐
      │   共有メモリ (PSRAM)             │
      │ ● RGB565 ダブルバッファ          │
      │ ● LED出力バッファ               │
      │ ● 同期フラグ (Semaphore)        │
      └─────────────────────────────────┘
```

## 詳細タスク配置

### Core 0 (PRO_CPU) - 入力・処理系

#### 優先度: 高
```cpp
void setup() {
    // Core 0で実行される初期化
    - FileManager::begin()
    - ConfigManager::loadConfig()
    - NetworkManager::begin()
    - MQTTManager::begin()
    - IMUManager::begin()
    - SoundManager::begin()
    - ImageManager::begin()
    
    // Core 1タスク起動
    xTaskCreatePinnedToCore(
        ledRenderTask,      // タスク関数
        "LED_Render",       // タスク名
        8192,               // スタックサイズ
        NULL,               // パラメータ
        2,                  // 優先度 (高)
        &ledTaskHandle,     // ハンドル
        1                   // Core 1に固定
    );
}

void loop() {
    // Core 0メインループ (1ms周期推奨)
    
    // 1. ネットワーク通信 (最優先)
    mqtt.loop();                    // ~0.1ms
    
    // 2. UDP画像受信・デコード
    if (imageManager.update()) {    // ~10-30ms (JPEG依存)
        // 新フレーム準備完了 → Core 1に通知
        xSemaphoreGive(frameReadySemaphore);
    }
    
    // 3. IMU更新
    imuSensor.update();             // ~1ms (100Hz)
    
    // 4. ジェスチャー検出
    gesture.update();               // ~0.5ms
    
    // 5. 定期送信 (10Hz)
    if (shouldPublishIMU()) {
        publishIMUData();           // ~1ms
    }
    
    // 6. サウンド処理
    // (非ブロッキング、割り込み駆動)
    
    delay(1);  // 他タスクに譲る
}
```

### Core 1 (APP_CPU) - 出力・描画系

#### 優先度: 最高 (リアルタイム性重視)
```cpp
void ledRenderTask(void* parameter) {
    const TickType_t frameDelay = pdMS_TO_TICKS(33);  // ~30fps
    
    while (true) {
        // 新フレーム待機 (タイムアウト付き)
        if (xSemaphoreTake(frameReadySemaphore, frameDelay)) {
            
            // 1. 画像データ取得 (読み取り専用バッファ)
            //    ImageManager内部でダブルバッファ管理
            
            // 2. LED座標マッピング
            for (int i = 0; i < NUM_LEDS; i++) {
                LEDCoord coord = ledLayout[i];
                
                // 3D座標 → UV座標変換
                float u, v;
                sphereToUV(coord.x, coord.y, coord.z, u, v);
                
                // UV → ピクセル座標
                uint16_t px = u * imageManager.getWidth();
                uint16_t py = v * imageManager.getHeight();
                
                // ピクセル色取得
                uint8_t r, g, b;
                imageManager.getPixel(px, py, r, g, b);
                
                // LEDバッファに書き込み
                ledBuffer[i] = CRGB(r, g, b);
            }
            
            // 3. LED出力 (DMA転送)
            FastLED.show();  // または I2S DMA
        }
        
        // フレームレート維持
        vTaskDelay(1);
    }
}
```

## 同期メカニズム

### セマフォによるフレーム同期

```cpp
// グローバル
SemaphoreHandle_t frameReadySemaphore;
SemaphoreHandle_t bufferMutex;

void setup() {
    // バイナリセマフォ (フレーム準備完了通知)
    frameReadySemaphore = xSemaphoreCreateBinary();
    
    // ミューテックス (バッファアクセス保護)
    bufferMutex = xSemaphoreCreateMutex();
}

// Core 0: 画像デコード完了時
void ImageManager::swapBuffers() {
    if (xSemaphoreTake(bufferMutex, portMAX_DELAY)) {
        // ポインタ交換
        uint16_t* temp = _drawBuffer;
        _drawBuffer = _decodeBuffer;
        _decodeBuffer = temp;
        
        xSemaphoreGive(bufferMutex);
    }
    
    // Core 1に通知
    xSemaphoreGive(frameReadySemaphore);
}

// Core 1: ピクセル読み取り時
uint16_t ImageManager::getPixelRGB565(uint16_t x, uint16_t y) {
    uint16_t result = 0;
    
    if (xSemaphoreTake(bufferMutex, pdMS_TO_TICKS(10))) {
        result = _drawBuffer[y * _width + x];
        xSemaphoreGive(bufferMutex);
    }
    
    return result;
}
```

## パフォーマンス最適化

### タスク優先度設定

| タスク | コア | 優先度 | 周期 | 処理時間 |
|--------|------|--------|------|----------|
| LED Render | 1 | 2 (高) | 33ms (30fps) | ~25ms |
| Arduino Loop | 0 | 1 (中) | 1ms | ~0.5ms |
| WiFi Task | 0 | 23 (最高) | イベント駆動 | ~0.1ms |
| IDLE Task | 0/1 | 0 (最低) | 常時 | - |

### メモリアクセス最適化

```cpp
// PSRAM配置 (大容量・低速)
uint16_t* _bufferA;   // ps_malloc() - 100KB
uint16_t* _bufferB;   // ps_malloc() - 100KB
uint8_t* _udpBuffer;  // ps_malloc() - 64KB

// SRAM配置 (小容量・高速)
CRGB ledBuffer[800];  // malloc() - 2.4KB
LEDCoord ledLayout[800];  // malloc() - 9.6KB

// DMA転送用 (キャッシュ整合性)
DRAM_ATTR uint8_t dmaBuffer[2400];  // LED出力用
```

### CPU負荷分散

```
Core 0 負荷:
├─ WiFi/MQTT: 5-10%
├─ UDP受信: 5%
├─ JPEG Decode: 20-40% (ピーク)
├─ IMU: 5%
├─ Gesture: 2%
└─ 余裕: 40-60%

Core 1 負荷:
├─ Coordinate Mapping: 30-40%
├─ LED Output (DMA): 10%
├─ 待機時間: 50-60%
```

## タスク間通信フロー

```
┌─────────────────────────────────────────────────────┐
│ Core 0: 画像受信・処理                               │
└──┬──────────────────────────────────────────────────┘
   │
   ▼
┌──────────────┐
│ UDP Receive  │ → JPEG Binary (64KB)
└──┬───────────┘
   │
   ▼
┌──────────────┐
│ JPEG Decode  │ → RGB565 Buffer B (100KB)
└──┬───────────┘
   │
   ▼
┌──────────────┐
│ Swap Buffers │ → _drawBuffer = Buffer B
└──┬───────────┘    _decodeBuffer = Buffer A
   │
   │ xSemaphoreGive(frameReadySemaphore)
   │
   ▼
┌─────────────────────────────────────────────────────┐
│ Core 1: LED描画                                      │
└──┬──────────────────────────────────────────────────┘
   │
   ▼
┌──────────────┐
│ Wait Frame   │ ← xSemaphoreTake(frameReadySemaphore)
└──┬───────────┘
   │
   ▼
┌──────────────┐
│ UV Mapping   │ → 800 LEDs × (x,y,z → u,v)
└──┬───────────┘
   │
   ▼
┌──────────────┐
│ Get Pixels   │ → getPixel(u,v) × 800
└──┬───────────┘
   │
   ▼
┌──────────────┐
│ LED Output   │ → FastLED.show() / I2S DMA
└──────────────┘
```

## 実装クラス設計

### LEDManager (新規)

```cpp
class LEDManager {
public:
    bool begin(ConfigManager& config, ImageManager& image);
    void startRenderTask(uint8_t core = 1, uint8_t priority = 2);
    void stopRenderTask();
    
private:
    static void renderTaskFunction(void* parameter);
    void renderFrame();
    void updateLEDBuffer();
    
    TaskHandle_t _renderTaskHandle;
    SemaphoreHandle_t _frameReadySemaphore;
    
    ImageManager* _imageManager;
    CRGB* _ledBuffer;
    LEDCoord* _ledLayout;
    uint16_t _numLEDs;
};
```

## メモリマップ

```
┌─────────────────────────────────────────────┐
│ ESP32-S3 Memory Map                         │
├─────────────────────────────────────────────┤
│ SRAM (512KB)                                │
│ ├─ Stack (Core 0): 8KB                      │
│ ├─ Stack (Core 1): 8KB                      │
│ ├─ Heap: ~400KB                             │
│ │  ├─ ledBuffer: 2.4KB                      │
│ │  ├─ ledLayout: 9.6KB                      │
│ │  └─ その他変数                            │
│ └─ WiFi/BT: ~100KB                          │
├─────────────────────────────────────────────┤
│ PSRAM (8MB)                                 │
│ ├─ Image Buffer A: 100KB                    │
│ ├─ Image Buffer B: 100KB                    │
│ ├─ UDP Buffer: 64KB                         │
│ ├─ JPEG Work Area: ~50KB (TJpg_Decoder)    │
│ └─ Free: ~7.7MB                             │
└─────────────────────────────────────────────┘
```

## 今後の最適化ポイント

1. **I2S DMA LED出力**
   - RMTよりも高速・低CPU負荷
   - 800 LEDs @ 30fps で安定動作

2. **IMU座標変換のGPU活用**
   - ESP32-S3にはGPUなし
   - ルックアップテーブルで高速化

3. **ダブルバッファの3重化**
   - 極端な場合: デコード中/スワップ待ち/描画中
   - メモリトレードオフ

4. **タスクWatchdog調整**
   - 長時間デコードでタイムアウトしないよう設定

## パフォーマンス目標

| 指標 | 目標値 | 実測値 (予想) |
|------|--------|---------------|
| フレームレート | 30 fps | 25-30 fps |
| デコード遅延 | < 30 ms | 15-25 ms |
| 描画遅延 | < 20 ms | 15-20 ms |
| Total遅延 | < 50 ms | 30-45 ms |
| CPU負荷 (Core 0) | < 80% | 60-70% |
| CPU負荷 (Core 1) | < 60% | 40-50% |
| メモリ使用量 | < 500KB | ~300KB |
