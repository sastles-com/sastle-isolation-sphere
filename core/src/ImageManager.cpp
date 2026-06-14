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
      _bufferA(nullptr),
      _bufferB(nullptr),
      _drawBuffer(nullptr),
      _decodeBuffer(nullptr),
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
      _tjpgTargetBuffer(nullptr),
      _frameReadyCallback(nullptr) {
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
    TJpgDec.setSwapBytes(true);  // RGB565バイトスワップ
    TJpgDec.setCallback(tjpgOutput);

    // バッファ解放セマフォ (decode∥render のハンドシェイク用)。初期はバッファ空き。
    _bufferFreeSem = xSemaphoreCreateBinary();
    if (_bufferFreeSem) {
        xSemaphoreGive(_bufferFreeSem);
    }

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
    // RGB565ダブルバッファをPSRAMに確保
    _bufferA = (uint16_t*)ps_malloc(_bufferSize);
    if (!_bufferA) {
        Serial.println("[ImageManager] Failed to allocate buffer A");
        return false;
    }
    memset(_bufferA, 0, _bufferSize);
    
    _bufferB = (uint16_t*)ps_malloc(_bufferSize);
    if (!_bufferB) {
        Serial.println("[ImageManager] Failed to allocate buffer B");
        free(_bufferA);
        _bufferA = nullptr;
        return false;
    }
    memset(_bufferB, 0, _bufferSize);
    
    // UDP受信バッファをPSRAMに確保
    _udpBuffer = (uint8_t*)ps_malloc(_udpBufferSize);
    if (!_udpBuffer) {
        Serial.println("[ImageManager] Failed to allocate UDP buffer");
        free(_bufferA);
        free(_bufferB);
        _bufferA = nullptr;
        _bufferB = nullptr;
        return false;
    }
    
    // 初期状態: A=描画, B=デコード
    _drawBuffer = _bufferA;
    _decodeBuffer = _bufferB;
    
    Serial.printf("[ImageManager] Buffers allocated successfully\n");
    Serial.printf("  Buffer A: %p\n", _bufferA);
    Serial.printf("  Buffer B: %p\n", _bufferB);
    Serial.printf("  UDP Buffer: %p\n", _udpBuffer);
    Serial.printf("  Free PSRAM: %d bytes\n", ESP.getFreePsram());
    
    return true;
}

void ImageManager::freeBuffers() {
    if (_bufferA) {
        free(_bufferA);
        _bufferA = nullptr;
    }
    if (_bufferB) {
        free(_bufferB);
        _bufferB = nullptr;
    }
    if (_udpBuffer) {
        free(_udpBuffer);
        _udpBuffer = nullptr;
    }
    _drawBuffer = nullptr;
    _decodeBuffer = nullptr;
}

// 受信キューからチャンクを取り出してフレームを再構成し、揃ったらデコードする。
// (バッファswapはしない)。1フレーム完成で true。再構成状態はメンバに保持し
// 複数回の呼び出しに跨って継続する。
bool ImageManager::decodeOneFrame() {
    if (!_initialized || !_network) {
        return false;
    }

    bool frameComplete = false;
    int n;
    while ((n = _network->recvDatagram(_chunkBuf, sizeof(_chunkBuf))) > 0) {
        _parseHits++;
        if (n < (int)sizeof(UDPChunkHeader)) {
            continue;
        }
        const UDPChunkHeader* h = (const UDPChunkHeader*)_chunkBuf;
        if (h->magic != UDP_IMAGE_MAGIC) {
            continue;
        }
        if (h->chunk_count == 0 || h->chunk_count > MAX_CHUNKS ||
            h->chunk_index >= h->chunk_count) {
            continue;
        }
        size_t dataLen = h->chunk_size;
        if (dataLen > MAX_CHUNK_DATA || sizeof(UDPChunkHeader) + dataLen > (size_t)n) {
            continue;
        }

        // 新しいフレームの開始 (frame_id が変化)
        if (h->frame_id != _reasmFrameId) {
            if (_reasmFrameId != 0xFFFFFFFF && _reasmReceived < _reasmChunkCount) {
                _framesDropped++;  // 前フレームは未完のまま破棄
            }
            _reasmFrameId = h->frame_id;
            _reasmChunkCount = h->chunk_count;
            _reasmReceived = 0;
            _reasmTotalSize = 0;
            _reasmGotMask = 0;
        }

        size_t off = (size_t)h->chunk_index * MAX_CHUNK_DATA;
        if (off + dataLen > _udpBufferSize) {
            continue;
        }
        uint64_t bit = 1ULL << h->chunk_index;
        if (!(_reasmGotMask & bit)) {
            memcpy(_udpBuffer + off, _chunkBuf + sizeof(UDPChunkHeader), dataLen);
            _reasmGotMask |= bit;
            _reasmReceived++;
            if (h->chunk_index == h->chunk_count - 1) {
                _reasmTotalSize = off + dataLen;  // 最終チャンクで全体サイズ確定
            }
        }

        if (_reasmReceived == _reasmChunkCount && _reasmTotalSize > 0) {
            // フレーム完成。_udpBuffer[0.._reasmTotalSize] が完全なJPEG。
            _lastJpegSize = _reasmTotalSize;
            _reasmFrameId = 0xFFFFFFFF;  // 完了 → 次フレーム待ち
            frameComplete = true;
            break;  // 残りチャンクは次フレーム分。今のフレームを先にデコード。
        }
    }

    if (!frameComplete) {
        return false;
    }
    if (!decodeJPEG(_udpBuffer, _lastJpegSize)) {
        _decodeErrors++;
        return false;
    }
    _framesReceived++;
    _lastFrameTime = millis();
    return true;
}

// 後方互換: 単一スレッドでの受信→デコード→swap (現在はデコードタスク経由で未使用)
bool ImageManager::update() {
    if (!decodeOneFrame()) {
        return false;
    }
    swapBuffers();
    _framesDecoded++;
    _fpsFrameCount++;
    calculateFPS();
    return true;
}

void ImageManager::decodeTaskFunc(void* param) {
    ImageManager* self = static_cast<ImageManager*>(param);
    Serial.printf("[ImageManager] Decode task started on core %d\n", xPortGetCoreID());
    for (;;) {
        if (self->decodeOneFrame()) {
            // renderが前フレームを描き終える(releaseBuffer)まで待ってから swap。
            // ダブルバッファなのでこれでテアリングなくパイプライン化される。
            xSemaphoreTake(self->_bufferFreeSem, portMAX_DELAY);
            self->swapBuffers();   // ポインタ交換 + frameReadyCallback(render起動)
            self->_framesDecoded++;
            self->_fpsFrameCount++;
            self->calculateFPS();
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

void ImageManager::releaseBuffer() {
    if (_bufferFreeSem) {
        xSemaphoreGive(_bufferFreeSem);
    }
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

void ImageManager::swapBuffers() {
    // ポインタ交換のみ（高速）
    uint16_t* temp = _drawBuffer;
    _drawBuffer = _decodeBuffer;
    _decodeBuffer = temp;
    
    // フレーム準備完了コールバックを呼び出す
    if (_frameReadyCallback) {
        _frameReadyCallback();
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
    if (!_initialized || !_drawBuffer) {
        return false;
    }
    
    if (x >= _width || y >= _height) {
        return false;
    }
    
    // RGB565を取得
    uint16_t rgb565 = _drawBuffer[y * _width + x];
    
    // RGB565 -> RGB888変換
    r = ((rgb565 >> 11) & 0x1F) << 3;  // R: 5bit -> 8bit
    g = ((rgb565 >> 5) & 0x3F) << 2;   // G: 6bit -> 8bit
    b = (rgb565 & 0x1F) << 3;          // B: 5bit -> 8bit
    
    return true;
}

uint16_t ImageManager::getPixelRGB565(uint16_t x, uint16_t y) {
    if (!_initialized || !_drawBuffer) {
        return 0;
    }
    
    if (x >= _width || y >= _height) {
        return 0;
    }
    
    return _drawBuffer[y * _width + x];
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
