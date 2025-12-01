#include "ConfigManager.h"
#include "DeviceManager.h"
#include "LedMapper.h"
#include "NetworkManager.h"
#include "adapters/WiFiAdapter.h"
#include <Arduino.h>

#ifndef NATIVE_ENV // Start conditional compilation for actual device
ConfigManager configManager;
WiFiAdapter wifiAdapter;
// The following lines are currently problematic in the firmware code and need to be fixed for actual deployment.
// For the purpose of this native test, we are completely excluding them.
// IMU imu(0,0); // Dummy for compilation
// LEDController led(configManager); // Dummy for compilation
// Speaker speaker(0); // Dummy for compilation
// DeviceManager deviceManager(configManager, imu, led, speaker);
// NetworkManager networkManager(wifiAdapter, configManager, deviceManager);
// LedMapper ledMapper;


void setup() {
  Serial.begin(115200);
  if (!configManager.loadLayout()) {
    Serial.println("Failed to load layout.csv");
  }

  // 2. Initialize Devices
  // deviceManager.begin();

  // 3. Initialize Network & ROS2
  // networkManager.begin();
}

void loop() {
  // Update devices (LED effects, etc.)
  // deviceManager.update();

  // Get IMU data
  // float w, x, y, z;
  // deviceManager.getImuData(w, x, y, z);

  // Publish IMU data
  // networkManager.publishImuData(w, x, y, z);

  // Update Network (handle ROS2 callbacks)
  // networkManager.update();

  delay(10); // Small delay to prevent WDT reset if loop is tight
}

#endif // NATIVE_ENV
