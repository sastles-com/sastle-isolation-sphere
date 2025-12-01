#pragma once

#include "interfaces/IDisplay.h"
#include <string>
#include <cstdarg>

class MockDisplay : public IDisplay {
public:
  std::string buffer;
  bool beginCalled = false;
  bool clearCalled = false;

  void begin() override {
    beginCalled = true;
  }

  void printf(const char *format, ...) override {
    char temp_buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(temp_buffer, sizeof(temp_buffer), format, args);
    va_end(args);
    buffer = temp_buffer;
  }

  void clear() override {
    clearCalled = true;
    buffer = "";
  }
};
