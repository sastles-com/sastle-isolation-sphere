#pragma once

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>
#include <utility/imumaths.h>
#include "ConfigManager.h"

// I2Cピン定義
#define IMU_I2C_SDA 2
#define IMU_I2C_SCL 1

namespace sastle {

class IMUManager {
public:
    IMUManager();
    ~IMUManager();
    
    // 初期化
    bool begin(ConfigManager& config, uint8_t sda = IMU_I2C_SDA, uint8_t scl = IMU_I2C_SCL);
    
    // データ更新
    bool update();
    
    // クォータニオン取得
    imu::Quaternion getQuaternion();
    bool getQuaternion(float& w, float& x, float& y, float& z);
    
    // オイラー角取得
    imu::Vector<3> getEuler();
    bool getEuler(float& heading, float& roll, float& pitch);
    
    // 加速度取得
    imu::Vector<3> getAccel();
    bool getAccel(float& x, float& y, float& z);
    
    // ジャイロ取得
    imu::Vector<3> getGyro();
    bool getGyro(float& x, float& y, float& z);
    
    // キャリブレーション
    void displayCalibrationStatus();
    bool isFullyCalibrated();
    void getCalibration(uint8_t& sys, uint8_t& gyro, uint8_t& accel, uint8_t& mag);
    
    // ステータス
    void printStatus();
    bool isInitialized() { return _initialized; }
    int8_t getTemperature();
    
private:
    Adafruit_BNO055 _bno;
    bool _initialized;
    
    // 最新データ
    imu::Quaternion _quat;
    imu::Vector<3> _euler;
    imu::Vector<3> _accel;
    imu::Vector<3> _gyro;
    
    unsigned long _lastUpdate;
    const unsigned long UPDATE_INTERVAL = 10; // 10ms = 100Hz
};

} // namespace sastle
