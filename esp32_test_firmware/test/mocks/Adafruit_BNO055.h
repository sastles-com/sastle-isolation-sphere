#pragma once
#include <cstdint>

namespace imu {
struct Quaternion {
  double w_val, x_val, y_val, z_val;
  Quaternion() : w_val(1), x_val(0), y_val(0), z_val(0) {}
  Quaternion(double w, double x, double y, double z)
      : w_val(w), x_val(x), y_val(y), z_val(z) {}
  double w() const { return w_val; }
  double x() const { return x_val; }
  double y() const { return y_val; }
  double z() const { return z_val; }
};
} // namespace imu

extern bool mock_bno_begin_success;
extern imu::Quaternion mock_bno_quat;

class Adafruit_BNO055 {
public:
  Adafruit_BNO055(int32_t sensorID = -1, uint8_t address = 0x28) {}
  bool begin() { return mock_bno_begin_success; }
  void setExtCrystalUse(bool usext) {}

  imu::Quaternion getQuat() { return mock_bno_quat; }
};
