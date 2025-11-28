#pragma once
#include "interfaces/IWebSocketAdapter.h"
#include <WebSocketsClient.h>

class ESP32WebSocketAdapter : public IWebSocketAdapter {
public:
  void begin(const char *host, uint16_t port, const char *url) override {
    webSocket.begin(host, port, url);
    webSocket.setReconnectInterval(5000);
  }

  void loop() override { webSocket.loop(); }

  void sendTXT(const char *payload) override { webSocket.sendTXT(payload); }

  void onEvent(WebSocketCallback callback) override {
    _callback = callback;
    webSocket.onEvent([this](WStype_t type, uint8_t *payload, size_t length) {
      if (this->_callback) {
        WebSocketEventType evtType = WS_EVT_ERROR;
        switch (type) {
        case WStype_CONNECTED:
          evtType = WS_EVT_CONNECT;
          break;
        case WStype_DISCONNECTED:
          evtType = WS_EVT_DISCONNECT;
          break;
        case WStype_TEXT:
          evtType = WS_EVT_TEXT;
          break;
        default:
          evtType = WS_EVT_DATA;
          break;
        }
        this->_callback(evtType, payload, length);
      }
    });
  }

private:
  WebSocketsClient webSocket;
  WebSocketCallback _callback;
};
