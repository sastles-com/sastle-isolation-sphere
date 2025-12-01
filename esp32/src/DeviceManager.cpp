#include "DeviceManager.h"
#include "DeviceManager.h"
#include "Config.h"
#ifdef ESP32
#include <M5Unified.h>
#include <LittleFS.h>
#else
#include "M5Unified.h"
#include "LittleFS.h"
#endif

DeviceManager::DeviceManager(ConfigManager &configMgr, IMU &imu, LEDController &ledController, Speaker &speaker, IDisplay &display)
    : configManager(configMgr), imu(imu), ledController(ledController), speaker(speaker), display(display) {}

bool DeviceManager::begin() {
  // Platform initialization
  
  // PSRAM check
  if (psramFound()) {
      Serial.printf("PSRAM: %d bytes free\n", ESP.getFreePsram());
  } else {
      Serial.println("PSRAM not found");
  }

  // Filesystem check
  if (LittleFS.begin(true)) {
      Serial.println("LittleFS Mounted");
      File root = LittleFS.open("/");
      File file = root.openNextFile();
      while(file){
          Serial.print("FILE: ");
          Serial.println(file.name());
          file = root.openNextFile();
      }
  } else {
      Serial.println("LittleFS Mount Failed");
      return false;
  }

  // Play startup sound (optional, maybe move to setup?)
  speaker.playStartup();
  return true;
}

void DeviceManager::update() {
  speaker.update();
  imu.update();
  ledController.update();
}

void DeviceManager::setLedState(int hue, int brightness) {
  ledController.setSolidColor(CHSV(hue, 255, brightness));
  ledController.show();
}

void DeviceManager::setLed(int index, CRGB color) {
  ledController.setLed(index, color);
}

void DeviceManager::showLeds() { ledController.show(); }

void DeviceManager::setPlaybackState(bool isPlaying) {
  if (isPlaying) {
    speaker.playStart();
  } else {
    speaker.playEnd();
  }
}

void DeviceManager::getImuData(float &w, float &x, float &y, float &z) {
  imu.getQuaternion(w, x, y, z);
}

void DeviceManager::displayMessage(const char *format, ...) {
  char buffer[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  display.printf(buffer);
}
