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

/// UDP映像チャンクヘッダー (16B)。1フレームのJPEGを複数チャンクに分割して送る
/// (UDP断片化を避けるため各チャンクはMTU内)。サーバー側も同形式で送出すること。
struct UDPChunkHeader {
    uint32_t magic;        ///< マジックナンバー 0x4A504547 ("JPEG")
    uint32_t frame_id;     ///< フレーム連番
    uint16_t chunk_index;  ///< チャンク番号 (0..chunk_count-1)
    uint16_t chunk_count;  ///< このフレームの総チャンク数
    uint16_t chunk_size;   ///< このチャンクのJPEGバイト数
    uint16_t reserved;     ///< 予約 (アライン)
} __attribute__((packed));

/// マジックナンバー "JPEG"
constexpr uint32_t UDP_IMAGE_MAGIC = 0x4A504547;

/// 最大UDPペイロードサイズ (64KB)
constexpr size_t MAX_UDP_IMAGE_SIZE = 65507;

/// 1チャンクのJPEGデータ最大長 (MTU内: 1500 - IP/UDP - 16Bヘッダ に余裕)
constexpr size_t MAX_CHUNK_DATA = 1400;
/// 1フレームの最大チャンク数 (_udpBuffer 65507 / 1400 ≒ 46)
constexpr uint16_t MAX_CHUNKS = 46;

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
     * @brief デコード専用タスクを開始 (Core分離による並列化)
     * @param core 実行コア (推奨: 0 = WiFi/lwIP と同居だがCPU処理なので可)
     * @note レンダリングタスク(別コア)と並列に動作させ、decode∥render を実現する。
     *       render が Core1, decode が Core0 で走り、ダブルバッファ + bufferFree
     *       セマフォでテアリングなくパイプライン化する。
     */
    bool startDecodeTask(uint8_t core = 0, uint8_t priority = 1, uint32_t stackSize = 8192);

    /**
     * @brief 表示待ちの完成フレームがあれば表示バッファに採用する (render側が毎パス呼ぶ)
     * @note これによりレンダリングは連続駆動(IMU再マッピング)しつつ、フレーム差し替えを
     *       独立に行える。新フレームが無ければ現在の表示バッファを維持。
     */
    void adoptReadyFrame();

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
    
    /**
     * @brief 統計情報を表示
     */
    void printStats();
    
private:
    bool _initialized;           ///< 初期化状態
    
    NetworkManager* _network;    ///< ネットワークマネージャーポインタ
    
    // 画像パラメータ
    uint16_t _width;             ///< デコード後の画像幅 (= src幅/_jpegScale)
    uint16_t _height;            ///< デコード後の画像高さ
    uint8_t _jpegScale = 2;      ///< 縮小デコード倍率 (1,2,4,8)
    size_t _bufferSize;          ///< 1バッファのサイズ (bytes)
    
    // トリプルバッファ (PSRAM)。描画(IMU再マッピング)を連続駆動しつつ、
    // フレーム差し替えを独立に行うため3枚使う(テアリング防止)。
    //   display = render が表示中 / ready = 表示待ちの完成フレーム / decode = 書込中
    // インデックス操作のみ _bufMux で排他(画素コピーはしないので一瞬)。
    uint16_t* _buf[3] = {nullptr, nullptr, nullptr};
    volatile int _displayIdx = 0;        ///< 表示中バッファ
    volatile int _readyIdx = -1;         ///< 表示待ち完成フレーム (-1=なし)
    volatile int _decodeIdx = 1;         ///< デコード書込中バッファ
    uint16_t* _displayBuffer = nullptr;  ///< getPixel が読む (= _buf[_displayIdx])
    uint16_t* _decodeBuffer = nullptr;   ///< tjpgが書く (= _buf[_decodeIdx])
    portMUX_TYPE _bufMux = portMUX_INITIALIZER_UNLOCKED;
    
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

public:
    uint32_t getParseHits() const { return _parseHits; }    ///< parsePacket>0 の回数 (UDP受信診断)
    uint32_t getDropped() const { return _framesDropped; }  ///< ドロップ数
private:
    uint32_t _parseHits = 0;     ///< parsePacket が >0 を返した累計


    // --- デコード並列化 (Core分離) ---
    TaskHandle_t _decodeTaskHandle = nullptr;     ///< デコードタスク
    bool decodeOneFrame();                        ///< 受信+再構成+デコード(swapはしない)
    static void decodeTaskFunc(void* param);      ///< デコードタスク本体

    // --- チャンク再構成 (decodeタスク専用なのでロック不要) ---
    uint8_t _chunkBuf[1500];                      ///< 受信データグラム取り出し用
    uint32_t _reasmFrameId = 0xFFFFFFFF;          ///< 再構成中フレームID
    uint16_t _reasmChunkCount = 0;                ///< 総チャンク数
    uint16_t _reasmReceived = 0;                  ///< 受信済みチャンク数
    uint32_t _reasmTotalSize = 0;                 ///< 完成JPEGサイズ
    uint64_t _reasmGotMask = 0;                   ///< 受信済みビットマスク (最大64ch)
    
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
    void publishFrame();  ///< decode完成フレームを ready に公開 (トリプルバッファ)
    
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
