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
    _jpegScale = 2;  // 320x160 → 160x80
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

bool ImageManager::update() {
    if (!_initialized || !_network) {
        return false;
    }
    
    size_t jpeg_size = 0;

    // 溜まっているUDPデータグラムを全てドレインし、最新フレームのみ採用する。
    // (バックログを残すと遅延が増えるだけ。受信キュー詰まりの回避にもなる)
    bool got = false;
    size_t latest_size = 0;
    while (receivePacket(jpeg_size)) {
        got = true;
        latest_size = jpeg_size;
    }
    if (!got) {
        return false;
    }
    jpeg_size = latest_size;  // _udpBuffer には最後のデータグラムが入っている

    // JPEG検証済みなので、ヘッダー分スキップしてデコード
    const uint8_t* jpeg_data = _udpBuffer + sizeof(UDPImageHeader);
    size_t jpeg_data_size = jpeg_size - sizeof(UDPImageHeader);
    
    // JPEGデコード
    if (!decodeJPEG(jpeg_data, jpeg_data_size)) {
        _decodeErrors++;
        return false;
    }
    
    // デコード成功 → バッファスワップ
    swapBuffers();
    _framesDecoded++;
    _fpsFrameCount++;
    
    calculateFPS();
    
    return true;
}

bool ImageManager::receivePacket(size_t& jpeg_size) {
    // UDPパケット受信チェック
    int packetSize = _network->parsePacket();
    if (packetSize <= 0) {
        return false;
    }
    _parseHits++;  // parsePacket が >0 を返した回数 (受信診断用)

    // パケットサイズチェック
    if (packetSize < (int)sizeof(UDPImageHeader)) {
        Serial.printf("[ImageManager] Packet too small: %d bytes\n", packetSize);
        _framesDropped++;
        return false;
    }
    
    if (packetSize > (int)_udpBufferSize) {
        Serial.printf("[ImageManager] Packet too large: %d bytes\n", packetSize);
        _framesDropped++;
        return false;
    }
    
    // パケット読み込み
    int bytesRead = _network->read(_udpBuffer, packetSize);
    if (bytesRead != packetSize) {
        Serial.printf("[ImageManager] Read error: expected %d, got %d\n", packetSize, bytesRead);
        _framesDropped++;
        return false;
    }
    
    // ヘッダー検証
    UDPImageHeader* header = (UDPImageHeader*)_udpBuffer;
    if (header->magic != UDP_IMAGE_MAGIC) {
        Serial.printf("[ImageManager] Invalid magic: 0x%08X\n", header->magic);
        _framesDropped++;
        return false;
    }
    
    _framesReceived++;
    _lastFrameTime = millis();
    _lastJpegSize = packetSize - sizeof(UDPImageHeader);
    jpeg_size = packetSize;
    
    if (_framesReceived % 100 == 0) {
        Serial.printf("[ImageManager] Frame #%d received (JPEG: %d bytes)\n", 
                     header->frame_id, _lastJpegSize);
    }
    
    return true;
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
