#include "NetworkManager.h"
#include "BoardConfig.h"

namespace sastle {

NetworkManager::NetworkManager() 
    : wifiConnected(false), udpStarted(false), udpLocalPort(0) {
}

NetworkManager::~NetworkManager() {
    disconnect();
}

bool NetworkManager::begin(ConfigManager& config) {
    Serial.println("\n=== Network Manager Initialization ===");
    
    WiFiConfig wifiConfig = config.getWiFiConfig();
    SphereConfig sphereConfig = config.getSphereConfig();
    
    if (!wifiConfig.enabled) {
        Serial.println("WiFi is disabled in config");
        return false;
    }
    
    Serial.printf("Connecting to WiFi: %s\n", wifiConfig.SSID.c_str());
    Serial.printf("Static IP: %s\n", sphereConfig.static_ip.c_str());
    
    // IPアドレスのパース
    IPAddress localIP, gateway, subnet;
    
    if (!localIP.fromString(sphereConfig.static_ip)) {
        Serial.println("ERROR: Invalid static IP address");
        return false;
    }
    
    // ゲートウェイとサブネットマスクを設定（通常は192.168.49.1と255.255.255.0）
    String ipBase = sphereConfig.static_ip.substring(0, sphereConfig.static_ip.lastIndexOf('.'));
    gateway.fromString(ipBase + ".1");
    subnet.fromString("255.255.255.0");
    
    Serial.printf("Gateway: %s\n", gateway.toString().c_str());
    Serial.printf("Subnet: %s\n", subnet.toString().c_str());
    
    // WiFi接続
    return connectWiFi(wifiConfig.SSID, wifiConfig.password, localIP, gateway, subnet);
}

bool NetworkManager::connectWiFi(const String& ssid, const String& password,
                                  const IPAddress& localIP, const IPAddress& gateway,
                                  const IPAddress& subnet) {
    // WiFiモード設定
    WiFi.mode(WIFI_STA);

    // モデム省電力を無効化 (重要)。
    // 省電力(既定ON)中はビーコン間でスリープし、再送のないUDPパケットを
    // 取りこぼす(TCP/MQTTは再送で生存、ICMP pingは応答型で通るため気付きにくい)。
    // 映像UDP受信のため必須。
    WiFi.setSleep(false);

    // 静的IP設定
    if (!WiFi.config(localIP, gateway, subnet)) {
        Serial.println("ERROR: Failed to configure static IP");
        return false;
    }
    
    // WiFi接続開始
    Serial.print("Connecting");
    WiFi.begin(ssid.c_str(), password.c_str());
    
    // 接続待ち（最大10秒）
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < kWifiConnectRetries) {
        delay(kWifiRetryDelayMs);
        Serial.print(".");
        attempts++;
    }
    Serial.println();
    
    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;

        // モデム省電力を無効化 (接続確立後に行うこと)。WiFi.begin() が PS を
        // 既定(MIN_MODEM)へ戻すため、begin前の設定は無効。省電力中はビーコン間で
        // スリープし再送のないUDPを取りこぼす(TCP/pingは生存)。映像UDP受信に必須。
        WiFi.setSleep(false);
        Serial.printf("WiFi sleep mode: %d (0=NONE/無効化成功)\n", (int)WiFi.getSleep());

        Serial.println("WiFi connected successfully!");
        Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("Gateway: %s\n", WiFi.gatewayIP().toString().c_str());
        Serial.printf("Subnet Mask: %s\n", WiFi.subnetMask().toString().c_str());
        Serial.printf("MAC Address: %s\n", WiFi.macAddress().c_str());
        Serial.printf("RSSI: %d dBm\n", WiFi.RSSI());
        return true;
    } else {
        Serial.println("WiFi connection failed!");
        wifiConnected = false;
        return false;
    }
}

void NetworkManager::disconnect() {
    if (udpStarted) {
        endUDP();
    }
    if (wifiConnected) {
        WiFi.disconnect();
        wifiConnected = false;
        Serial.println("WiFi disconnected");
    }
}

bool NetworkManager::isConnected() {
    wifiConnected = (WiFi.status() == WL_CONNECTED);
    return wifiConnected;
}

String NetworkManager::getLocalIP() {
    return WiFi.localIP().toString();
}

String NetworkManager::getSSID() {
    return WiFi.SSID();
}

int NetworkManager::getRSSI() {
    return WiFi.RSSI();
}

bool NetworkManager::beginUDP(uint16_t localPort) {
    if (!wifiConnected) {
        Serial.println("ERROR: WiFi not connected, cannot start UDP");
        return false;
    }
    
    Serial.printf("Starting UDP(Async) on port %d\n", localPort);

    udpLocalPort = localPort;

    // 受信データグラム用キューを確保 (バースト吸収)
    if (!_rxQueue) {
        _rxQueue = xQueueCreate(kRxQueueLen, sizeof(UdpDatagram));
        if (!_rxQueue) {
            Serial.println("ERROR: Failed to create UDP rx queue");
            return false;
        }
    }

    // AsyncUDP: コールバック(AsyncUDPタスク文脈, 直列)で各データグラムをキューへ。
    // (WiFiUDP の BSDソケットポーリング受信が本環境で機能しなかったため置換)
    if (_audp.listen(localPort)) {
        _audp.onPacket([this](AsyncUDPPacket packet) {
            size_t n = packet.length();
            if (n == 0 || n > kMaxDatagram) {
                return;
            }
            _cbDg.len = (uint16_t)n;
            memcpy(_cbDg.data, packet.data(), n);
            _rxRemoteIP = packet.remoteIP();
            _rxRemotePort = packet.remotePort();
            // キュー満杯時はドロップ (最新優先のため、必要なら拡張)
            xQueueSend(_rxQueue, &_cbDg, 0);
        });
        udpStarted = true;
        Serial.printf("UDP(Async) listening on port %d\n", localPort);
        return true;
    } else {
        Serial.println("ERROR: AsyncUDP listen failed");
        udpStarted = false;
        return false;
    }
}

void NetworkManager::endUDP() {
    if (udpStarted) {
        _audp.close();
        udpStarted = false;
        Serial.println("UDP stopped");
    }
}

bool NetworkManager::sendUDP(const IPAddress& ip, uint16_t port, const uint8_t* data, size_t length) {
    if (!udpStarted) {
        Serial.println("ERROR: UDP not started");
        return false;
    }
    
    if (udp.beginPacket(ip, port)) {
        size_t written = udp.write(data, length);
        if (udp.endPacket()) {
            // ログ削除（main.cppで出力）
            return true;
        }
    }
    
    Serial.println("ERROR: Failed to send UDP packet");
    return false;
}

bool NetworkManager::sendUDP(const char* host, uint16_t port, const uint8_t* data, size_t length) {
    if (!udpStarted) {
        Serial.println("ERROR: UDP not started");
        return false;
    }
    
    if (udp.beginPacket(host, port)) {
        size_t written = udp.write(data, length);
        if (udp.endPacket()) {
            Serial.printf("UDP sent %d bytes to %s:%d\n", written, host, port);
            return true;
        }
    }
    
    Serial.println("ERROR: Failed to send UDP packet");
    return false;
}

int NetworkManager::recvDatagram(uint8_t* buffer, size_t length) {
    if (!udpStarted || !_rxQueue) {
        return 0;
    }
    UdpDatagram dg;  // 呼び出し側(decodeタスク, 8192 stack)で確保
    if (xQueueReceive(_rxQueue, &dg, 0) != pdTRUE) {
        return 0;  // キュー空
    }
    size_t n = dg.len;
    if (n > length) n = length;
    memcpy(buffer, dg.data, n);
    return (int)n;
}

IPAddress NetworkManager::remoteIP() {
    return _rxRemoteIP;
}

uint16_t NetworkManager::remotePort() {
    return _rxRemotePort;
}

void NetworkManager::printStatus() {
    Serial.println("\n=== Network Status ===");
    
    if (isConnected()) {
        Serial.println("WiFi: Connected");
        Serial.printf("  SSID: %s\n", getSSID().c_str());
        Serial.printf("  IP: %s\n", getLocalIP().c_str());
        Serial.printf("  RSSI: %d dBm\n", getRSSI());
        Serial.printf("  MAC: %s\n", WiFi.macAddress().c_str());
    } else {
        Serial.println("WiFi: Disconnected");
    }
    
    if (udpStarted) {
        Serial.printf("UDP: Active (port %d)\n", udpLocalPort);
    } else {
        Serial.println("UDP: Inactive");
    }
    
    Serial.println("===================\n");
}

} // namespace sastle
