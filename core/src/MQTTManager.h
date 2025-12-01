/**
 * @file MQTTManager.h
 * @brief MQTT通信管理クラス
 * @author sastle-com
 * @date 2025-12-01
 */

#pragma once

#include <WiFi.h>
#include <PubSubClient.h>
#include "ConfigManager.h"

namespace sastle {

/**
 * @class MQTTManager
 * @brief MQTT通信を管理するクラス
 * 
 * MQTTブローカーへの接続、パブリッシュ/サブスクライブ、
 * 自動再接続機能を提供します。IMUデータの送信やコマンド受信に使用されます。
 */
class MQTTManager {
public:
    MQTTManager();
    ~MQTTManager();
    
    /**
     * @brief MQTT接続を初期化
     * @param config 設定マネージャー参照
     * @return true 初期化成功, false 初期化失敗
     */
    bool begin(ConfigManager& config);
    
    /**
     * @brief MQTTブローカーに接続
     * @return true 接続成功, false 接続失敗
     */
    bool connect();
    
    /**
     * @brief MQTT接続状態を取得
     * @return true 接続中, false 未接続
     */
    bool isConnected();
    
    /**
     * @brief MQTTブローカーから切断
     */
    void disconnect();
    
    /**
     * @brief MQTT keep-aliveとメッセージ処理
     * @note メインループ内で定期的に呼び出す必要があります
     */
    void loop();
    
    /**
     * @brief メッセージをパブリッシュ
     * @param topic トピック名
     * @param payload ペイロード (JSON文字列など)
     * @param retained retain フラグ (デフォルト: false)
     * @return true パブリッシュ成功, false パブリッシュ失敗
     */
    bool publish(const char* topic, const char* payload, bool retained = false);
    
    /**
     * @brief トピックをサブスクライブ
     * @param topic サブスクライブするトピック名
     * @return true サブスクライブ成功, false サブスクライブ失敗
     */
    bool subscribe(const char* topic);
    
    /**
     * @brief トピックのサブスクライブを解除
     * @param topic サブスクライブ解除するトピック名
     * @return true 解除成功, false 解除失敗
     */
    bool unsubscribe(const char* topic);
    
    /**
     * @brief メッセージ受信時のコールバック関数を設定
     * @param callback コールバック関数ポインタ
     * @note コールバック関数のシグネチャ: void callback(char* topic, uint8_t* payload, unsigned int length)
     */
    void setCallback(void (*callback)(char*, uint8_t*, unsigned int));
    
    /**
     * @brief MQTTステータスを表示
     */
    void printStatus();
    
    /**
     * @brief クライアントIDを取得
     * @return クライアントID文字列
     */
    const char* getClientId() { return _clientId; }
    
private:
    WiFiClient _wifiClient;           ///< WiFiクライアント
    PubSubClient _mqttClient;         ///< MQTTクライアント
    
    char _broker[64];                 ///< ブローカーアドレス
    uint16_t _port;                   ///< ブローカーポート番号
    char _clientId[32];               ///< クライアントID
    char _username[32];               ///< 認証ユーザー名
    char _password[32];               ///< 認証パスワード
    
    bool _initialized;                ///< 初期化状態
    unsigned long _lastReconnectAttempt;  ///< 最終再接続試行時刻
    const unsigned long RECONNECT_INTERVAL = 5000; ///< 再接続間隔 (5秒)
    
    /**
     * @brief MQTT再接続処理
     * @return true 再接続成功, false 再接続失敗
     */
    bool _reconnect();
};

} // namespace sastle
