#include "NetworkManager.h"

NetworkManager::NetworkManager(INetworkAdapter &networkAdapter,
                               ConfigManager &configMgr, DeviceManager &devMgr)
    : adapter(networkAdapter), configManager(configMgr), deviceManager(devMgr) {}

void NetworkManager::begin() {
  String ssid = configManager.getWifiSSID();
  String password = configManager.getWifiPassword();

  if (ssid.length() > 0) {
      adapter.begin(ssid.c_str(), password.c_str());
  }

  // Setup MQTT
  mqttClient.setServer(configManager.getAgentIP().c_str(), configManager.getAgentPort());
  mqttClient.setClientId(configManager.getNodeName().c_str());
  
  // Using lambda or std::bind for callbacks if AsyncMqttClient supports it, 
  // or static wrapper. The mock supports std::function.
  // Real AsyncMqttClient uses function pointers or std::function depending on fork.
  // Assuming standard AsyncMqttClient which uses callbacks.
  // We'll use a lambda that captures 'this' if the library supports it, 
  // otherwise we need a static instance pointer.
  // For now, let's assume we can bind.
  
  using namespace std::placeholders;
  mqttClient.onConnect(std::bind(&NetworkManager::onMqttConnect, this, _1));
  mqttClient.onMessage(std::bind(&NetworkManager::onMqttMessage, this, _1, _2, _3));
}

void NetworkManager::update() {
    // Check WiFi status and connect MQTT if needed
    if (adapter.status() == NETWORK_CONNECTED) {
        if (!mqttClient.connected()) {
            mqttClient.connect();
        }
    }
    
    // UDP handling will go here
}

void NetworkManager::publishImuData(float w, float x, float y, float z) {
    if (mqttClient.connected()) {
        char payload[100];
        snprintf(payload, sizeof(payload), "{\"w\":%.3f,\"x\":%.3f,\"y\":%.3f,\"z\":%.3f}", w, x, y, z);
        mqttClient.publish("sphere/imu", 0, false, payload);
    }
}

void NetworkManager::onMqttConnect(bool sessionPresent) {
    _mqttConnected = true;
    // Subscribe to topics
    // mqttClient.subscribe("sphere/ui", 0);
}

void NetworkManager::onMqttMessage(char* topic, char* payload, size_t len) {
    // Handle message
}
