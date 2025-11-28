#pragma once
#include <cstdint>

struct CRGB {
  uint8_t r;
  uint8_t g;
  uint8_t b;
  CRGB() : r(0), g(0), b(0) {}
  CRGB(uint8_t r, uint8_t g, uint8_t b) : r(r), g(g), b(b) {}
};

class CFastLED {
public:
  uint8_t lastBrightness = 0;

  template <int PIN> void addLeds(CRGB *data, int nLeds) {}

  template <int CHIPSET, int PIN> void addLeds(CRGB *data, int nLeds) {}

  template <int CHIPSET, int PIN, int RGB_ORDER> void addLeds(CRGB *data, int nLeds, int offset = 0) {}

  void setBrightness(uint8_t scale) {}
  void show() {}
  void clear(bool writeData = false) {}
};

#ifndef FASTLED_MOCK_IMPL
extern CFastLED FastLED;
#else
CFastLED FastLED;
#endif

inline CRGB CHSV(uint8_t h, uint8_t s, uint8_t v) { return CRGB(); }

#define NEOPIXEL 1
#define WS2812 2
#define GRB 3
