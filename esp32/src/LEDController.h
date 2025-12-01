#pragma once
#include "ConfigManager.h"
#include <Arduino.h>
#include <FastLED.h>
#include <vector>

// Define max LEDs per strip to allocate memory statically if needed,
// or use dynamic allocation if FastLED supports it well (it prefers static).
// For now, let's assume a max number or use a fixed size buffer.
#define MAX_LEDS 100
#define NUM_STRIPS 4

class LEDController {
public:
  LEDController(ConfigManager &configManager);
  virtual void begin();
  virtual void update();
  virtual void setSolidColor(CRGB color);
  virtual void setLed(int index, CRGB color);
  virtual void show();
  void clear();
  void setEffect(int effectId);

private:
  ConfigManager &configManager;
  // MFT2025 Config
  static const int LEDS_STRIP_1 = 180;
  static const int LEDS_STRIP_2 = 220;
  static const int LEDS_STRIP_3 = 220;
  static const int LEDS_STRIP_4 = 180;
  static const int TOTAL_LEDS =
      LEDS_STRIP_1 + LEDS_STRIP_2 + LEDS_STRIP_3 + LEDS_STRIP_4;

  CRGB leds[TOTAL_LEDS];
};
