# HANDOFF 2026-08-24 — 映像マッピング全面修正 & IMU連動の未解決課題

> 目的: 実機(sphere001)での映像表示問題を解決した記録と、IMU連動・クラッシュ問題の
> 継続調査のための引き継ぎ。関連コミット: `31e0177`〜`e6b8282` (main にマージ済み)。

---

## 1. 背景

「動画を再生すると正しく表示されない(画像が崩れる/ノイズ)」から始まり、原因が
**4層に積み重なっていた**ことが判明した。表示系は解決、IMU系は一部未解決。

診断は「テストパターン先行」で進めた。使用したパターンは WebUI のライブラリに
`kind=pattern` で登録済み(子午線スイープ / 静止子午線 / 緯度スイープ / 5色バンド)。

---

## 2. 解決済み: 映像マッピング

原因は独立した4つで、すべて実機で解決を確認した。

| # | 原因 | 修正 | コミット |
|---|------|------|----------|
| 1 | ファーム `sphereToUV()` が軸転置。しかも `_atan2` の第1引数が常に≥0で u∈[0.5,1.0] → **画像の右半分しか使っていなかった** | Z極軸の標準正距円筒へ是正 (ツイン `HoloSphere.jsx:71` と逐語一致) | `31e0177` |
| 2 | 天地が逆(北極が下にマッピング) | レイアウトCSVをY軸周り180°回転 | `06888ce` |
| 3 | CSVの行ブロック順と物理ゴア順が逆回り | 行ブロック単位で並べ替え | `66e25aa` |
| 4 | **カセット内の巡回経路が設計時の仮想経路のままで実FPC配線と不一致**(細線が蛇行・白キャップが崩れる主因) | 実PCB経路から再導出 | `06888ce` |

### 重要な落とし穴

- **ファームは CSV の `strip` 列を見ていない。ファイルの行順でチェーンに割り当てる。**
  レイアウトを並べ替えるときはラベルではなく**行ブロック自体**を動かすこと
  (ラベルだけ書き換えても描画に一切影響せず、「何も変わらない」を2回起こした)。
- **CSV変更は `pio run -t uploadfs -e atoms3r_ota` が必須**。ファーム書き込みだけでは
  LittleFS上の古いCSVが残る。

### 実FPC配線順の確定方法

- **カセット頂点(極) = ローカル `num49`** (PCB上の `D50`。D1=num0 始まり)。
  実機のピクセル単体点灯テストで検証済み。
- 北カセット: FPC-isolation-sphere の `shell-cad/scripts/export_led_positions.py` が
  手動検証済み legend (`output/fpc_unfold_c0.csv`) から生成した順序を採用。
- 南カセット: 「同一基板の180°回転取付(適回転合同)」という物理事実から**幾何導出**。
  回転軸は一意に決まった(軸経度162°、残差 <0.0001mm)。導出した極 `num129` が
  実機テスト結果と独立に一致したことが正しさの裏付け。
- ツール: `core/tools/normalize_layout.py` (`--rotate-y180` / `--reverse-strips` /
  `--flip-axis`)。raw CSV から data CSV を再生成する。

**残課題**: FPC側リポジトリの `output/fpc_unfold_c5.csv` (南カセット legend) が
未更新のため、`export_led_positions.py` を再実行すると南半分が誤順に戻る。
上流を直すか、当リポジトリ側の幾何導出を正とするか決める必要がある。

---

## 3. 解決済み: 電源・起動の安定化

| 症状 | 原因 | 対策 |
|------|------|------|
| 起動時に一部カセットが真っ白に点灯し、電源が落ちて起動失敗 | WS2812 がデータ線フロート中のノイズを拾ってランダム点灯(白=フル電流) | `LEDManager::earlyBlank()` を `setup()` の**最初の行**で呼び全消灯。実機で解消確認 |
| 高負荷フレームでの電圧降下 | 全白などで電流が容量超過 | `FastLED.setMaxPowerInVoltsAndMilliamps(5, 2000)` (2A上限)、オープニング輝度30% |
| 原因不明の瞬断が多発 | **「sphere001」を名乗る2台目のAtomS3**が同一静的IP(.101)・同一MQTT IDで衝突 | 2台目は電源OFF運用。正式共存は Phase 5 で |

`earlyBlank()` は FastLED を**実バッファで直接登録**する方式にした。
`setLeds()` によるバッファ差し替えは ESP32 RMT ドライバで効かず、
**全消灯のまま(strip/XYZ/video が何も表示されない)**になる。LEDバッファは
静的確保 (`s_ledBufferStatic`) で `earlyBlank()` と `begin()` が共有する。

### 実機のMACについて

実機 sphere001 の MAC は `ac:27:6e:d1:93:7c`。`core/data/config.json` に記載の
`F0:9E:9E:32:67:D0` は別ユニットか古い記録。IP衝突の切り分け時に注意。

---

## 4. 未解決: IMU連動

### 判明した事実

- **I2C は 100kHz 必須**。400kHz では BNO055 のクロックストレッチでレジスタ読みが
  化ける(w のみ変動し x/y/z=0、ノルム≠1 の値が混入)。
- **Adafruit の `getQuat()` は I2C 失敗を無視して古いバッファを返す**。
  そのため「停滞 → たまに成功して大ジャンプ」というカクつきになる。
  → エラー検出付きの直接レジスタ読み(0x20 から8バイト)+リトライに置換。
- **この個体はノルム0.96〜0.99 の quat を常用する**。「単位でなければ破棄」にすると
  正常値の半分以上を捨てて実効レートが半減する(実測 disc=21〜41/s)。
  → 妥当範囲(0.5〜2.0)なら**正規化して受理**、明らかな化けのみ破棄に変更。
- 座標系: BNO055 の quat は本システム規約と回転の向きが逆。**完全共役 (w,-x,-y,-z)**
  で一致する(3軸それぞれ実機テストで確認)。`getQuaternion()` 内で変換しているため
  LED描画・MQTT配信(ツイン)・ジェスチャすべてに一括適用される。
- `setExtCrystalUse(false)` (外部水晶なしボード)。fusionモード間の**直接切替は無視される**
  ので `OPERATION_MODE_CONFIG` を必ず経由する。
- 動作モードは **IMUPLUS (6軸、磁気不使用)**。NDOF だと磁気キャリブ未完了時に
  ヨーが「動かない→90°ジャンプ」する。本機はLED大電流・LiPoが磁気センサー近傍にあり
  磁気データが常に乱れるため NDOF は不採用。
- 実効レート計装 `[RATE]` で `loop=43/s, imu_read=43/s, fail=0/s` を確認。
  設計100Hzには届いていない(loop に MQTT・LCD描画・OTA が同居しているため)。

### 残っている症状

1. **速い回転で追従が遅れる/カクつく**。以前の Isolation cube は35Hzで滑らかだったのに
   対し、こちらは体感で明らかに劣る。ノルム正規化受理の修正を**まだ実機で検証できていない**
   (デバイスがオフラインのままビルド済み)。
2. **fusion 出力そのものが死んでいる疑い**。`mode=8` (IMUPLUS) は正しく、gyro/accel の
   生データも健全なのに quat/euler が回転に追従しないことがあった。
   **クローンチップの可能性**(BNO055 は偽物が流通しており、fusion だけ機能しない個体がある)。
3. **動画再生中に回転させるとフリーズ→再起動**。子午線が移動する動画で特に再現しやすい。
   リセット理由の常時監視を仕掛けてあるが、まだ捕まえていない。

### 次の一手(優先順)

1. **ビルド済みファームの書き込み**: ノルム正規化受理 + `_quat` のコア間排他
   (renderタスクとの torn read 防止) + `[RATE]` 計装。電源が入り次第
   `cd core && pio run -e atoms3r_ota -t upload`。
2. **クラッシュのリセット理由を確定**: `PANIC`/`WDT` ならソフト(今日初めて発火するように
   なったジェスチャ検出パス `GestureManager::detectRotation` が容疑)、
   `POWERON`/`BROWNOUT` なら回転によるマグネットコネクタへの機械的ストレス。
3. **fusion が死んでいると確定したら Madgwick ソフト融合へ移行**。
   `IMU_SENSOR_M5IMU` 側に `MadgwickAHRS` の実装が既にあるので、BNO055 の生 gyro/accel に
   流用すればチップの真贋に依存しなくなる(1時間程度)。
4. 追従レートをさらに上げるなら、**IMU読み取りを専用タスク(高優先度)に分離**、または
   レンダリングタスク(core1)側で直読みして cube と同じ密結合に戻す。

---

## 5. 返済すべき技術的負債 (main に入っている)

| 場所 | 内容 |
|------|------|
| `core/src/IMUManager.cpp` | fusion停止ウォッチドッグの自動復旧が **`if (fusionDead && false)`** で無効化中(thrashing防止のため一時的に切った)。原因確定後に戻す |
| `core/src/main.cpp` | `[RATE]` 計装ログ(loop/imu_read/fail/disc)。調査完了後に削除または頻度を落とす |
| `core/src/IMUManager.cpp` | `quat read stats` が `Serial` 直書きで MQTT ログに出ない。`sastle::Log` 経由に直すか削除 |
| `core/src/main.cpp` | 周期IMUログが euler/cal/mode 込みで冗長。落ち着いたら簡素化 |

---

## 6. 副次的に入れた改善

- **VideoStreamer 停止時に黒フレームを1枚送信** (`_send_black_frame`)。デバイスは最後に
  受信したフレームを表示し続けるため、これが無いと停止後も残像が残る。
- **`led {imu_comp: bool}`** で IMU補正をランタイム切替可能に(診断でボディ固定表示にできる)。
  OFF時は `_pxLUT` の高速パスに入る。
- **PatternPanel に XYZ ボタン**(映像表示のまま軸マーカーを重畳)。`state_manager` が
  `led.axis` を状態として保持するようになり選択が維持される。
- **`[BOOT] Reset reason`** を起動ログに出力(POWERON/PANIC/BROWNOUT の切り分け用)。
- **config.json の `params` ブロックを起動時に反映**(以前は完全に無視されていた)。
  輝度デフォルトは core/server ともに 50 に統一。
- **LED配置グラフ**: プレビューPNGを両READMEに追加し、インタラクティブ版は
  GitHub Pages (`docs/led_layout_3d.html`) で配信。

---

## 7. 実機作業のチートシート

```bash
# ファーム書き込み (OTA, UDP 3232)
cd core && pio run -e atoms3r_ota -t upload

# LittleFS (CSV/config 変更時は必須)
cd core && pio run -e atoms3r_ota -t uploadfs

# レイアウトCSV再生成
cd core && python3 tools/normalize_layout.py --rotate-y180 --reverse-strips

# 可視化HTML再生成 (docs/ にもコピーすること)
cd core/data && python3 plot_led_layout_3d.py && cp led_layout_3d.html ../../docs/

# テストパターン配信 (サーバー経由。WebUIのパターンタブからも可)
curl -s -X POST http://localhost:9000/api/playlist/play/<id>

# デバイスログ監視
mosquitto_sub -h localhost -t 'sphere/+/log' -v

# 診断コマンド (MQTT直送。サーバー再起動が要らない)
mosquitto_pub -h localhost -t sphere/all/command/led -m '{"mode":"test","pattern":"strip_id"}'
mosquitto_pub -h localhost -t sphere/all/command/led -m '{"mode":"sphere","axis":true}'
mosquitto_pub -h localhost -t sphere/all/command/led -m '{"imu_comp":false}'
mosquitto_pub -h localhost -t sphere/all/command/params -m '{"brightness":40}'
```

注意:
- **`mode:"test"` は輝度を24に固定し映像をバイパスする**。診断後は必ず `{"mode":"sphere"}` に戻す。
- **単体LEDの点灯テストは輝度40%以上で**。8%などでは数個のLEDは目視できない。
- pixels モードの index = `strip * 160 + num`。MQTTバッファ上限2KBなので
  ペイロードは分割して送る(約50要素まで)。
- WebUI は **ポート9000** (`isolation-server.service`, uvicorn)。8080は別アプリ。

---

## 8. 次フェーズの計画

`~/.claude/plans/curried-singing-wozniak.md` に Phase 4 / Phase 5 の詳細設計がある。

- **Phase 4: 天地キャリブレーション** — 「SET ZERO」ボタンで基準quaternionを捕捉し
  `q_corr = q_ref⁻¹ ⊗ q` を適用。永続化は **NVS (Preferences)** を使う
  (config.json 書き戻しは `uploadfs` で消えるため不可)。IMU のヨードリフト対策も兼ねる。
- **Phase 5: 複数core制御** — デバイスレジストリ、`sphere/<id>/command/#` の個別購読、
  VideoStreamer の複数ターゲット配信、OTAホスト名の config 化。
  2台目AtomS3に別IDを振る作業もここに合流する。
