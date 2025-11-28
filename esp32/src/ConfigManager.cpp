#include "ConfigManager.h"
#ifdef ESP32
#include <LittleFS.h>
#endif

ConfigManager::ConfigManager() {
  // Set defaults
  config.agent_port = 1883;
  config.node_name = "isolation_sphere_esp32";
  config.lcd_enable = false;
}

bool ConfigManager::begin() {
#ifdef ESP32
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS Mount Failed");
    return false;
  }
#endif
  return true;
}

bool ConfigManager::loadConfig() {
#ifdef ESP32
  File file = LittleFS.open("/config.json", "r");
  if (!file) {
    Serial.println("Failed to open config.json");
    return false;
  }
  String json = file.readString();
  file.close();
  return parseConfig(json);
#else
  return false;
#endif
}

bool ConfigManager::parseConfig(const String &json) {
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, json);
  if (error) {
    Serial.println("Failed to parse config.json");
    return false;
  }

  // Update keys to match new spec
  if (doc.containsKey("network")) {
    config.wifi_ssid = doc["network"]["ssid"].as<String>();
    config.wifi_password = doc["network"]["password"].as<String>();
  } else if (doc.containsKey("wifi")) { // Fallback for old config
    config.wifi_ssid = doc["wifi"]["ssid"].as<String>();
    config.wifi_password = doc["wifi"]["password"].as<String>();
  }

  if (doc.containsKey("agent")) {
    config.agent_ip = doc["agent"]["ip"].as<String>();
    config.agent_port = doc["agent"]["port"] | 1883;
  } else if (doc.containsKey("ros2")) { // Fallback
    config.agent_ip = doc["ros2"]["agent_ip"].as<String>();
    config.agent_port = doc["ros2"]["agent_port"] | 8888;
  }

  if (doc.containsKey("node_name")) {
    config.node_name = doc["node_name"].as<String>();
  } else if (doc.containsKey("ros2") && doc["ros2"].containsKey("node_name")) {
    config.node_name = doc["ros2"]["node_name"].as<String>();
  }

  if (doc.containsKey("debug")) {
    config.lcd_enable = doc["debug"]["lcd_enable"] | false;
  }

  return true;
}

bool ConfigManager::loadLayout() {
#ifdef ESP32
  File file = LittleFS.open("/led_layout.csv", "r");
  if (!file) {
    Serial.println("Failed to open led_layout.csv");
    return false;
  }
  String csv = file.readString();
  file.close();
  return parseLayout(csv);
#else
  return false;
#endif
}

bool ConfigManager::parseLayout(const String &csv) {
  layout.clear();

  int startIndex = 0;
  int endIndex = csv.indexOf('\n');

  while (endIndex >= 0 || startIndex < csv.length()) {
    String line;
    if (endIndex >= 0) {
      line = csv.substring(startIndex, endIndex);
    } else {
      line = csv.substring(startIndex);
    }
    line.trim();

    // Update indices for next loop
    if (endIndex >= 0) {
      startIndex = endIndex + 1;
      endIndex = csv.indexOf('\n', startIndex);
    } else {
      startIndex = csv.length(); // End loop
    }

    if (line.length() == 0)
      continue;

    // Simple CSV parsing
    int firstComma = line.indexOf(',');
    int secondComma = line.indexOf(',', firstComma + 1);
    int thirdComma = line.indexOf(',', secondComma + 1);
    int fourthComma = line.indexOf(',', thirdComma + 1);
    int fifthComma = line.indexOf(',', fourthComma + 1);

    // Format: faceID, stripID, stripIndex, x, y, z
    // We need at least 5 commas for 6 values
    if (fifthComma > 0) {
      LEDPosition pos;
      // We map stripID * 1000 + stripIndex to ID for now, or just keep raw
      // The struct has 'id', 'x', 'y', 'z'.
      // Let's assume ID is just a sequential index or we need to update struct.
      // For now, let's just parse x, y, z from the last 3 columns.

      pos.x = line.substring(thirdComma + 1, fourthComma).toFloat();
      pos.y = line.substring(fourthComma + 1, fifthComma).toFloat();
      pos.z = line.substring(fifthComma + 1).toFloat();

      // ID generation logic (temporary)
      pos.id = layout.size();

      layout.push_back(pos);
    } else if (thirdComma > 0 && fourthComma < 0) {
      // Old format: id, x, y, z ? Or just x,y,z?
      // Let's support the dummy format I created: 0,0,0,x,y,z
    }
  }
  return layout.size() > 0;
}

String ConfigManager::getWifiSSID() const { return config.wifi_ssid; }
String ConfigManager::getWifiPassword() const { return config.wifi_password; }
String ConfigManager::getAgentIP() const { return config.agent_ip; }
int ConfigManager::getAgentPort() const { return config.agent_port; }
String ConfigManager::getNodeName() const { return config.node_name; }
bool ConfigManager::getLcdDebug() const { return config.lcd_enable; }

const std::vector<LEDPosition> &ConfigManager::getLayout() const {
  return layout;
}
