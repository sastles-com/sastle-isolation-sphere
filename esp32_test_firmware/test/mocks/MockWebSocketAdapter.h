#pragma once
#include "../src/interfaces/IWebSocketAdapter.h"

class MockWebSocketAdapter : public IWebSocketAdapter {
public:
  WebSocketCallback _callback;
  bool _connected = false;

  std::string _lastPayload;

  void begin(const char *host, uint16_t port, const char *url = "/") override {
    _connected = true;
  }

  void loop() override {}

  void sendTXT(const char *payload) override { _lastPayload = payload; }

  void onEvent(WebSocketCallback callback) override { _callback = callback; }

  // Helper to simulate incoming events
  void simulateEvent(WebSocketEventType type, uint8_t *payload, size_t length) {
    if (_callback) {
      _callback(type, payload, length);
    }
  }
};
