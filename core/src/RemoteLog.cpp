#include "RemoteLog.h"
#include "MQTTManager.h"
#include <string.h>

namespace sastle {

RemoteLog Log;

void RemoteLog::begin(MQTTManager* mqtt, const char* suffix) {
    _mqtt = mqtt;
    _suffix = suffix;
}

size_t RemoteLog::write(uint8_t c) {
    // まず必ず Serial にミラー (USB 接続時はそのまま見える)
    Serial.write(c);

    if (c == '\r') {
        return 1;  // CR は行組み立てに含めない
    }
    if (c == '\n') {
        _line[_lineLen] = '\0';
        if (_lineLen > 0) {
            // 接続済みなら即 publish、未接続なら退避
            if (!publishLine(_line)) {
                pushBacklog(_line, _lineLen);
            }
        }
        _lineLen = 0;
        return 1;
    }
    if (_lineLen < sizeof(_line) - 1) {
        _line[_lineLen++] = (char)c;
    }
    // バッファ溢れ時は超過分を捨てる (行頭は保持)
    return 1;
}

size_t RemoteLog::write(const uint8_t* buffer, size_t size) {
    for (size_t i = 0; i < size; ++i) {
        write(buffer[i]);
    }
    return size;
}

bool RemoteLog::publishLine(const char* line) {
    if (!_mqtt || !_suffix) {
        return false;
    }
    if (_busy) {
        // publish 経路が内部でログ出力した場合の再入。
        // Serial には既に出ているので MQTT は諦める (true=退避不要)。
        return true;
    }
    _busy = true;
    bool ok = false;
    if (_mqtt->isConnected()) {
        // 宛先は接続後の clientId (config の sphere.id) から動的解決
        ok = _mqtt->publishDevice(_suffix, line, false);
    }
    _busy = false;
    return ok;
}

void RemoteLog::pushBacklog(const char* line, size_t len) {
    // '\n' 区切りで追記。容量を超えたら以降は破棄し、
    // 起動シーケンス (最初のログ) を優先的に残す。
    if (_backlogLen + len + 1 > kBacklogCapacity) {
        _backlogDropped = true;
        return;
    }
    memcpy(_backlog + _backlogLen, line, len);
    _backlogLen += len;
    _backlog[_backlogLen++] = '\n';
}

void RemoteLog::loop() {
    if (_backlogLen == 0 || !_mqtt || !_suffix || !_mqtt->isConnected()) {
        return;
    }
    // 退避済みログを行ごとに送出
    size_t start = 0;
    for (size_t i = 0; i < _backlogLen; ++i) {
        if (_backlog[i] == '\n') {
            _backlog[i] = '\0';
            if (i > start) {
                publishLine(_backlog + start);
            }
            start = i + 1;
        }
    }
    _backlogLen = 0;
    if (_backlogDropped) {
        _backlogDropped = false;
        publishLine("[RemoteLog] (一部の起動前ログは容量超過で破棄されました)");
    }
}

} // namespace sastle
