# 開発引き継ぎ資料 — Isolation Sphere

最終更新: 2026-06-14 / 作成環境: macOS (開発機) → 引き継ぎ先: サーバー稼働 PC (Ubuntu)

このドキュメントは、**サーバーを稼働させている PC 上で開発を継続する**ための引き継ぎ資料。
プロジェクト全体像・現状・環境構築・検証手順・次の作業をまとめる。
詳細は各 `README.md` / `core/doc/` / `docs/` を参照。

> **追補 (2026-06-13a)**: ボード抽象化 (M5AtomS3R/XIAO)、Mac→Ubuntu の OTG 書き込みフロー、
> ESP32 専用 P2P AP の実構築と device↔broker↔server 疎通までを完了。
> 詳細は [`docs/HANDOFF_2026-06-13_bench_bringup.md`](docs/HANDOFF_2026-06-13_bench_bringup.md)、
> AP 再現は [`server/scripts/setup_p2p_ap.sh`](server/scripts/setup_p2p_ap.sh)。
>
> **追補 (2026-06-13b)**: ボール封止で USB が届かない開発機向けに **ケーブルレス開発**を実装・実機確認。
> ① **WiFi 無線書き込み (espota OTA)** — GMKTec から `pio run -e atoms3r_ota -t upload`。
> ② **MQTT デバッグログ** — `sphere/sphere001/log` を Web UI (**CONTROL → Logs** タブ) にリアルタイム表示。
> 途中で **partitions.csv の otadata 欠落バグ**を発見・修正 (これが無いと OTA は永遠に有効化失敗)。
> 詳細は §5 完了済み / §8 次にやること を参照。
>
> **追補 (2026-06-13c)**: ① **デバイス ID 動的化** — トピックの `sphere001` ハードコードを排除し
> config.json の `sphere.id` を参照 (実機確認済み)。② **LED 駆動ベンチ実走** — `led_drive_test` 実機計測:
> **5 ストリップ×160=800 LED 全て初期化成功** (pins 5/6/7/8/38)、**show()=15.0ms (≒上限66fps、30fps目標クリア)**。
> ③ **OTA ロールバック策は不要** (メンテ時に分解可能とユーザー確定)。④ **otadata の運用上の罠**を §9 に追記。
> ⑤ GMKTec を main ブランチへ整合 (audit 作業は stash 退避)。
>
> **追補 (2026-06-14): 映像パイプライン最適化で 10Hz 達成 → ~20fps**。計測駆動で
> ボトルネックを潰した。詳細は §10。要点:
> ① **UDP映像受信が完全に壊れていた** → WiFiUDP(BSDソケットポーリング)が本環境で
> 機能せず受信ゼロ。**AsyncUDP(lwIPコールバック直結)へ置換して修復**(これが最重要)。
> ② **真のFPS律速は LCDデバッグ描画**(128×128を毎フレーム writePixel ~150ms)だった
> → 一括 pushImage + 10Hzスロットルで解消。③ マッピング 27→4ms(FastMath float化)。
> ④ デコードを Core0 専用タスクへ分離し描画(Core1)と並列化 → ~20fps。

---

## 1. プロジェクト全体像

直径 ~110mm の **球体型 LED ディスプレイ**。800 個の WS2812 を球面に配置し、
リアルタイム映像表示 + IMU 姿勢補正 + WebUI/ジョイスティック制御を行う統合システム。

```
┌──────────┐   MQTT(制御/状態)   ┌──────────────┐   MQTT+UDP   ┌──────────────┐
│  WebUI    │ ───────────────▶ │   Server      │ ──────────▶ │  ESP32 Core   │
│ (ブラウザ) │ ◀─ WebSocket ──  │ FastAPI+React │ ◀── MQTT ── │  M5AtomS3R    │
└──────────┘                   │ MQTT Broker   │             │ 800 LED + IMU │
                               └──────────────┘             └──────────────┘
```

- **制御・状態**: MQTT (port 1883)
- **映像ストリーム**: UDP (port 8889、RGB565 JPEG 320×160)
- **UI リアルタイム更新**: WebSocket (`/ws`)

### 関連リポジトリ

| リポジトリ | 役割 | パス (開発機) |
|---|---|---|
| **sastle-isolation-sphere** (本リポジトリ) | ファームウェア (`core/`) + サーバー (`server/`) | `~/work/sastle-isolation-sphere` |
| **FPC-isolation-sphere** | ハードウェア (KiCad 基板4枚 + Blender CAD) | `~/work/FPC-isolation-sphere` |

---

## 2. リポジトリ構成

```
sastle-isolation-sphere/
├── core/                       ESP32 ファームウェア (PlatformIO)
│   ├── src/                    C++ ソース (main.cpp, LEDManager, IMUManager, ...)
│   │   ├── BoardConfig.h        ハードウェア定数 (ピン/LED数/FPS)
│   │   ├── MqttTopics.h         MQTT トピック定義 (サーバーと対応)
│   │   └── led_drive_test/      ★ V2 LED駆動ベンチ検証ファーム (本体とは独立)
│   ├── data/config.json        デバイス設定 (WiFi/broker/UDP) — サーバーも参照
│   ├── doc/                     設計ドキュメント (dual_core, imu, led_drive_test 等)
│   └── platformio.ini          ビルド環境 (atoms3r / led_drive_test / native)
├── server/                     制御サーバー
│   ├── app/                    FastAPI (main.py, api/, services/, core/config.py)
│   ├── frontend/               React UI (dist/ にビルド済み、main.py が静的配信)
│   ├── scripts/                ★ verify_server_comm.* (通信検証ハーネス)
│   ├── tests/                  pytest
│   └── pyproject.toml          Python 依存 (uv 管理)
├── docs/                       システム設計ドキュメント
├── HANDOFF.md                  ★ このファイル
├── migration_guide_ubuntu.md   Ubuntu 移行ガイド (※ROS2 記述は古い、下記注意)
└── server_*.sh / fix_server_service.sh   systemd 運用スクリプト
```

---

## 3. サーバー PC での開発環境セットアップ

### 3.1 サーバー (Python)

```bash
cd ~/work/.../server          # 本番では /home/yakatano/work/m5atoms3r/repo/server
uv sync                        # pyproject.toml から .venv を構築
# 依存: fastapi, uvicorn[standard], websockets, paho-mqtt>=2.0, pydantic v2, evdev(Linux)
```

開発時の起動 (ホットリロード):

```bash
SPHERE_MQTT_BROKER=localhost .venv/bin/python -m uvicorn app.main:app \
  --host 0.0.0.0 --port 9000 --reload
```

- ブローカーは環境変数 `SPHERE_MQTT_BROKER` で上書き可。未指定時は
  `core/data/config.json` の `wifi.broker` (現在 `192.168.49.1`) → `localhost` の順で解決。
- フロントエンドは `frontend/dist/` が `main.py` から自動配信される。
  UI を変更する場合は `cd frontend && npm install && npm run build`。

### 3.2 本番運用 (systemd)

本番サーバーは systemd サービス `isolation-server` として常駐 (port **9000**)。

```bash
bash server_status.sh    # 状態確認 + 直近ログ
bash server_restart.sh   # 再起動
bash server_logs.sh      # ログ追従 (journalctl -f)
# サービス定義の修正: fix_server_service.sh (ExecStart/WorkingDirectory/User を編集)
```

### 3.3 MQTT ブローカー

ローカル検証には mosquitto が必要:

```bash
sudo apt install mosquitto mosquitto-clients   # Ubuntu
mosquitto -p 1883 &                            # 単発起動
mosquitto_sub -t 'sphere/#' -v                 # 全トピック傍受 (デバッグに便利)
```

### 3.4 ファームウェア (PlatformIO)

```bash
cd core
pio run -e atoms3r              # 本体ファームをビルド
pio run -e atoms3r -t upload    # AtomS3R へ書き込み (upload_port=/dev/ttyACM0)
pio run -e led_drive_test -t upload   # LED駆動ベンチ検証ファーム
```

> ⚠️ **開発機 (macOS) の落とし穴**: pyenv shim の `pio` (6.1.15) はツール取得が
> HTTPClientError で壊れていた。`~/.platformio/penv/bin/pio` (6.1.19) を使用。
> サーバー PC では `pip install platformio` で素直に入る想定。

---

## 4. 通信契約 (MQTT)

`core/src/MqttTopics.h` ⇔ `server/app/core/config.py` で対応。デバイス ID は `sphere001`。

| 方向 | トピック | ペイロード |
|---|---|---|
| デバイス→サーバー | `sphere/sphere001/imu` | `{"w","x","y","z"}` クォータニオン |
| デバイス→サーバー | `sphere/sphere001/state` | デバイス状態 |
| デバイス→サーバー | `sphere/sphere001/status` | `"online"`/`"offline"` (retained) |
| デバイス→サーバー | `sphere/sphere001/log` | デバッグログ1行 (**プレーンテキスト**, JSON ではない) |
| サーバー→デバイス | `sphere/all/command/{playback,params,led,system}` | コマンド (例 `{"brightness":85}`) |
| サーバー→デバイス | `sphere/all/state` | 全状態 (retained, ブロードキャスト) |

REST API (FastAPI): `/api/command/{playback,params,led}`, `/api/config`, `/api/playlist`, `/health`。
WebSocket `/ws` は接続時に現在状態を送り、以後 `STATE_UPDATE` をプッシュ。
ログは別種メッセージ `{"type":"LOG_LINE","payload":{"line":...}}` で配信 (状態には混ぜない)。

---

## 5. これまでの作業と現状

### 完了済み

- **リファクタリング一巡** (core/server/frontend): 定数集約 (BoardConfig.h, MqttTopics.h)、
  重複統合、ROS2 残骸とデバッグログの除去、pydantic v2 移行。git log 参照。
- **LED 配置データ更新** (V2 ジオメトリの `led_layout` 再生成・UV マッピング更新) — ユーザー確認済み。
- **LED 駆動ベンチ検証ファーム** (`core/src/led_drive_test/`): 5ストリップ×160=800 LED を
  AtomS3R で駆動できるか検証する独立環境。ビルド成功。詳細 `core/doc/led_drive_test.md`。
- **サーバー⇔デバイス通信検証ハーネス** (`server/scripts/verify_server_comm.*`):
  実機不要で MQTT 契約を双方向検証。**4/4 ケース成功を確認済み**。
  詳細 `server/scripts/README_verify_comm.md`。
- **ケーブルレス開発 (WiFi 無線書き込み + MQTT ログ)** — 2026-06-13、実機 (AtomS3R) で end-to-end 確認済み:
  - **espota OTA**: `core/src/OtaManager.{h,cpp}` (ArduinoOTA ラッパー、OTA 開始時に
    `LEDManager::stopRenderTask()` で描画停止)。`platformio.ini` に `[env:atoms3r_ota]` /
    `[env:xiao_esp32s3_ota]` を追加。GMKTec から `pio run -e atoms3r_ota -t upload` で
    無線書き込み → 自動リブートまで確認。
  - **partitions.csv の otadata 修正**: 旧テーブルは ota_0/ota_1 はあるが **otadata 欠落**で、
    OTA 転送 100% 成功でも `esp_ota_set_boot_partition` が記録できず `End failed`
    (`Could Not Activate The Firmware`) になっていた。nvs を 0x6000→0x4000 に縮小し
    0xd000 に `otadata,data,ota,0x2000` を挿入 (ota_0/ota_1/spiffs のオフセット不変=
    LittleFS データ温存)。**OTA を使う ESP32 では otadata 必須**。
  - **MQTT デバッグログ**: `core/src/RemoteLog.{h,cpp}` が Serial と MQTT へ tee
    (`sastle::Log`、MQTT 未接続時は最大 4KB バックログに退避→接続後フラッシュ)。
    トピック `sphere/sphere001/log`。`common.h` の `DEBUG_*` と main.cpp の Serial 出力を経由。
    サーバー側は `mqtt_service.py` が購読 (JSON パース前にテキスト分岐) →
    `state_manager.broadcast_log()` で WS へ `LOG_LINE` 配信。UI は **CONTROL → Logs** タブ
    (`frontend/src/components/debug/LogPanel.jsx`)。
  - 詳細手順はメモリ `ubuntu-flashing-environment` に記録。**OTA 対応ファームは初回のみ
    USB(OTG) で焼く必要がある** (ブートストラップ)。

### 確定したハード前提 (ユーザー確認済み 2026-06-11)

- LED データ線: 基板ネット GPIO01〜05 = AtomS3R 実ピン **G5/G6/G7/G8/G38** (G39 予備)。
- I2C (BNO055): Grove ポート **G2(SDA)/G1(SCL)**。
- V2 確定仕様: **5 ストリップ × 160 = 800 LED** (現行本体ファーム `BoardConfig.h` は 4 ストリップ G5-G8 のまま)。

---

## 6. 検証ハーネスの使い方

### LED 駆動 (実機 + WS2812 ストリップ)

```bash
cd core && pio run -e led_drive_test -t upload
```
本体ボタンで STRIP_ID / CHASE / RAINBOW / WHITE_64 / FPS_BENCH を切替。
LCD とシリアル(115200) に FPS と show() 平均時間を表示。合格基準は doc 参照。

### サーバー通信 (実機不要)

```bash
bash server/scripts/verify_server_comm.sh
```
mosquitto + FastAPI サーバーを自動起動 → デバイスシミュレータで双方向検証 → 停止。

---

## 7. ハードウェア構成 (FPC-isolation-sphere リポジトリ)

KiCad 10 プロジェクト4枚 (S式テキスト、`kiban/`)。**現状の状態で発注済み、後で修正の可能性あり**。

| 基板 | 役割 | 主要部品 |
|---|---|---|
| `FPC-north` | LED シェル FPC | WS2812C-2020 ×80 + 0603 C ×80、2層、ステンシル版あり |
| `core-M5atom-FPC` | ESP32 コア基板 | AtomS3R 接続ヘッダ、BNO055 IMU、スピーカー PKLCS1212 |
| `power-IP2326-MP1584-FPC` | 充電+降圧電源 | TS2326(IP2326系) + MP1584EN |
| `mother-ring` | 分配ハブ | ポゴピン 1x06 ×5 で +5V/GND/データを5カセットに分配 |

電源チェーン: 電池 → power 基板 → mother-ring (ポゴピン分配) → FPC-north (LED) + core-M5atom-FPC。

CAD: `shell-cad/output/shell_cassettes--02.blend` (Blender 4.0.1)。
ツール (開発機 macOS): Blender 4.1 (`/Applications/Blender.app/.../Blender --background`)、
KiCad 10 (`/Applications/KiCad/KiCad.app/.../kicad-cli`)。

---

## 8. 次にやること (推奨順)

1. ~~LED 駆動ベンチ実走~~ → **計測完了** (2026-06-13c, `led_drive_test` 実機): 5 ストリップ
   (pins 5/6/7/8/38) 全初期化 OK、**show()=15.0ms (≒上限66fps、30fps クリア)**。
   残り (要・人手): FPS_BENCH の最大持続 fps (本体ボタン操作)、CHASE のチェーン順序目視、
   WHITE_LIMITED の全白電流実測 (電流計)。
2. **本体ファーム 5 ストリップ化** (ベンチ合格を受けて実施可): `BoardConfig.h` を
   4→5 ストリップ (G38 追加) に拡張、`LEDManager` の配列 (`_stripPins[4]` 等) を 5 に。
   ※ 現行 LED レイアウト/UV マッピングが 5×160 前提と一致するか要確認。
3. **実機通信結合**: `core/data/config.json` の `wifi.broker` を実ブローカー IP に設定、
   ファーム書き込み、`mosquitto_sub -t 'sphere/#' -v` で実機の配信/受信を観測。
4. **基板到着後**: core-M5atom-FPC 実機で BNO055(G2/G1) + スピーカー(GPIO08) 込み統合確認。

### ケーブルレス開発まわりの残作業 (2026-06-13b で実装した OTA/ログの磨き込み)

5. **OTA の堅牢化**:
   - ~~ロールバック安全策~~ → **不要** (2026-06-13c, メンテ時に分解可能とユーザー確定)。
   - **OTA パスワードを config 化**: 現状 `OtaManager.cpp` の `kOtaPassword` と
     `platformio.ini` の `--auth` に `isolation-sphere-ota` をハードコード。
     config.json には既に `system.ota` (username/password/listen_port) があるので、それを読む形に統一する。
   - **XIAO ESP32S3 での espota 実機確認** (現状 AtomS3R のみ確認済み。env は用意済み)。
   - **OTA 中の安全性**: 現状 LED 描画タスクのみ停止。IMU/MQTT タスク等も含め検証。
6. **MQTT ログの改善**:
   - **ノイズ低減 / ログレベル**: IMU(3秒毎)・state(5秒毎) ログが log トピックを占有する。
     レベル制御 (`g_debugEnabled` 連動 or トピック分離) や UI 側フィルタを検討。
   - **生 ESP_LOG の取りこぼし**: `sastle::Log` 経由のみ MQTT に乗る。`log_e`/`ESP_LOGx`
     (ライブラリ内部ログ) は Serial にしか出ない。`esp_log_set_vprintf` フックで拾うか検討。
   - **frontend バンドル肥大警告**: ビルドで 500KB 超の警告。code-split は未対応 (動作影響なし)。
7. **IP 体系の統一**: `config.json` は旧 `192.168.49.x`、新 server 仕様書は `192.168.100.1/24`。
   現状ベンチは旧体系で稼働。将来どちらかに統一する (メモリ `ubuntu-flashing-environment` 参照)。
8. ~~デバイス ID の動的化~~ → **完了** (2026-06-13c)。config.json `sphere.id` 駆動に。
   ただし **OTA ホスト名** (`OtaManager.cpp` の `kOtaHostname`) はまだ固定。複数台運用時はここも config 化。
9. ~~GMKTec の git 整合~~ → **完了** (2026-06-13c)。main へ切替済み、audit 作業は `git stash@{0}` に退避。
   audit 再開時は `git stash pop` (OTA 系の重複ファイルは main と同一内容なので衝突は main 側を採用)。

---

## 9. 注意・落とし穴

- **migration_guide_ubuntu.md の ROS2 記述は古い**: サーバーから ROS2/micro-ROS は
  削除済み (refactor commit `7b4edec`)。ROS2 セットアップ手順は無視してよい。
- **ポート**: 本番 systemd は **9000**、開発の uvicorn 例や検証ハーネスは 8000 を使用。
  混在に注意 (検証ハーネスは `--http localhost:8000` を引数で変更可)。
- **config.json は ESP32 とサーバーで共有**: `core/data/config.json` を両者が読む
  (`MIGRATION_PLAN.md` 参照)。broker/UDP ポート変更時は両側に波及する。
- **LED 全白の電流**: 800 LED 全白@255 は ~48A。ベンチ・本番とも輝度上限運用必須
  (検証ファームは 64/255 に強制)。ポゴピン定格 RTLECS 1.5A/pin。
- **デバイス ID は config.json の `sphere.id` 駆動** (2026-06-13c〜): ファーム
  (`MQTTManager::publishDevice`) もサーバー (`_load_device_id`) も `sphere.id` を読む。
  変更は config.json 1 箇所でよいが、**ファームとサーバーが同じ config.json を見ること**が前提。
- **⚠️ otadata の罠 (OTA→OTG 切替時)**: espota で書き込むと起動パーティションが ota_1 に
  切り替わる。その後 `pio run -t upload` (OTG=esptool) は ota_0 に書くため、**otadata が指す
  ota_1 (古いファーム) が起動し続け、OTG 書き込みが反映されない**ように見える。OTG を確実に
  反映するには otadata を消してから:
  `python .platformio/packages/tool-esptoolpy/esptool.py --chip esp32s3 -p /dev/ttyACM0 erase_region 0xd000 0x2000`
  → リセットすると ota_0 (今焼いたファーム) が起動する。LittleFS は無傷。
  (別ファーム env=`led_drive_test` へ OTG で切り替える時も同様に要 otadata 消去。)
- **⚠️ UDP受信は AsyncUDP 必須** (2026-06-14): `WiFiUDP`(BSDソケットのポーリング受信)は
  本ハード/coreで機能せず、`parsePacket()` が常に0を返し映像UDPを全く受信できなかった
  (TCP/MQTT・ICMPは正常、リンク0%ロス、パケットはAP側から送出済みなのにデバイスが受信せず)。
  `NetworkManager` の受信を **AsyncUDP**(lwIP `udp_recv` コールバック直結)に置換して解決。
  WiFiUDP に戻さないこと。併せて接続後 `WiFi.setSleep(false)` も必要。

---

## 10. 映像パイプライン性能 (2026-06-14)

計測駆動 (`[PERF]` ログ: map/out/decode/fps/recv/hits/drop) でボトルネックを順に解消し、
**10Hz目標 → 実測 ~20fps** を達成。各工程の実測 (320×160 JPEG / 800 LED, AtomS3R):

| 工程 | 値 | メモ |
|------|----|----|
| JPEGデコード | ~38–47ms | エントロピー復号支配。setJpgScale では縮まらない。Core0専用タスク |
| マッピング(球面サンプリング) | ~4ms | FastMath を double→**HW単精度sqrtf/float化**で 27→4ms。単一ループ化 + IMU-off時 静的UV-LUT |
| LED出力 show() | ~14ms | 800 WS2812 RMT。物理下限 |
| LCDデバッグ描画 | ~150ms→数ms | **真のFPS律速だった**。1画素 writePixel ×16384 → 一括 pushImage + 10Hzスロットル |

### 最終アーキテクチャ (end-to-end)
```
サーバー(server/scripts/stream_to_sphere.py): 動画/画像/パターン → 320x160 JPEG化
   → 16Bヘッダ(magic/frame_id/chunk_index/chunk_count/chunk_size) で ~1400Bチャンクに分割 → UDP:8889
        ↓ (アプリ層チャンク分割で IP断片化を回避)
Core0 デコードタスク: AsyncUDP受信(キュー) → frame_id単位でチャンク再構成(_udpBuffer)
   → JPEGデコード(~45ms) → publishFrame() でトリプルバッファの ready に公開
        ↓ (display/ready/decode の3枚, インデックス操作のみ排他 → テアリングなし)
Core1 レンダリングタスク: 連続駆動 ~50Hz: adoptReadyFrame()(新フレーム採用) →
   最新IMUクォータニオンで球面再マッピング(map ~4ms) + show()(~14ms)
```

**描画(IMU)と動画は分離・並列**:
- **IMU姿勢追従 ~50Hz**(動画の有無に依存せず連続再マッピング)。
- **動画差し替え ~15-20Hz**(decode律速、トリプルバッファで独立差替、drop=0)。

**重要な設計判断/落とし穴**:
- **UDP断片化**: 320x160 JPEG(数KB)はMTU超で断片化し、ESP32(WiFiUDP/AsyncUDPとも)で
  受信不可。**アプリ層でチャンク分割**(送信=stream_to_sphere.py / 受信=ImageManager再構成)で解決。
- **受信は AsyncUDP 必須**(WiFiUDPのポーリング受信は本環境で機能しない。§9参照)。
- マッピングの旧FastMath(double演算)が 27ms→4ms に。LCDデバッグ描画が真のFPS律速だった。

**今後さらに上げるなら**: デコードが律速 (~45ms)。入力解像度/フレームレート/生RGB565転送 等。
ただしIMU追従は既に~50Hzで滑らかなので、必要なのは動画fpsを上げたい場合のみ。
計測/送出ツール: `server/scripts/stream_to_sphere.py` (本番), GMKTec `/tmp/send_chunked.py` (テスト)。

---

## 11. 未実装機能 / 機能実装ロードマップ (2026-06-14)

映像の球面表示パイプライン(受信→再構成→デコード→IMUマッピング→描画)とケーブルレス開発
(OTA/ログ)は完成・実機確認済み。ここから先の**未実装機能**を棚卸しした。実装順は **B → A → C → D**
(土台→実運用→デバイス機能→入力)を想定。出典: `server/task.md` + コード内TODO + ベンチ検証知見。

### B. デバイス操作の到達性(配線の穴) ★最初 → ✅ 完了 (2026-06-14)
- **WSコマンドが実機に届かない**問題を修正済み。`StateManager.handle_websocket_message` が
  `_update_*` 後に `sphere/all/command/{params,playback,led}` へ publish するようにした
  (`_publish_command`)。WS経路の SET_PARAMS が `sphere/all/command/params` に到達することを
  mosquitto_sub で検証済み。

### A. 映像再生の統合(Phase2・実運用の本命) → ✅ 完了 (2026-06-14)
全サブ機能を実装・ローカル検証済み(実機LED確認は基板到着後)。
- **A-① DB配線+アップロード** ✅: `playlist.py` を SQLite バックエンドに刷新。
  `POST /videos`(multipart 保存 + opencv メタ抽出)/`GET /videos`/`DELETE /videos/{id}`。
  `main.py` lifespan で `app.state.db` 初期化。
- **A-③ 再生→ストリーミング連携** ✅: `app/services/video_streamer.py` (`VideoStreamer`)。
  別スレッドで動画を opencv デコード→320x160→JPEGチャンク(protocol §4)→UDP送出。
  `play/pause/resume/stop/play_entries`。送信先は config.json から解決。
- **A-② プレイリストアイテム管理 + 順次再生** ✅: `GET /playlists/{id}`(items詳細)、
  `POST/DELETE/PUT /playlists/{id}/items`(追加/削除/並び替え)、`POST /playlists/{id}/play`
  (loop/shuffle 対応の順次再生)。DB に `get_playlist_items_detailed`/`set_playlist_items` 追加。
- **A-④ フロント配線** ✅: `VideoManager`(アップロード/一覧/削除/再生)・`PlaylistManager`
  (PL作成/削除/アイテム編集/並び替え/PL再生)を実API配線。NOW STREAMING バー + 2秒ポーリング。
  旧 `mockVideos.js` 削除。
- 検証: TestClient + UDPループバックで upload→play→playback→stop、プレイリスト順次巡回
  (全動画を巡回・UDPフレーム受信)、FK安全な動画削除を確認。
- **実機検証済み (2026-06-14)**: GMKTec へデプロイ(`git pull`+`uv sync`+frontend `npm run build`+
  サーバー再起動)し、アップロード→再生→**実機LCDに fire 動画が安定表示・色も正常**を確認。
  `cv2.imencode`(BGR)で色は正しく出る(変換不要)。送信先 192.168.49.101:8889、320×160。

### A+. 再生UX改善 (2026-06-14, 実機検証済み)
- **無信号時の STANDBY ステータス画面** (`core` 88d68f8): 映像フレームが途切れると LCD を
  device 自前のライブ情報(uptime/IP/RSSI/heap/fps/IMU + ハートビート)へ自動切替。
  「最後のフレーム焼き付き」解消と「映像は無いが生きている」表示を両立。`LCDManager::drawStatus`
  (M5Canvasスプライト)、main.cpp で `frames_received` 鮮度により映像/STANDBY を切替
  (未受信は起動3秒後、途切れ1.5秒)。**OTA(espota)で書き込み済み**。
- **ループ再生ON/OFFトグル** (`server/frontend` 3d4b43f): `VideoStreamer._loop` 可変化 +
  `POST /playback/loop`、NOW STREAMING バーに 🔁(緑=ON)。OFFで再生し切ると停止→STANDBY。
- **応答性修正** (`server` c033232): 30fps動画を捌けず送出スレッドが sleepゼロで回り uvicorn の
  イベントループを GIL 飢餓させ、WebUI 操作が ~10秒無応答だった。**送出fpsを MAX_STREAM_FPS(20)で
  上限化(skipフレーム間引き・速度維持)+毎フレーム最低3ms sleep** で解消。
- **運用の罠**: GMKTec に手動起動の `stream_to_sphere.py` が残ると VideoStreamer と二重送信して
  表示が交錯する → `pkill -f stream_to_sphere.py`。deviceハング時は uptime が固定 → 電源リセット。
- **未確認/残**: 実機LED(基板未着)。WS state へ `current_playlist_id` 未連携。
  STANDBY閾値1.5s・MAX_STREAM_FPS=20 は要チューニング余地。

### C. デバイス機能のTODO (firmware)
- **LED pixel(個別制御)モード**未実装 (`CommandHandler::_handleLed` "pixels")。
- **ジェスチャー→実アクション**未実装 (`GestureManager`: 検出・publishのみ。画像切替/輝度変更が空)。
- **IMUキャリブレーション** / **config reload** コマンド未実装 (`CommandHandler::_handleSystem`)。
- **state のプレースホルダ**: 実FPS(`ledManager.getStats().fps`)/温度/NTP時刻/seq を実値に
  (`CommandHandler::getState`)。
- **5ストリップ化**: `BoardConfig.h` 4→5 (G38追加) + `LEDManager` 配列拡張。ベンチ合格済み。
- **OTAホスト名/パスワードのconfig化**: `OtaManager.cpp` のハードコードを config.json
  (`system.ota`) 参照へ。
- **IMU姿勢予測**(レイテンシ補償): 現フュージョン済みq + ジャイロで一次デッドレコニング(任意)。

### D. ジョイスティック (Phase3)
- `server/joystick/` (daemon/device_manager/mapper, ~257行) は存在するが**動作・MQTT連携・
  起動統合が未確認/未整備**。evdev でのUSB検出、PS4対応、ボタンマッピング、MQTT直publish。

### E. その他
- XIAO ボードのピン値が暫定 (`board_xiao_esp32s3.h` TODO)。
- IP体系の統一 (旧 192.168.49.x ↔ 新 192.168.100.x、§9参照)。

---

## 12. 検討メモ (将来検討用) — いつでも再検討できるよう論点を集約

実装を急がないが、設計判断の根拠ごと残しておきたい検討事項。各項目「現状 / 論点 /
選択肢と見積り / 暫定方針」の順でまとめる。

### 12.1 ジャイロ (IMU) のバックエンド検討 — BNO055 ↔ M5内蔵IMU

- **現状 (2026-06-16, ブランチ `feat/imu-m5-internal-switch` 実装済み・ビルド検証済み)**:
  `IMUManager` を **ビルド時切替**にした。
  - 既定: 外部 **BNO055**(9軸オンチップ融合 NDOF、方位は地磁気で絶対基準)。
  - 切替: `pio run -e atoms3r_m5imu` で **M5AtomS3R 内蔵 BMI270**(6軸)+ 自前
    **Madgwick 融合**(`core/src/MadgwickAHRS.h`、ヘッダオンリ)。
  - 切替方式: `-D IMU_SENSOR_M5IMU` を足すだけ。M5IMU が BNO055 より優先される設計
    (継承した `-D IMU_SENSOR_BNO055` はそのままでよい)。公開API
    (`getQuaternion/getEuler/getAccel/getGyro`)と単位(accel=m/s²/gyro=deg/s/euler=度)は
    両者で同一なので、上位の `LEDManager`/`GestureManager` は無改修。
  - 起動時に**静止ジャイロを~1秒平均してバイアス除去**(`calibrateGyroBias`)。校正は
    全 `begin()`(LCDManager の `M5.begin` 含む)完了後の**初回 `update()` で遅延実行**
    — begin 中に校正すると後続 `M5.begin` の IMU 再初期化で無効化されるため。
- **論点**: M5内蔵は **6軸=地磁気なし → ヨー(方位)が経時ドリフト**する。ロール/ピッチは
  重力で補正され安定。BNO055 は 9軸で方位が絶対。球体表示に**絶対方位が要るか**が分岐点。
  - 映像の球面マッピングは「重力基準で上下が合っていれば良い/方位は相対でも可」なら 6軸で十分。
  - 「特定方位に映像を固定したい(地面に対する絶対ヨー)」なら 9軸(BNO055)か地磁気追加が必要。
- **選択肢と見積り**:
  - (a) BNO055 維持: 方位絶対・追加実装ゼロ。外部部品/配線/I2Cが必要。
  - (b) M5内蔵6軸: 部品削減・配線レス。**ヨードリフト**。`beta`(既定0.1)調整で
    応答↔ノイズのトレードオフ。ドリフト量の実測ログ用に `_lastDriftLog` メンバを
    確保済み(未配線・プレースホルダ)。
  - (c) 将来: 内蔵IMU + 外部地磁気のみ追加して 9軸相当に拡張、も理論上は可能。
- **暫定方針**: 実機でヨードリフトの実用上の許容度を**実測してから**既定を決める。
  当面は BNO055 既定のまま、`atoms3r_m5imu` env で随時比較できる状態を維持。
  関連: §11-C の「IMU姿勢予測(レイテンシ補償)」と併せて検討。

### 12.2 LEDカラーのマルチサンプリング (六角形内の複数点平均) — リソース検討

- **現状**: 1 LED = **1点の最近傍サンプリング**。`LEDManager::updateLEDBuffer` が全800LEDを
  1パスで、LED方向→(IMU回転)→`sphereToUV`→`getPixel(px,py)` 1回。映像は 320×160 に
  ダウンスケール済み(球面LEDは疎)。§10 時点でマッピングは ~4ms まで最適化、**律速は
  デコード ~45ms** でマッピング側に余力あり。
- **論点(ユーザー質問)**: 各LEDが受け持つ六角形領域内の**複数点を平均**するとリソースを
  圧迫するか。→ **どの空間でサンプルするかで桁が変わる**のが要点。
- **コスト構造 (最重要)**:
  - `getPixel` 自体は「配列1読み+3アンパック+境界チェック」で安い (≈0.2–0.3µs/回)。
  - **高い**のは IMU補正ON時の**座標変換**(`rotateByQuaternion`+`sphereToUV` は atan2/asin、
    LEDあたり~5µs弱)。**ここを K倍にするか1回に保つかで結論が真逆になる。**
- **選択肢と見積り**:
  - (1) ★**推奨: 変換は1回・平均は画像空間** — LED方向→(IMU回転)→`sphereToUV` で中心
    (px,py)を**1回だけ**出し、その周囲K点を `getPixel` して平均→1色。**三角関数は1回のまま**、
    増えるのは `getPixel`×(K-1) の線形増だけ。IMU回転下でも対称カーネル(box/円板)なら
    フットプリントの向き回転にほぼ不変で堅牢。
    - *代償(精度)*: 正距円筒投影はスケールが緯度で激変するため、**固定カーネル≠実フットプリント**
      (極で不足/赤道で過剰)。継ぎ目(u=0↔1ラップ)と極の境界処理が必要(px ラップ/py クランプ)。
      厳密化するなら「**LEDごとの近傍オフセット/重みを LUT に前計算**」(歪み補正込み・構築時1回)。
  - (2) **球面空間で六角形内にK点を分布**させ各点を回転+UV: 最も正確だが**変換コストがK倍**
    (map ~4ms→~4ms×K、K=7 で ~28ms)。これが「律速リスク」の本来の対象。(1)で回避できる。
  - (3) **サーバ側でアンチエイリアス**: ダウンスケール時に box/mipmap で面積平均しておけば、
    端末は1点サンプルのままでも実質マルチサンプル相当。**最も安い**(端末計算ゼロ)。
- **更新レートへの影響 (案(1)採用時、§10ベンチ前提の見積り)**:
  パイプラインは2コア分離: Core0 デコード ~45ms(動画差替 ~20Hz) ‖ Core1 map(~4ms)+show(~14ms)
  =~18ms(IMU追従 ~50Hz)。平均化が乗るのは **Core1 の map だけ**。

  | K(1LEDあたり点数) | map増分 | map合計 | Core1ループ | IMU追従 |
  |------|------|------|------|------|
  | 1 (現状) | — | ~4ms | ~18ms | ~55Hz |
  | 4 (2×2) | +~0.7ms | ~4.7ms | ~19ms | ~53Hz |
  | 7 (六角=中心+6) | +~1.5ms | ~5.5ms | ~19.5ms | ~51Hz |
  | 9 (3×3) | +~2.0ms | ~6.0ms | ~20ms | ~50Hz |

  - **動画更新レート: ほぼ不変(~20Hz)** — decode律速かつ別コア、トリプルバッファで独立。
  - **IMU追従/描画: 数%低下のみ**(K=7 で ~55→~51Hz)。`show()`14msが固定で大きく増分は薄まる。
  - ※ `getPixel` コストは見積り。実装後 `[PERF]` の `map` ログ1行で実測可能。
- **暫定方針**: まず (3) サーバ側面積平均、または (1) 変換1回+画像空間K点平均で費用対効果を見る。
  (1)は更新レートをほぼ落とさず導入できる。(2) 球面厳密版は不採用寄り(変換K倍)。**測定駆動**で。
- **実装済み (2026-06-16, 案(1)採用)**: `LEDManager::sampleAveraged()` を実装。
  - **円周サンプリング**: 中心(px,py) + **半径Rの円周上N点**を画像空間で平均。座標変換
    (`sphereToUV`)は中心1点のみ。円周オフセットは `setMultisample()` で設定変更時に
    1回だけ cos/sin 前計算 → **毎フレームは整数オフセット加算のみで三角関数ゼロ**。
  - 当初は六角形(実フェイス幾何)案も検討したが、**円周サンプリングに変更**。これにより
    フェイス頂点の幾何は不要 → **レイアウトCSVに頂点座標/頂点IDを持つ必要なし**
    (現状の `FaceID,strip,strip_num,x,y,z`=中心方向のみで十分)。将来「実フェイス形状に
    忠実な異方カーネル」が要るときだけ頂点幾何が必要だが、それも Blender CAD 側で再生成可能。
  - **config駆動で可変**: `config.json` の `led.multisample.{enabled,radius_px,points}`
    (既定 ON / 2.0px / 6点 = K=7)。`ConfigManager::getLedMultisample*()`、
    LEDManager に `setMultisample()`/getter (将来 MQTT ライブ調整に流用可)。
    x=経度ラップ / y=緯度クランプで継ぎ目・極を処理。
  - **未確認**: 実機でのちらつき低減効果と `[PERF]` map 実測。半径2px は保守的初期値
    (効果弱→3px / ボケ過ぎ→縮小)。`atoms3r`/`atoms3r_m5imu` ともビルド検証済み。
