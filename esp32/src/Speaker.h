#pragma once
#include <Arduino.h>

class Speaker {
public:
  Speaker(int pin);
  virtual void begin();
  virtual void playStartup();
  void playSelect();
  virtual void playStart();
  virtual void playEnd();
  virtual void update(); // For non-blocking sound generation if needed

private:
  int pin;
  void tone(unsigned int frequency, unsigned long duration);
};
