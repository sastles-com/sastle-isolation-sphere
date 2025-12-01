#include "IMU.h"

//IMU::IMU(int sda, int scl, TwoWire& wire) : sda_pin(sda), scl_pin(scl), _wire(wire), bno(55, 0x28, &wire) {}
IMU::IMU(int sda, int scl) : sda_pin(sda), scl_pin(scl), bno(-1, 0x28) {}

bool IMU::begin() {
  // Serial.printf("IMU::begin() called. Pins: SDA=%d, SCL=%d\n", sda_pin, scl_pin);
  
  // Wire1 initialized in main.cpp
  // Just ensure begin is called (idempotent)
  Wire.begin(sda_pin, scl_pin);

  return bno.begin();
  
  // // Diagnostic check
  // Serial.print("Checking for BNO055 at 0x28... ");
  // Wire.beginTransmission(0x28);
  // byte error = Wire.endTransmission();
  // if (error == 0) {
  //     Serial.println("FOUND!");
  // } else {
  //     Serial.printf("NOT FOUND (Error %d)\n", error);
  //     Serial.println("Scanning entire I2C bus...");
  //     int nDevices = 0;
  //     for(byte address = 1; address < 127; address++ ) {
  //       Wire.beginTransmission(address);
  //       error = Wire.endTransmission();
  //       if (error == 0) {
  //         Serial.print("I2C device found at address 0x");
  //         if (address < 16) Serial.print("0");
  //         Serial.println(address, HEX);
  //         nDevices++;
  //       }
  //     }
  //     if (nDevices == 0) Serial.println("No I2C devices found\n");
  //     else Serial.println("done\n");
  // }
  
  // // Set clock to 50kHz to improve stability with BNO055 clock stretching
  // Wire.setClock(50000);
  
  // // Retry BNO055 initialization up to 3 times
  // int retries = 3;
  // bool success = false;
  // for (int i = 0; i < retries && !success; i++) {
  //   if (i > 0) {
  //     Serial.printf("Retry %d/%d...\n", i, retries - 1);
  //     delay(500);
  //   }
  //   if (bno.begin()) {
  //     success = true;
  //   }
  // }
  
  // if (!success) {
  //   Serial.println("Ooops, no BNO055 detected ... Check your wiring or I2C ADDR!");
  //   return false;
  // }
  
  // delay(1000);
  // bno.setExtCrystalUse(true);
  // initialized = true;
  // Serial.println("IMU Initialized successfully");
  // return true;
}

void IMU::update() {
  // if (!initialized) return;
  
  // static unsigned long lastUpdate = 0;
  // if (millis() - lastUpdate < 10) return; // Limit to 100Hz
  // lastUpdate = millis();

  quat = bno.getQuat();
  
  // // Debug print occasionally
  // static unsigned long lastDebug = 0;
  // if (millis() - lastDebug > 2000) {
  //     Serial.printf("IMU Quat: %.2f, %.2f, %.2f, %.2f\n", quat.w(), quat.x(), quat.y(), quat.z());
  //     lastDebug = millis();
  // }
}

void IMU::getQuaternion(float &w, float &x, float &y, float &z) {
  w = quat.w();
  x = quat.x();
  y = quat.y();
  z = quat.z();
}
