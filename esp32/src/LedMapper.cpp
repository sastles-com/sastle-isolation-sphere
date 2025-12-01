#include "LedMapper.h"
#ifdef ESP32
#include <LittleFS.h>
#endif

LedMapper::LedMapper() { useEmbeddedCoordinates(); }

bool LedMapper::loadLayout(const char *csvPath) {
#ifdef ESP32
  if (!LittleFS.begin(false, "/littlefs", 10, "littlefs")) {
    Serial.println("LittleFS init failed");
    return false;
  }

  File file = LittleFS.open(csvPath, "r");
  if (!file) {
    Serial.println("Failed to open CSV");
    return false;
  }
  String csv = file.readString();
  file.close();
  return parseLayout(csv);
#else
  return false;
#endif
}

bool LedMapper::parseLayout(const String &csv) {
  ledCoords.clear();

  int startIndex = 0;
  int endIndex = csv.indexOf('\n');
  bool firstLine = true;

  while (endIndex >= 0 || startIndex < csv.length()) {
    String line;
    if (endIndex >= 0) {
      line = csv.substring(startIndex, endIndex);
    } else {
      line = csv.substring(startIndex);
    }
    line.trim();

    // Update indices for next loop
    if (endIndex >= 0) {
      startIndex = endIndex + 1;
      endIndex = csv.indexOf('\n', startIndex);
    } else {
      startIndex = csv.length(); // End loop
    }

    if (firstLine) {
      firstLine = false;
      continue;
    }

    if (line.length() == 0)
      continue;

    // Simple parsing logic matching MFT2025
    int commaCount = 0;
    int commaPositions[5];
    for (int i = 0; i < line.length(); i++) {
      if (line[i] == ',' && commaCount < 5) {
        commaPositions[commaCount++] = i;
      }
    }

    if (commaCount >= 5) {
      LEDCoord coord;
      coord.faceID = line.substring(0, commaPositions[0]).toInt();
      coord.strip =
          line.substring(commaPositions[0] + 1, commaPositions[1]).toInt();
      coord.strip_num =
          line.substring(commaPositions[1] + 1, commaPositions[2]).toInt();
      coord.x =
          line.substring(commaPositions[2] + 1, commaPositions[3]).toFloat();
      coord.y =
          line.substring(commaPositions[3] + 1, commaPositions[4]).toFloat();
      coord.z = line.substring(commaPositions[4] + 1).toFloat();

      ledCoords.push_back(coord);
    }
  }
  return ledCoords.size() > 0;
}

void LedMapper::useEmbeddedCoordinates() {
  // Fallback: generate spherical coordinates
  ledCoords.clear();
  // MFT2025 has 800 LEDs.
  for (int i = 0; i < 800; i++) {
    // Simple spherical distribution for fallback
    float theta = random(0, 3600) * 0.001f;
    float phi = random(0, 6283) * 0.001f;

    LEDCoord coord;
    coord.faceID = i;
    coord.strip = i / 200;
    coord.strip_num = i % 200;
    coord.x = sin(theta) * cos(phi);
    coord.y = sin(theta) * sin(phi);
    coord.z = cos(theta);
    ledCoords.push_back(coord);
  }
}

float LedMapper::fastSqrt(float x) {
  if (x <= 0.0f)
    return 0.0f;
  union {
    float f;
    uint32_t i;
  } u;
  u.f = x;
  u.i = (u.i >> 1) + 0x1fbb67a8;
  u.f = 0.5f * (u.f + x / u.f);
  return u.f;
}

float LedMapper::fastInvSqrt(float x) {
  if (x <= 0.0f)
    return 0.0f;
  union {
    float f;
    uint32_t i;
  } u;
  u.f = x;
  u.i = 0x5f3759df - (u.i >> 1);
  u.f = u.f * (1.5f - 0.5f * x * u.f * u.f);
  return u.f;
}

void LedMapper::mapToPanorama(float x, float y, float z, float &u, float &v) {
  // Logic from MFT2025 sphericalToUV

  // Normalize
  float length_sq = x * x + y * y + z * z;
  if (length_sq > 0.000001f) {
    float inv_length = fastInvSqrt(length_sq);
    x *= inv_length;
    y *= inv_length;
    z *= inv_length;
  }

  // Longitude (X-Z plane)
  float longitude;
  float abs_x = (x >= 0) ? x : -x;
  float abs_z = (z >= 0) ? z : -z;

  if (abs_x > abs_z) {
    float ratio = z / x;
    longitude = ratio * CUBE_NEON_PI * 0.25f;
    if (x < 0)
      longitude += CUBE_NEON_PI;
  } else {
    float ratio = (abs_z > 0.001f) ? (x / z) : 0.0f;
    longitude = CUBE_NEON_HALF_PI - ratio * CUBE_NEON_PI * 0.25f;
    if (z < 0)
      longitude += CUBE_NEON_PI;
  }

  // Latitude (Y axis)
  float xz_length_sq = x * x + z * z;
  float latitude;

  if (xz_length_sq > 0.000001f) {
    float xz_length = fastSqrt(xz_length_sq);
    float y_ratio = y / xz_length;
    float abs_y_ratio = (y_ratio >= 0) ? y_ratio : -y_ratio;

    if (abs_y_ratio < 0.7f) {
      latitude = y_ratio * CUBE_NEON_HALF_PI;
    } else {
      latitude = atan2(y, xz_length);
    }
  } else {
    latitude = (y > 0) ? CUBE_NEON_HALF_PI : -CUBE_NEON_HALF_PI;
  }

  u = (longitude + CUBE_NEON_PI) * CUBE_NEON_INV_TWO_PI;
  v = (latitude + CUBE_NEON_HALF_PI) * CUBE_NEON_INV_PI;

  u = constrain(u, 0.0f, 1.0f);
  v = constrain(v, 0.0f, 1.0f);
}

int LedMapper::getTotalLeds() const { return ledCoords.size(); }

const LEDCoord &LedMapper::getLedCoord(int index) const {
  return ledCoords[index];
}
