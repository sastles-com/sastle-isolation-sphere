#ifndef __COMMON_H__
#define __COMMON_H__

// グローバルデバッグフラグ (ConfigManager::begin()で設定)
extern bool g_debugEnabled;

#include "RemoteLog.h"

// デバッグ出力マクロ (system.debugフラグで制御)
// 出力は Serial と MQTT (sphere/sphere001/log) の双方へ tee される。
#define DEBUG_PRINT(...)    do { if (g_debugEnabled) ::sastle::Log.print(__VA_ARGS__); } while(0)
#define DEBUG_PRINTLN(...)  do { if (g_debugEnabled) ::sastle::Log.println(__VA_ARGS__); } while(0)
#define DEBUG_PRINTF(...)   do { if (g_debugEnabled) ::sastle::Log.printf(__VA_ARGS__); } while(0)

#endif /* __COMMON_H__ */
