#pragma once
#include <stdint.h>

class TwoWire {
public:
    void begin(int sda=-1, int scl=-1, uint32_t frequency=0) {}
};

extern TwoWire Wire;
