> [English](image_manager_design.md) · **日本語**

# ImageManager クラス設計

## 概要
UDP経由で受信したJPEG画像をデコードし、ダブルバッファでLED描画用データを提供するクラス。

## 要件
1. UDP経由でJPEG圧縮画像を受信
2. ダブルバッファによる非ブロッキング画像更新
3. ピクセル座標指定でRGB値を取得
4. PSRAM活用による大容量バッファ
5. 将来: ファイルシステムからのJPEG読み込みサポート

## クラス構造

```cpp
class ImageManager {
public:
    // 初期化
    bool begin(ConfigManager& config, NetworkManager& network);
    
    // UDP画像受信・デコード (非ブロッキング)
    bool receiveAndDecode();
    
    // バッファスワップ (デコード完了時)
    void swapBuffers();
    
    // ピクセルアクセス (描画バッファから)
    bool getPixel(uint16_t x, uint16_t y, uint8_t& r, uint8_t& g, uint8_t& b);
    uint16_t getPixelRGB565(uint16_t x, uint16_t y);
    
    // ファイルからJPEG読み込み (将来拡張)
    bool loadFromFile(const char* path);
    
    // 統計情報
    uint32_t getFrameCount();
    uint32_t getDroppedFrames();
    float getFPS();
    
private:
    // ダブルバッファ (PSRAM)
    uint16_t* _bufferA;      // RGB565バッファA
    uint16_t* _bufferB;      // RGB565バッファB
    uint16_t* _drawBuffer;   // 描画用 (読み取り専用)
    uint16_t* _decodeBuffer; // デコード用 (書き込み)
    
    // 画像パラメータ
    uint16_t _width;
    uint16_t _height;
    size_t _bufferSize;
    
    // UDP受信バッファ (PSRAM)
    uint8_t* _udpBuffer;
    size_t _udpBufferSize;
    
    // TJpg_Decoderコールバック
    static bool jpegOutput(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap);
    
    // 統計
    uint32_t _frameCount;
    uint32_t _droppedFrames;
    unsigned long _lastFrameTime;
};
```

## メモリ配置

```
PSRAM (8MB)
├─ Buffer A (320x160x2 = 100KB)
├─ Buffer B (320x160x2 = 100KB)
├─ UDP Buffer (最大64KB)
└─ JPEG作業領域 (TJpg_Decoder内部)
```

## データフロー

```
UDP Packet (JPEG)
    ↓
UDP Buffer (PSRAM)
    ↓
TJpg_Decoder
    ↓
Decode Buffer (Buffer A or B)
    ↓ swapBuffers()
Draw Buffer (Buffer B or A)
    ↓ getPixel()
LED Mapping
```

## UDP画像プロトコル

### シングルパケット方式 (推奨)
```
[Header 4 bytes][JPEG Data N bytes]
Header:
  - Magic: 0x4A50 (JP)
  - Width: uint16_t
  - Height: uint16_t (実装では無視、config.jsonから取得)
```

### マルチパケット方式 (将来拡張)
大きな画像を複数パケットに分割して送信
```
[Header][Sequence][Fragment][JPEG Data]
```

## TJpg_Decoder 統合

### platformio.ini
```ini
lib_deps = 
    bodmer/TJpg_Decoder @ ^1.0.8
```

### 初期化
```cpp
TJpgDec.setJpgScale(1);  // 1, 2, 4, 8
TJpgDec.setSwapBytes(true);  // RGB565のバイト順
TJpgDec.setCallback(jpegOutput);
```

### デコード
```cpp
TJpgDec.drawJpg(0, 0, _udpBuffer, _udpDataSize);
```

## パフォーマンス目標

- デコード速度: 30fps以上 (320x160 JPEG)
- バッファスワップ: < 1ms (ポインタ交換のみ)
- ピクセルアクセス: < 1μs

## 実装フェーズ

### Phase 1: 基本実装
- ダブルバッファ確保
- TJpg_Decoderセットアップ
- UDP受信からデコードまでの基本フロー
- getPixel() 実装

### Phase 2: 最適化
- フレームドロップ検出
- FPS計測
- エラーハンドリング

### Phase 3: 拡張機能
- ファイルシステムからのJPEG読み込み
- マルチパケットJPEG対応
- 画像回転・スケーリング
