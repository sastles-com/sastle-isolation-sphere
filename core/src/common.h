#ifndef __COMMON_H__
#define __COMMON_H__

// グローバルデバッグフラグ (ConfigManager::begin()で設定)
extern bool g_debugEnabled;

// デバッグ出力マクロ (system.debugフラグで制御)
#define DEBUG_PRINT(...)    do { if (g_debugEnabled) Serial.print(__VA_ARGS__); } while(0)
#define DEBUG_PRINTLN(...)  do { if (g_debugEnabled) Serial.println(__VA_ARGS__); } while(0)
#define DEBUG_PRINTF(...)   do { if (g_debugEnabled) Serial.printf(__VA_ARGS__); } while(0)

#endif /* __COMMON_H__ */
