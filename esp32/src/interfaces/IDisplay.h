#pragma once

#include <cstdarg> // For va_list, va_start, va_end

class IDisplay {
public:
  virtual ~IDisplay() {}
  virtual void begin() = 0;
  virtual void printf(const char *format, ...) = 0;
  virtual void clear() = 0;
};
