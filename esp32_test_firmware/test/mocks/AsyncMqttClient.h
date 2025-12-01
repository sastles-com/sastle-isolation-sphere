#pragma once
#include <functional>
#include <string>
#include <vector>
#include <Arduino.h>

typedef std::function<void(bool sessionPresent)> AsyncMqttClientConnectHandler;
typedef std::function<void(char* topic, char* payload, size_t len)> AsyncMqttClientMessageHandler;

class AsyncMqttClient {
public:
    bool _connected = false;
    String _serverIp;
    uint16_t _serverPort;
    String _clientId;
    
    AsyncMqttClientConnectHandler _onConnect;
    AsyncMqttClientMessageHandler _onMessage;

    void onConnect(AsyncMqttClientConnectHandler handler) {
        _onConnect = handler;
    }
    
    void onMessage(AsyncMqttClientMessageHandler handler) {
        _onMessage = handler;
    }
    
    void setServer(const char* ip, uint16_t port) {
        _serverIp = ip;
        _serverPort = port;
    }
    
    void setClientId(const char* id) {
        _clientId = id;
    }
    
    bool _connectCalled = false;

    void connect() {
        _connectCalled = true;
        // In a real mock, we might trigger onConnect immediately or via a helper
    }
    
    void publish(const char* topic, uint8_t qos, bool retain, const char* payload) {
        // Record published message
    }
    
    // Helper for tests
    void simulateConnect() {
        _connected = true;
        if (_onConnect) _onConnect(false);
    }
    
    bool connected() {
        return _connected;
    }
};
