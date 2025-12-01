#pragma once

#include "interfaces/IDisplay.h"
#include <M5Unified.h>

class M5DisplayAdapter : public IDisplay {
public:
  void begin() override {
    // M5.begin() is expected to be called outside,
    // so nothing specific is needed here for the display alone.
  }

  void printf(const char *format, ...) override {
    // Note: M5.Lcd.printf does not handle variadic args in the same way as standard printf.
    // It's better to format the string first and then print.
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    M5.Lcd.print(buffer);
  }

  void clear() override {
    M5.Lcd.clear();
  }
};
