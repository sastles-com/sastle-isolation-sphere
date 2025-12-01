#pragma once
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>

// Mock Serial
class MockSerial {
public:
  void begin(unsigned long baud) {}
  void print(const char *s) { std::cout << s; }
  void print(int n) { std::cout << n; }
  void println(const char *s) { std::cout << s << std::endl; }
  void println(int n) { std::cout << n << std::endl; }
  void println() { std::cout << std::endl; }
  void printf(const char *format, ...) {
    // Simple mock, ignores args for now or use vsnprintf if needed
    std::cout << format << std::endl;
  }
};

#ifndef M5ATOM_MOCK_IMPL
extern MockSerial Serial;
#else
MockSerial Serial;
#endif

// Mock delay
inline void delay(unsigned long ms) {}

// Mock millis
inline unsigned long millis() { return 0; }

extern int lastToneFreq;
extern bool toneCalled;

inline void tone(uint8_t pin, unsigned int frequency,
                 unsigned long duration = 0) {
  lastToneFreq = frequency;
  toneCalled = true;
}
inline void noTone(uint8_t pin) {
  lastToneFreq = 0;
  toneCalled = false;
}

// Mock String
using String = std::string;
