#include "ConfigManager.h"
#include <WiFi.h>

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
    // 新しい JSON になったので自機エントリのキャッシュを捨てる (次回参照時に再解決)
    _sphereResolved = false;
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

String ConfigManager::_normalizeMac(const String& mac) {
    // "F0:9E:9E:32:67:D0" / "f0-9e-9e-32-67-d0" / "f09e9e3267d0" を同一視する
    String out;
    out.reserve(12);
    for (size_t i = 0; i < mac.length(); ++i) {
        const char c = mac[i];
        if (c == ':' || c == '-' || c == '.' || c == ' ') continue;
        out += (char)toupper((unsigned char)c);
    }
    return out;
}

String ConfigManager::getSelfMac() {
    if (_selfMac.length() == 0) {
        // WiFi 未初期化 (WIFI_MODE_NULL) でも efuse から読めるため、
        // NetworkManager::begin() より前のこの時点で参照して問題ない。
        _selfMac = WiFi.macAddress();
    }
    return _selfMac;
}

void ConfigManager::_parseSphereEntry(JsonVariantConst src, SphereConfig& out) {
    out.id = src["id"] | "";
    out.mac = src["mac"] | "";
    out.static_ip = src["static_ip"] | "";
    out.LED_enabled = src["features"]["LED"] | false;
    out.IMU_type = src["features"]["IMU"] | "";
    out.ui_enabled = src["features"]["ui"] | false;

    JsonVariantConst lcd = src["features"]["LCD"];
    out.lcd.width = lcd["width"] | 128;
    out.lcd.height = lcd["height"] | 128;
    out.lcd.rotation = lcd["rotation"] | 0;
    out.lcd.offset[0] = lcd["offset"][0] | 0;
    out.lcd.offset[1] = lcd["offset"][1] | 0;
    out.lcd.color_depth = lcd["color_depth"] | 16;
    out.lcd.switch_enabled = lcd["switch"] | true;
    out.lcd.debug = lcd["debug"] | true;
}

void ConfigManager::_resolveSphere() {
    _sphereResolved = true;  // 失敗しても再解決ループに入らないよう先に立てる

    const String selfMac = getSelfMac();
    const String selfKey = _normalizeMac(selfMac);

    JsonArrayConst spheres = doc["spheres"].as<JsonArrayConst>();
    if (!spheres.isNull() && spheres.size() > 0) {
        // 1. 自機 MAC と一致するエントリ (通常経路)
        for (JsonVariantConst entry : spheres) {
            const String key = _normalizeMac(entry["mac"] | "");
            if (key.length() > 0 && key == selfKey) {
                _parseSphereEntry(entry, _sphere);
                Serial.printf("[Config] Sphere resolved by MAC: %s (mac=%s)\n",
                              _sphere.id.c_str(), selfMac.c_str());
                return;
            }
        }

#ifdef SPHERE_ID
        // 2. ビルドフラグ -D SPHERE_ID="sphereNNN" 指定 (MAC 未採番の新基板用)
        for (JsonVariantConst entry : spheres) {
            if (String(SPHERE_ID) == String(entry["id"] | "")) {
                _parseSphereEntry(entry, _sphere);
                Serial.printf("[Config] Sphere resolved by build flag SPHERE_ID: %s (mac=%s)\n",
                              _sphere.id.c_str(), selfMac.c_str());
                return;
            }
        }
        Serial.printf("[Config] WARNING: SPHERE_ID=\"%s\" is not in spheres[]\n", SPHERE_ID);
#endif

        // 3. mac が空の「未採番スロット」があればそれを使う。
        //    既に MAC 登録済みのエントリ (稼働中の core) と id/static_ip が衝突すると
        //    同一 IP・同一 MQTT clientId の2台になり、ブローカーが古い接続を切って
        //    双方が再接続を繰り返す (docs/HANDOFF_2026-08-24: 「原因不明の瞬断」)。
        //    新基板を足すときは空 mac + 未使用 IP のスロットを1つ用意しておけば、
        //    起動ログの "Own MAC:" を後から記入するだけで衝突せずに立ち上がる。
        for (JsonVariantConst entry : spheres) {
            if (_normalizeMac(entry["mac"] | "").length() == 0) {
                _parseSphereEntry(entry, _sphere);
                Serial.printf("[Config] WARNING: own MAC %s not registered; using unclaimed slot '%s'\n",
                              selfMac.c_str(), _sphere.id.c_str());
                Serial.printf("[Config]          -> spheres[] の \"%s\" に \"mac\": \"%s\" を記入してください\n",
                              _sphere.id.c_str(), selfMac.c_str());
                return;
            }
        }

        // 4. 先頭エントリにフォールバック。稼働中の core と IP/clientId が衝突する
        //    可能性があるため最後の手段 (上記の衝突を招くので警告を目立たせる)。
        _parseSphereEntry(spheres[0], _sphere);
        Serial.printf("[Config] WARNING: own MAC %s not found in spheres[]; using '%s'.\n",
                      selfMac.c_str(), _sphere.id.c_str());
        Serial.printf("[Config]          -> config.json の spheres[] に \"mac\": \"%s\" を登録してください\n",
                      selfMac.c_str());
        return;
    }

    // 5. 旧形式の単一キー "sphere" (spheres[] 移行前の config.json との後方互換)
    JsonVariantConst legacy = doc["sphere"];
    if (!legacy.isNull()) {
        _parseSphereEntry(legacy, _sphere);
        Serial.printf("[Config] Legacy single \"sphere\" entry used: %s (mac=%s)\n",
                      _sphere.id.c_str(), selfMac.c_str());
        return;
    }

    // どちらも無い: 空の既定値 (MQTT clientId が空になるため接続は失敗する)
    _parseSphereEntry(JsonVariantConst(), _sphere);
    Serial.printf("[Config] ERROR: no \"spheres\"/\"sphere\" entry in config.json (mac=%s)\n",
                  selfMac.c_str());
}

SphereConfig ConfigManager::getSphereConfig() {
    if (!_sphereResolved) {
        _resolveSphere();
    }
    return _sphere;
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
    Serial.printf("  Own MAC: %s (config.json の spheres[].mac と照合)\n", getSelfMac().c_str());
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

    Serial.printf("\nParams (startup defaults):\n");
    Serial.printf("  Brightness: %d%%, Speed: %d%%, Hue: %d, Saturation: %d%%\n",
                  getParamBrightness(), getParamSpeed(),
                  getParamHue(), getParamSaturation());

    Serial.println("====================\n");
}

} // namespace sastle
