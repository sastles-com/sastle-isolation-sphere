/**
 * @file IMUManager.h
 * @brief IMUセンサー管理クラス (BNO055 / M5Atom 内蔵 BMI270 をビルド時に切替)
 * @author sastle-com
 * @date 2025-12-01
 *
 * バックエンドは build_flags の -D で選択する:
 *   -D IMU_SENSOR_BNO055  : 外部 BNO055 (9軸オンチップ融合, NDOF) ※既定
 *   -D IMU_SENSOR_M5IMU   : M5Atom 内蔵 BMI270 (6軸) + ソフト Madgwick 融合
 * いずれの場合も公開 API (getQuaternion/getEuler/getAccel/getGyro) は同一で、
 * 戻り値型 imu::Quaternion / imu::Vector<3> も共通 (imumaths.h はヘッダオンリ)。
 */

#pragma once

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <utility/imumaths.h>
#include "ConfigManager.h"
#include "BoardConfig.h"

// --- バックエンド選択 ---
//   既定は BNO055。build_flags に -D IMU_SENSOR_M5IMU を足すと、(BNO055 が同時に
//   定義されていても) M5内蔵IMU を優先する。これにより M5IMU 用 env は通常 env を
//   extends したうえで -D IMU_SENSOR_M5IMU を1行足すだけで切替できる。
#if !defined(IMU_SENSOR_BNO055) && !defined(IMU_SENSOR_M5IMU)
#define IMU_SENSOR_BNO055
#endif

#if defined(IMU_SENSOR_M5IMU)
#include <M5Unified.h>
#include "MadgwickAHRS.h"
#else
#include <Adafruit_BNO055.h>
#endif

namespace sastle {

/**
 * @class IMUManager
 * @brief IMUセンサーを管理するクラス
 *
 * クォータニオン、オイラー角、加速度、ジャイロデータの取得機能を提供します。
 * 100Hzでの高速データ更新に対応しています。
 * BNO055 はオンチップ9軸融合、M5IMU は BMI270(6軸)+ソフト融合で姿勢を推定します。
 */
class IMUManager {
public:
    IMUManager();
    ~IMUManager();
    
    /**
     * @brief IMUセンサーを初期化
     * @param config 設定マネージャー参照
     * @param sda I2C SDAピン番号 (デフォルト: kImuI2cSda)
     * @param scl I2C SCLピン番号 (デフォルト: kImuI2cScl)
     * @return true 初期化成功, false 初期化失敗
     */
    bool begin(ConfigManager& config, uint8_t sda = kImuI2cSda, uint8_t scl = kImuI2cScl);
    
    /**
     * @brief センサーデータを更新
     * @return true 更新成功, false 更新失敗
     * @note 100Hz (10ms間隔) で呼び出すことを推奨
     */
    bool update();
    
    /**
     * @brief クォータニオンを取得
     * @return クォータニオンベクトル (w, x, y, z)
     */
    imu::Quaternion getQuaternion();
    
    /**
     * @brief クォータニオンを個別変数で取得
     * @param w クォータニオンw成分 (出力)
     * @param x クォータニオンx成分 (出力)
     * @param y クォータニオンy成分 (出力)
     * @param z クォータニオンz成分 (出力)
     * @return true 取得成功, false 取得失敗
     */
    bool getQuaternion(float& w, float& x, float& y, float& z);
    
    /**
     * @brief オイラー角を取得
     * @return オイラー角ベクトル (heading, roll, pitch)
     */
    imu::Vector<3> getEuler();
    
    /**
     * @brief オイラー角を個別変数で取得
     * @param heading ヘディング角 (yaw, 度) (出力)
     * @param roll ロール角 (度) (出力)
     * @param pitch ピッチ角 (度) (出力)
     * @return true 取得成功, false 取得失敗
     */
    bool getEuler(float& heading, float& roll, float& pitch);
    
    /**
     * @brief 加速度を取得
     * @return 加速度ベクトル (x, y, z) [m/s²]
     */
    imu::Vector<3> getAccel();
    
    /**
     * @brief 加速度を個別変数で取得
     * @param x X軸加速度 [m/s²] (出力)
     * @param y Y軸加速度 [m/s²] (出力)
     * @param z Z軸加速度 [m/s²] (出力)
     * @return true 取得成功, false 取得失敗
     */
    bool getAccel(float& x, float& y, float& z);
    
    /**
     * @brief ジャイロデータを取得
     * @return ジャイロベクトル (x, y, z) [deg/s]
     */
    imu::Vector<3> getGyro();
    
    /**
     * @brief ジャイロデータを個別変数で取得
     * @param x X軸角速度 [deg/s] (出力)
     * @param y Y軸角速度 [deg/s] (出力)
     * @param z Z軸角速度 [deg/s] (出力)
     * @return true 取得成功, false 取得失敗
     * @note BNO055 / M5IMU いずれの実装も deg/s で格納している。
     */
    bool getGyro(float& x, float& y, float& z);
    
    /**
     * @brief キャリブレーション状態を表示
     */
    void displayCalibrationStatus();
    
    /**
     * @brief 完全キャリブレーション状態を確認
     * @return true 全センサーがキャリブレーション済み, false 未完了
     */
    bool isFullyCalibrated();
    
    /**
     * @brief キャリブレーション状態を個別取得
     * @param sys システムキャリブレーション値 (0-3) (出力)
     * @param gyro ジャイロキャリブレーション値 (0-3) (出力)
     * @param accel 加速度計キャリブレーション値 (0-3) (出力)
     * @param mag 磁気計キャリブレーション値 (0-3) (出力)
     */
    void getCalibration(uint8_t& sys, uint8_t& gyro, uint8_t& accel, uint8_t& mag);

    /**
     * @brief 現在の動作モードレジスタ値 (BNO055: 8=IMUPLUS, 12=NDOF / M5IMU: 255)
     */
    uint8_t getOperationMode();
    
    /**
     * @brief IMUステータスを表示
     */
    void printStatus();
    
    /**
     * @brief 初期化状態を取得
     * @return true 初期化済み, false 未初期化
     */
    bool isInitialized() { return _initialized; }

    /**
     * @brief 専用タスクで IMU をポーリングする (推奨)
     *
     * Arduino の loop() から update() を呼ぶ方式では、同じ core1 で動く
     * LED レンダリングタスク (優先度2) に押されて実効 43Hz まで落ち、かつ
     * サンプル間隔が 1ms〜22ms とばらついていた。描画は毎フレーム「最新の」
     * quat を読むため、間隔のばらつきがそのまま「同じ姿勢を保持 → 突然2ステップ
     * 分ジャンプ」というカクつきになる。専用タスク + vTaskDelayUntil で
     * 位相ゆらぎのない固定周期にすることが滑らかさの本質的な条件。
     *
     * @param core     実行コア (LEDレンダリングと分けるため 0 を推奨)
     * @param priority 優先度 (デコードタスク=1 より高い 3 を推奨)
     * @param stackSize スタックサイズ [byte]
     * @return true 起動成功
     */
    bool startTask(uint8_t core = 0, uint8_t priority = 3, uint32_t stackSize = 4096);

    /**
     * @brief 専用タスクが動作中か (loop からの update() を抑止する判定に使う)
     */
    bool isTaskRunning() const { return _taskRunning; }

    // 計装用カウンタ (main の周期ログでレート計算に使う)
    uint32_t debugReadTotal() const { return _wdReadTotal; }
    uint32_t debugReadFails() const { return _wdReadFails; }
    uint32_t debugDiscards()  const { return _wdDiscards; }
    uint32_t debugZeroReads() const { return _wdZeroReads; }
    /// 受理された姿勢更新の通番。描画側が「前回と同じ姿勢か」を判定できる。
    uint32_t quatSeq() const { return _quatSeq; }
    
    /**
     * @brief センサー温度を取得
     * @return 温度 (℃)
     */
    int8_t getTemperature();
    
private:
    bool _initialized;             ///< 初期化状態フラグ

    imu::Quaternion _quat;         ///< 最新クォータニオン
    imu::Vector<3> _euler;         ///< 最新オイラー角 (x=heading, y=roll, z=pitch)
    imu::Vector<3> _accel;         ///< 最新加速度 [m/s²]
    imu::Vector<3> _gyro;          ///< 最新ジャイロ [deg/s]

    unsigned long _lastUpdate;     ///< 最終更新時刻 (ms)
    const unsigned long UPDATE_INTERVAL = 10; ///< 更新間隔 10ms = 100Hz

    // 凍結検知ウォッチドッグ (BNO055 が電源瞬断等で融合停止しレジスタが
    // 固まる事象への対策)。健全なセンサーはノイズで毎回値が揺らぐため、
    // quat/gyro/accel が全てビット同一のまま長時間続いたら凍結と判定する。
    float _wdPrev[10] = {0};       ///< 前回読み値 (quat4 + gyro3 + accel3)
    unsigned long _wdLastChange = 0; ///< 最後に値が変化した時刻 (ms)
    uint16_t _wdRecoveries = 0;    ///< 復旧試行回数
    uint32_t _wdDiscards = 0;      ///< 化けquat破棄の累計
    uint8_t _wdConsecDiscards = 0; ///< 連続破棄数 (再同期判定用)
    portMUX_TYPE _quatMux = portMUX_INITIALIZER_UNLOCKED; ///< _quat のコア間排他
    uint32_t _wdReadFails = 0;     ///< I2C quat読み出し失敗の累計
    uint32_t _wdReadTotal = 0;     ///< I2C quat読み出し試行の累計
    uint32_t _wdZeroReads = 0;     ///< I2Cが成功を返しつつ全ゼロだった回数
    volatile uint32_t _quatSeq = 0;///< 姿勢を受理するたびに +1 (描画側の鮮度判定用)
    TaskHandle_t _taskHandle = nullptr; ///< 専用ポーリングタスク
    volatile bool _taskRunning = false;
    static void taskFunction(void* parameter); ///< 固定周期ポーリングループ
    bool _updateOnce();            ///< 時間ゲートなしの1回分の取得 (タスクから呼ぶ)

    // --- I2C バスの排他 ---
    // IMU タスク (core0) が 100Hz でセンサーを読む一方、診断系の getter
    // (getCalibration/getOperationMode/getTemperature) は loop タスク (core1)
    // から呼ばれる。ESP32 の I2C HAL はロックを「API呼び出しごと」にしか
    // 取らないため、quat 読み出しの
    //     endTransmission(false)  → repeated start のためSTOPを出さない
    //     requestFrom(...)
    // の"間"に別コアのトランザクションが割り込むと、双方が誤ったレジスタ窓を
    // 読む。実測: mode が 8 と 61(不正値) を交互に返し、静止中の加速度が
    // 2.2 m/s² (正しくは約9.81) になっていた。
    // 複数トランザクションから成るシーケンスを1単位として保護する。
    // 再帰ミューテックスなのは printStatus() 等が他の公開getterを呼ぶため。
    SemaphoreHandle_t _i2cMutex = nullptr;
    void _lockI2c();
    void _unlockI2c();

    /// RAII ガード。early return が多いので都度 unlock を書かずに済ませる。
    struct I2cGuard {
        IMUManager* m;
        explicit I2cGuard(IMUManager* mm) : m(mm) { m->_lockI2c(); }
        ~I2cGuard() { m->_unlockI2c(); }
    };
    static constexpr unsigned long kFrozenTimeoutMs = 10000;

#if defined(IMU_SENSOR_M5IMU)
    MadgwickAHRS _ahrs;            ///< ソフト姿勢推定フィルタ (6軸)
    unsigned long _lastMicros;     ///< 前回更新のマイクロ秒 (dt算出用)
    bool _biasReady;               ///< ジャイロバイアス校正済みフラグ
    float _gyroBias[3];            ///< ジャイロゼロ点バイアス [deg/s] (x,y,z)
    unsigned long _lastDriftLog;   ///< ドリフト計測ログの最終出力時刻 (ms)

    /// @brief 起動時に静止状態のジャイロを平均してバイアスを推定
    void calibrateGyroBias();
#else
    Adafruit_BNO055 _bno;          ///< BNO055センサーオブジェクト
#endif
};

} // namespace sastle
