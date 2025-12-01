#pragma once
#include <stdint.h>

class M5Unified {
public:
    bool beginCalled = false;
    void begin() { beginCalled = true; }
    void update() {}
};

extern M5Unified M5;
