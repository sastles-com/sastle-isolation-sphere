/**
 * @file ImageManager.h
 * @brief UDP JPEG画像受信・デコード・ダブルバッファ管理クラス
 * @author sastle-com
 * @date 2025-12-01
 */

#pragma once

#include <Arduino.h>
#include <TJpg_Decoder.h>
#include "ConfigManager.h"
#include "NetworkManager.h"

namespace sastle {

/// フレーム準備完了コールバック型
typedef void (*FrameReadyCallback)(void);

/// UDP画像パケットヘッダー
struct UDPImageHeader {
    uint32_t magic;      ///< マジックナンバー 0x4A504547 ("JPEG")
    uint32_t frame_id;   ///< フレーム連番
} __attribute__((packed));

/// マジックナンバー "JPEG"
constexpr uint32_t UDP_IMAGE_MAGIC = 0x4A504547;

/// 最大UDPペイロードサイズ (64KB)
constexpr size_t MAX_UDP_IMAGE_SIZE = 65507;

/**
 * @struct ImageStats
 * @brief 画像処理統計情報
 */
struct ImageStats {
    uint32_t frames_received;    ///< 受信フレーム数
    uint32_t frames_decoded;     ///< デコード成功フレーム数
    uint32_t frames_dropped;     ///< ドロップフレーム数
    uint32_t decode_errors;      ///< デコードエラー数
    float fps;                   ///< 現在のFPS
    unsigned long last_frame_time; ///< 最終フレーム受信時刻
    size_t last_jpeg_size;       ///< 最終JPEG画像サイズ
    uint32_t decode_time_us;     ///< 最後のJPEGデコード所要時間 (μs)
};

/**
 * @class ImageManager
 * @brief UDP JPEG画像の受信・デコード・ダブルバッファ管理クラス
 * 
 * UDP経由で受信したJPEG圧縮画像をデコードし、
 * ダブルバッファでLED描画用RGB565データを提供します。
 * PSRAM上に2つのバッファを確保し、非ブロッキングで画像更新を行います。
 */
class ImageManager {
public:
    ImageManager();
    ~ImageManager();
    
    /**
     * @brief 画像マネージャーを初期化
     * @param config 設定マネージャー参照
     * @param network ネットワークマネージャー参照
     * @return true 初期化成功, false 初期化失敗
     */
    bool begin(ConfigManager& config, NetworkManager& network);
    
    /**
     * @brief リソースを解放
     */
    void end();
    
    /**
     * @brief UDP画像受信とデコードを実行
     * @return true 新しいフレームをデコード, false フレームなし/エラー
     * @note メインループ内で定期的に呼び出す
     */
    bool update();
    
    /**
     * @brief 指定座標のピクセル色を取得 (RGB)
     * @param x X座標 (0 ~ width-1)
     * @param y Y座標 (0 ~ height-1)
     * @param r 赤成分 (0-255) (出力)
     * @param g 緑成分 (0-255) (出力)
     * @param b 青成分 (0-255) (出力)
     * @return true 取得成功, false 座標範囲外
     */
    bool getPixel(uint16_t x, uint16_t y, uint8_t& r, uint8_t& g, uint8_t& b);
    
    /**
     * @brief 指定座標のピクセル色を取得 (RGB565)
     * @param x X座標 (0 ~ width-1)
     * @param y Y座標 (0 ~ height-1)
     * @return RGB565形式のピクセル値, 範囲外の場合は0
     */
    uint16_t getPixelRGB565(uint16_t x, uint16_t y);
    
    /**
     * @brief 画像幅を取得
     * @return 画像幅 (ピクセル)
     */
    uint16_t getWidth() const { return _width; }
    
    /**
     * @brief 画像高さを取得
     * @return 画像高さ (ピクセル)
     */
    uint16_t getHeight() const { return _height; }
    
    /**
     * @brief 統計情報を取得
     * @return ImageStats構造体
     */
    ImageStats getStats() const;
    
    /**
     * @brief 初期化状態を取得
     * @return true 初期化済み, false 未初期化
     */
    bool isInitialized() const { return _initialized; }
    
    /**
     * @brief フレーム準備完了コールバックを設定
     * @param callback コールバック関数
     * @note デコード完了後、バッファスワップ時に呼び出されます
     */
    void setFrameReadyCallback(FrameReadyCallback callback) { _frameReadyCallback = callback; }
    
    /**
     * @brief 統計情報を表示
     */
    void printStats();
    
private:
    bool _initialized;           ///< 初期化状態
    
    NetworkManager* _network;    ///< ネットワークマネージャーポインタ
    
    // 画像パラメータ
    uint16_t _width;             ///< 画像幅
    uint16_t _height;            ///< 画像高さ
    size_t _bufferSize;          ///< 1バッファのサイズ (bytes)
    
    // ダブルバッファ (PSRAM)
    uint16_t* _bufferA;          ///< RGB565バッファA
    uint16_t* _bufferB;          ///< RGB565バッファB
    uint16_t* _drawBuffer;       ///< 現在の描画用バッファ (読み取り専用)
    uint16_t* _decodeBuffer;     ///< 現在のデコード用バッファ (書き込み)
    
    // UDP受信バッファ (PSRAM)
    uint8_t* _udpBuffer;         ///< JPEG受信バッファ
    size_t _udpBufferSize;       ///< UDPバッファサイズ
    
    // 統計情報
    uint32_t _framesReceived;    ///< 受信フレーム数
    uint32_t _framesDecoded;     ///< デコード成功数
    uint32_t _framesDropped;     ///< ドロップ数
    uint32_t _decodeErrors;      ///< デコードエラー数
    unsigned long _lastFrameTime; ///< 最終フレーム時刻
    unsigned long _fpsTimestamp;  ///< FPS計測開始時刻
    uint32_t _fpsFrameCount;     ///< FPS計測用フレーム数
    float _currentFPS;           ///< 現在のFPS
    size_t _lastJpegSize;        ///< 最終JPEGサイズ
    uint32_t _lastDecodeUs = 0;  ///< 最後のJPEGデコード所要時間 (μs)

    FrameReadyCallback _frameReadyCallback;  ///< フレーム準備完了コールバック
    
    /**
     * @brief PSRAMにバッファを確保
     * @return true 確保成功, false 確保失敗
     */
    bool allocateBuffers();
    
    /**
     * @brief バッファを解放
     */
    void freeBuffers();
    
    /**
     * @brief UDP画像パケットを受信
     * @param jpeg_size 受信したJPEGサイズ (出力)
     * @return true 受信成功, false 受信なし/エラー
     */
    bool receivePacket(size_t& jpeg_size);
    
    /**
     * @brief JPEGをデコードしてバッファに展開
     * @param jpeg_data JPEGデータポインタ
     * @param jpeg_size JPEGデータサイズ
     * @return true デコード成功, false デコード失敗
     */
    bool decodeJPEG(const uint8_t* jpeg_data, size_t jpeg_size);
    
    /**
     * @brief バッファをスワップ (描画⇔デコード)
     */
    void swapBuffers();
    
    /**
     * @brief FPSを計算
     */
    void calculateFPS();
    
    /**
     * @brief TJpg_Decoder出力コールバック (静的)
     * @param x X座標
     * @param y Y座標
     * @param w 幅
     * @param h 高さ
     * @param bitmap RGB565ビットマップデータ
     * @return true 継続, false 中断
     */
    static bool tjpgOutput(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap);
    
    /**
     * @brief インスタンスポインタ (TJpg_Decoderコールバック用)
     */
    static ImageManager* _instance;
    
    /**
     * @brief TJpg_Decoder実行時のターゲットバッファ
     */
    uint16_t* _tjpgTargetBuffer;
};

} // namespace sastle
