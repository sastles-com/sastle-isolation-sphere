#include "MQTTManager.h"
#include "MqttTopics.h"
#include <Arduino.h>

namespace sastle {

void MQTTManager::_deviceTopic(const char* suffix, char* out, size_t len) {
    snprintf(out, len, "sphere/%s/%s", _clientId, suffix);
}

bool MQTTManager::publishDevice(const char* suffix, const char* payload, bool retained) {
    char topic[64];
    _deviceTopic(suffix, topic, sizeof(topic));
    return publish(topic, payload, retained);
}

MQTTManager::MQTTManager() 
    : _mqttClient(_wifiClient),
      _port(1883),
      _initialized(false),
      _lastReconnectAttempt(0) {
    memset(_broker, 0, sizeof(_broker));
    memset(_clientId, 0, sizeof(_clientId));
    memset(_username, 0, sizeof(_username));
    memset(_password, 0, sizeof(_password));
}

MQTTManager::~MQTTManager() {
    if (_mqttClient.connected()) {
        _mqttClient.disconnect();
    }
}

bool MQTTManager::begin(ConfigManager& config) {
    Serial.println("\n=== MQTT Manager Initialization ===");
    
    // WiFi接続確認
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not connected! Cannot initialize MQTT.");
        return false;
    }
    
    // 設定読み込み
    auto wifiConfig = config.getWiFiConfig();
    auto sphereConfig = config.getSphereConfig();
    
    // ブローカー設定
    strncpy(_broker, wifiConfig.broker.c_str(), sizeof(_broker) - 1);
    _port = wifiConfig.mqtt_port;
    
    // クライアントID (sphere名を使用)
    strncpy(_clientId, sphereConfig.id.c_str(), sizeof(_clientId) - 1);
    
    // 認証情報 (オプション)
    // ユーザー名/パスワードが必要な場合は config.json に追加
    strcpy(_username, "");
    strcpy(_password, "");
    
    Serial.printf("Broker: %s:%d\n", _broker, _port);
    Serial.printf("Client ID: %s\n", _clientId);
    
    // MQTTクライアント設定
    _mqttClient.setServer(_broker, _port);
    _mqttClient.setKeepAlive(15);  // 15秒のキープアライブ
    _mqttClient.setSocketTimeout(5);  // 5秒のタイムアウト
    
    // バッファサイズ設定 (デフォルト256バイトでは不足)。
    // 送受信で共用されるため、受信最大 (led コマンドの pixels 配列) に合わせる。
    // 1要素 約39B なので 2048B で概ね 50 LED/メッセージまで個別指定できる。
    // 全 800 LED の一括更新は UDP の画像ストリーム経路 (オンデバイスレンダリング)
    // が担当するため、ここは部分更新用の容量で足りる。
    _mqttClient.setBufferSize(kMqttBufferSize);
    
    _initialized = true;
    
    // 初回接続試行
    if (connect()) {
        Serial.println("MQTT connected successfully!");
        return true;
    } else {
        Serial.println("Initial MQTT connection failed (will retry)");
        return true; // 初期化は成功、接続は後で再試行
    }
}

bool MQTTManager::connect() {
    if (!_initialized) {
        Serial.println("MQTT not initialized!");
        return false;
    }
    
    if (_mqttClient.connected()) {
        return true;
    }
    
    Serial.printf("Connecting to MQTT broker: %s:%d as %s...\n", _broker, _port, _clientId);
    
    bool connected = false;
    if (strlen(_username) > 0) {
        connected = _mqttClient.connect(_clientId, _username, _password);
    } else {
        connected = _mqttClient.connect(_clientId);
    }
    
    if (connected) {
        Serial.println("MQTT connected!");
        
        // 接続時のデフォルト購読
        // 1. デバイス固有のコマンドトピック
        char topic[64];
        _deviceTopic("command", topic, sizeof(topic));
        subscribe(topic);

        // 2. 全デバイス向けの状態トピック (StateManagerから配信)
        subscribe(topics::kAllState);

        // 3. 全デバイス向けのコマンドトピック
        subscribe(topics::kAllCommandWild);  // ワイルドカードで全コマンドを購読

        // ステータス送信（JSON形式）
        _deviceTopic("status", topic, sizeof(topic));
        char statusPayload[128];
        snprintf(statusPayload, sizeof(statusPayload), 
                 "{\"status\":\"online\",\"uptime\":%lu,\"timestamp\":%lu}",
                 millis() / 1000, millis());
        publish(topic, statusPayload, true);
        
        return true;
    } else {
        Serial.printf("MQTT connection failed, rc=%d\n", _mqttClient.state());
        return false;
    }
}

bool MQTTManager::_reconnect() {
    unsigned long now = millis();
    if (now - _lastReconnectAttempt < RECONNECT_INTERVAL) {
        return false;
    }
    
    _lastReconnectAttempt = now;
    return connect();
}

bool MQTTManager::isConnected() {
    return _mqttClient.connected();
}

void MQTTManager::disconnect() {
    if (_mqttClient.connected()) {
        // 切断前にオフラインステータス送信
        char topic[64];
        _deviceTopic("status", topic, sizeof(topic));
        publish(topic, "offline", true);
        
        _mqttClient.disconnect();
        Serial.println("MQTT disconnected");
    }
}

void MQTTManager::loop() {
    if (!_initialized) {
        return;
    }
    
    if (_mqttClient.connected()) {
        _mqttClient.loop();
    } else {
        // 自動再接続
        _reconnect();
    }
}

bool MQTTManager::publish(const char* topic, const char* payload, bool retained) {
    if (!_mqttClient.connected()) {
        Serial.println("MQTT not connected, cannot publish");
        return false;
    }
    
    bool result = _mqttClient.publish(topic, payload, retained);
    if (!result) {
        Serial.printf("[MQTT] Publish failed to %s\n", topic);
    }
    // IMUなど高頻度のログは出力しない（必要に応じてコマンド受信時のみログ出力）
    return result;
}

bool MQTTManager::subscribe(const char* topic) {
    if (!_mqttClient.connected()) {
        Serial.println("MQTT not connected, cannot subscribe");
        return false;
    }
    
    bool result = _mqttClient.subscribe(topic);
    if (result) {
        Serial.printf("[MQTT] Subscribed to: %s\n", topic);
    } else {
        Serial.printf("[MQTT] Subscribe failed: %s\n", topic);
    }
    return result;
}

bool MQTTManager::unsubscribe(const char* topic) {
    if (!_mqttClient.connected()) {
        return false;
    }
    
    bool result = _mqttClient.unsubscribe(topic);
    if (result) {
        Serial.printf("[MQTT] Unsubscribed from: %s\n", topic);
    }
    return result;
}

void MQTTManager::setCallback(void (*callback)(char*, uint8_t*, unsigned int)) {
    _mqttClient.setCallback(callback);
}

void MQTTManager::printStatus() {
    Serial.println("\n=== MQTT Status ===");
    Serial.printf("Broker: %s:%d\n", _broker, _port);
    Serial.printf("Client ID: %s\n", _clientId);
    Serial.printf("Connected: %s\n", _mqttClient.connected() ? "Yes" : "No");
    if (_mqttClient.connected()) {
        Serial.println("State: Connected");
    } else {
        int state = _mqttClient.state();
        Serial.printf("State: ");
        switch (state) {
            case -4: Serial.println("MQTT_CONNECTION_TIMEOUT"); break;
            case -3: Serial.println("MQTT_CONNECTION_LOST"); break;
            case -2: Serial.println("MQTT_CONNECT_FAILED"); break;
            case -1: Serial.println("MQTT_DISCONNECTED"); break;
            case 0:  Serial.println("MQTT_CONNECTED"); break;
            case 1:  Serial.println("MQTT_CONNECT_BAD_PROTOCOL"); break;
            case 2:  Serial.println("MQTT_CONNECT_BAD_CLIENT_ID"); break;
            case 3:  Serial.println("MQTT_CONNECT_UNAVAILABLE"); break;
            case 4:  Serial.println("MQTT_CONNECT_BAD_CREDENTIALS"); break;
            case 5:  Serial.println("MQTT_CONNECT_UNAUTHORIZED"); break;
            default: Serial.printf("Unknown (%d)\n", state); break;
        }
    }
    Serial.println("===================");
}

} // namespace sastle
