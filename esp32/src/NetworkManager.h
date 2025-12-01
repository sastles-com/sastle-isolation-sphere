#pragma once
#include "ConfigManager.h"
#include "DeviceManager.h"
#include "interfaces/INetworkAdapter.h"
#include <M5Unified.h>

#ifdef NATIVE_ENV
#include "../test/mocks/AsyncMqttClient.h"
#else
#include <AsyncMqttClient.h>
#include <WiFiUDP.h>
#endif

class NetworkManager {
public:
  NetworkManager(INetworkAdapter &adapter, ConfigManager &configManager,
                 DeviceManager &deviceManager);
  void begin();
  void update();
  void publishImuData(float w, float x, float y, float z);

#ifdef NATIVE_ENV
public:
#else
private:
#endif
  INetworkAdapter &adapter;
  ConfigManager &configManager;
  DeviceManager &deviceManager;

  AsyncMqttClient mqttClient;
  
#ifndef NATIVE_ENV
  WiFiUDP udp;
#endif

  bool _mqttConnected = false;

  void onMqttConnect(bool sessionPresent);
  void onMqttMessage(char* topic, char* payload, size_t len);
};
