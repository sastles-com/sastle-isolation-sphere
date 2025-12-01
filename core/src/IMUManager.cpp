#include "IMUManager.h"
#include <Arduino.h>

namespace sastle {

IMUManager::IMUManager() 
    : _bno(55, 0x28), // BNO055のI2Cアドレス: 0x28 (ADRピンがLOW)
      _initialized(false),
      _lastUpdate(0) {
}

IMUManager::~IMUManager() {
}

bool IMUManager::begin(ConfigManager& config, uint8_t sda, uint8_t scl) {
    Serial.println("\n=== IMU Manager Initialization ===");
    
    // I2C初期化（既に初期化されている場合はスキップ）
    if (!Wire.begin(sda, scl, 400000)) {  // 400kHzで初期化
        Serial.println("I2C bus initialization failed or already initialized");
    }
    delay(100);
    
    Serial.printf("I2C initialized (SDA: GPIO%d, SCL: GPIO%d)\n", sda, scl);
    
    // BNO055初期化
    if (!_bno.begin()) {
        Serial.println("BNO055 not detected! Check wiring.");
        return false;
    }
    
    Serial.println("BNO055 detected!");
    
    // センサー情報表示
    sensor_t sensor;
    _bno.getSensor(&sensor);
    Serial.println("------------------------------------");
    Serial.print("Sensor:       "); Serial.println(sensor.name);
    Serial.print("Driver Ver:   "); Serial.println(sensor.version);
    Serial.print("Unique ID:    "); Serial.println(sensor.sensor_id);
    Serial.print("Max Value:    "); Serial.print(sensor.max_value); Serial.println(" xxx");
    Serial.print("Min Value:    "); Serial.print(sensor.min_value); Serial.println(" xxx");
    Serial.print("Resolution:   "); Serial.print(sensor.resolution); Serial.println(" xxx");
    Serial.println("------------------------------------");
    
    delay(100);
    
    // クリスタルを使用（より高精度）
    _bno.setExtCrystalUse(true);
    
    // 動作モードをNDOF（9軸融合）に設定
    _bno.setMode(OPERATION_MODE_NDOF);
    delay(20);
    
    _initialized = true;
    Serial.println("BNO055 initialized successfully!");
    
    // 初期キャリブレーション状態表示
    displayCalibrationStatus();
    
    return true;
}

bool IMUManager::update() {
    if (!_initialized) {
        return false;
    }
    
    unsigned long now = millis();
    if (now - _lastUpdate < UPDATE_INTERVAL) {
        return true; // まだ更新タイミングではない
    }
    
    _lastUpdate = now;
    
    // データ取得
    _quat = _bno.getQuat();
    _euler = _bno.getVector(Adafruit_BNO055::VECTOR_EULER);
    _accel = _bno.getVector(Adafruit_BNO055::VECTOR_ACCELEROMETER);
    _gyro = _bno.getVector(Adafruit_BNO055::VECTOR_GYROSCOPE);
    
    return true;
}

imu::Quaternion IMUManager::getQuaternion() {
    return _quat;
}

bool IMUManager::getQuaternion(float& w, float& x, float& y, float& z) {
    if (!_initialized) {
        return false;
    }
    
    w = _quat.w();
    x = _quat.x();
    y = _quat.y();
    z = _quat.z();
    
    return true;
}

imu::Vector<3> IMUManager::getEuler() {
    return _euler;
}

bool IMUManager::getEuler(float& heading, float& roll, float& pitch) {
    if (!_initialized) {
        return false;
    }
    
    heading = _euler.x();
    roll = _euler.y();
    pitch = _euler.z();
    
    return true;
}

imu::Vector<3> IMUManager::getAccel() {
    return _accel;
}

bool IMUManager::getAccel(float& x, float& y, float& z) {
    if (!_initialized) {
        return false;
    }
    
    x = _accel.x();
    y = _accel.y();
    z = _accel.z();
    
    return true;
}

imu::Vector<3> IMUManager::getGyro() {
    return _gyro;
}

bool IMUManager::getGyro(float& x, float& y, float& z) {
    if (!_initialized) {
        return false;
    }
    
    x = _gyro.x();
    y = _gyro.y();
    z = _gyro.z();
    
    return true;
}

void IMUManager::displayCalibrationStatus() {
    if (!_initialized) {
        Serial.println("IMU not initialized!");
        return;
    }
    
    uint8_t system, gyro, accel, mag = 0;
    _bno.getCalibration(&system, &gyro, &accel, &mag);
    
    Serial.println("\n=== Calibration Status ===");
    Serial.print("System: "); Serial.print(system, DEC);
    Serial.print(" Gyro: "); Serial.print(gyro, DEC);
    Serial.print(" Accel: "); Serial.print(accel, DEC);
    Serial.print(" Mag: "); Serial.println(mag, DEC);
    Serial.println("(0=uncalibrated, 3=fully calibrated)");
    Serial.println("==========================");
}

bool IMUManager::isFullyCalibrated() {
    if (!_initialized) {
        return false;
    }
    
    uint8_t system, gyro, accel, mag = 0;
    _bno.getCalibration(&system, &gyro, &accel, &mag);
    
    return (system == 3 && gyro == 3 && accel == 3 && mag == 3);
}

void IMUManager::getCalibration(uint8_t& sys, uint8_t& gyro, uint8_t& accel, uint8_t& mag) {
    if (_initialized) {
        _bno.getCalibration(&sys, &gyro, &accel, &mag);
    } else {
        sys = gyro = accel = mag = 0;
    }
}

int8_t IMUManager::getTemperature() {
    if (!_initialized) {
        return 0;
    }
    return _bno.getTemp();
}

void IMUManager::printStatus() {
    Serial.println("\n=== IMU Status ===");
    
    if (!_initialized) {
        Serial.println("Status: Not initialized");
        Serial.println("==================");
        return;
    }
    
    Serial.println("Status: Initialized");
    Serial.printf("Temperature: %d°C\n", getTemperature());
    
    // キャリブレーション状態
    uint8_t system, gyro, accel, mag = 0;
    getCalibration(system, gyro, accel, mag);
    Serial.printf("Calibration - Sys:%d Gyro:%d Accel:%d Mag:%d\n", 
                  system, gyro, accel, mag);
    
    // クォータニオン
    Serial.printf("Quaternion - w:%.3f x:%.3f y:%.3f z:%.3f\n",
                  _quat.w(), _quat.x(), _quat.y(), _quat.z());
    
    // オイラー角
    Serial.printf("Euler - Heading:%.2f° Roll:%.2f° Pitch:%.2f°\n",
                  _euler.x(), _euler.y(), _euler.z());
    
    Serial.println("==================");
}

} // namespace sastle
