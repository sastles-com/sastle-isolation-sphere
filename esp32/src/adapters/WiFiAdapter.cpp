#include "adapters/WiFiAdapter.h"
#include "Arduino.h" // For Serial

void WiFiAdapter::begin(const char *ssid, const char *password) {
  Serial.printf("Attempting to connect to SSID: %s\n", ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
}

NetworkStatus WiFiAdapter::status() {
  if (WiFi.status() == WL_CONNECTED) {
    return NETWORK_CONNECTED;
  } else if (WiFi.status() == WL_DISCONNECTED) {
    return NETWORK_DISCONNECTED;
  }
  return NETWORK_CONNECTING;
}

std::string WiFiAdapter::getLocalIP() {
  return WiFi.localIP().toString().c_str();
}
