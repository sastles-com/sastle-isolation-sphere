#include "Speaker.h"

Speaker::Speaker(int pin) : pin(pin) {}

void Speaker::begin() {
  pinMode(pin, OUTPUT);
  // Setup PWM channel if using ledc, but simple tone() might suffice or manual
  // PWM ESP32 Arduino core has tone() but it uses ledc. Let's use ledc directly
  // for better control if needed, or just standard tone(). For M5AtomS3, we
  // might need to check if it has a built-in speaker driver or just a piezo.
  // Assuming piezo on a pin.
}

void Speaker::tone(unsigned int frequency, unsigned long duration) {
  ::tone(pin, frequency, duration);
  delay(duration); // Blocking for simplicity for now, can be improved
  noTone(pin);
}

void Speaker::playStartup() {
  tone(1000, 100);
  tone(1500, 100);
  tone(2000, 100);
}

void Speaker::playSelect() { tone(2000, 50); }

void Speaker::playStart() {
  tone(1000, 200);
  tone(2000, 400);
}

void Speaker::playEnd() {
  tone(2000, 200);
  tone(1000, 400);
}

void Speaker::update() {
  // Non-blocking logic would go here
}
