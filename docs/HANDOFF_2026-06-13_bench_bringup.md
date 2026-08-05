# 引き継ぎ (2026-06-13) — ボード抽象化 & ベンチ立ち上げ

作成環境: macOS 開発機 / 実機・サーバー: Ubuntu ビルドホスト (`<build-host>`)
親資料: [`HANDOFF.md`](../HANDOFF.md)（全体像）。本書はそこに**今回追加した内容の差分**。

このセッションで「ボード抽象化の実装」「OTG 書き込みの実証」「ESP32 専用 P2P AP の構築」「device↔broker↔server の疎通」までを完了した。

---

## 1. ボード抽象化 (M5AtomS3R / XIAO ESP32S3)

デバイス個別の物理ハード設定を `core/src/boards/` に集約し、`platformio.ini` の `-D BOARD_*` で切替える構造にした (commit `795e204`)。

```
core/src/boards/
├── board.h                ディスパッチャ (-D BOARD_* を見て下を include)
├── board_atoms3r.h        M5AtomS3R: LED=G5/6/7/8, I2C=SDA2/SCL1, ブザー=G39, LCD有
└── board_xiao_esp32s3.h   XIAO: LED/I2C/ブザーは暫定値(TODO), LCD無/ブザー無
```

- `BoardConfig.h` は `board.h` を include する薄いエントリポイントに変更（既存の `#include "BoardConfig.h"` はそのまま動く）。
- `SoundManager`=`BOARD_HAS_BUZZER`、`LCDManager`(M5Unified依存)=`BOARD_HAS_LCD` でガード。XIAO ではどちらも no-op。
- `platformio.ini`: `[env:atoms3r]` に `-D BOARD_M5ATOMS3R`、新規 `[env:xiao_esp32s3]`（`board=seeed_xiao_esp32s3`、M5Unified 除外、`upload_port` 自動検出）。
- **両 env でビルド成功確認済み**（Mac / Ubuntu）。

### 役割分担の原則
- **`config.json`** = 実行時設定（WiFi / broker / 輝度 / 機能 ON-OFF）
- **`board_*.h`** = 物理ハード（LEDピン / I2C / スピーカー）
- LED データピンは FastLED の `addLeds<>` がテンプレート引数＝**コンパイル時定数必須**なので config.json には出せない（ボードヘッダ固定）。

### 残 TODO
- `board_xiao_esp32s3.h` のピン値は**暫定**（LED D0-D3=GPIO1/2/3/4, I2C SDA=5/SCL=6）。XIAO の実配線確定後にこの1ファイルを更新する。
- XIAO 実機はまだ未接続（書き込み検証は XIAO を挿してから）。

---

## 2. 開発フロー: Mac 編集 → Ubuntu 書き込み

実機 (M5AtomS3R) は Ubuntu ビルドホストに USB 接続。コード編集は Mac、ビルド/書き込みは Ubuntu で行う。

```
Mac で編集 → git commit/push (origin/main)
  → ssh <build-host> → git pull → pio build/upload
```

### SSH / 環境 (構築済み)
- ssh エイリアス `<build-host>` = `<user>@<tailscale-ip>`(Tailscale 経由)。**SSH 公開鍵で passwordless**。sudo はパスワード要。
- Ubuntu の git `origin` は **SSH に変更済み**（HTTPS は認証情報なしで fetch 不可。GitHub SSH 鍵で通る）。
- PlatformIO: `~/.platformio/penv/bin/pio` (6.1.19)。公式インストーラで導入（前提に apt `python3-venv` `python3-pip`）。
- 作業ユーザを `dialout` に追加済み。グループ反映に再ログインが要るので、同一セッションでは `sg dialout -c '<cmd>'` を使う。デバイスは `/dev/ttyACM0`。

### 書き込みコマンド (OTG, 実証済み)
```bash
ssh <build-host>
cd ~/work/sastle-isolation-sphere/core
sg dialout -c '~/.platformio/penv/bin/pio run -e atoms3r -t upload'    # アプリ
sg dialout -c '~/.platformio/penv/bin/pio run -e atoms3r -t uploadfs'  # LittleFS (config.json/images)
```
- ESP32-S3 ネイティブ USB-OTG + `ARDUINO_USB_CDC_ON_BOOT=1` でボタン操作なしの自動リセット書き込みが通る（ハッシュ検証済み）。
- **`uploadfs` を忘れると** `config.json` 読込失敗で setup 停止する。
- AP を後から立てた場合、デバイスは WiFi 失敗で `while(1)` 停止したままなので **esptool でリセット**して setup() を再実行させる:
  ```bash
  sg dialout -c '~/.platformio/penv/bin/python \
    ~/.platformio/packages/tool-esptoolpy/esptool.py \
    --chip esp32s3 --port /dev/ttyACM0 --after hard_reset flash_id'
  ```

---

## 3. ESP32 専用 P2P AP (ベンチネットワーク)

設計上 ESP32 はサーバーの **USB WiFi ドングルが立てる AP** に接続する。今回 Ubuntu に実構築した。

- ドングル: **Realtek RTL88x2BU (AC1200, `0bda:b812`)**。in-kernel `rtw88_8822bu` が AP モード対応（DKMS 不要）。
- IF: `wlx90de8068da46`。AP IP `192.168.49.1/24`、SSID `ESP32-P2P-Direct`、WPA2、ch6、country JP。
- DHCP=dnsmasq(.50–.100、デバイスは static .101)、ブローカー=mosquitto `0.0.0.0:1883` 匿名許可。
- NetworkManager にドングルを `unmanaged` 化して干渉を防止。SSH は tailscale 経由なので AP 操作の影響を受けない。
- **全て systemd で enable 済み → 再起動後も自動復帰**。

### 再現スクリプト
構築手順は **[`server/scripts/setup_p2p_ap.sh`](../server/scripts/setup_p2p_ap.sh)** にスクリプト化済み。
```bash
sudo server/scripts/setup_p2p_ap.sh          # 構築・起動 (IF は自動検出)
sudo server/scripts/setup_p2p_ap.sh --down   # 撤去
```
SSID/PSK/IP/ch は環境変数 (`AP_SSID` 等) で上書き可。

### 既知のハマり
- ドングルが USB 列挙に失敗 (`device descriptor read, error -71`) することがある → **別の USB ポートに挿し替える**と安定した。
- `iw` は別途 apt 導入が必要。

---

## 4. 疎通確認の結果 (2026-06-13)

```
ping 192.168.49.101 → 0% loss
sphere/sphere001/status {"status":"online",...}      ← デバイス
sphere/sphere001/state  {...}                         ← デバイス状態 (retained)
sphere/sphere001/imu    {"w":..,"x":..,"y":..,"z":..}  ← IMU ストリーム
sphere/all/state        {...,"fps":60,"temp":42.0}    ← サーバー(uvicorn:9000)も同ブローカーに参加
```
device ↔ broker(mosquitto) ↔ server(FastAPI) のフルスタックが成立。

確認コマンド:
```bash
ssh <build-host>
mosquitto_sub -h 192.168.49.1 -t '#' -v        # 全トピック傍受
sg dialout -c 'timeout 12 cat /dev/ttyACM0'    # デバイスのシリアルログ
```

---

## 5. 次にやること

1. **XIAO 実機検証**: 実配線確定 → `board_xiao_esp32s3.h` 更新 → XIAO を Ubuntu に挿して `pio run -e xiao_esp32s3 -t upload`。
2. **5 ストリップ化**: `board_*.h` の `kNumStrips=5` + `LEDManager` の `addLeds` 5本目（HANDOFF.md §8 と同じ。S3 の RMT 4ch 制約に注意）。
3. **LED 実点灯確認**: `mosquitto_pub -h 192.168.49.1 -t 'sphere/all/command/led' -m '{...}'` でパターン送出 → 実ストリップ表示。
4. **IP 体系の整理**: 現状 AP は旧 `192.168.49.x`（`config.json` 準拠）。新仕様の `192.168.100.x` への統一は要検討（`config.json` 変更 + 再 uploadfs + `setup_p2p_ap.sh` の AP_IP 変更）。
