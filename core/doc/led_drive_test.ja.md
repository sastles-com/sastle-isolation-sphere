> [English](led_drive_test.md) · **日本語**

# M5AtomS3R LED駆動検証 (V2ハードウェア構成)

V2基板 (FPC-isolation-sphere/kiban/) は発注済みだが、到着前に
**M5AtomS3R 単体 + 手持ちのWS2812ストリップ**で「5ストリップ×160 LED = 800 LED」
構成を駆動できるかをベンチ検証するためのテストファームウェア。

- ソース: `src/led_drive_test/led_drive_test_main.cpp`
- ビルド環境: `pio run -e led_drive_test -t upload` (本体 `env:atoms3r` とは独立)

## 検証の論点

| # | 論点 | 背景 |
|---|---|---|
| 1 | 5本目のデータ出力 | ESP32-S3 の RMT TX チャネルは **4ch**。FastLED が5本目をどう扱うか (多重化で逐次送信 → show() 時間が約2倍になる想定) を実測する |
| 2 | show() 時間 | 160 LED × 30µs ≈ 4.8ms/ストリップ。4ch並列+1本逐次なら ≈9.6ms → 30fps (33ms周期) には十分収まる見込み。実測で確認 |
| 3 | チェーン順序 | CHASE モードで DIN→DOUT の流れと160個のカウントを目視確認 |
| 4 | 電流と輝度上限 | 全白@255 は 800 LED で約48A となり論外。輝度上限 64/255 をコードで強制 (ポゴピン RTLECS 1.5A/pin ×2枝 の制約に対応する運用の予行) |

## ピン割当 (要確認)

core-M5atom-FPC 基板のネット `GPIO01`〜`GPIO05` が5本のLEDデータ線。
AtomS3R 底面ソケット (J3 "M5atom-L" / J4 "M5atomS3-R") との物理対応は
**以下の仮説**でデフォルト値を設定している。**基板設計者の確認が必要**:

| 基板ネット | 仮説の ESP32-S3 ピン | ビルドフラグ |
|---|---|---|
| GPIO01 | G5 | `LED_TEST_PIN0` (default 5) |
| GPIO02 | G6 | `LED_TEST_PIN1` (default 6) |
| GPIO03 | G7 | `LED_TEST_PIN2` (default 7) |
| GPIO04 | G8 | `LED_TEST_PIN3` (default 8) |
| GPIO05 | G38 | `LED_TEST_PIN4` (default 38) |
| GPIO06 (予備?) | G39 | — |

違っていた場合は `platformio.ini` の `env:led_drive_test` に
`-D LED_TEST_PIN4=39` のように追加して上書きする。

参考: 現行本体ファーム (`BoardConfig.h`) は4ストリップ G5/G6/G7/G8。
I2C (BNO055) は Grove ポート G2(SDA)/G1(SCL) — 基板の J9 経由。

## テストモード (本体ボタンで切替)

| モード | 表示 | 確認内容 |
|---|---|---|
| STRIP_ID | ストリップ毎の単色 (R/G/B/Y/M) | 5出力すべての疎通、配線とストリップ番号の対応 |
| CHASE | 白1ピクセルが走る + 先頭に識別色 | チェーン順序、LED数 (160) |
| RAINBOW | レインボースクロール | 描画のなめらかさ、ちらつき |
| WHITE_64 | 輝度64の全白 | 電流実測 (クランプメータ/電源表示) |
| FPS_BENCH | 最速ループ | show() 平均時間と上限FPS |

LCD とシリアル (115200) に FPS / show()平均時間 / ピン割当を毎秒表示する。

## 期待される結果 (合格基準)

- 5ストリップすべて点灯・正しい色
- FPS_BENCH で show() ≈ 10ms 以下 → 実効 100fps 級、本番目標 30fps に余裕
- 30fps モード時にちらつき・色化けなし
- WHITE_64 での電流が電源・ポゴピン定格の想定内

show() が大幅に遅い (>20ms) 場合の代替策:
FastLED の ESP32-S3 I2S/LCD並列ドライバ (`FASTLED_USES_ESP32S3_I2S`) で
最大16ストリップ完全並列出力に切り替える。
