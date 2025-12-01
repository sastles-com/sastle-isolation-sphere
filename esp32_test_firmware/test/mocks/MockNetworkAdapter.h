#pragma once
#include "../src/interfaces/INetworkAdapter.h"

class MockNetworkAdapter : public INetworkAdapter {
public:
  NetworkStatus _status = NETWORK_DISCONNECTED;
  std::string _ip = "0.0.0.0";
  std::string _ssid;
  std::string _password;

  bool _connectImmediately = false;
  bool _failConnection = false;

  void begin(const char *ssid, const char *password) override {
    _ssid = ssid;
    _password = password;
    if (_connectImmediately) {
      _status = NETWORK_CONNECTED;
    } else if (_failConnection) {
      _status = NETWORK_DISCONNECTED;
    } else {
      _status = NETWORK_CONNECTING;
    }
  }

  NetworkStatus status() override { return _status; }

  std::string getLocalIP() override { return _ip; }

  // Helper for tests
  void setStatus(NetworkStatus s) { _status = s; }
};
