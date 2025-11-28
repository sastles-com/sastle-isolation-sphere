#pragma once
#include "IMU.h"

class MockIMU : public IMU {
public:
    // Constructor matching the base class if needed, or default
    MockIMU() : IMU(0, 0) {} // Assuming base constructor takes pins

    bool beginCalled = false;
    bool updateCalled = false;
    float mockW = 1.0, mockX = 0.0, mockY = 0.0, mockZ = 0.0;

    bool begin() override {
        beginCalled = true;
        return true;
    }

    void update() override {
        updateCalled = true;
    }

    void getQuaternion(float &w, float &x, float &y, float &z) override {
        w = mockW;
        x = mockX;
        y = mockY;
        z = mockZ;
    }
};
