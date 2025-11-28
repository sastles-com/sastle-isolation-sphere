#pragma once
#include "LEDController.h"

class MockLEDController : public LEDController {
public:
  bool beginCalled = false;
  bool updateCalled = false;
  CRGB lastLedColor;
  int lastLedIndex;

  MockLEDController(ConfigManager& configMgr) : LEDController(configMgr) {}
  void begin() { // Changed return type to void
    beginCalled = true;
  }
  void update() { updateCalled = true; }
  void setSolidColor(CRGB color) {}
  void setLed(int index, CRGB color) {
    lastLedIndex = index;
    lastLedColor = color;
  }
  void show() {}
};
