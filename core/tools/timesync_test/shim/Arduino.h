// ホストテスト用の最小 Arduino シム。millis() をテストから制御できるようにする。
#pragma once
#include <cstdint>
#include <cstring>

// テスト側が書き換える擬似ミリ秒カウンタ (ESP32 の millis() は 32bit)。
extern uint32_t g_fakeMillis;

inline unsigned long millis() { return (unsigned long)g_fakeMillis; }
