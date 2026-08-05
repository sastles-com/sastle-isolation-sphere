> [English](README.md) · **日本語**

# TimeSync ホスト単体テスト

実機・MQTT broker なしで `TimeSync` の時刻計算ロジックを検証するテスト。
`millis()` をシム (`shim/Arduino.h`) で差し替え、実際の `core/src/TimeSync.cpp`
をそのままホスト (g++) でコンパイル・リンクして実行する。

PlatformIO の `test/` ではなく `tools/` 配下に置いているのは、`pio test` が
ターゲット (ESP32) 向けにビルドしようとして干渉するのを避けるため。

## 実行

```sh
cd core
pio run              # 一度ビルドして ArduinoJson を .pio/libdeps に用意 (初回のみ)
bash tools/timesync_test/run.sh
```

全チェックが `[ok]` で `0 failures` なら合格 (終了コード 0)。

## 検証項目

1. 初回同期とクロック補間 (`syncedNow()` が経過に追従)
2. EMA 平滑化 (小ジッタは 1/4 だけ反映)
3. 単発外れ値の棄却 (WiFi ジッタスパイクを弾く)
4. 連続外れ値での追従再同期 (サーバー再起動等の実クロック飛びに追従)
5. **millis() 32bit ラップ跨ぎの吸収** (最重要: `syncedNow()` が連続し、
   ラップ後ビーコンが外れ値誤判定されない)
6. 不正ペイロード (非JSON / `epoch_ms` 欠落) の拒否

設計: `core/doc/time_sync_show.md`
