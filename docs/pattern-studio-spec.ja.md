> [English](pattern-studio-spec.md) · **日本語**

# Pattern Studio 仕様 & 実装計画

> 状態: **草案 / 協議用**。最終更新: 2026-07-08
> 関連: [playlist_system_design.md](./playlist_system_design.md) /
> [ui-v2-design-spec.md](./ui-v2-design-spec.md) / VideoStreamer(320x160 equirectangular JPEG/UDP)

---

## 0. 背景と決定事項

- スフィアの表示コンテンツは **equirectangular 320×160・2:1・JPEG・≤20fps**。
  サーバ(`VideoStreamer`)が OpenCV でデコード→`cv2.resize(320,160)`→JPEG→UDP送出。
  **投影変換はせず、アップロード動画そのものが equirectangular マップ**。コアは表示専用。
- パターン(手書き文字＋エフェクト等)の**最終成果物は「普通の動画」**であり、
  既存の video/playlist パイプラインにそのまま乗せられる。
- よって編集は操作アプリに詰め込まず、**専用 Web「Pattern Studio」に分離**する。
  操作アプリには **「Pattern」タブ = 素材一覧 ＋ Studio 起動リンク（ランチャー）** を追加。
- パターン＝**video の一種**（`kind` で区別）。プレイリストの動画間への挿入は既存機能のまま可能。
- **コア/ファーム変更は一切なし**。DB は最小追加のみ。

---

## 1. スコープ

### In scope（初期）
- 手書き文字/テキスト・基本図形・色・背景。
- エフェクト: 球面スピン(経度スクロール)/面内回転/拡大パルス/フェード。
- 尺・fps・ループ回数の設定。**ループ不変条件の強制**(§2)。
- equirectangular 2:1 の WYSIWYG プレビュー(ループ再生でシーム確認)＋任意で球面プレビュー。
- クライアント描画 → 動画書き出し → 既存 `POST /api/playlist/videos` へアップロード。

### Out of scope（初期。将来オプション）
- サーバ側ライブ生成(パラメータをリアルタイム描画)。P5 で昇格候補。
- コア/ファーム変更。
- フル機能タイムライン/キーフレームエディタ(初期は数値＋イージング程度)。

---

## 2. 出力仕様（デバイス整合＝不変条件）

- **アスペクト 2:1（equirectangular）必須**。作業解像度は 640×320 等の2倍でも可
  (サーバが 320×160 に resize する)。
- **fps ≤ 20**（推奨 15–20。デバイスのデコード ~20fps 上限）。
- **コーデック**: 既存動画は mp4(H.264)。デバイス側は `cv2.VideoCapture` でデコードするため、
  **mp4/H.264 を第一とする**(§10 の webm リスク参照)。
- **ループ不変条件（loopy パノラマ＋ループ再生）**:
  1. **経度ラップ**: 右端(+180°)からはみ出た分は左端(−180°)へ回り込む。
     全幅レイヤの水平ロール、またはシームまたぎ時は水平タイル合成(x, x−W, x+W)で担保。
  2. **時間ループ**: 先頭↔末尾を連続に。**総移動＝幅Wの整数倍**、
     **全可変パラメータは尺内で整数周期**(面内回転=360°整数回転、拡大=sinパルス、スピン=整数周回)。
  3. **末尾フレーム(t=T)を重複させない**(t=T≡t=0)。N枚は位相 0..(N−1)/N を出力。
- 文字は **赤道帯**に置く(equirectangular の面内回転は極付近で歪むため)。

---

## 3. UX / 編集機能

- **手書きキャンバス**(タッチ/マウス、透過)。**テキスト入力**(フォント選択)。基本図形。
- レイヤ(初期は1〜数枚)、色/背景/不透明度。
- **エフェクト**: 球面スピン(経度スクロール)/面内回転(360°整数)/拡大パルス(sin)/フェード。
- 尺・fps・ループ周回数(整数)・速度。
- **プレビュー**: 2:1 キャンバス(ループ再生でシーム確認)＋任意で球面プレビュー
  (既存 `HoloSphere`/three.js を流用可)。
- **保存**: 「Sphere へ保存」= 動画書き出し→アップロード。タイトル指定。

---

## 4. 統合契約（サーバ連携＝最小）

- **フォーマット取得**: `GET /api/config/settings`(image width/height/fps 上限)。
  無ければ軽量 `GET /api/config/image` を追加。
- **アップロード**: 既存 `POST /api/playlist/videos`(multipart: file + title)。
  **`kind=pattern` を渡す**ため Form に `kind` を追加(既定 'video')。
- 結果は `videos` に登録され、Pattern タブに一覧・playlist に挿入可能。

---

## 5. 操作アプリ側の変更（Pattern タブ = ランチャー）

- `LibrarySheet` の `SEGMENTS` に `patterns` 追加 → **Playlists / Videos / Patterns** の3セグメント。
- Patterns セグメント: `kind==='pattern'` の素材を `VideoGrid` 同様に一覧
  (`PatternGrid`/`PatternCard` は `VideoCard` を流用・軽微改変)。
  - 各カードに「**Studio で編集**」リンク(`/studio?video={id}`)。
  - 「**新規作成**」ボタン(`/studio`)。
- 素材は video なので、Videos セグメントにも出す/出さないはフィルタ選択(既定: 分離表示)。
- **用語衝突注意**: 既存 `control/PatternPanel.jsx`(LED表示モード SPHERE/OFF)は別物。
  混乱回避のため LED 側を「Display / LED Mode」へ改称検討(任意)。

---

## 6. データモデル変更（最小）

- `videos` に **`kind TEXT DEFAULT 'video'`** 追加('video' | 'pattern')。
  - マイグレーション: `ALTER TABLE videos ADD COLUMN kind TEXT DEFAULT 'video'`(既存行は 'video')。
- `create_video(..., kind='video')` 追加。`get_videos(kind=None)` に絞り込み。
- `GET /api/playlist/videos?kind=pattern` でフィルタ。`upload_video` が Form `kind` を受ける。

---

## 7. サムネイル修正（別要望・同時対応）

**原因**: `upload_video` がサムネを生成しておらず `thumbnail_path` が常に Null。
加えてメディアの HTTP 配信経路が無い(`/assets` のみ mount)。`VideoCard` は
`video.thumbnail_path` を `<img src>` に使うため、常にプレースホルダ表示になる。

**対応**:
1. `upload_video` で OpenCV により代表フレーム(先頭〜10%位置の非黒フレーム)を抽出 →
   `data/thumbnails/{uuid}.jpg` 保存 → `thumbnail_path` 格納。1:1 クロップ(カードは 1:1)。
2. 配信: `GET /api/playlist/videos/{id}/thumbnail`(FileResponse) 追加。
   API 応答に **`thumbnail_url`**(= 上記URL) を付与。
3. `VideoCard`: `src = video.thumbnail_url || video.thumbnail_path`(後方互換)。
4. 既存動画のバックフィル: 初回リクエスト時の遅延生成、または一括スクリプト。
- パターン動画にも同じサムネが付く(Pattern カードでも表示)。

---

## 8. 技術選定 / ホスティング

- **Studio**: 別 Vite + React アプリ(three.js 既存流用可)。描画 Canvas2D/WebGL。
  書き出しは **mp4(H.264)**。手段: `ffmpeg.wasm`(確実) を第一、`MediaRecorder`(webm) は §10 検証後。
- **配信案**:
  - **A) 同一 FastAPI の別ルート `/studio`**(`frontend-studio/dist` を StaticFiles で mount)。
    → 同一オリジンで**アップロードが CORS フリー**、配布一括。**推奨**。
  - B) 完全独立の静的アプリ(別ポート/ホスト)。開発は速いが CORS 対応要。
- **注意**: 現在 catch-all `@app.get("/{full_path:path}")` が全て拾うため、
  `/studio` の mount は **catch-all より前**に置く(既存 `/assets` mount と同様)。

---

## 9. 実装計画（フェーズ）

| P | 内容 | 依存 | 規模 |
|---|------|------|------|
| **P0** | **サムネイル修正**(upload生成＋配信＋VideoCard) | なし | 小・即効 |
| P1 | データモデル: `videos.kind` 追加＋API フィルタ＋upload の kind | P0 | 小 |
| P2 | 操作アプリ Pattern タブ(一覧＋Studio リンク・新規/編集) | P1 | 中 |
| P3 | Pattern Studio 最小版: 手書き＋テキスト＋球面スピン＋ループ不変条件＋mp4書出＋アップロード。`/studio` 同一オリジン配信 | P1 | 中〜大 |
| P4 | Studio 拡充: 面内回転/拡大パルス/フェード・色/背景・球面プレビュー・複数レイヤ・尺/fps/周回数 | P3 | 大 |
| P5 | (将来)サーバ側ライブ生成へ昇格(同一描画ロジック再利用・コア無改修) | P3 | 大 |

- **P0 は独立**して先行実装可(サムネ表示は今すぐ効く価値)。
- Studio(P3) は操作アプリと疎結合(接点はアップロード契約のみ)なので並行開発可。

---

## 10. リスク・要検証

1. **コーデック互換(最重要)**: デバイス配信は `cv2.VideoCapture` で開く。
   **webm(VP8/9) が OpenCV ビルドでデコードできるか未確認**。ALLOWED_EXT に webm はあるが、
   実際に再生できるかは要検証。**不可なら Studio は mp4(H.264/ffmpeg.wasm) を出力**、
   または upload 時にサーバ側で正規化トランスコード(汎用アップロードにも有効・スコープ増)。
2. **equirectangular 面内回転の極歪み** → 文字は赤道帯運用をUIでガイド。
3. **320×160 / ≤20fps の見え方** は実機確認(細い文字・高速回転の破綻)。
4. **用語衝突**: 既存 `PatternPanel`(LEDモード) と Pattern タブ/Studio。改称で整理(任意)。
5. **ループ端の連続性**: 総移動=W整数倍・整数周期・末尾非重複を Studio 側で強制(自動スナップ)。

---

## 11. 未解決の論点（次回協議）

- Studio の書き出し方式: `ffmpeg.wasm`(mp4確実・重い) か `MediaRecorder`(webm軽い・要検証)。
- Studio の配信: 同一オリジン `/studio`(推奨) か 独立アプリ。
- `videos.kind` 分離表示か、Videos に統合表示＋バッジか。
- サムネ配信: 専用エンドポイント か `data/` 静的 mount。
- 既存 LED `PatternPanel` の改称可否。
