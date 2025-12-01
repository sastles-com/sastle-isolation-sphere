#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>

struct LEDPosition {
  int id;
  float x;
  float y;
  float z;
};

struct ConfigData {
  String wifi_ssid;
  String wifi_password;
  String agent_ip;
  int agent_port;
  String node_name;
  bool lcd_enable;
};

class ConfigManager {
public:
  ConfigManager();
  bool begin();
  bool loadConfig();
  bool loadLayout();

  // Public for testing
  bool parseConfig(const String &json);
  bool parseLayout(const String &csv);

  String getWifiSSID() const;
  String getWifiPassword() const;
  String getAgentIP() const;
  int getAgentPort() const;
  String getNodeName() const;
  bool getLcdDebug() const;

  const std::vector<LEDPosition> &getLayout() const;

private:
  ConfigData config;
  std::vector<LEDPosition> layout;
};
