#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>

// Enum mapping to WStype_t for abstraction
enum WebSocketEventType {
  WS_EVT_CONNECT,
  WS_EVT_DISCONNECT,
  WS_EVT_TEXT,
  WS_EVT_ERROR,
  WS_EVT_DATA
};

using WebSocketCallback = std::function<void(WebSocketEventType type,
                                             uint8_t *payload, size_t length)>;

class IWebSocketAdapter {
public:
  virtual ~IWebSocketAdapter() = default;
  virtual void begin(const char *host, uint16_t port, const char *url) = 0;
  virtual void loop() = 0;
  virtual void sendTXT(const char *payload) = 0;
  virtual void onEvent(WebSocketCallback callback) = 0;
};
