#pragma once
#include "Speaker.h"

class MockSpeaker : public Speaker {
public:
  bool playStartupCalled = false;
  bool playStartCalled = false;
  bool playEndCalled = false;
  bool updateCalled = false;

  MockSpeaker() : Speaker(0) {} // Restored base class constructor call
  void playStartup() { playStartupCalled = true; }
  void playStart() { playStartCalled = true; }
  void playEnd() { playEndCalled = true; }
  void update() { updateCalled = true; }
};
