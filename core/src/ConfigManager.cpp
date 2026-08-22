#include "ConfigManager.h"

// グローバルデバッグフラグ (main.cppで定義)
extern bool g_debugEnabled;

namespace sastle {

ConfigManager::ConfigManager() : doc(8192) {
}

ConfigManager::~ConfigManager() {
}

bool ConfigManager::loadConfig(const char* path) {
    Serial.printf("Loading config from: %s\n", path);
    
    String jsonStr;
    if (!FileManager::readFile(path, jsonStr)) {
        Serial.println("Failed to read config file");
        return false;
    }
    
    Serial.printf("Config file size: %d bytes\n", jsonStr.length());
    
    return parseJSON(jsonStr);
}

bool ConfigManager::parseJSON(const String& jsonStr) {
    DeserializationError error = deserializeJson(doc, jsonStr);
    
    if (error) {
        Serial.printf("JSON parse error: %s\n", error.c_str());
        return false;
    }
    
    Serial.println("Config parsed successfully");
    return true;
}

bool ConfigManager::saveConfig(const char* path) {
    String output;
    serializeJsonPretty(doc, output);
    
    if (FileManager::writeFile(path, output)) {
        Serial.printf("Config saved to: %s\n", path);
        return true;
    } else {
        Serial.println("Failed to save config");
        return false;
    }
}

SystemConfig ConfigManager::getSystemConfig() {
    SystemConfig config;
    config.debug = doc["system"]["debug"] | false;
    config.PSRAM = doc["system"]["PSRAM"] | false;
    
    // グローバルデバッグフラグを設定
    g_debugEnabled = config.debug;
    
    return config;
}

OTAConfig ConfigManager::getOTAConfig() {
    OTAConfig config;
    config.enabled = doc["system"]["ota"]["enabled"] | true;
    config.username = doc["system"]["ota"]["username"] | "admin";
    config.password = doc["system"]["ota"]["password"] | "";
    config.listen_port = doc["system"]["ota"]["listen_port"] | 3232;
    return config;
}

PathsConfig ConfigManager::getPathsConfig() {
    PathsConfig config;
    config.config = doc["system"]["paths"]["config"] | "/littlefs/config.json";
    config.images = doc["system"]["paths"]["images"] | "/littlefs/images/";
    config.opening = doc["system"]["paths"]["opening"] | "/littlefs/images/opening/";
    config.layout = doc["system"]["paths"]["layout"] | "/littlefs/led_layouts-5strip.csv";
    config.logs = doc["system"]["paths"]["logs"] | "/littlefs/logs/";
    return config;
}

WiFiConfig ConfigManager::getWiFiConfig() {
    WiFiConfig config;
    config.SSID = doc["wifi"]["SSID"] | "";
    config.password = doc["wifi"]["password"] | "";
    config.enabled = doc["wifi"]["enabled"] | true;
    config.broker = doc["wifi"]["broker"] | "";
    config.mqtt_port = doc["wifi"]["mqtt_port"] | 1883;
    config.udp_port = doc["wifi"]["udp_port"] | 8889;
    return config;
}

ImageConfig ConfigManager::getImageConfig() {
    ImageConfig config;
    config.width = doc["image"]["width"] | 320;
    config.height = doc["image"]["height"] | 160;
    config.format = doc["image"]["format"] | "RGB565";
    config.type = doc["image"]["type"] | "JPEG";
    return config;
}

UIConfig ConfigManager::getUIConfig() {
    UIConfig config;
    config.gesture_enabled = doc["ui"]["gesture_enabled"] | true;
    config.dim_on_entry = doc["ui"]["dim_on_entry"] | true;
    config.overlay_mode = doc["ui"]["overlay_mode"] | "overlay";
    config.brightness_profile = doc["ui"]["brightness_profile"] | "auto";
    return config;
}

SphereConfig ConfigManager::getSphereConfig() {
    SphereConfig config;
    config.id = doc["sphere"]["id"] | "";
    config.mac = doc["sphere"]["mac"] | "";
    config.static_ip = doc["sphere"]["static_ip"] | "";
    config.LED_enabled = doc["sphere"]["features"]["LED"] | false;
    config.IMU_type = doc["sphere"]["features"]["IMU"] | "";
    config.ui_enabled = doc["sphere"]["features"]["ui"] | false;
    
    // LCD config
    config.lcd.width = doc["sphere"]["features"]["LCD"]["width"] | 128;
    config.lcd.height = doc["sphere"]["features"]["LCD"]["height"] | 128;
    config.lcd.rotation = doc["sphere"]["features"]["LCD"]["rotation"] | 0;
    config.lcd.offset[0] = doc["sphere"]["features"]["LCD"]["offset"][0] | 0;
    config.lcd.offset[1] = doc["sphere"]["features"]["LCD"]["offset"][1] | 0;
    config.lcd.color_depth = doc["sphere"]["features"]["LCD"]["color_depth"] | 16;
    config.lcd.switch_enabled = doc["sphere"]["features"]["LCD"]["switch"] | true;
    config.lcd.debug = doc["sphere"]["features"]["LCD"]["debug"] | true;
    
    return config;
}

void ConfigManager::printConfig() {
    Serial.println("\n=== Configuration ===");
    
    SystemConfig sys = getSystemConfig();
    Serial.printf("System:\n");
    Serial.printf("  PSRAM: %s, Debug: %s\n", 
                  sys.PSRAM ? "enabled" : "disabled",
                  sys.debug ? "enabled" : "disabled");
    
    OTAConfig ota = getOTAConfig();
    Serial.printf("\nOTA: %s\n", ota.enabled ? "enabled" : "disabled");
    Serial.printf("  Username: %s, Port: %d\n", ota.username.c_str(), ota.listen_port);
    
    WiFiConfig wifi = getWiFiConfig();
    Serial.printf("\nWiFi: %s\n", wifi.enabled ? "enabled" : "disabled");
    Serial.printf("  SSID: %s\n", wifi.SSID.c_str());
    Serial.printf("  MQTT Broker: %s:%d\n", wifi.broker.c_str(), wifi.mqtt_port);
    Serial.printf("  UDP Port: %d\n", wifi.udp_port);
    
    ImageConfig img = getImageConfig();
    Serial.printf("\nImage: %dx%d %s (%s)\n", 
                  img.width, img.height, img.format.c_str(), img.type.c_str());
    
    UIConfig ui = getUIConfig();
    Serial.printf("\nUI:\n");
    Serial.printf("  Gesture: %s, Dim: %s\n",
                  ui.gesture_enabled ? "enabled" : "disabled",
                  ui.dim_on_entry ? "enabled" : "disabled");
    Serial.printf("  Overlay: %s, Brightness: %s\n",
                  ui.overlay_mode.c_str(), ui.brightness_profile.c_str());
    
    SphereConfig sphere = getSphereConfig();
    Serial.printf("\nSphere: %s\n", sphere.id.c_str());
    Serial.printf("  MAC: %s\n", sphere.mac.c_str());
    Serial.printf("  IP: %s\n", sphere.static_ip.c_str());
    Serial.printf("  LED: %s, IMU: %s, UI: %s\n",
                  sphere.LED_enabled ? "enabled" : "disabled",
                  sphere.IMU_type.c_str(),
                  sphere.ui_enabled ? "enabled" : "disabled");
    Serial.printf("  LCD: %dx%d, Depth: %d, Debug: %s\n",
                  sphere.lcd.width, sphere.lcd.height,
                  sphere.lcd.color_depth,
                  sphere.lcd.debug ? "enabled" : "disabled");
    
    PathsConfig paths = getPathsConfig();
    Serial.printf("\nPaths:\n");
    Serial.printf("  Config: %s\n", paths.config.c_str());
    Serial.printf("  Images: %s\n", paths.images.c_str());
    Serial.printf("  Layout: %s\n", paths.layout.c_str());
    
    Serial.println("====================\n");
}

} // namespace sastle
