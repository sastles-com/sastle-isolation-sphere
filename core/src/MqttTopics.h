#ifndef __MQTT_TOPICS_H__
#define __MQTT_TOPICS_H__

// MQTT トピック定義の集約ヘッダー。
// 文字列はすべて従来ソース内に散在していたリテラルと同一。
// サーバー側 (server/app/core/config.py) のトピック定義と対応している。
// NOTE: デバイスID "sphere001" のハードコードは既存挙動を維持している。
//       config.json から動的生成すると外部システムとの通信が変わるため、
//       変更する場合はサーバー側と同時に対応すること。

namespace sastle {
namespace topics {

// 購読 (サーバー → デバイス)
constexpr const char* kAllState          = "sphere/all/state";
constexpr const char* kAllClock          = "sphere/all/clock";   // 時刻同期ビーコン (1秒周期)
constexpr const char* kAllCommandWild    = "sphere/all/command/#";
constexpr const char* kCommandParams     = "sphere/all/command/params";
constexpr const char* kCommandPlayback   = "sphere/all/command/playback";
constexpr const char* kCommandLed        = "sphere/all/command/led";
constexpr const char* kCommandSystem     = "sphere/all/command/system";

// 配信 (デバイス → サーバー) はデバイス ID を含むため定数にしない。
// MQTTManager::publishDevice("<suffix>", ...) が config.json の sphere.id から
// "sphere/<id>/<suffix>" を実行時生成する。使用中の suffix:
//   imu / state / gesture / ui_mode / status / log
// サーバー側 (server/app/services/mqtt_service.py) も sphere.id を読んで対応する。

} // namespace topics
} // namespace sastle

#endif /* __MQTT_TOPICS_H__ */
