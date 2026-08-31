/**
 * @file ConfigManager.h
 * @brief JSON設定ファイル管理クラス
 * @author sastle-com
 * @date 2025-12-01
 */

#ifndef __CONFIG_MANAGER_H__
#define __CONFIG_MANAGER_H__

#include "common.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include "FileManager.h"

namespace sastle {

/**
 * @struct SystemConfig
 * @brief システム基本設定
 */
struct SystemConfig {
    bool debug;          ///< デバッグモード有効化
    bool PSRAM;          ///< PSRAM使用フラグ
};

/**
 * @struct OTAConfig
 * @brief OTA (Over-The-Air) アップデート設定
 */
struct OTAConfig {
    bool enabled;        ///< OTA有効化
    String username;     ///< OTA認証ユーザー名
    String password;     ///< OTA認証パスワード
    int listen_port;     ///< OTAリッスンポート
};

/**
 * @struct PathsConfig
 * @brief ファイルシステムパス設定
 */
struct PathsConfig {
    String config;       ///< 設定ファイルパス
    String images;       ///< 画像ディレクトリパス
    String opening;      ///< オープニング画像パス
    String layout;       ///< LEDレイアウトファイルパス
    String logs;         ///< ログディレクトリパス
};

/**
 * @struct WiFiConfig
 * @brief WiFi/ネットワーク設定
 */
struct WiFiConfig {
    String SSID;         ///< WiFi SSID
    String password;     ///< WiFiパスワード
    bool enabled;        ///< WiFi有効化
    String broker;       ///< MQTTブローカーアドレス
    int mqtt_port;       ///< MQTTポート番号
    int udp_port;        ///< UDPポート番号
};

/**
 * @struct ImageConfig
 * @brief 画像設定
 */
struct ImageConfig {
    int width;           ///< 画像幅
    int height;          ///< 画像高さ
    String format;       ///< 画像フォーマット (例: RGB565)
    String type;         ///< 画像タイプ (例: JPEG)
};

/**
 * @struct UIConfig
 * @brief ユーザーインターフェース設定
 */
struct UIConfig {
    bool gesture_enabled;        ///< ジェスチャー入力有効化
    bool dim_on_entry;           ///< UI開始時の減光
    String overlay_mode;         ///< オーバーレイモード
    String brightness_profile;   ///< 輝度プロファイル
};

/**
 * @struct LCDConfig
 * @brief LCDディスプレイ設定
 */
struct LCDConfig {
    int width;           ///< LCD幅
    int height;          ///< LCD高さ
    int rotation;        ///< 画面回転角度
    int offset[2];       ///< 表示オフセット [x, y]
    int color_depth;     ///< 色深度 (ビット)
    bool switch_enabled; ///< LCD切替有効化
    bool debug;          ///< LCDデバッグ表示
};

/**
 * @struct SphereConfig
 * @brief 球体デバイス固有設定
 */
struct SphereConfig {
    String id;           ///< デバイスID
    String mac;          ///< MACアドレス
    String static_ip;    ///< 静的IPアドレス
    bool LED_enabled;    ///< LED制御有効化
    String IMU_type;     ///< IMUセンサータイプ (例: BNO055)
    LCDConfig lcd;       ///< LCD設定
    bool ui_enabled;     ///< UI有効化
};

/**
 * @class ConfigManager
 * @brief JSON設定ファイルの読み込み・管理クラス
 * 
 * LittleFSから設定ファイル(config.json)を読み込み、
 * 構造化されたデータとしてアクセスを提供します。
 */
class ConfigManager {
public:
    ConfigManager();
    virtual ~ConfigManager();

    /**
     * @brief 設定ファイルをロード
     * @param path 設定ファイルパス (デフォルト: "/config.json")
     * @return true ロード成功, false ロード失敗
     */
    bool loadConfig(const char* path = "/config.json");
    
    /**
     * @brief 設定ファイルを保存
     * @param path 設定ファイルパス (デフォルト: "/config.json")
     * @return true 保存成功, false 保存失敗
     */
    bool saveConfig(const char* path = "/config.json");
    
    /**
     * @brief JSONドキュメントを取得
     * @return DynamicJsonDocument参照
     */
    DynamicJsonDocument& getDocument() { return doc; }
    
    /**
     * @brief システム設定を取得
     * @return SystemConfig構造体
     */
    SystemConfig getSystemConfig();
    
    /**
     * @brief OTA設定を取得
     * @return OTAConfig構造体
     */
    OTAConfig getOTAConfig();
    
    /**
     * @brief パス設定を取得
     * @return PathsConfig構造体
     */
    PathsConfig getPathsConfig();
    
    /**
     * @brief WiFi設定を取得
     * @return WiFiConfig構造体
     */
    WiFiConfig getWiFiConfig();
    
    /**
     * @brief 画像設定を取得
     * @return ImageConfig構造体
     */
    ImageConfig getImageConfig();
    
    /**
     * @brief UI設定を取得
     * @return UIConfig構造体
     */
    UIConfig getUIConfig();
    
    /**
     * @brief 自機の球体設定を取得
     * @return SphereConfig構造体
     *
     * config.json の spheres[] から「自機の MAC アドレスに一致するエントリ」を
     * 選んで返す。これにより全 core が同一の config.json (uploadfs) を共有できる。
     * 解決結果はキャッシュされ、初回呼び出し時に選択理由をログ出力する。
     *
     * 解決順序:
     *   1. spheres[] のうち mac が自機 MAC と一致するエントリ
     *   2. ビルドフラグ -D SPHERE_ID="sphereNNN" と id が一致するエントリ
     *      (新基板で MAC が未採番のときのブートストラップ用)
     *   3. mac が空の未採番スロット (警告ログ付き。稼働中 core との IP/clientId
     *      衝突を避けるため spheres[0] より先に選ぶ)
     *   4. spheres[0] (警告ログ付き。衝突の可能性あり)
     *   5. 旧形式の単一キー "sphere" (後方互換)
     */
    SphereConfig getSphereConfig();

    /**
     * @brief 自機の WiFi STA MAC アドレスを "AA:BB:CC:DD:EE:FF" 形式で取得
     * @note WiFi 未初期化でも efuse から読めるため config 解決時に使用できる。
     */
    String getSelfMac();
    
    /**
     * @brief PSRAM有効状態を取得
     * @return true 有効, false 無効
     */
    bool isPSRAMEnabled() { return doc["system"]["PSRAM"] | false; }
    
    /**
     * @brief デバッグモード状態を取得
     * @return true 有効, false 無効
     */
    bool isDebugEnabled() { return doc["system"]["debug"] | false; }
    
    String getWiFiSSID() { return doc["wifi"]["SSID"] | ""; }
    String getWiFiPassword() { return doc["wifi"]["password"] | ""; }
    String getMQTTBroker() { return doc["wifi"]["broker"] | ""; }
    int getMQTTPort() { return doc["wifi"]["mqtt_port"] | 1883; }
    int getUDPPort() { return doc["wifi"]["udp_port"] | 8889; }
    
    // sphere 系は spheres[] から自機エントリを解決した結果を返す (getSphereConfig 経由)
    String getSphereID() { return getSphereConfig().id; }
    String getSphereMAC() { return getSphereConfig().mac; }
    String getSphereIP() { return getSphereConfig().static_ip; }
    
    bool isLEDEnabled() { return getSphereConfig().LED_enabled; }
    String getIMUType() { return getSphereConfig().IMU_type; }
    
    String getLayoutPath() { return doc["system"]["paths"]["layout"] | "/led_layouts-5strip.csv"; }
    String getImagesPath() { return doc["system"]["paths"]["images"] | "/images/"; }
    String getOpeningPath() { return doc["system"]["paths"]["opening"] | "/images/opening/"; }

    // 起動オープニングパターン (LEDManager::playOpening)。スキップは enabled=false。
    bool getOpeningActionEnabled() { return doc["system"]["opening_action"]["enabled"] | true; }
    uint16_t getOpeningActionDurationMs() { return doc["system"]["opening_action"]["duration_ms"] | 1200; }
    
    int getImageWidth() { return doc["image"]["width"] | 320; }
    int getImageHeight() { return doc["image"]["height"] | 160; }
    String getImageFormat() { return doc["image"]["format"] | "RGB565"; }
    String getImageType() { return doc["image"]["type"] | "JPEG"; }
    
    /**
     * @brief LCD表示デバッグモードを取得
     * @return true デバッグ表示有効, false 無効
     */
    bool getLCDDebugEnabled() { return getSphereConfig().lcd.debug; }

    // params ブロック = 表示パラメータの起動時デフォルト (CommandHandler::_params)。
    // MQTT の params コマンドで実行時に上書きされる。fallback は server 側
    // (config_service.DEFAULT_PARAMS) と同じ値に揃えておく。
    uint8_t getParamBrightness() { return doc["params"]["brightness"] | 50; }
    uint8_t getParamSpeed() { return doc["params"]["speed"] | 50; }
    uint16_t getParamHue() { return doc["params"]["hue"] | 120; }
    uint8_t getParamSaturation() { return doc["params"]["saturation"] | 100; }

    // LEDカラーのマルチサンプリング (中心+半径R円周上N点を画像空間で平均)。
    // 実機でちらつき低減↔ボケ/負荷のトレードオフを調整するための可変パラメータ。
    bool getLedMultisampleEnabled() { return doc["led"]["multisample"]["enabled"] | true; }
    float getLedMultisampleRadius() { return doc["led"]["multisample"]["radius_px"] | 2.0f; }
    uint8_t getLedMultisamplePoints() { return doc["led"]["multisample"]["points"] | 6; }

    // デバッグ出力
    void printConfig();
    
private:
    DynamicJsonDocument doc{8192};  // 8KB buffer for config
    bool parseJSON(const String& jsonStr);

    SphereConfig _sphere;            ///< 解決済みの自機エントリ (キャッシュ)
    bool _sphereResolved = false;    ///< _sphere が解決済みか
    String _selfMac;                 ///< 自機 MAC (キャッシュ)

    /// spheres[] から自機エントリを解決して _sphere に格納する
    void _resolveSphere();
    /// JSON の1エントリを SphereConfig へパースする
    static void _parseSphereEntry(JsonVariantConst src, SphereConfig& out);
    /// MAC 比較用の正規化 (区切り文字除去 + 大文字化)
    static String _normalizeMac(const String& mac);
};

} // namespace sastle

#endif // __CONFIG_MANAGER_H__
