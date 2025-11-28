#include "IMU.h"

IMU::IMU(int sda, int scl) : sda_pin(sda), scl_pin(scl), bno(55, 0x28) {}

bool IMU::begin() {
  Wire.begin(sda_pin, scl_pin);
  if (!bno.begin()) {
    Serial.print(
        "Ooops, no BNO055 detected ... Check your wiring or I2C ADDR!");
    return false;
  }
  delay(1000);
  bno.setExtCrystalUse(true);
  return true;
}

void IMU::update() { quat = bno.getQuat(); }

void IMU::getQuaternion(float &w, float &x, float &y, float &z) {
  w = quat.w();
  x = quat.x();
  y = quat.y();
  z = quat.z();
}
