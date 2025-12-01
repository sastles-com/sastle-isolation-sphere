/**
 * @file LEDManager.h
 * @brief LED制御マネージャー - デュアルコアレンダリング対応
 * 
 * ESP32-S3のデュアルコアを活用し、Core 1で800個のLEDへの描画を行います。
 * ImageManagerから画像データを取得し、3D座標からUV座標へ変換してLEDに出力します。
 */

#ifndef LED_MANAGER_H
#define LED_MANAGER_H

#include <Arduino.h>
#include "ConfigManager.h"
#include "ImageManager.h"
#include "IMUManager.h"
#include <FastLED.h>

namespace sastle {

/**
 * @brief LED座標構造体
 * 
 * LEDの3D空間座標を保持します。
 */
struct LEDCoord {
    uint16_t faceID;      ///< 面ID
    uint8_t strip;        ///< ストリップ番号 (0-3)
    uint16_t stripNum;    ///< ストリップ内インデックス
    float x;              ///< X座標 (正規化済み)
    float y;              ///< Y座標 (正規化済み)
    float z;              ///< Z座標 (正規化済み)
};

/**
 * @brief LED統計情報
 */
struct LEDStats {
    uint32_t frames_rendered;    ///< 描画フレーム数
    uint32_t frames_dropped;     ///< ドロップフレーム数
    float fps;                   ///< 描画FPS
    uint32_t render_time_us;     ///< 最後のレンダリング時間 (μs)
    uint32_t mapping_time_us;    ///< 座標マッピング時間 (μs)
    uint32_t output_time_us;     ///< LED出力時間 (μs)
};

/**
 * @brief LEDマネージャークラス
 * 
 * ESP32-S3のCore 1でLED描画タスクを実行します。
 * ImageManagerから画像データを取得し、LEDレイアウトに基づいて
 * 3D座標→UV座標変換を行い、WS2812BストリップへDMA出力します。
 * 
 * @note このクラスはCore 1で動作するタスクを管理します
 */
class LEDManager {
public:
    LEDManager();
    ~LEDManager();
    
    /**
     * @brief LEDマネージャーを初期化
     * 
     * @param config 設定マネージャー
     * @param imageManager 画像マネージャー
     * @param imuManager IMUマネージャー（姿勢補正用、nullptrで無効化）
     * @return 初期化成功時true
     */
    bool begin(ConfigManager& config, ImageManager& imageManager, IMUManager* imuManager = nullptr);
    
    /**
     * @brief レンダリングタスクを開始
     * 
     * @param core 実行するコア番号 (デフォルト: 1)
     * @param priority タスク優先度 (デフォルト: 2)
     * @param stackSize スタックサイズ (デフォルト: 8192)
     * @return タスク起動成功時true
     */
    bool startRenderTask(uint8_t core = 1, uint8_t priority = 2, uint32_t stackSize = 8192);
    
    /**
     * @brief レンダリングタスクを停止
     */
    void stopRenderTask();
    
    /**
     * @brief 初期化状態を取得
     * 
     * @return 初期化済みならtrue
     */
    bool isInitialized() const { return _initialized; }
    
    /**
     * @brief タスク実行状態を取得
     * 
     * @return タスク実行中ならtrue
     */
    bool isRunning() const { return _taskRunning; }
    
    /**
     * @brief 統計情報を取得
     * 
     * @return LED統計情報
     */
    LEDStats getStats() const { return _stats; }
    
    /**
     * @brief 全LEDを指定色に設定
     * 
     * @param r 赤成分 (0-255)
     * @param g 緑成分 (0-255)
     * @param b 青成分 (0-255)
     */
    void fillSolid(uint8_t r, uint8_t g, uint8_t b);
    
    /**
     * @brief LEDを即座に更新
     * 
     * @note 通常はタスクが自動更新するため、手動呼び出しは不要
     */
    void show();
    
    /**
     * @brief 輝度を設定
     * 
     * @param brightness 輝度 (0-255)
     */
    void setBrightness(uint8_t brightness);
    
    /**
     * @brief IMU姿勢補正を有効化/無効化
     * 
     * @param enabled true: 有効, false: 無効
     */
    void setIMUCompensation(bool enabled);
    
    /**
     * @brief デバッグ情報を出力
     */
    void printStatus();
    
    /**
     * @brief フレーム準備完了通知コールバック (静的)
     * @note ImageManagerから呼ばれる
     */
    static void onFrameReady();
    
private:
    /**
     * @brief LEDレイアウトファイルを読み込み
     * 
     * @param path レイアウトファイルパス
     * @return 読み込み成功時true
     */
    bool loadLayout(const char* path);
    
    /**
     * @brief 3D座標をUV座標に変換
     * 
     * @param x X座標
     * @param y Y座標
     * @param z Z座標
     * @param u 出力U座標 (0.0-1.0)
     * @param v 出力V座標 (0.0-1.0)
     */
    void sphereToUV(float x, float y, float z, float& u, float& v);
    
    /**
     * @brief Quaternionで3D座標を回転
     * 
     * @param x X座標（入出力）
     * @param y Y座標（入出力）
     * @param z Z座標（入出力）
     * @param qw Quaternion w成分
     * @param qx Quaternion x成分
     * @param qy Quaternion y成分
     * @param qz Quaternion z成分
     */
    void rotateByQuaternion(float& x, float& y, float& z, float qw, float qx, float qy, float qz);
    
    /**
     * @brief レンダリングタスク関数 (static)
     * 
     * @param parameter LEDManagerインスタンスへのポインタ
     */
    static void renderTaskFunction(void* parameter);
    
    /**
     * @brief 1フレーム描画処理
     */
    void renderFrame();
    
    /**
     * @brief LEDバッファを更新
     */
    void updateLEDBuffer();
    
    /**
     * @brief 特定ストリップのLEDバッファを更新
     * @param stripIndex ストリップ番号 (0-3)
     */
    void updateStripBuffer(uint8_t stripIndex);
    
    /**
     * @brief 全ストリップのLED出力を並列実行 (DMA)
     * @note RMTハードウェアにより4ストリップが並列出力される
     */
    void showParallel();
    
    // メンバー変数
    bool _initialized;               ///< 初期化フラグ
    bool _taskRunning;               ///< タスク実行フラグ
    
    ConfigManager* _config;          ///< 設定マネージャーへのポインタ
    ImageManager* _imageManager;     ///< 画像マネージャーへのポインタ
    IMUManager* _imuManager;         ///< IMUマネージャーへのポインタ（姿勢補正用）
    
    bool _imuCompensationEnabled;    ///< IMU姿勢補正の有効化フラグ
    
    TaskHandle_t _renderTaskHandle;  ///< レンダリングタスクハンドル
    SemaphoreHandle_t _frameReadySemaphore;  ///< フレーム準備完了セマフォ
    
    CRGB* _ledBuffer;                ///< LEDバッファ (SRAM)
    LEDCoord* _ledLayout;            ///< LEDレイアウト (SRAM)
    uint16_t _numLEDs;               ///< LED総数
    
    uint8_t _stripPins[4];           ///< ストリップGPIOピン
    uint16_t _ledsPerStrip[4];       ///< ストリップ毎のLED数
    uint16_t _stripStartIndex[4];    ///< ストリップ開始インデックス
    CRGB* _stripBuffers[4];          ///< ストリップ毎のバッファポインタ
    
    LEDStats _stats;                 ///< 統計情報
    unsigned long _lastFPSUpdate;    ///< 最後のFPS更新時刻
    uint32_t _frameCount;            ///< フレームカウント
    
    static LEDManager* _instance;    ///< 静的インスタンスポインタ (コールバック用)
};

} // namespace sastle

#endif // LED_MANAGER_H
