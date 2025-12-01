#pragma once

#include <string> // For std::string in native environment

// Minimal mock for IPAddress
struct IPAddress {
    uint8_t _bytes[4];
    std::string toString() { return "0.0.0.0"; }
};

// Minimal mock for wl_status_t enum
enum wl_status_t {
    WL_IDLE_STATUS = 0,
    WL_NO_SSID_AVAIL,
    WL_SCAN_COMPLETED,
    WL_CONNECTED,
    WL_CONNECT_FAILED,
    WL_CONNECTION_LOST,
    WL_DISCONNECTED,
    WL_AP_LISTENING,
    WL_AP_CONNECTED,
    WL_AP_FAILED
};

// Minimal mock for WiFiMode_t enum
enum {
    WIFI_MODE_NULL = 0,
    WIFI_MODE_STA,
    WIFI_MODE_AP,
    WIFI_MODE_APSTA,
    WIFI_MODE_MAX
};
const int WIFI_STA = WIFI_MODE_STA; // Define WIFI_STA using the enum

// Minimal mock for WiFiClass
class WiFiClass {
public:
    void mode(int m) {}
    void begin(const char* ssid, const char* password) {}
    wl_status_t status() { return WL_CONNECTED; } // Always return connected for mock
    IPAddress localIP() { return IPAddress(); }
    bool isConnected() { return true; } // Always return true for mock
};

extern WiFiClass WiFi; // Declare a global mock instance
