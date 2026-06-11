# 開発引き継ぎ資料 — Isolation Sphere

最終更新: 2026-06-11 / 作成環境: macOS (開発機) → 引き継ぎ先: サーバー稼働 PC (Ubuntu)

このドキュメントは、**サーバーを稼働させている PC 上で開発を継続する**ための引き継ぎ資料。
プロジェクト全体像・現状・環境構築・検証手順・次の作業をまとめる。
詳細は各 `README.md` / `core/doc/` / `docs/` を参照。

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
| サーバー→デバイス | `sphere/all/command/{playback,params,led,system}` | コマンド (例 `{"brightness":85}`) |
| サーバー→デバイス | `sphere/all/state` | 全状態 (retained, ブロードキャスト) |

REST API (FastAPI): `/api/command/{playback,params,led}`, `/api/config`, `/api/playlist`, `/health`。
WebSocket `/ws` は接続時に現在状態を送り、以後 `STATE_UPDATE` をプッシュ。

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

1. **LED 駆動ベンチ**: AtomS3R + WS2812 ストリップで `led_drive_test` を実走。
   特に ESP32-S3 の RMT TX が 4ch のため、5本目のストリップでの `show()` 時間と
   FPS を実測 (FPS_BENCH モード)。遅ければ FastLED の S3 用 I2S 並列ドライバへ。
2. **本体ファーム 5 ストリップ化**: ベンチ結果を受けて `BoardConfig.h` を
   4→5 ストリップ (G38 追加) に拡張、`LEDManager` の配列 (`_stripPins[4]` 等) を 5 に。
3. **実機通信結合**: `core/data/config.json` の `wifi.broker` を実ブローカー IP に設定、
   ファーム書き込み、`mosquitto_sub -t 'sphere/#' -v` で実機の配信/受信を観測。
4. **基板到着後**: core-M5atom-FPC 実機で BNO055(G2/G1) + スピーカー(GPIO08) 込み統合確認。

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
- **デバイス ID `sphere001` はハードコード**: ファームとサーバー双方に直書き。
  変更時は両側同時に。
