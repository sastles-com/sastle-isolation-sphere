#include "IMUManager.h"
#include <Arduino.h>
#include "RemoteLog.h"

namespace sastle {

#if defined(IMU_SENSOR_M5IMU)
IMUManager::IMUManager()
    : _initialized(false),
      _lastUpdate(0),
      _ahrs(0.1f),
      _lastMicros(0),
      _biasReady(false),
      _gyroBias{0.0f, 0.0f, 0.0f},
      _lastDriftLog(0) {
}
#else
IMUManager::IMUManager()
    : _initialized(false),
      _lastUpdate(0),
      _bno(55, 0x28) { // BNO055のI2Cアドレス: 0x28 (ADRピンがLOW)
}
#endif

IMUManager::~IMUManager() {
}

#if !defined(IMU_SENSOR_M5IMU)
bool IMUManager::begin(ConfigManager& config, uint8_t sda, uint8_t scl) {
    Serial.println("\n=== IMU Manager Initialization (BNO055) ===");

    // I2C 排他用ミューテックス。startTask() より前に必ず作る (作られる前に
    // _lockI2c() が呼ばれても no-op で安全だが、この時点ではまだ単一タスク)。
    if (!_i2cMutex) {
        _i2cMutex = xSemaphoreCreateRecursiveMutex();
        if (!_i2cMutex) {
            Serial.println("[IMU] I2C mutex creation failed");
            return false;
        }
    }

    // I2C初期化（既に初期化されている場合はスキップ）
    // BNO055 は I2C クロックストレッチが長く、400kHz ではレジスタ読み出しが
    // 化ける (quat の w だけ動き x/y/z=0、ノルム≠1 の壊れた値を実機で確認)。
    // 100kHz (エラー検出+リトライ付き直接読みがあるため、失敗は検出・再試行される)。
    if (!Wire.begin(sda, scl, _i2cHz)) {
        Serial.println("I2C bus initialization failed or already initialized");
    }
    delay(100);

    Serial.printf("I2C initialized (SDA: GPIO%d, SCL: GPIO%d)\n", sda, scl);

    // BNO055初期化
    if (!_bno.begin()) {
        Serial.println("BNO055 not detected! Check wiring.");
        return false;
    }

    Serial.println("BNO055 detected!");

    // センサー情報表示
    sensor_t sensor;
    _bno.getSensor(&sensor);
    Serial.println("------------------------------------");
    Serial.print("Sensor:       "); Serial.println(sensor.name);
    Serial.print("Driver Ver:   "); Serial.println(sensor.version);
    Serial.print("Unique ID:    "); Serial.println(sensor.sensor_id);
    Serial.print("Max Value:    "); Serial.print(sensor.max_value); Serial.println(" xxx");
    Serial.print("Min Value:    "); Serial.print(sensor.min_value); Serial.println(" xxx");
    Serial.print("Resolution:   "); Serial.print(sensor.resolution); Serial.println(" xxx");
    Serial.println("------------------------------------");

    delay(100);

    // 外部クリスタルは使用しない。
    // 注意: 外部水晶が実装されていないボードで true にすると、生センサー値
    // (gyro/accel) は読めるのに fusion 出力 (quat) が単位のまま固まる。
    // 実機で cal=0300 + quat恒久(1,0,0,0) の症状を確認したため false に変更。
    _bno.setExtCrystalUse(false);

    // 動作モードは IMUPLUS (加速度+ジャイロの6軸融合、磁気不使用)。
    // NDOF (9軸) は磁気キャリブレーション未完了時にヨーが「動かない→90°ジャンプ」
    // する上、本機はLED大電流・LiPoが磁気センサー近傍にあり磁気データが常に乱れる
    // ため不採用。IMUPLUS はヨーがジャイロ積分で滑らか (ドリフトは SET ZERO で解消)。
    // 注意: BNO055 は fusionモード間の直接切替を無視するため、必ず CONFIG を経由する
    // (begin() が NDOF にするので、直接 IMUPLUS を書いても効かない)。
    _bno.setMode(OPERATION_MODE_CONFIG);
    delay(25);
    _bno.setMode(OPERATION_MODE_IMUPLUS);
    delay(20);

    // 診断: モード/システム状態を起動ログに残す (mode=8 が IMUPLUS)
    {
        uint8_t sysStatus = 0, selfTest = 0, sysError = 0;
        _bno.getSystemStatus(&sysStatus, &selfTest, &sysError);
        Serial.printf("[IMU] boot diag: mode=%d sys_status=%u selftest=0x%X sys_error=%u\n",
                      (int)_bno.getMode(), sysStatus, selfTest, sysError);
    }

    _initialized = true;
    Serial.println("BNO055 initialized successfully!");

    // 初期キャリブレーション状態表示
    displayCalibrationStatus();

    return true;
}

bool IMUManager::update() {
    if (!_initialized) {
        return false;
    }
    // 専用タスク稼働中は I2C の二重アクセスになるため loop からの呼び出しは無視。
    if (_taskRunning) {
        return true;
    }

    unsigned long now = millis();
    if (now - _lastUpdate < UPDATE_INTERVAL) {
        return true; // まだ更新タイミングではない
    }

    _lastUpdate = now;
    return _updateOnce();
}

#ifndef IMU_SENSOR_M5IMU
bool IMUManager::_readVector6(uint8_t reg, float scale, imu::Vector<3>& out) {
    I2cGuard guard(this);
    for (int attempt = 0; attempt < 2; attempt++) {
        uint8_t b[6];
        if (_wordRead) {
            // 2B×3 トランザクション (quat と同じ理由。6B 一括は 0xFF 化する個体がある)
            bool wOk = true;
            for (int k = 0; k < 3 && wOk; k++) {
                Wire.beginTransmission(0x28);
                Wire.write((uint8_t)(reg + 2 * k));
                if (Wire.endTransmission(false) != 0) { wOk = false; break; }
                if (Wire.requestFrom(0x28, (uint8_t)2) != 2) { wOk = false; break; }
                b[2 * k] = Wire.read();
                b[2 * k + 1] = Wire.read();
            }
            if (!wOk) continue;
        } else {
            Wire.beginTransmission(0x28);
            Wire.write(reg);
            if (Wire.endTransmission(false) != 0) continue;   // repeated start
            if (Wire.requestFrom(0x28, (uint8_t)6) != 6) continue;
            for (int i = 0; i < 6; i++) b[i] = Wire.read();
        }
        // quat と同じ部分読み検出 (末尾 z=0x0000 → ゼロ埋めの疑い)。gyro の z は
        // 静止時に本当に 0 になることが多い (実測 partial=20/s 相当) ので、ここでは
        // _wdPartialReads に数えず (quat 専用の指標に保つ)、再試行しても同じなら
        // 今回は諦めて前回値を維持する (診断用データなので欠けても支障はない)。
        if (b[4] == 0 && b[5] == 0) continue;
        if (b[4] == 0xFF && b[5] == 0xFF) {
            // 0xFF 埋めの部分読み。生バイトを 5 秒に 1 回だけ MQTT ログへ (計装)。
            static unsigned long lastFfLog = 0;
            const unsigned long nowFf = millis();
            if (nowFf - lastFfLog > 5000) {
                lastFfLog = nowFf;
                sastle::Log.printf("[IMU] vec reg=0x%02X partial(0xFF) raw=%02X%02X %02X%02X %02X%02X\n",
                                   reg, b[1], b[0], b[3], b[2], b[5], b[4]);
            }
            continue;
        }
        out = imu::Vector<3>((int16_t)((b[1] << 8) | b[0]) * scale,
                             (int16_t)((b[3] << 8) | b[2]) * scale,
                             (int16_t)((b[5] << 8) | b[4]) * scale);
        return true;
    }
    return false;
}
#endif

void IMUManager::taskFunction(void* parameter) {
    IMUManager* self = static_cast<IMUManager*>(parameter);
    Serial.printf("[IMU_Task] Started on core %d (%lu Hz)\n",
                  xPortGetCoreID(), 1000UL / self->UPDATE_INTERVAL);

    // vTaskDelayUntil で「前回起床時刻 + 10ms」に固定する。vTaskDelay(10) だと
    // 処理時間ぶん周期が伸びて位相がずれていくため、姿勢サンプル間隔を一定に
    // 保つにはこちらが必須。
    TickType_t lastWake = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(self->UPDATE_INTERVAL);
    while (true) {
        self->_updateOnce();
        vTaskDelayUntil(&lastWake, period);
    }
}

bool IMUManager::startTask(uint8_t core, uint8_t priority, uint32_t stackSize) {
    if (!_initialized) {
        Serial.println("[IMU_Task] Not initialized");
        return false;
    }
    if (_taskRunning) {
        return true;
    }
    // 先に立ててから生成する: タスクが動き出した直後に loop 側の update() が
    // 割り込んで I2C を触るのを防ぐ。
    _taskRunning = true;
    if (xTaskCreatePinnedToCore(taskFunction, "IMU_Poll", stackSize, this,
                                priority, &_taskHandle, core) != pdPASS) {
        _taskRunning = false;
        _taskHandle = nullptr;
        Serial.println("[IMU_Task] Failed to create task");
        return false;
    }
    return true;
}

void IMUManager::_lockI2c() {
    if (_i2cMutex) {
        xSemaphoreTakeRecursive(_i2cMutex, portMAX_DELAY);
    }
}

void IMUManager::_unlockI2c() {
    if (_i2cMutex) {
        xSemaphoreGiveRecursive(_i2cMutex);
    }
}

bool IMUManager::_updateOnce() {
    if (!_initialized) {
        return false;
    }
    const unsigned long now = millis();
    _lastUpdate = now;

    // I2C クロック変更要求 (MQTT led {"imu_i2c_khz"}) はトランザクションの隙間で
    // IMU タスク自身が適用する (他コアから setClock を叩いて読みと衝突させない)。
    if (_i2cHzPending) {
        const uint32_t hz = _i2cHzPending;
        _i2cHzPending = 0;
        I2cGuard guard(this);
        Wire.setClock(hz);
        _i2cHz = hz;
        sastle::Log.printf("[IMU] I2C clock -> %lu kHz\n", (unsigned long)(hz / 1000));
    }

    // データ取得: Adafruit の getQuat() は I2C 失敗を無視して古いバッファを返す
    // (実測: 回転中でも約8割のサンプルが停滞→たまに成功してジャンプ)。
    // エラー検出付きの直接レジスタ読みに置き換え、失敗時は前回値を保持する。
    imu::Quaternion q = _quat;  // 読めなければ前回値
    // NOTE: gyro/euler/quat を 0x14 から 20 バイト連続読みする実装を試したが、
    // 実機では quat 部分がゴミになり全棄却された (2026-09-06, disc=100/s)。
    // BNO055 のバースト長制限か ESP32 側の問題かは未確定。実績のある 8 バイト
    // 単独読みに固定し、補助データは 6 バイト単位の個別トランザクションで読む。
    const bool aux = _auxReads;
    uint8_t b[8] = {0};
    bool readOk = false;
    {
        // endTransmission(false) と requestFrom() は repeated start で1つの
        // シーケンスを成す。その間に他コアの I2C が割り込むと別のレジスタ窓を
        // 読んでしまうため、シーケンス全体を排他する。
        I2cGuard guard(this);
        for (int attempt = 0; attempt < 3 && !readOk; attempt++) {
            if (_wordRead) {
                // 実験: 2 バイト×4 トランザクション (w,x,y,z を個別に読む)。
                // 4 回の読みが融合更新 (10ms) をまたぐと成分が混ざるが、隣接
                // サンプル差は 300°/s でも 3° なので下流の検査で問題にならない。
                bool wOk = true;
                for (int k = 0; k < 4 && wOk; k++) {
                    Wire.beginTransmission(0x28);
                    Wire.write((uint8_t)(0x20 + 2 * k));
                    if (Wire.endTransmission(false) != 0) { wOk = false; break; }
                    if (Wire.requestFrom(0x28, (uint8_t)2) != 2) { wOk = false; break; }
                    b[2 * k] = Wire.read();
                    b[2 * k + 1] = Wire.read();
                }
                if (!wOk) continue;
            } else {
                Wire.beginTransmission(0x28);
                Wire.write(0x20);  // QUA_DATA_W_LSB
                if (Wire.endTransmission(false) != 0) continue;   // repeated start
                if (Wire.requestFrom(0x28, (uint8_t)8) != 8) continue;
                for (int i = 0; i < 8; i++) b[i] = Wire.read();
            }
            const uint8_t* qb = b;

            // 実測: I2C が「成功」を返しつつ 8バイト全ゼロを返すことがある
            // (加速度も同じ周期で 0.0 と 9.9 を交互に返していた)。全ゼロ quat は
            // ノルム0なので下流のガードが必ず棄却し、姿勢が凍結する
            // (disc=100/s, step_max=0.00deg を実測)。ここで読み失敗として扱い
            // 再試行させる。BNO055 が全ゼロの姿勢を返すことは原理的に無い
            // (単位クォータニオンなので必ずどれかの成分が非0)。
            bool allZero = true;
            for (int i = 0; i < 8; i++) { if (qb[i] != 0) { allZero = false; break; } }
            if (allZero) { _wdZeroReads++; continue; }

            // 部分読みガード (2026-09-06 実測): 全ゼロの亜種として「先頭 2/4/6 バイト
            // だけ有効で残りがゼロ埋め」が回転中に頻発する。(w,x,0,0) はノルム²が
            // 0.5 を超えると正規化されて X 軸だけ回った偽姿勢として受理され、
            // (w,0,0,0) は恒等姿勢になる (回転中に q=(1,0,0,0) なのに gyro=139°/s
            // という矛盾ログを実測)。いずれも z の 2 バイトが 0x0000 になるので、
            // ここで検出して再試行する。z が本当に 0 ぴったりになる確率は 1/16384
            // で、その場合も次サイクルで読み直すだけなので副作用は無い。
            if (qb[6] == 0 && qb[7] == 0) { _wdPartialReads++; continue; }
            // 同じく末尾が 0xFFFF (= -1 LSB) の亜種。I2C でスレーブが途中で SDA を
            // 駆動しなくなると残りは 1 (0xFF) として読める。6 バイト読みの gyro/acc/
            // euler で「x だけ妥当で y,z が -1 LSB」が回転中に頻発したのを実測。
            if (qb[6] == 0xFF && qb[7] == 0xFF) { _wdPartialReads++; continue; }

            const float s = 1.0f / 16384.0f;  // 2^14 LSB/unit
            q = imu::Quaternion(
                (int16_t)((qb[1] << 8) | qb[0]) * s,
                (int16_t)((qb[3] << 8) | qb[2]) * s,
                (int16_t)((qb[5] << 8) | qb[4]) * s,
                (int16_t)((qb[7] << 8) | qb[6]) * s);
            readOk = true;
        }
        if (!readOk) {
            _wdReadFails++;
            // 生バイトを出して「ゼロ埋めか化けか」を切り分ける。5秒に1回だけ。
            // Serial ではなく sastle::Log なので MQTT で観測できる。
            static unsigned long lastRawLog = 0;
            const unsigned long nowRaw = millis();
            if (nowRaw - lastRawLog > 5000) {
                lastRawLog = nowRaw;
                const uint8_t* qb = b;
                sastle::Log.printf(
                    "[IMU] quat read failed, raw=%02X %02X %02X %02X %02X %02X %02X %02X"
                    " (zero=%lu partial=%lu fails=%lu/%lu)\n",
                    qb[0], qb[1], qb[2], qb[3], qb[4], qb[5], qb[6], qb[7],
                    (unsigned long)_wdZeroReads, (unsigned long)_wdPartialReads,
                    (unsigned long)_wdReadFails, (unsigned long)_wdReadTotal);
            }
        }
        _wdReadTotal++;
    }
    // I2C化けガード1: このBNO055個体はノルム0.96〜0.99のquatを常用するため、
    // 「単位でない=破棄」にすると正常値の半分を捨ててしまう (実測 disc=loopの
    // 半分〜全部でカクつきの主因になった)。妥当な範囲なら正規化して受理し、
    // 明らかな化け (ノルムが大きく崩れている) だけ破棄する。
    // 範囲は個体の実測 (ノルム² 0.92〜0.98) に余裕を持たせた 0.85〜1.15。旧 0.5〜2.0
    // では部分読み (w,x,0,0) が w²+x²>0.5 で素通りしていた。部分読み自体は上の
    // z=0 ガードで弾くので、ここは取りこぼしの誤差上限を抑えるバックストップ。
    const float n2 = (float)(q.w()*q.w() + q.x()*q.x() + q.y()*q.y() + q.z()*q.z());
    // 読めなかったサイクルは前回値を「再受理」しない (以前は前回値がそのまま
    // ノルム/連続性検査を通って _quatSeq が進み、描画側の stale が 9% のまま
    // 読み失敗 90/s を隠していた)。前回値は保持され、seq は進まない。
    bool ok = readOk && (n2 > 0.85f && n2 < 1.15f);
    if (ok && fabsf(n2 - 1.0f) > 1e-4f) {
        const float inv = 1.0f / sqrtf(n2);
        q = imu::Quaternion(q.w() * inv, q.x() * inv, q.y() * inv, q.z() * inv);
    }
    // I2C化けガード2 (連続性): 1サンプル(10ms)で25°超の姿勢ジャンプは物理的に
    // あり得ない (2500°/s)。ノルムが偶然1に近い化け値もこれで弾く。
    // q と -q は同じ回転なので |dot| で比較する。
    if (ok) {
        const float dot = fabsf((float)(q.w()*_quat.w() + q.x()*_quat.x() +
                                        q.y()*_quat.y() + q.z()*_quat.z()));
        // 動的しきい値: 前回受理からの経過時間×角速度に余裕を掛けた角度まで許容。
        // 固定25°だと「読み損ねの隙間+速い回転」で正常値を誤破棄し追従が止まる。
        static unsigned long lastAcceptMs = 0;
        float dts = (now - lastAcceptMs) * 0.001f;
        if (dts < 0.01f) dts = 0.01f;
        if (dts > 0.5f)  dts = 0.5f;
        // 注意: BNO055 の VECTOR_GYROSCOPE は deg/s を返す (ヘッダのコメントは
        // rad/s と書いてあるが誤り)。つまり下の maxAngle は意図の約57倍ゆるい。
        // これは意図して維持している: 以前ここを厳密にしていたため「読み損ねの
        // 隙間 + 速い回転」で正常値を大量に誤破棄し (disc が imu_read の半分)、
        // 追従が止まる主因になっていた。ガードは「明らかな化け値だけ弾く」緩い
        // 網として使い、精度は上流 (I2C 100kHz + エラー検出付き直接読み) で担保する。
        // aux OFF 中は gyro が更新されないので、手持ち回転の想定最大 360°/s で代用する
        // (固定 15° に落とすと「読み損ねの隙間 + 速い回転」で正常値を誤棄却する)。
        const float gyroMag = aux ? (fabsf((float)_gyro.x()) + fabsf((float)_gyro.y()) +
                                    fabsf((float)_gyro.z()))    // [deg/s]
                                  : 360.0f;
        const float maxAngle = 0.26f + gyroMag * dts * 1.5f;    // [rad] 基本15°+速度比例
        const float dotClamped = dot > 1.0f ? 1.0f : dot;
        const float ang = 2.0f * acosf(dotClamped);             // 姿勢差 [rad]
        if (ang > maxAngle && _wdConsecDiscards < 10) {
            ok = false;
        } else {
            lastAcceptMs = now;
        }
    }
    if (ok) {
        // renderタスク (core1) が読む最中の引き裂かれ (torn read) を防ぐ
        taskENTER_CRITICAL(&_quatMux);
        _quat = q;
        _quatSeq++;
        taskEXIT_CRITICAL(&_quatMux);
        _wdConsecDiscards = 0;
    } else if (readOk) {   // 読み失敗は fail に数えるので disc とは分ける
        static unsigned long lastBadLog = 0;
        _wdDiscards++;
        _wdConsecDiscards++;
        // 棄却した生バイトを MQTT ログに出す (2秒に1回)。「どのバイトから化けるか」
        // を実機で特定するための計装。
        if (now - lastBadLog > 2000) {
            lastBadLog = now;
            sastle::Log.printf(
                "[IMU] Discarded quat |q|^2=%.3f raw=%02X%02X %02X%02X %02X%02X %02X%02X"
                " (discards=%lu)\n",
                n2, b[1], b[0], b[3], b[2], b[5], b[4], b[7], b[6],
                (unsigned long)_wdDiscards);
        }
    }
    // 補助データ (aux ON のときだけ)。I2C時間の節約で間引く:
    //   accel/gyro (0x08 / 0x14, 6B, 100 LSB/(m/s²) / 16 LSB/(°/s)) = 2回に1回
    //   euler      (0x1A, 6B, 16 LSB/°)                            = 10回に1回
    // Adafruit getVector() は I2C 失敗を無視して古いバッファを返すため使わず、
    // quat と同じ部分読み検出付きの _readVector6() で読む。
    static uint8_t subCycle = 0;
    subCycle++;
    if (aux && (subCycle & 1) == 0) {
        imu::Vector<3> a, g;
        const bool okA = _readVector6(0x08, 1.0f / 100.0f, a);
        const bool okG = _readVector6(0x14, 1.0f / 16.0f, g);
        taskENTER_CRITICAL(&_quatMux);
        if (okA) _accel = a;
        if (okG) _gyro = g;
        taskEXIT_CRITICAL(&_quatMux);
    }
    if (subCycle >= 10) {
        subCycle = 0;
        imu::Vector<3> e;
        if (aux && _readVector6(0x1A, 1.0f / 16.0f, e)) {
            // _euler は loop タスク (GestureManager) が読むので排他して差し替える
            taskENTER_CRITICAL(&_quatMux);
            _euler = e;
            taskEXIT_CRITICAL(&_quatMux);
        }
    }

    // 融合停止ウォッチドッグ (電源瞬断で BNO055 の fusion が止まり、quat が
    // 姿勢に追従しなくなる事象への対策)。2秒ごとに SYS_STATUS を確認し、
    // 「fusion動作中(=5)」以外なら再初期化する。加えて「回転している
    // (gyroが大きい) のに quat が動かない」場合も融合停止と判定する。
    if (now - _wdLastChange > 2000) {
        _wdLastChange = now;

        uint8_t sysStatus = 0, selfTest = 0, sysError = 0;
        {
            I2cGuard guard(this);
            _bno.getSystemStatus(&sysStatus, &selfTest, &sysError);
        }

        const float gyroMag = fabsf((float)_gyro.x()) + fabsf((float)_gyro.y()) +
                              fabsf((float)_gyro.z());
        const float dq = fabsf((float)_quat.w() - _wdPrev[0]) +
                         fabsf((float)_quat.x() - _wdPrev[1]) +
                         fabsf((float)_quat.y() - _wdPrev[2]) +
                         fabsf((float)_quat.z() - _wdPrev[3]);
        _wdPrev[0] = (float)_quat.w(); _wdPrev[1] = (float)_quat.x();
        _wdPrev[2] = (float)_quat.y(); _wdPrev[3] = (float)_quat.z();

        const bool fusionDead = (sysStatus != 5) || (gyroMag > 0.3f && dq < 0.001f);
        if (fusionDead && false) {  // 調査中は自動復旧を無効化 (thrashing防止)
            _wdRecoveries++;
            Serial.printf("[IMU] Fusion dead (sys=%u err=%u gyro=%.2f dq=%.4f) — recover #%u\n",
                          sysStatus, sysError, gyroMag, dq, _wdRecoveries);
            if (_wdRecoveries % 3 != 0) {
                // まずモード入れ直し (軽量)
                _bno.setMode(OPERATION_MODE_CONFIG);
                delay(25);
                _bno.setMode(OPERATION_MODE_IMUPLUS);
                delay(20);
            } else {
                // 3回に1回はフル再初期化 (リセット込み)
                Serial.println("[IMU] Mode cycle insufficient — full re-begin");
                if (_bno.begin()) {
                    delay(50);
                    _bno.setExtCrystalUse(false);  // begin() と同じ設定 (外部水晶なし)
                    // begin() は NDOF にするため CONFIG 経由で IMUPLUS へ
                    _bno.setMode(OPERATION_MODE_CONFIG);
                    delay(25);
                    _bno.setMode(OPERATION_MODE_IMUPLUS);
                    delay(20);
                }
            }
        }
    }

    return true;
}

#else  // IMU_SENSOR_M5IMU
bool IMUManager::begin(ConfigManager& config, uint8_t sda, uint8_t scl) {
    (void)sda; (void)scl;  // 内蔵IMUは内部I2Cバスを使うため外部ピン指定は不使用
    Serial.println("\n=== IMU Manager Initialization (M5 internal IMU) ===");

    // I2C 排他用 (BNO055 側と同じ理由: IMUタスクと loop の同時アクセス防止)
    if (!_i2cMutex) {
        _i2cMutex = xSemaphoreCreateRecursiveMutex();
        if (!_i2cMutex) {
            Serial.println("[IMU] I2C mutex creation failed");
            return false;
        }
    }

    // M5Unified 本体初期化。LCDManager は LCDデバッグ無効時に M5.begin を呼ばないため、
    // IMU 側で確実に内部I2C/IMU電源を立ち上げておく (二重 begin は M5Unified が許容)。
    auto cfg = M5.config();
    M5.begin(cfg);

    if (!M5.Imu.begin()) {
        Serial.println("M5 internal IMU not detected!");
        return false;
    }

    const char* name = "unknown";
    switch (M5.Imu.getType()) {
        case m5::imu_bmi270:  name = "BMI270";  break;
        case m5::imu_mpu6886: name = "MPU6886"; break;
        case m5::imu_mpu6050: name = "MPU6050"; break;
        case m5::imu_mpu9250: name = "MPU9250"; break;
        case m5::imu_sh200q:  name = "SH200Q";  break;
        default: break;
    }
    Serial.printf("M5 internal IMU detected: %s (6-axis fusion)\n", name);

    _initialized = true;
    _biasReady = false;   // ジャイロバイアスは初回 update() で静止データから推定
    _lastMicros = micros();
    Serial.println("M5 internal IMU initialized (gyro bias pending)");

    return true;
}

bool IMUManager::update() {
    if (!_initialized) {
        return false;
    }

    unsigned long now = millis();
    if (now - _lastUpdate < UPDATE_INTERVAL) {
        return true; // まだ更新タイミングではない
    }
    _lastUpdate = now;

    {
        I2cGuard guard(this);   // loop 側の getTemperature() と排他
        M5.Imu.update();
    }

    // 全初期化 (LCDManager の M5.begin を含む) 完了後の最初の機会にバイアスを推定。
    // begin() 直後に行うと LCDManager の後続 M5.begin で IMU が再初期化され無効化される。
    if (!_biasReady) {
        calibrateGyroBias();
        _lastMicros = micros();  // 校正に要した時間は dt から除外
        return true;
    }

    float ax, ay, az;   // 加速度 [G]
    float gx, gy, gz;   // 角速度 [deg/s]
    M5.Imu.getAccel(&ax, &ay, &az);
    M5.Imu.getGyro(&gx, &gy, &gz);

    // バイアス除去 (deg/s)
    float cgx = gx - _gyroBias[0];
    float cgy = gy - _gyroBias[1];
    float cgz = gz - _gyroBias[2];

    // dt 算出 (異常値はガード)
    unsigned long nowUs = micros();
    float dt = (nowUs - _lastMicros) * 1e-6f;
    _lastMicros = nowUs;
    if (dt <= 0.0f || dt > 0.5f) {
        dt = (float)UPDATE_INTERVAL * 1e-3f;
    }

    // Madgwick 6軸融合 (ジャイロは rad/s 入力)
    const float DEG2RAD = 0.0174532925199433f;
    _ahrs.updateIMU(cgx * DEG2RAD, cgy * DEG2RAD, cgz * DEG2RAD,
                    ax, ay, az, dt);

    // 公開 API 互換のため imu:: 型へ詰め替え。単位は BNO055 に合わせる:
    //   quat=正規化, euler=度, accel=m/s², gyro=deg/s
    _quat = imu::Quaternion(_ahrs.w(), _ahrs.x(), _ahrs.y(), _ahrs.z());
    float heading, roll, pitch;
    _ahrs.getEulerDeg(heading, roll, pitch);
    _euler = imu::Vector<3>(heading, roll, pitch);

    const float G = 9.80665f;
    _accel = imu::Vector<3>(ax * G, ay * G, az * G);
    _gyro  = imu::Vector<3>(cgx, cgy, cgz);

    return true;
}

void IMUManager::calibrateGyroBias() {
    Serial.println("[IMU] Calibrating gyro bias... keep the device still (~1s)");

    // M5 の自動オフセット補正は自前のバイアス推定と二重補正になるため無効化
    M5.Imu.setCalibration(0, 0, 0);

    const int kSamples = 200;
    double sx = 0.0, sy = 0.0, sz = 0.0;
    int got = 0;
    for (int i = 0; i < kSamples; ++i) {
        M5.Imu.update();
        float gx, gy, gz;
        if (M5.Imu.getGyro(&gx, &gy, &gz)) {
            sx += gx; sy += gy; sz += gz;
            ++got;
        }
        delay(5);  // 約1秒で 200サンプル
    }

    if (got > 0) {
        _gyroBias[0] = (float)(sx / got);
        _gyroBias[1] = (float)(sy / got);
        _gyroBias[2] = (float)(sz / got);
    } else {
        _gyroBias[0] = _gyroBias[1] = _gyroBias[2] = 0.0f;
    }
    _biasReady = true;

    Serial.printf("[IMU] Gyro bias [deg/s]: x=%.4f y=%.4f z=%.4f (n=%d)\n",
                  _gyroBias[0], _gyroBias[1], _gyroBias[2], got);
}
#endif

uint8_t IMUManager::getOperationMode() {
#if !defined(IMU_SENSOR_M5IMU)
    if (!_initialized) return 0xFE;
    I2cGuard guard(this);   // IMUタスクの読み出しシーケンスに割り込まない
    return (uint8_t)_bno.getMode();
#else
    return 0xFF;
#endif
}

imu::Quaternion IMUManager::getQuaternion() {
    taskENTER_CRITICAL(&_quatMux);
    const imu::Quaternion q = _quat;
    taskEXIT_CRITICAL(&_quatMux);
    return q;
}

bool IMUManager::getQuaternion(float& w, float& x, float& y, float& z) {
    if (!_initialized) {
        return false;
    }

    // センサー→LED座標系の変換: BNO055 の quaternion は本システムの規約と
    // 回転の向きが逆 (実機テスト 2026-08-23: 生値では X/Y/Z 全軸で補正方向が逆)。
    // 完全共役 q → q* = (w,-x,-y,-z) で逆回転に変換する。
    // ここで変換することで LED描画・MQTT配信(ツイン)・ジェスチャすべてに
    // 一括適用される。読み出しはコア間排他の下でコピーする (torn read防止)。
    taskENTER_CRITICAL(&_quatMux);
    const float qw = (float)_quat.w(), qx = (float)_quat.x(),
                qy = (float)_quat.y(), qz = (float)_quat.z();
    taskEXIT_CRITICAL(&_quatMux);
    w = qw;
    x = -qx;
    y = -qy;
    z = -qz;

    return true;
}

// _euler は IMU タスク (core0) が書き、GestureManager は loop (core1) から読む。
// imu::Vector<3> は double×3 = 24byte でアトミックに読めないため、ジェスチャー
// 判定が引き裂かれた値 (別サンプルの成分の混合) を見ないよう排他する。
imu::Vector<3> IMUManager::getEuler() {
    taskENTER_CRITICAL(&_quatMux);
    const imu::Vector<3> e = _euler;
    taskEXIT_CRITICAL(&_quatMux);
    return e;
}

bool IMUManager::getEuler(float& heading, float& roll, float& pitch) {
    if (!_initialized) {
        return false;
    }

    taskENTER_CRITICAL(&_quatMux);
    const float h = (float)_euler.x(), r = (float)_euler.y(), p = (float)_euler.z();
    taskEXIT_CRITICAL(&_quatMux);

    heading = h;
    roll = r;
    pitch = p;

    return true;
}

// _accel/_gyro も IMU タスク (core0) が書き loop (core1) が読むので排他する。
// これらは I2C を触らない (キャッシュ済みの値を返すだけ)。
imu::Vector<3> IMUManager::getAccel() {
    taskENTER_CRITICAL(&_quatMux);
    const imu::Vector<3> a = _accel;
    taskEXIT_CRITICAL(&_quatMux);
    return a;
}

bool IMUManager::getAccel(float& x, float& y, float& z) {
    if (!_initialized) {
        return false;
    }

    taskENTER_CRITICAL(&_quatMux);
    const float ax = (float)_accel.x(), ay = (float)_accel.y(), az = (float)_accel.z();
    taskEXIT_CRITICAL(&_quatMux);

    x = ax;
    y = ay;
    z = az;

    return true;
}

imu::Vector<3> IMUManager::getGyro() {
    taskENTER_CRITICAL(&_quatMux);
    const imu::Vector<3> g = _gyro;
    taskEXIT_CRITICAL(&_quatMux);
    return g;
}

bool IMUManager::getGyro(float& x, float& y, float& z) {
    if (!_initialized) {
        return false;
    }

    taskENTER_CRITICAL(&_quatMux);
    const float gx = (float)_gyro.x(), gy = (float)_gyro.y(), gz = (float)_gyro.z();
    taskEXIT_CRITICAL(&_quatMux);

    x = gx;
    y = gy;
    z = gz;

    return true;
}

#if !defined(IMU_SENSOR_M5IMU)
void IMUManager::displayCalibrationStatus() {
    if (!_initialized) {
        Serial.println("IMU not initialized!");
        return;
    }

    uint8_t system, gyro, accel, mag = 0;
    {
        I2cGuard guard(this);
        _bno.getCalibration(&system, &gyro, &accel, &mag);
    }

    Serial.println("\n=== Calibration Status ===");
    Serial.print("System: "); Serial.print(system, DEC);
    Serial.print(" Gyro: "); Serial.print(gyro, DEC);
    Serial.print(" Accel: "); Serial.print(accel, DEC);
    Serial.print(" Mag: "); Serial.println(mag, DEC);
    Serial.println("(0=uncalibrated, 3=fully calibrated)");
    Serial.println("==========================");
}

bool IMUManager::isFullyCalibrated() {
    if (!_initialized) {
        return false;
    }

    uint8_t system, gyro, accel, mag = 0;
    {
        I2cGuard guard(this);
        _bno.getCalibration(&system, &gyro, &accel, &mag);
    }

    return (system == 3 && gyro == 3 && accel == 3 && mag == 3);
}

void IMUManager::getCalibration(uint8_t& sys, uint8_t& gyro, uint8_t& accel, uint8_t& mag) {
    if (_initialized) {
        I2cGuard guard(this);   // loop タスクから呼ばれる: IMUタスクと排他する
        _bno.getCalibration(&sys, &gyro, &accel, &mag);
    } else {
        sys = gyro = accel = mag = 0;
    }
}

int8_t IMUManager::getTemperature() {
    if (!_initialized) {
        return 0;
    }
    I2cGuard guard(this);
    return _bno.getTemp();
}

#else  // IMU_SENSOR_M5IMU
void IMUManager::displayCalibrationStatus() {
    if (!_initialized) {
        Serial.println("IMU not initialized!");
        return;
    }

    Serial.println("\n=== IMU Status (M5 internal, 6-axis) ===");
    Serial.printf("Gyro bias: %s [deg/s] x=%.4f y=%.4f z=%.4f\n",
                  _biasReady ? "ready" : "pending",
                  _gyroBias[0], _gyroBias[1], _gyroBias[2]);
    Serial.println("(no magnetometer: heading drifts over time)");
    Serial.println("========================================");
}

bool IMUManager::isFullyCalibrated() {
    // 6軸構成にオンチップ校正レベルは無いため、ジャイロバイアス推定済みを完了とみなす
    return _initialized && _biasReady;
}

void IMUManager::getCalibration(uint8_t& sys, uint8_t& gyro, uint8_t& accel, uint8_t& mag) {
    // BNO055 互換の 0..3 スケールに写像。地磁気を持たないため mag は常に 0。
    uint8_t level = (_initialized && _biasReady) ? 3 : 0;
    sys = level;
    gyro = level;
    accel = level;
    mag = 0;
}

int8_t IMUManager::getTemperature() {
    if (!_initialized) {
        return 0;
    }
    float t = 0.0f;
    I2cGuard guard(this);
    M5.Imu.getTemp(&t);
    return (int8_t)t;
}
#endif

void IMUManager::printStatus() {
    Serial.println("\n=== IMU Status ===");
    
    if (!_initialized) {
        Serial.println("Status: Not initialized");
        Serial.println("==================");
        return;
    }
    
    Serial.println("Status: Initialized");
    Serial.printf("Temperature: %d°C\n", getTemperature());
    
    // キャリブレーション状態
    uint8_t system, gyro, accel, mag = 0;
    getCalibration(system, gyro, accel, mag);
    Serial.printf("Calibration - Sys:%d Gyro:%d Accel:%d Mag:%d\n", 
                  system, gyro, accel, mag);
    
    // クォータニオン
    Serial.printf("Quaternion - w:%.3f x:%.3f y:%.3f z:%.3f\n",
                  _quat.w(), _quat.x(), _quat.y(), _quat.z());
    
    // オイラー角
    Serial.printf("Euler - Heading:%.2f° Roll:%.2f° Pitch:%.2f°\n",
                  _euler.x(), _euler.y(), _euler.z());
    
    Serial.println("==================");
}

} // namespace sastle
