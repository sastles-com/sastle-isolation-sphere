#pragma once
#include <Arduino.h>
#include <vector>

struct LEDCoord {
  int faceID;
  int strip;
  int strip_num;
  float x, y, z;
};

class LedMapper {
public:
  LedMapper();
  bool loadLayout(const char *csvPath);
  bool parseLayout(const String &csv); // Added for testing
  void mapToPanorama(float x, float y, float z, float &u, float &v);

  // Accessors
  int getTotalLeds() const;
  const LEDCoord &getLedCoord(int index) const;

private:
  std::vector<LEDCoord> ledCoords;

  // Constants from MFT2025
  static constexpr float CUBE_NEON_PI = 3.14159265f;
  static constexpr float CUBE_NEON_HALF_PI = 1.57079632f;
  static constexpr float CUBE_NEON_INV_TWO_PI = 0.15915494f;
  static constexpr float CUBE_NEON_INV_PI = 0.31830988f;

  // Helpers
  float fastSqrt(float x);
  float fastInvSqrt(float x);
  void useEmbeddedCoordinates();
};
