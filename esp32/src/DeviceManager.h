#pragma once
#include "ConfigManager.h"
#include "IMU.h"
#include "LEDController.h"
#include "Speaker.h"
#include "interfaces/IDisplay.h"

class DeviceManager {
public:
  DeviceManager(ConfigManager &configManager, IMU &imu, LEDController &ledController, Speaker &speaker, IDisplay &display);
  bool begin();
  void update();

  // Control methods
  void setLed(int index, CRGB color);
  void setLedState(int hue, int brightness);
  void setPlaybackState(bool isPlaying);
  void getImuData(float &w, float &x, float &y, float &z);
  void showLeds();
  void displayMessage(const char *format, ...);

private:
  ConfigManager &configManager;
  Speaker &speaker;
  IMU &imu;
  LEDController &ledController;
  IDisplay &display;
};
