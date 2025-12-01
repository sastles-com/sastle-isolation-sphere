#ifndef __CONFIG_MANAGER_H__
#define __CONFIG_MANAGER_H__

#include "common.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include "FileManager.h"

namespace sastle {

// 構造体定義
struct SystemConfig {
    bool debug;
    bool PSRAM;
};

struct OTAConfig {
    bool enabled;
    String username;
    String password;
    int listen_port;
};

struct PathsConfig {
    String config;
    String images;
    String opening;
    String layout;
    String logs;
};

struct WiFiConfig {
    String SSID;
    String password;
    bool enabled;
    String broker;
    int mqtt_port;
    int udp_port;
};

struct ImageConfig {
    int width;
    int height;
    String format;
    String type;
};

struct UIConfig {
    bool gesture_enabled;
    bool dim_on_entry;
    String overlay_mode;
    String brightness_profile;
};

struct LCDConfig {
    int width;
    int height;
    int rotation;
    int offset[2];
    int color_depth;
    bool switch_enabled;
    bool debug;
};

struct SphereConfig {
    String id;
    String mac;
    String static_ip;
    bool LED_enabled;
    String IMU_type;
    LCDConfig lcd;
    bool ui_enabled;
};

class ConfigManager {
public:
    ConfigManager();
    virtual ~ConfigManager();

    // 設定ロード
    bool loadConfig(const char* path = "/config.json");
    
    // 設定保存
    bool saveConfig(const char* path = "/config.json");
    
    // JSONドキュメントを取得
    DynamicJsonDocument& getDocument() { return doc; }
    
    // 各セクションの取得
    SystemConfig getSystemConfig();
    OTAConfig getOTAConfig();
    PathsConfig getPathsConfig();
    WiFiConfig getWiFiConfig();
    ImageConfig getImageConfig();
    UIConfig getUIConfig();
    SphereConfig getSphereConfig();
    
    // 便利なゲッター
    bool isPSRAMEnabled() { return doc["system"]["PSRAM"] | false; }
    bool isDebugEnabled() { return doc["system"]["debug"] | false; }
    
    String getWiFiSSID() { return doc["wifi"]["SSID"] | ""; }
    String getWiFiPassword() { return doc["wifi"]["password"] | ""; }
    String getMQTTBroker() { return doc["wifi"]["broker"] | ""; }
    int getMQTTPort() { return doc["wifi"]["mqtt_port"] | 1883; }
    int getUDPPort() { return doc["wifi"]["udp_port"] | 8889; }
    
    String getSphereID() { return doc["sphere"]["id"] | ""; }
    String getSphereMAC() { return doc["sphere"]["mac"] | ""; }
    String getSphereIP() { return doc["sphere"]["static_ip"] | ""; }
    
    bool isLEDEnabled() { return doc["sphere"]["features"]["LED"] | false; }
    String getIMUType() { return doc["sphere"]["features"]["IMU"] | ""; }
    
    // デバッグ出力
    void printConfig();
    
private:
    DynamicJsonDocument doc{8192};  // 8KB buffer for config
    bool parseJSON(const String& jsonStr);
};

} // namespace sastle

#endif // __CONFIG_MANAGER_H__
