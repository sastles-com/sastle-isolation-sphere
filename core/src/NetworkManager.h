/**
 * @file NetworkManager.h
 * @brief WiFi接続とUDP通信管理クラス
 * @author sastle-com
 * @date 2025-12-01
 */

#ifndef __NETWORK_MANAGER_H__
#define __NETWORK_MANAGER_H__

#include "common.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <AsyncUDP.h>
#include "ConfigManager.h"

namespace sastle {

/**
 * @class NetworkManager
 * @brief WiFi接続とUDP通信を管理するクラス
 * 
 * 静的IPでのWiFi接続、UDPパケットの送受信機能を提供します。
 * 主にIMUデータの受信と映像データの送信に使用されます。
 */
class NetworkManager {
public:
    NetworkManager();
    virtual ~NetworkManager();

    /**
     * @brief WiFi接続を初期化
     * @param config 設定マネージャー参照
     * @return true 接続成功, false 接続失敗
     */
    bool begin(ConfigManager& config);
    
    /**
     * @brief WiFi接続を切断
     */
    void disconnect();
    
    /**
     * @brief WiFi接続状態を取得
     * @return true 接続中, false 未接続
     */
    bool isConnected();
    
    /**
     * @brief ローカルIPアドレスを取得
     * @return IPアドレス文字列
     */
    String getLocalIP();
    
    /**
     * @brief 接続中のSSIDを取得
     * @return SSID文字列
     */
    String getSSID();
    
    /**
     * @brief WiFi信号強度を取得
     * @return RSSI値 (dBm)
     */
    int getRSSI();
    
    /**
     * @brief UDP通信を開始
     * @param localPort 受信ポート番号
     * @return true 開始成功, false 開始失敗
     */
    bool beginUDP(uint16_t localPort);
    
    /**
     * @brief UDP通信を終了
     */
    void endUDP();
    
    /**
     * @brief UDPパケットをIPアドレス宛に送信
     * @param ip 送信先IPアドレス
     * @param port 送信先ポート番号
     * @param data 送信データバッファ
     * @param length データ長
     * @return true 送信成功, false 送信失敗
     */
    bool sendUDP(const IPAddress& ip, uint16_t port, const uint8_t* data, size_t length);
    
    /**
     * @brief UDPパケットをホスト名宛に送信
     * @param host 送信先ホスト名
     * @param port 送信先ポート番号
     * @param data 送信データバッファ
     * @param length データ長
     * @return true 送信成功, false 送信失敗
     */
    bool sendUDP(const char* host, uint16_t port, const uint8_t* data, size_t length);
    
    /**
     * @brief 受信キューから1データグラム(=チャンク)を取り出す (非ブロッキング)
     * @param buffer 読み込みバッファ
     * @param length バッファ長
     * @return 取り出したバイト数, 0=キュー空
     * @note 受信は AsyncUDP コールバックがキューに積む。フレーム再構成は呼び出し側で。
     */
    int recvDatagram(uint8_t* buffer, size_t length);

    /**
     * @brief 送信元IPアドレスを取得
     * @return 送信元IPアドレス
     */
    IPAddress remoteIP();
    
    /**
     * @brief 送信元ポート番号を取得
     * @return 送信元ポート番号
     */
    uint16_t remotePort();
    
    /**
     * @brief ネットワークステータスを表示
     */
    void printStatus();
    
private:
    WiFiUDP udp;             ///< UDP送信用 (WiFiUDP)
    bool wifiConnected;      ///< WiFi接続状態
    bool udpStarted;         ///< UDP開始状態
    uint16_t udpLocalPort;   ///< UDPローカルポート

    // 受信は AsyncUDP (lwIP udp_recv コールバック直結)。WiFiUDP の BSDソケット
    // ポーリング受信が本ハード/coreで機能しなかったため置換。
    // 各データグラム(=チャンク)をFreeRTOSキューに積み、recvDatagram() で取り出す。
    // (断片化回避のためフレームは送信側でMTU内チャンクに分割される。再構成は
    //  ImageManager が行う。キューがコールバック⇄decodeタスク間の並行性を吸収。)
    static constexpr size_t kMaxDatagram = 1500;  ///< 1データグラム最大 (MTU内)
    static constexpr int kRxQueueLen = 32;        ///< 受信キュー段数
    struct UdpDatagram {                          ///< 受信キューの要素
        uint16_t len;
        uint8_t data[kMaxDatagram];
    };
    AsyncUDP _audp;                               ///< 受信用
    QueueHandle_t _rxQueue = nullptr;             ///< 受信データグラムキュー
    UdpDatagram _cbDg;                            ///< コールバック用作業領域(AsyncUDPタスク専用)
    IPAddress _rxRemoteIP;                         ///< 最新送信元IP
    uint16_t _rxRemotePort = 0;                   ///< 最新送信元ポート
    
    /**
     * @brief WiFi接続ヘルパー関数
     * @param ssid WiFi SSID
     * @param password WiFiパスワード
     * @param localIP 静的IPアドレス
     * @param gateway ゲートウェイアドレス
     * @param subnet サブネットマスク
     * @return true 接続成功, false 接続失敗
     */
    bool connectWiFi(const String& ssid, const String& password, 
                     const IPAddress& localIP, const IPAddress& gateway, 
                     const IPAddress& subnet);
};

} // namespace sastle

#endif // __NETWORK_MANAGER_H__
