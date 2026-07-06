---
name: verify
description: server/frontend (UI v2 Sphere Player) の変更を実ブラウザで検証する手順
---

# frontend UI v2 の検証レシピ

## ビルド / 起動

```bash
cd server/frontend
npm run build                       # 警告ゼロ化は P5 の基準 (three.js のチャンクサイズ警告は既知)
npm run dev -- --port 5173 --strictPort   # バックグラウンドで起動
```

バックエンド無しで起動すると WebSocket は接続失敗して 3s ごとに再接続を試みる
(コンソールに WebSocket エラーが出るのは想定内。ステータスバーは OFFLINE 表示になる)。

## ブラウザ駆動 (Playwright)

- scratchpad に `npm install playwright && npx playwright install chromium` で用意。
- REST は `page.route('**/api/**', ...)` でモックすると再生系の配線を HTTP 境界で検証できる。
  主要モック: `GET /api/playlist/playback` (status/video_id/playlist_id/path/loop),
  `GET /api/playlist/playlists`, `GET /api/config/settings`。
- playback は 2s ポーリングなので、操作後のアサーションは 2.5s 待つ。
- モバイルは viewport 390x844。フリック/ドラッグは `page.mouse` のドラッグで代替できる
  (framer-motion の pan はマウス対応)。
- シート位置はプレースホルダ文言の getBoundingClientRect で判定できる。

## 検証すべきフロー

1. STAGE: canvas 1枚 + StatusBar + NowPlaying + TransportDock が表示される
2. 中央上フリック → LIBRARY が half (55%) にスナップ、さらに上で full、下フリックで閉じる
3. 中央下フリック / ⚙︎ クリック → CONTROL DRAWER (75%)、scrim タップで閉じる
4. 再生▶ → `POST /api/playlist/playback/start` が飛び、ボタンが ⏸ に変わる (2s 後)
5. BRT スライダーのドラッグで aria-valuenow が変わる
6. デスクトップ (1280x800): `L` / `,` / `Space` キーのフォールバック

## 落とし穴

- TransportDock は stage のパン誤爆防止に pointerdown の伝播を止めている。
  **capture で止めると子のスライダーが死ぬ** — bubble (`onPointerDown`) で止めること。
- Playwright スクリプト内でトップレベル定数を `URL` と名付けると `new URL()` が壊れる。
