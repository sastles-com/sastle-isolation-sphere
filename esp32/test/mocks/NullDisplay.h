#pragma once

#include "interfaces/IDisplay.h"

// A null object implementation of the IDisplay interface.
// Used for boards that do not have a physical display.
class NullDisplay : public IDisplay {
public:
  void begin() override {
    // Do nothing
  }

  void printf(const char *format, ...) override {
    // Do nothing
  }

  void clear() override {
    // Do nothing
  }
};
