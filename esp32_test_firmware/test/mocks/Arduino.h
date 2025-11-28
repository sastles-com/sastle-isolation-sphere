#pragma once

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdint.h>
#include <string>
#include <vector>
#include <cstdarg>

#ifndef PI
#define PI 3.1415926535897932384626433832795
// ESP Mock
class EspClass {
public:
    uint32_t getFreePsram() { return 4000000; } // Mock 4MB
};
extern EspClass ESP;

#endif

// Mock String class
class String : public std::string {
public:
  String() : std::string() {}
  String(const char *s) : std::string(s ? s : "") {}
  String(const std::string &s) : std::string(s) {}
  String(int i) : std::string(std::to_string(i)) {}
  String(float f) : std::string(std::to_string(f)) {}

  int toInt() const { return std::stoi(*this); }
  float toFloat() const { return std::stof(*this); }

  String substring(int begin, int end = -1) const {
    if (begin < 0)
      begin = 0;
    if (begin >= (int)length())
      return "";
    if (end == -1 || end > (int)length())
      return substr(begin);
    return substr(begin, end - begin);
  }

  int indexOf(char c, int fromIndex = 0) const {
    size_t pos = find(c, fromIndex);
    return (pos == std::string::npos) ? -1 : (int)pos;
  }

  void trim() {
    // Simple trim
    size_t first = find_first_not_of(' ');
    if (std::string::npos == first) {
      clear();
      return;
    }
    size_t last = find_last_not_of(' ');
    *this = substr(first, (last - first + 1));
  }
};

// Mock Serial
class Stream {
public:
    virtual int available() { return 0; }
    virtual int read() { return -1; }
    virtual int peek() { return -1; }
    virtual void flush() {}
    virtual size_t write(uint8_t) { return 1; }
    virtual size_t write(const uint8_t *buffer, size_t size) { return size; }
};

class SerialMock : public Stream {
public:
    void begin(unsigned long baud) {}
    void println(const char *msg) { std::cout << msg << std::endl; }
    void println(String msg) { std::cout << msg << std::endl; }
    void print(const char *msg) { std::cout << msg; }
    void printf(const char *format, ...) {
        char buffer[256];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        std::cout << buffer;
    }
};

extern SerialMock Serial;

// Mock delay
inline void delay(int ms) {}

// Mock math/utility
template <typename T>
const T &constrain(const T &amt, const T &low, const T &high) {
  return (amt < low) ? low : ((amt > high) ? high : amt);
}

inline long random(long min, long max) {
  return min + (std::rand() % (max - min));
}

inline long map(long x, long in_min, long in_max, long out_min, long out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

#define OUTPUT 1
#define INPUT 0

inline void pinMode(uint8_t pin, uint8_t mode) {}
inline void tone(uint8_t pin, unsigned int frequency, unsigned long duration = 0) {}
inline void noTone(uint8_t pin) {}
inline bool psramFound() { return true; } // Mock psramFound
