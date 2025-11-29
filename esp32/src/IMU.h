#pragma once
// #include <Adafruit_BNO055.h> // Original include
#include <Adafruit_Sensor.h>
#include <Arduino.h>
#include <Wire.h>

#ifdef NATIVE_ENV
#include "../../test/mocks/Adafruit_BNO055.h" // Path to mock Adafruit_BNO055.h
#else
#include <Adafruit_BNO055.h> // Path to real Adafruit_BNO055.h
#endif

class IMU {
public:
  IMU(int sda, int scl);
  virtual bool begin();
  virtual void update();
  virtual void getQuaternion(float &w, float &x, float &y, float &z);

private:
  int sda_pin;
  int scl_pin;
  Adafruit_BNO055 bno;
  imu::Quaternion quat;
  bool initialized = false;
  unsigned long lastUpdate = 0;
};

