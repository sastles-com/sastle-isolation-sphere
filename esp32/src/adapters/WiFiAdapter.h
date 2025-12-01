#pragma once

#include "interfaces/INetworkAdapter.h"

#ifdef NATIVE_ENV
#include "../../test/mocks/WiFi.h" // Path to mock WiFi.h relative to src/adapters/
#else
#include <WiFi.h> // Path to real WiFi.h
#endif

class WiFiAdapter : public INetworkAdapter {
public:
  void begin(const char *ssid, const char *password) override;
  NetworkStatus status() override;
  std::string getLocalIP() override;
};
