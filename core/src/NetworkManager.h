#ifndef __NETWORK_MANAGER_H__
#define __NETWORK_MANAGER_H__

#include "common.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include "ConfigManager.h"

namespace sastle {

class NetworkManager {
public:
    NetworkManager();
    virtual ~NetworkManager();

    // WiFi接続
    bool begin(ConfigManager& config);
    void disconnect();
    bool isConnected();
    
    // WiFi情報取得
    String getLocalIP();
    String getSSID();
    int getRSSI();
    
    // UDP通信
    bool beginUDP(uint16_t localPort);
    void endUDP();
    
    // UDP送信
    bool sendUDP(const IPAddress& ip, uint16_t port, const uint8_t* data, size_t length);
    bool sendUDP(const char* host, uint16_t port, const uint8_t* data, size_t length);
    
    // UDP受信
    int parsePacket();
    int available();
    int read(uint8_t* buffer, size_t length);
    IPAddress remoteIP();
    uint16_t remotePort();
    
    // ステータス表示
    void printStatus();
    
private:
    WiFiUDP udp;
    bool wifiConnected;
    bool udpStarted;
    uint16_t udpLocalPort;
    
    // WiFi接続ヘルパー
    bool connectWiFi(const String& ssid, const String& password, 
                     const IPAddress& localIP, const IPAddress& gateway, 
                     const IPAddress& subnet);
};

} // namespace sastle

#endif // __NETWORK_MANAGER_H__
