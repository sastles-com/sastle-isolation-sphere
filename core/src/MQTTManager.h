#pragma once

#include <WiFi.h>
#include <PubSubClient.h>
#include "ConfigManager.h"

namespace sastle {

class MQTTManager {
public:
    MQTTManager();
    ~MQTTManager();
    
    // 初期化
    bool begin(ConfigManager& config);
    
    // 接続管理
    bool connect();
    bool isConnected();
    void disconnect();
    void loop();  // keep-aliveとメッセージ処理
    
    // メッセージ送受信
    bool publish(const char* topic, const char* payload, bool retained = false);
    bool subscribe(const char* topic);
    bool unsubscribe(const char* topic);
    
    // コールバック設定
    void setCallback(void (*callback)(char*, uint8_t*, unsigned int));
    
    // ステータス
    void printStatus();
    const char* getClientId() { return _clientId; }
    
private:
    WiFiClient _wifiClient;
    PubSubClient _mqttClient;
    
    char _broker[64];
    uint16_t _port;
    char _clientId[32];
    char _username[32];
    char _password[32];
    
    bool _initialized;
    unsigned long _lastReconnectAttempt;
    const unsigned long RECONNECT_INTERVAL = 5000; // 5秒
    
    bool _reconnect();
};

} // namespace sastle
