#ifdef NATIVE_ENV

#include "Arduino.h"
#include "LittleFS.h"
#include "Wire.h"
#include "FastLED.h"

// Include mock headers that declare extern global instances
#include "../mocks/WiFi.h"
#include "../mocks/Adafruit_BNO055.h"

unsigned long millis() { return 0; }
void delay(unsigned long) {}

SerialMock Serial;
LittleFSClass LittleFS;
EspClass ESP;
TwoWire Wire;
CFastLED FastLED;

// Mock implementations
int mock_wire_sda = -1;
int mock_wire_scl = -1;

void TwoWire::begin(int sda, int scl, uint32_t frequency) {
    mock_wire_sda = sda;
    mock_wire_scl = scl;
}


// Define global mock instances
WiFiClass WiFi;
bool mock_bno_begin_success = true; // Default to success
imu::Quaternion mock_bno_quat;

#endif // NATIVE_ENV

