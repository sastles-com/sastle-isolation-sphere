#pragma once
#include "interfaces/INetworkAdapter.h"
#include <WiFi.h>

class ESP32NetworkAdapter : public INetworkAdapter {
public:
  void begin(const char *ssid, const char *password) override {
    WiFi.begin(ssid, password);
  }

  NetworkStatus status() override {
    if (WiFi.status() == WL_CONNECTED) {
      return NETWORK_CONNECTED;
    } else {
      return NETWORK_DISCONNECTED;
    }
  }

  std::string getLocalIP() override {
    return std::string(WiFi.localIP().toString().c_str());
  }
};
