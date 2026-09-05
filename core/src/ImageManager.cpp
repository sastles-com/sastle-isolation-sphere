/**
 * @file ImageManager.cpp
 * @brief ImageManager実装
 */

#include "ImageManager.h"

namespace sastle {

// 静的メンバー初期化
ImageManager* ImageManager::_instance = nullptr;

ImageManager::ImageManager()
    : _initialized(false),
      _network(nullptr),
      _width(320),
      _height(160),
      _bufferSize(0),
      _udpBuffer(nullptr),
      _udpBufferSize(MAX_UDP_IMAGE_SIZE),
      _framesReceived(0),
      _framesDecoded(0),
      _framesDropped(0),
      _decodeErrors(0),
      _lastFrameTime(0),
      _fpsTimestamp(0),
      _fpsFrameCount(0),
      _currentFPS(0.0f),
      _lastJpegSize(0),
      _tjpgTargetBuffer(nullptr) {
    _instance = this;
}

ImageManager::~ImageManager() {
    end();
    _instance = nullptr;
}

bool ImageManager::begin(ConfigManager& config, NetworkManager& network) {
    if (_initialized) {
        Serial.println("[ImageManager] Already initialized");
        return true;
    }
    
    _network = &network;
    
    // 画像サイズを設定から取得。JPEGは送信側の解像度(例 320x160)だが、
    // 800 LED にはそれほど要らないため setJpgScale で 1/_jpegScale に縮小デコードする
    // (デコード時間 ∝ 出力画素数 なので大幅に軽量化。帯域・送信側は変更不要)。
    ImageConfig imgConfig = config.getImageConfig();
    // デコード時間はエントロピー復号支配でスケール非依存(実測 scale1=scale2)。
    // 縮小しても速くならず律速でもないため、フル解像度(scale 1)で復号する。
    _jpegScale = 1;  // 320x160 をそのままデコード
    _width = imgConfig.width / _jpegScale;
    _height = imgConfig.height / _jpegScale;
    _bufferSize = _width * _height * sizeof(uint16_t); // RGB565

    Serial.printf("[ImageManager] Initializing: decode %dx%d (src %dx%d, scale 1/%d, RGB565)\n",
                  _width, _height, imgConfig.width, imgConfig.height, _jpegScale);
    Serial.printf("  Buffer size: %d bytes each\n", _bufferSize);
    Serial.printf("  Total memory: %d bytes\n", _bufferSize * 2 + _udpBufferSize);
    
    // PSRAMチェック
    if (!psramFound()) {
        Serial.println("[ImageManager] ERROR: PSRAM not found!");
        return false;
    }
    
    Serial.printf("  Free PSRAM: %d bytes\n", ESP.getFreePsram());
    
    // バッファ確保
    if (!allocateBuffers()) {
        Serial.println("[ImageManager] ERROR: Buffer allocation failed");
        return false;
    }
    
    // TJpg_Decoder初期化
    TJpgDec.setJpgScale(_jpegScale);  // 縮小デコード (1,2,4,8)
    // デコード結果は uint16_t バッファへ memcpy 格納し、getPixel() が
    // ネイティブ RGB565 (R=15..11, G=10..5, B=4..0) としてビット抽出する。
    // そのため TFT 転送用のバイトスワップは掛けない (true にすると保存値の
    // 上位/下位バイトが入れ替わり、読み出し時に色チャンネルが崩れて BGR 風になる)。
    TJpgDec.setSwapBytes(false);
    TJpgDec.setCallback(tjpgOutput);

    _initialized = true;
    _fpsTimestamp = millis();
    
    Serial.println("[ImageManager] Initialized successfully");
    
    return true;
}

void ImageManager::end() {
    if (!_initialized) {
        return;
    }
    
    freeBuffers();
    _initialized = false;
    
    Serial.println("[ImageManager] Deinitialized");
}

bool ImageManager::allocateBuffers() {
    // RGB565 トリプルバッファ
    if (!_pool.allocate(_bufferSize)) {
        Serial.println("[ImageManager] Failed to allocate frame buffers");
        return false;
    }

    // UDP受信(チャンク再構成)バッファ
    _udpBuffer = (uint8_t*)ps_malloc(_udpBufferSize);
    if (!_udpBuffer) {
        Serial.println("[ImageManager] Failed to allocate UDP buffer");
        _pool.freeAll();
        return false;
    }

    _displayBuffer = _pool.displayBuffer();
    _decodeBuffer = _pool.decodeBuffer();
    _reassembler.begin(_udpBuffer, _udpBufferSize);

    Serial.printf("[ImageManager] Buffers allocated. Free PSRAM: %d bytes\n", ESP.getFreePsram());
    return true;
}

void ImageManager::freeBuffers() {
    _pool.freeAll();
    if (_udpBuffer) {
        free(_udpBuffer);
        _udpBuffer = nullptr;
    }
    _displayBuffer = nullptr;
    _decodeBuffer = nullptr;
}

// 受信キューからチャンクを取り出して FrameReassembler に渡し、フレームが揃ったら
// デコードする(公開はしない)。再構成状態は _reassembler が保持し、複数回の呼び出しに
// 跨って継続する。
bool ImageManager::decodeOneFrame() {
    if (!_initialized || !_network) {
        return false;
    }

    bool frameComplete = false;
    size_t jpegSize = 0;
    int n;
    while ((n = _network->recvDatagram(_chunkBuf, sizeof(_chunkBuf))) > 0) {
        _parseHits++;
        if (_reassembler.addChunk(_chunkBuf, (size_t)n, jpegSize)) {
            frameComplete = true;
            break;  // 残りチャンクは次フレーム分。今のフレームを先にデコード。
        }
    }

    if (!frameComplete) {
        return false;
    }
    _lastJpegSize = jpegSize;
    if (!decodeJPEG(_udpBuffer, jpegSize)) {
        _decodeErrors++;
        return false;
    }
    _framesReceived++;
    _lastFrameTime = millis();
    return true;
}

void ImageManager::decodeTaskFunc(void* param) {
    ImageManager* self = static_cast<ImageManager*>(param);
    Serial.printf("[ImageManager] Decode task started on core %d\n", xPortGetCoreID());
    for (;;) {
        if (self->decodeOneFrame()) {
            // 完成フレームを ready に公開 (トリプルバッファ)。renderは連続駆動で
            // 次パスに adoptReadyFrame() で採用する。ハンドシェイク不要。
            self->publishFrame();
            self->_framesDecoded++;
            self->_fpsFrameCount++;
            self->calculateFPS();
            // 連続受信中も 1 tick 譲る。デコードは 1 フレーム 50〜100ms で 15fps だと
            // core0 を 80〜100% 占有し、アイドルタスクが 5 秒走れずに TASK_WDT で
            // リセットした (2026-09-06 実測、重い JPEG 20KB のシーンで頻発)。
            // 毎フレーム 1ms 譲れば WDT は構造的に防げ、コストは 1.5% 以下。
            vTaskDelay(1);
        } else {
            vTaskDelay(1);  // フレーム未受信時は譲る
        }
    }
}

bool ImageManager::startDecodeTask(uint8_t core, uint8_t priority, uint32_t stackSize) {
    if (_decodeTaskHandle) {
        return true;
    }
    BaseType_t r = xTaskCreatePinnedToCore(
        decodeTaskFunc, "img_decode", stackSize, this, priority, &_decodeTaskHandle, core);
    if (r != pdPASS) {
        Serial.println("[ImageManager] Failed to create decode task");
        return false;
    }
    Serial.printf("[ImageManager] Decode task pinned to core %d (prio %d)\n", core, priority);
    return true;
}

// render側(毎パス): 表示待ち完成フレームがあれば表示バッファに採用する。
// render側: 表示待ちの完成フレームがあれば display に採用し、_displayBuffer を更新する。
void ImageManager::adoptReadyFrame() {
    _pool.adopt();
    _displayBuffer = _pool.displayBuffer();
}


bool ImageManager::decodeJPEG(const uint8_t* jpeg_data, size_t jpeg_size) {
    // デコードターゲットバッファを設定
    _tjpgTargetBuffer = _decodeBuffer;
    
    // JPEGデコード実行
    uint16_t w = 0, h = 0;
    if (TJpgDec.getJpgSize(&w, &h, jpeg_data, jpeg_size) != JDR_OK) {
        Serial.println("[ImageManager] Failed to get JPEG size");
        return false;
    }
    
    // サイズ検証 (getJpgSize は元JPEGの解像度を返す。縮小デコードするので元寸で比較)
    if (w != _width * _jpegScale || h != _height * _jpegScale) {
        Serial.printf("[ImageManager] Size mismatch: expected %dx%d, got %dx%d\n",
                     _width * _jpegScale, _height * _jpegScale, w, h);
        return false;
    }
    
    // デコード実行 (所要時間を計測)
    unsigned long decodeStart = micros();
    if (TJpgDec.drawJpg(0, 0, jpeg_data, jpeg_size) != JDR_OK) {
        Serial.println("[ImageManager] JPEG decode failed");
        return false;
    }
    _lastDecodeUs = (uint32_t)(micros() - decodeStart);

    return true;
}

// decode側: 完成フレームを ready に公開し、次の書込先(_decodeBuffer)を更新する。
void ImageManager::publishFrame() {
    bool dropped = _pool.publish();
    _decodeBuffer = _pool.decodeBuffer();
    if (dropped) {
        _framesDropped++;  // render が追いつかず表示前に破棄
    }
}

void ImageManager::calculateFPS() {
    unsigned long now = millis();
    unsigned long elapsed = now - _fpsTimestamp;
    
    // 1秒ごとにFPS計算
    if (elapsed >= 1000) {
        _currentFPS = (_fpsFrameCount * 1000.0f) / elapsed;
        _fpsFrameCount = 0;
        _fpsTimestamp = now;
    }
}

bool ImageManager::getPixel(uint16_t x, uint16_t y, uint8_t& r, uint8_t& g, uint8_t& b) {
    if (!_initialized || !_displayBuffer) {
        return false;
    }
    
    if (x >= _width || y >= _height) {
        return false;
    }
    
    // RGB565を取得
    uint16_t rgb565 = _displayBuffer[y * _width + x];
    
    // RGB565 -> RGB888変換
    r = ((rgb565 >> 11) & 0x1F) << 3;  // R: 5bit -> 8bit
    g = ((rgb565 >> 5) & 0x3F) << 2;   // G: 6bit -> 8bit
    b = (rgb565 & 0x1F) << 3;          // B: 5bit -> 8bit
    
    return true;
}


ImageStats ImageManager::getStats() const {
    ImageStats stats;
    stats.frames_received = _framesReceived;
    stats.frames_decoded = _framesDecoded;
    stats.frames_dropped = _framesDropped;
    stats.decode_errors = _decodeErrors;
    stats.fps = _currentFPS;
    stats.last_frame_time = _lastFrameTime;
    stats.last_jpeg_size = _lastJpegSize;
    stats.decode_time_us = _lastDecodeUs;
    return stats;
}

void ImageManager::printStats() {
    Serial.println("\n=== ImageManager Statistics ===");
    Serial.printf("Frames Received: %u\n", _framesReceived);
    Serial.printf("Frames Decoded:  %u\n", _framesDecoded);
    Serial.printf("Frames Dropped:  %u\n", _framesDropped);
    Serial.printf("Decode Errors:   %u\n", _decodeErrors);
    Serial.printf("Current FPS:     %.2f\n", _currentFPS);
    Serial.printf("Last JPEG Size:  %zu bytes\n", _lastJpegSize);
    Serial.printf("Last Frame:      %lu ms ago\n", millis() - _lastFrameTime);
    Serial.printf("Buffer Size:     %zu bytes x2\n", _bufferSize);
    Serial.printf("Free PSRAM:      %d bytes\n", ESP.getFreePsram());
}

// TJpg_Decoderコールバック (静的)
bool ImageManager::tjpgOutput(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
    if (!_instance || !_instance->_tjpgTargetBuffer) {
        return false;
    }
    
    // ビットマップをターゲットバッファにコピー
    uint16_t* target = _instance->_tjpgTargetBuffer;
    uint16_t width = _instance->_width;
    
    for (uint16_t row = 0; row < h; row++) {
        uint16_t* dest = target + ((y + row) * width + x);
        uint16_t* src = bitmap + (row * w);
        memcpy(dest, src, w * sizeof(uint16_t));
    }
    
    return true;  // 継続
}

} // namespace sastle
