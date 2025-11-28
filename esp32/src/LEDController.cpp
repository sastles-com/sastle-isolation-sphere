#include "LEDController.h"
#include "Config.h" // For PIN definitions

LEDController::LEDController(ConfigManager &configMgr)
    : configManager(configMgr) {}

void LEDController::begin() {
  int offset = 0;
  FastLED.addLeds<WS2812, PIN_LED_1, GRB>(leds, offset, LEDS_STRIP_1);
  offset += LEDS_STRIP_1;
  FastLED.addLeds<WS2812, PIN_LED_2, GRB>(leds, offset, LEDS_STRIP_2);
  offset += LEDS_STRIP_2;
  FastLED.addLeds<WS2812, PIN_LED_3, GRB>(leds, offset, LEDS_STRIP_3);
  offset += LEDS_STRIP_3;
  FastLED.addLeds<WS2812, PIN_LED_4, GRB>(leds, offset, LEDS_STRIP_4);

  FastLED.setBrightness(50);
}

void LEDController::update() { FastLED.show(); }

void LEDController::setSolidColor(CRGB color) {
  for (int i = 0; i < TOTAL_LEDS; i++) {
    leds[i] = color;
  }
  FastLED.show();
}

void LEDController::setLed(int index, CRGB color) {
  if (index >= 0 && index < TOTAL_LEDS) {
    leds[index] = color;
  }
}

void LEDController::show() { FastLED.show(); }

void LEDController::clear() { FastLED.clear(); }

void LEDController::setEffect(int effectId) {
  // Implement effects based on layout
}
