# UI v2 設計仕様書 — Sphere Player

対象: `server/frontend/` の全面刷新
作成: 2026-07-06 / 発注者確認済みの方向性: **球体中心プレイヤー型 × グラスモーフィズム**
実装者向け: この文書単体で実装に着手できることを目標に書かれている。不明点は推測せず発注者に確認すること。

> **復帰ポイント**: 現行UI(サイバーネオン4タブ)は git タグ **`ui-v1-cyber-neon`** で GitHub に保存済み。
> 本作業はブランチ **`feat/ui-v2`** 上で行い、現行UIには一切手を入れずに置き換える。

---

## 0. 要約

Isolation Sphere (800 LED 球体ディスプレイ) の操作 WebUI を、**タブ切替型ダッシュボード**から
**「球体が常に主役のプレイヤー」**へ一新する。スマホ(縦持ち)がメイン、PC でも完全動作。
フリック(スワイプ)を一等市民のナビゲーションとし、すべてのジェスチャーに視認可能な
アフォーダンスと PC 向けフォールバック(クリック/キーボード)を用意する。

デザイン言語はグラスモーフィズム。**UI の環境光が球体の hue パラメータに追従する**
「ダイナミックアクセント」を本デザインの署名 (signature) とする。

---

## 1. 全体像 — メンタルモデル

「音楽プレイヤー」のメンタルモデルを借りる。Spotify の Now Playing 画面が最も近い。

```
                    ┌──────────────────────┐
                    │   CONTROL DRAWER     │  ← 下フリックで降りてくる
                    │ (システム/IMU/ログ)    │
                    └──────────▲───────────┘
                               │ 上端から下フリック
┌──────────────────────────────┴───────────────────────────────┐
│                                                              │
│                        STAGE (常駐)                           │
│    3D球体 + ステータス + Now Playing + トランスポート            │
│                                                              │
│   ← 横フリック: トラック送り/戻し →                              │
│   右端縦ドラッグ: 明るさ / 左端縦ドラッグ: 色相                    │
│                                                              │
└──────────────────────────────▲───────────────────────────────┘
                               │ 下端から上フリック
                    ┌──────────┴───────────┐
                    │   LIBRARY SHEET      │  ← 上フリックでせり上がる
                    │ (プレイリスト/動画)     │
                    └──────────────────────┘
```

- **STAGE**: 唯一の常駐画面。3D球体 (`HoloSphere`) が背景全面に描画され、IMU で回転する。
- **LIBRARY SHEET**: 下端から上フリックで出現するボトムシート (half / full の2段デテント)。
- **CONTROL DRAWER**: 上端から下フリックで出現するトップドロワー。
- タブバーは廃止。画面遷移はすべて「シートの出し入れ」で表現する。

---

## 2. 画面仕様

### 2.1 STAGE (常駐メイン画面)

```
┌─────────────────────────────┐
│ ● STREAMING      28fps 42°C │ ← ステータスバー (glass pill)
│                          ⚙︎ │    ⚙︎ = CONTROL DRAWER を開くボタン
│                             │
│         ╭─────────╮         │
│        ╱           ╲        │
│       │   3D球体    │       │ ← HoloSphere (IMU回転, 全面 canvas)
│        ╲           ╱        │    背後に hue 追従のオーロラグロー
│         ╰─────────╯         │
│                             │
│  morning_mix                │ ← Now Playing (playlist / video 名)
│  ▸ demo01.mp4    1:23/2:04  │
│                             │
│ ┌─────────────────────────┐ │
│ │  ⏮   ▶ / ⏸   ⏭    ⟳    │ │ ← トランスポートドック (glass)
│ │ ────────●──────  BRT 64%│ │    明るさミニスライダー常駐
│ └─────────────────────────┘ │
│        ── (ハンドル) ──      │ ← LIBRARY のグラブハンドル
└─────────────────────────────┘
```

構成要素:

| 要素 | 内容 | データソース |
|---|---|---|
| ステータスバー | 接続状態 ●(緑=WS+device online / 黄=WSのみ / 赤=切断)、再生状態 (STREAMING/PAUSED/IDLE)、fps、temp | WS `STATE_UPDATE` payload.system / `/api/playlist/playback` |
| 3D球体 | 既存 `HoloSphere.jsx` を流用。回転=IMU quaternion、色/輝度=params | WS `STATE_UPDATE` payload.imu / payload.params |
| オーロラグロー | 球体背後の radial-gradient。色は `--accent` (hue追従) | payload.params.hue |
| Now Playing | アクティブプレイリスト名、再生中動画名、位置/長さ | `/api/playlist/playback` (2s ポーリング、現行踏襲) |
| トランスポート | ⏮ 前 / ▶⏸ 再生・一時停止 / ⏭ 次 / ⟳ ループ + 明るさスライダー | §5 の API 対応表 |

### 2.2 LIBRARY SHEET (上フリック)

- ボトムシート。デテント: **half (55%)** / **full (画面 - safe-area)**。ドラッグで連続的に追従し、
  離した速度と位置でデテントにスナップ (spring)。
- 内部は 2 セグメント: **Playlists / Videos** (глass セグメントコントロール、横フリックでも切替可)。
- **Playlists**: カード縦リスト。カード = 名前 + 曲数 + サムネイル。
  - タップ → アクティブ化 (`PUT /api/config/settings` playback.active_playlist) + 再生開始確認
  - 再生中プレイリストには `--accent` のインジケータ
- **Videos**: 2列グリッド (現行 VideoCard 踏襲)。アップロード / 削除 / 単発再生 (`POST /api/playlist/play/{video_id}`)。
- シートを閉じる: 下フリック / ハンドルタップ / 背景 (STAGE の見えている部分) タップ。

### 2.3 CONTROL DRAWER (下フリック)

- トップドロワー、1 デテント (75%)。内部縦スクロール。セクション順:
  1. **DEVICE** — device_status、fps、temp、IP、`sphere/{id}/status`。再起動等 system コマンドは将来枠
  2. **ORIENTATION** — 軸表示トグル、姿勢オフセットジョイスティック (現行 `NeonJoystick` の glass 版)
  3. **TUNE** — speed / saturation スライダー (brightness/hue は STAGE のエッジドラッグと重複してよい)
  4. **PATTERN** — 現行 PatternControl 相当 (solid/off 等の LED モード)
  5. **CONFIG** — 現行 ConfigEditor 相当 (フォーム。第1フェーズでは既存実装の移植でよい)
  6. **LOGS** — タップでフルスクリーンのログビューアへ (等幅フォント、自動追従スクロール、一時停止)

### 2.4 エッジドラッグ (STAGE 上のパラメータ直接操作)

動画プレイヤーの慣習 (右端=明るさ) を借用する:

| ジェスチャー | パラメータ | フィードバック |
|---|---|---|
| **右端 20% を縦ドラッグ** | brightness (0-100) | 画面右に縦ゲージ + 数値が出現、離すと1s後フェード |
| **左端 20% を縦ドラッグ** | hue (0-360) | 画面左に hue グラデーションゲージ + 球体と UI アクセントが即応 |

- ドラッグ中は 60ms デバウンスで `SET_PARAMS` を送信 (現行 SphereControl の 100ms 踏襲、体感優先で 60ms)。
- エッジドラッグ開始領域はシートのフリックと競合しない (シート開閉は中央 60% で判定)。

### 2.5 横フリック (STAGE) — トラック送り

- STAGE 中央域の横フリック = **次/前の動画へスキップ**。
- 球体がフリック方向へ慣性で一瞬流れて戻るマイクロモーション (物理的な「送った」感)。
- API: 次 = `POST /api/playlist/playback/start` の次項目送り。**サーバーに skip API が無い場合は
  第1フェーズでは「プレイリスト内の次の video_id を `/play/{video_id}` で直接再生」で代替**
  (プレイリスト items は `/api/playlist/playlists/{id}` で取得可能)。skip API の追加はサーバー側タスクとして起票。

### 2.6 PC (マウス/キーボード) フォールバック

すべてのジェスチャーに非ジェスチャー手段を必ず併設する:

| 操作 | タッチ | PC |
|---|---|---|
| LIBRARY 開閉 | 上フリック | ハンドルクリック / `L` キー |
| CONTROL 開閉 | 下フリック | ⚙︎ クリック / `,` キー |
| 再生/一時停止 | ▶タップ | クリック / `Space` |
| トラック送り | 横フリック | ⏮⏭ クリック / `←` `→` |
| 明るさ | 右端ドラッグ | スライダー / `↑` `↓` |
| シート内ドラッグ | ドラッグ | マウスドラッグも有効 (framer-motion の drag はマウス対応) |

- ホバー可能なデバイスでは操作要素に hover 状態を定義 (`@media (hover: hover)`)。
- レイアウトは 720px 以上で中央 640px カラムに制約 (スマホ版をそのまま中央に置く。
  2カラム化などの PC 専用レイアウトは行わない — 操作モデルの一貫性を優先)。

---

## 3. ビジュアルデザイン仕様

### 3.1 デザイントークン (CSS custom properties)

```css
:root {
  /* ground */
  --ground: #05080f;            /* 深宇宙。純黒でなく青バイアス */
  --ground-2: #0a1020;          /* シート内のわずかな段差 */

  /* dynamic accent — 球体の hue パラメータに JS で追従させる */
  --sphere-hue: 190;                                   /* STATE_UPDATE params.hue で更新 */
  --accent: hsl(var(--sphere-hue) 85% 62%);
  --accent-soft: hsl(var(--sphere-hue) 70% 55% / .25);
  --aurora: radial-gradient(60% 45% at 50% 42%,
              hsl(var(--sphere-hue) 80% 50% / .22), transparent 70%);

  /* glass */
  --glass-bg: rgb(255 255 255 / .07);
  --glass-stroke: rgb(255 255 255 / .14);
  --glass-blur: blur(24px) saturate(1.5);
  --radius: 20px;
  --radius-pill: 999px;

  /* text tiers */
  --tx-1: rgb(255 255 255 / .95);
  --tx-2: rgb(255 255 255 / .68);
  --tx-3: rgb(255 255 255 / .42);

  /* semantic (アクセントとは独立) */
  --ok: #34d399;  --warn: #fbbf24;  --err: #f87171;
}
```

- glass 面は必ず `background: var(--glass-bg); backdrop-filter: var(--glass-blur);
  border: 1px solid var(--glass-stroke);` の3点セット。**グローやドロップシャドウの多用は禁止**。
  発光してよいのは球体・オーロラ・アクセントインジケータのみ。
- 本アプリは「LED 球体を暗所で操作する」ユースケースが主のため **ダークテーマ単一で確定**
  (ライトテーマは作らない。これは省略ではなく選択)。

### 3.2 タイポグラフィ

| 役割 | フェイス | 用法 |
|---|---|---|
| UI 全般 | **Manrope Variable** (`@fontsource-variable/manrope`) | 400/500/700。見出しは 700 + `letter-spacing: -0.01em` |
| 数値・ログ | **JetBrains Mono** (`@fontsource/jetbrains-mono`) | fps/温度/時刻/ログ。`font-variant-numeric: tabular-nums` |
| ラベル | Manrope 500 uppercase | 11px, `letter-spacing: .08em`, `--tx-3` |

型スケール: `12 / 14 / 16 / 20 / 28px`。Now Playing 曲名 = 20px、プレイリスト名 = 16px。

> ⚠️ **フォントは必ず npm パッケージで同梱** (`@fontsource-*`)。本サーバーは
> ESP32 の P2P AP などオフライン LAN で配信されるため、**外部 CDN (Google Fonts 等) への参照は一切禁止**。
> これは絵文字アイコン・アイコン CDN・解析スクリプトにも適用する。

### 3.3 モーション

- ライブラリ: **framer-motion**。シートは `drag="y"` + spring (`stiffness: 400, damping: 40`)。
- シート出現はオーバーシュートなしの spring。マイクロモーション (ボタン押下 scale .96、
  トラック送りの球体慣性) は 150-250ms。
- `prefers-reduced-motion: reduce` ではシートをフェード切替に落とし、球体の自動回転演出を止める。

---

## 4. 技術設計

### 4.1 スタック変更

| 項目 | 現行 | v2 |
|---|---|---|
| ナビゲーション | MUI Tabs + BottomNavigation + react-swipeable | **自作 Stage/Sheet + framer-motion drag** |
| UI コンポーネント | MUI (Box/Chip/Slider...) | **自作 glass コンポーネント** (§4.3)。MUI は段階的に全廃 |
| ジェスチャー | react-swipeable | **framer-motion** (drag / pan / スナップ全部これに統一。react-swipeable は削除) |
| 3D | react-three-fiber + HoloSphere | **そのまま流用** |
| 状態/通信 | WebSocketContext + useStateUpdate + lib/api | **そのまま流用** (§5 の契約は不変) |
| ルーティング | react-router (実質未使用) | **削除** (単一画面のため) |
| フォント | CDN 依存なし(system) | @fontsource-variable/manrope + @fontsource/jetbrains-mono |

追加依存: `framer-motion`, `@fontsource-variable/manrope`, `@fontsource/jetbrains-mono`
削除依存 (最終フェーズ): `@mui/material`, `@mui/icons-material`, `@emotion/*`, `react-swipeable`, `react-router-dom`, `react-knob-headless`

アイコン: MUI icons の代わりに **インライン SVG を自作** (`components/ui/icons.jsx` に集約、
stroke 1.5px の線画で統一)。必要数は 15 個程度 (play/pause/stop/skip/loop/gear/chevron/upload/trash/...)。

### 4.2 ディレクトリ構成 (v2)

```
server/frontend/src/
├── App.jsx                     # WebSocketProvider + <SpherePlayer/> のみ
├── theme/tokens.css            # §3.1 のトークン (単一ソース)
├── pages/SpherePlayer.jsx      # STAGE + 2シートの組み立て、ジェスチャー配線
├── components/
│   ├── stage/
│   │   ├── SphereStage.jsx     # HoloSphere + オーロラ + エッジドラッグ
│   │   ├── StatusBar.jsx       # 接続/再生状態/fps/temp
│   │   ├── NowPlaying.jsx
│   │   ├── TransportDock.jsx
│   │   └── EdgeParamGauge.jsx  # 右端/左端ドラッグのゲージ表示
│   ├── sheets/
│   │   ├── GlassSheet.jsx      # 汎用シート (bottom/top, デテント, drag)
│   │   ├── LibrarySheet.jsx    # Playlists/Videos セグメント
│   │   └── ControlDrawer.jsx   # DEVICE/ORIENTATION/TUNE/PATTERN/CONFIG/LOGS
│   ├── library/                # PlaylistList / VideoGrid / VideoCard (現行から移植)
│   ├── control/                # OrientationPad / PatternPanel / ConfigForm / LogViewer
│   └── ui/                     # GlassButton / GlassSlider / SegmentControl / icons.jsx
├── contexts/WebSocketContext.jsx   # 流用 (変更しない)
├── hooks/
│   ├── useSphereState.js       # 流用
│   ├── usePlayback.js          # playback ポーリング+操作を集約 (現行 SphereDashboard から抽出)
│   └── useDynamicAccent.js     # params.hue → --sphere-hue を document に反映
├── lib/{api.js, format.js}     # 流用
└── components/sphere/HoloSphere.jsx  # 流用 (props 互換を維持)
```

### 4.3 自作 UI コンポーネント最小セット

| コンポーネント | 要件 |
|---|---|
| `GlassSheet` | direction (bottom/top)、detents 配列、drag 追従、スナップ、背景 scrim(タップで閉)、safe-area 対応 |
| `GlassSlider` | 44px タッチターゲット、ドラッグ中の値ツールチップ、`role="slider"` + キーボード操作 |
| `GlassButton` | pill 型 / icon 型。押下 scale。`:focus-visible` リング必須 |
| `SegmentControl` | 2-3 セグメント、アクセント色のスライドインジケータ |
| `EdgeParamGauge` | 縦ゲージ + 現在値。表示/フェードは opacity のみで実装 (layout thrash 回避) |

### 4.4 モバイル対応の必須実装

```html
<meta name="viewport" content="width=device-width, initial-scale=1,
      viewport-fit=cover, user-scalable=no">
<meta name="theme-color" content="#05080f">
```

- 高さは `100dvh`。下部ドックは `padding-bottom: env(safe-area-inset-bottom)`。
- `html, body { overscroll-behavior: none; }` (プル更新・戻るジェスチャー暴発防止、現行踏襲)。
- シート内スクロールとシートドラッグの競合: シートが full かつ内部スクロールが天面にある時のみ
  下ドラッグをシートに渡す (framer-motion の `dragListener` 制御 or touch-action 切替)。
- 3D 描画: `HoloSphere` の DPR 上限を 2 に制限、`frameloop="demand"` 化は任意 (IMU 更新駆動)。

---

## 5. サーバー通信契約 (変更不可 — UI はこの契約に合わせる)

### WebSocket `/ws`

| 方向 | type | payload | UI での用途 |
|---|---|---|---|
| ← 受信 | `STATE_UPDATE` | 全状態 `{imu, playback, params, led, system, seq, timestamp}` | 球体回転 / ステータス / スライダー同期 / `--sphere-hue` |
| ← 受信 | `LOG_LINE` | `{line}` | LogViewer (Context 側で 500 行リングバッファ済) |
| → 送信 | `SET_PARAMS` | `{brightness?, speed?, hue?, saturation?}` (0-100, hue 0-360) | エッジドラッグ / TUNE スライダー |
| → 送信 | `SET_PLAYBACK` | `{action: "play"\|"pause"\|"stop"\|"toggle"}` | トランスポート |
| → 送信 | `SET_LED` | `{mode: "sphere"\|"pixels"\|"off", pixels?}` | PATTERN パネル |

### REST (`lib/api.js` 経由)

| エンドポイント | 用途 |
|---|---|
| `GET/POST/DELETE /api/playlist/videos[/{id}]` | 動画一覧 / アップロード / 削除 |
| `GET/POST/DELETE /api/playlist/playlists[/{id}]` | プレイリスト CRUD |
| `POST/DELETE/PUT /api/playlist/playlists/{id}/items[/{item_id}]` | 項目編集・並べ替え |
| `POST /api/playlist/play/{video_id}` | 単発再生 (横フリックの代替実装にも使用) |
| `POST /api/playlist/playback/{start\|pause\|stop\|loop}` / `GET /api/playlist/playback` | 再生制御 / 実状態 (2s ポーリング) |
| `GET/PUT /api/config/settings` | active_playlist / loop の永続化 |
| `GET/POST /api/config/` | CONFIG フォーム |

注意 (現行実装からの知見):
- brightness 等のスライダーは **送信 60ms デバウンス** + `STATE_UPDATE` エコーバックでの
  自己上書き抑制 (ドラッグ中は受信値を無視) を必ず入れる。
- playback の真値は MQTT 側 (`GET /playback`) であり `STATE_UPDATE` と二重ソース。
  v2 では `usePlayback` フックに集約し、コンポーネントから直接 fetch しない。

---

## 6. 実装フェーズと受け入れ基準

ブランチ: `feat/ui-v2`。フェーズごとにコミットし、動作確認は `npm run dev` (Vite) +
実サーバー or `verify_server_comm.sh` の環境で行う。

| Phase | 内容 | 受け入れ基準 |
|---|---|---|
| **P1 シェル** | tokens.css / GlassSheet / Stage 骨格 / トランスポート / 2シートのジェスチャー開閉 (中身はプレースホルダ可) | スマホ実機で: 上下フリックでシートが物理的に追従・スナップし、STAGE の球体が常に見えている。再生/停止が効く |
| **P2 ライブラリ** | LibrarySheet 実装 (Playlists/Videos)、Now Playing 配線 | プレイリスト選択→再生→Now Playing 反映の一連がスマホで完結 |
| **P3 パラメータ** | エッジドラッグ + EdgeParamGauge + useDynamicAccent | 右端ドラッグで実機 LED の明るさが変わり、左端ドラッグで hue と UI アクセント色が同時に変わる |
| **P4 コントロール** | ControlDrawer 全セクション (ORIENTATION/PATTERN/CONFIG/LOGS 移植) | 現行 CONTROL タブの全機能が新 UI から実行できる |
| **P5 仕上げ** | PC キーボード / hover / reduced-motion / MUI・react-swipeable・router 削除 / 旧コンポーネント削除 / `npm run build` → dist 更新 | `package.json` から §4.1 の削除依存が消え、build が警告なしで通り、FastAPI 配信 (`/`) で全機能動作 |

横断の受け入れ基準:
- iPhone Safari / Android Chrome / PC Chrome で動作 (縦持ち優先)。
- オフライン LAN (外部 CDN 到達不可) で全アセットが配信される。
- Lighthouse mobile で CLS ≈ 0、操作可能まで < 3s (LAN)。
- 旧 UI への復帰は `git checkout ui-v1-cyber-neon -- server/frontend` で可能なこと (frontend 外を触らない)。

---

## 7. やらないこと (スコープ外)

- サーバー API / MQTT 契約の変更 (skip API 追加は別タスクとして起票のみ)
- ライトテーマ、i18n、PWA manifest/Service Worker (将来候補)
- ジョイスティック物理コントローラ UI、LED ピクセル単位エディタの新規開発 (現行相当の移植まで)
