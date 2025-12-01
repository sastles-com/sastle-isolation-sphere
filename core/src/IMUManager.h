/**
 * @file IMUManager.h
 * @brief IMUセンサー (BNO055) 管理クラス
 * @author sastle-com
 * @date 2025-12-01
 */

#pragma once

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>
#include <utility/imumaths.h>
#include "ConfigManager.h"

/// I2C SDAピン番号
#define IMU_I2C_SDA 2
/// I2C SCLピン番号
#define IMU_I2C_SCL 1

namespace sastle {

/**
 * @class IMUManager
 * @brief BNO055 IMUセンサーを管理するクラス
 * 
 * 9軸センサーフュージョンによるクォータニオン、オイラー角、
 * 加速度、ジャイロデータの取得機能を提供します。
 * 100Hzでの高速データ更新に対応しています。
 */
class IMUManager {
public:
    IMUManager();
    ~IMUManager();
    
    /**
     * @brief IMUセンサーを初期化
     * @param config 設定マネージャー参照
     * @param sda I2C SDAピン番号 (デフォルト: IMU_I2C_SDA)
     * @param scl I2C SCLピン番号 (デフォルト: IMU_I2C_SCL)
     * @return true 初期化成功, false 初期化失敗
     */
    bool begin(ConfigManager& config, uint8_t sda = IMU_I2C_SDA, uint8_t scl = IMU_I2C_SCL);
    
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
     * @return ジャイロベクトル (x, y, z) [rad/s]
     */
    imu::Vector<3> getGyro();
    
    /**
     * @brief ジャイロデータを個別変数で取得
     * @param x X軸角速度 [rad/s] (出力)
     * @param y Y軸角速度 [rad/s] (出力)
     * @param z Z軸角速度 [rad/s] (出力)
     * @return true 取得成功, false 取得失敗
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
     * @brief IMUステータスを表示
     */
    void printStatus();
    
    /**
     * @brief 初期化状態を取得
     * @return true 初期化済み, false 未初期化
     */
    bool isInitialized() { return _initialized; }
    
    /**
     * @brief センサー温度を取得
     * @return 温度 (℃)
     */
    int8_t getTemperature();
    
private:
    Adafruit_BNO055 _bno;          ///< BNO055センサーオブジェクト
    bool _initialized;             ///< 初期化状態フラグ
    
    imu::Quaternion _quat;         ///< 最新クォータニオン
    imu::Vector<3> _euler;         ///< 最新オイラー角
    imu::Vector<3> _accel;         ///< 最新加速度
    imu::Vector<3> _gyro;          ///< 最新ジャイロ
    
    unsigned long _lastUpdate;     ///< 最終更新時刻 (ms)
    const unsigned long UPDATE_INTERVAL = 10; ///< 更新間隔 10ms = 100Hz
};

} // namespace sastle
