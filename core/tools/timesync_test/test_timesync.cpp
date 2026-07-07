// TimeSync ロジックのホスト単体テスト (実機・broker 不要)。
// millis() をシムで制御し、実際の core/src/TimeSync.cpp をそのままリンクする。
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include "TimeSync.h"

uint32_t g_fakeMillis = 0;  // Arduino シムが参照する擬似ミリ秒

static int g_failures = 0;
static int g_checks = 0;

static void check(bool cond, const char* what) {
    ++g_checks;
    if (!cond) {
        ++g_failures;
        printf("  [FAIL] %s\n", what);
    } else {
        printf("  [ok]   %s\n", what);
    }
}

// epoch_ms を持つビーコン JSON を投入するヘルパ
static bool feed(sastle::TimeSync& ts, int64_t epoch_ms, uint32_t seq) {
    char buf[128];
    snprintf(buf, sizeof(buf), "{\"epoch_ms\":%lld,\"seq\":%u}",
             (long long)epoch_ms, seq);
    return ts.onClockMessage(buf);
}

// ---- Test 1: 初回同期 & クロック補間 ----
static void test_initial_sync() {
    printf("Test 1: 初回同期と補間\n");
    sastle::TimeSync ts;
    const int64_t E = 1751940000000LL;  // サーバー epoch (ms)

    g_fakeMillis = 1000;
    check(!ts.isSynced(), "投入前は未同期");
    check(feed(ts, E, 1), "初回ビーコンを受理");
    check(ts.isSynced(), "受信後に同期済み");
    check(ts.syncedNow() == E, "millis=1000 で syncedNow == epoch");
    check(ts.lastSeq() == 1, "seq を記録");

    g_fakeMillis = 3500;  // 2500ms 経過
    check(ts.syncedNow() == E + 2500, "2500ms 経過後は epoch+2500 に補間");
}

// ---- Test 2: EMA 平滑化 (小ジッタは 1/4 だけ反映) ----
static void test_ema_smoothing() {
    printf("Test 2: EMA 平滑化\n");
    sastle::TimeSync ts;
    const int64_t E = 1751940000000LL;

    g_fakeMillis = 1000;
    feed(ts, E, 1);
    int64_t off0 = ts.offset();

    // 同一 millis のまま +40ms ずれた epoch を投入 (閾値200ms未満=正常サンプル)。
    // sample = (E+40) - 1000 = off0 + 40。offset は 40/4 = 10 だけ動く。
    feed(ts, E + 40, 2);
    check(ts.offset() == off0 + 10, "40ms のズレは 1/4 (10ms) だけ offset に反映");
}

// ---- Test 3: 単発の外れ値は棄却される ----
static void test_outlier_reject() {
    printf("Test 3: 単発外れ値の棄却\n");
    sastle::TimeSync ts;
    const int64_t E = 1751940000000LL;

    g_fakeMillis = 1000;
    feed(ts, E, 1);
    int64_t off0 = ts.offset();

    feed(ts, E + 5000, 2);  // +5000ms スパイク (閾値超え)
    check(ts.offset() == off0, "単発スパイクでは offset を動かさない");
}

// ---- Test 4: 外れ値が連続したら追従再同期 ----
static void test_outlier_resync() {
    printf("Test 4: 連続外れ値で追従再同期\n");
    sastle::TimeSync ts;
    const int64_t E = 1751940000000LL;

    g_fakeMillis = 1000;
    feed(ts, E, 1);
    int64_t off0 = ts.offset();

    // OUTLIER_LIMIT=5。4回までは棄却、5回目で追従。
    const int64_t JUMP = 5000;
    for (int i = 0; i < 4; ++i) feed(ts, E + JUMP, 10 + i);
    check(ts.offset() == off0, "4回連続の外れ値までは棄却");

    feed(ts, E + JUMP, 20);  // 5回目
    check(ts.offset() == off0 + JUMP, "5回連続で新しい時刻へ追従再同期");
}

// ---- Test 5: millis() 32bit ラップ跨ぎ (最重要) ----
static void test_millis_wrap() {
    printf("Test 5: millis 32bit ラップ吸収\n");
    sastle::TimeSync ts;
    const int64_t E = 1751940000000LL;

    // ラップ直前で同期
    g_fakeMillis = 0xFFFFFF00u;  // = 4294967040
    feed(ts, E, 1);
    int64_t off0 = ts.offset();

    // 512ms 経過してラップ (0xFFFFFF00 + 0x200 = 0x100000100 → 32bit で 0x100)
    g_fakeMillis = 0x00000100u;
    check(ts.syncedNow() == E + 0x200, "ラップ跨ぎでも syncedNow は連続 (+512ms)");

    // ラップ後に整合したビーコンが来ても外れ値扱いされない
    bool ok = feed(ts, E + 0x200, 2);
    check(ok, "ラップ後ビーコンを受理");
    check(ts.offset() == off0, "ラップ後の整合ビーコンで offset が飛ばない (外れ値誤判定なし)");
}

// ---- Test 6: 不正 JSON / 必須キー欠落 ----
static void test_bad_payload() {
    printf("Test 6: 不正ペイロード\n");
    sastle::TimeSync ts;
    g_fakeMillis = 1000;
    check(!ts.onClockMessage("not json"), "非JSONは拒否");
    check(!ts.onClockMessage("{\"seq\":1}"), "epoch_ms 欠落は拒否");
    check(!ts.isSynced(), "不正ペイロードでは同期しない");
}

int main() {
    test_initial_sync();
    test_ema_smoothing();
    test_outlier_reject();
    test_outlier_resync();
    test_millis_wrap();
    test_bad_payload();

    printf("\n==== %d checks, %d failures ====\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
