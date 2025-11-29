#pragma once
#include <stdint.h>

extern int mock_wire_sda;
extern int mock_wire_scl;

class TwoWire {
public:
    void begin(int sda=-1, int scl=-1, uint32_t frequency=0);
};

extern TwoWire Wire;
