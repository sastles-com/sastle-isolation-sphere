/**
 * @file RemoteLog.h
 * @brief Serial と MQTT の双方へ出力する tee ロガー
 * @author sastle-com
 *
 * USB ケーブルが届かない (ボール封止) 状態でも、デバッグログを
 * MQTT トピック経由で Web UI から確認できるようにする。
 * 行 ('\n') 単位で MQTT へ publish し、MQTT 未接続時は一定量だけ
 * リングバッファに退避して接続後に loop() でフラッシュする。
 */

#ifndef __REMOTE_LOG_H__
#define __REMOTE_LOG_H__

#include <Arduino.h>
#include <stddef.h>

namespace sastle {

class MQTTManager;  // 前方宣言 (実体は RemoteLog.cpp で include)

class RemoteLog : public Print {
public:
    /**
     * @brief 送信先 MQTT とトピックを登録する
     * @param mqtt  MQTTManager インスタンス (接続前でも可)
     * @param topic ログを publish するトピック
     */
    void begin(MQTTManager* mqtt, const char* topic);

    // Print インターフェース: 全出力を Serial にミラーしつつ行を組み立てる
    size_t write(uint8_t c) override;
    size_t write(const uint8_t* buffer, size_t size) override;

    /**
     * @brief 退避済みログを MQTT へフラッシュする (メインループから呼ぶ)
     */
    void loop();

private:
    bool publishLine(const char* line);
    void pushBacklog(const char* line, size_t len);

    MQTTManager* _mqtt = nullptr;
    const char* _topic = nullptr;

    char _line[240];           ///< 組み立て中の1行
    size_t _lineLen = 0;

    static constexpr size_t kBacklogCapacity = 4096;
    char _backlog[kBacklogCapacity];  ///< 未送信ログ ('\n' 区切り, 起動初期を優先保持)
    size_t _backlogLen = 0;
    bool _backlogDropped = false;     ///< 容量超過で破棄が発生したか
    bool _busy = false;               ///< publish 経路からの再入防止
};

extern RemoteLog Log;

} // namespace sastle

#endif /* __REMOTE_LOG_H__ */
