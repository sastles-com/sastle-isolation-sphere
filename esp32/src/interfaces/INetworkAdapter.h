#pragma once
#include <string>

enum NetworkStatus {
  NETWORK_DISCONNECTED,
  NETWORK_CONNECTED,
  NETWORK_CONNECTING
};

class INetworkAdapter {
public:
  virtual ~INetworkAdapter() = default;
  virtual void begin(const char *ssid, const char *password) = 0;
  virtual NetworkStatus status() = 0;
  virtual std::string getLocalIP() = 0;
};
