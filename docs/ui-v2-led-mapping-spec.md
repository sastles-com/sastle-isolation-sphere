# UI v2 追補仕様 — 実機 LED レイアウトの 3D 球体マッピング

親仕様: [`ui-v2-design-spec.md`](ui-v2-design-spec.md) / ブランチ: `feat/ui-v2`
作成: 2026-07-06 / 実装者 (Opus セッション) 向け指示書。この文書単体で着手できる。

---

## 0. 背景 — ストリーミング設計の現状整理 (前提知識)

**球体へ流れるデータは「圧縮画像」であり、800 点の LED RGB ではない。**

```
[Server] video_streamer.py                          [Device] ESP32 (AtomS3R)
 動画を OpenCV でデコード                              ImageManager: UDP受信 → TJpg_Decoder
 → 320x160 リサイズ → JPEG 圧縮                       → RGB565 フレームバッファ
 → UDP :8889 へ分割送信 (MAGIC "JPEG")     ────▶      LEDManager: LED毎に 3D座標→UV変換して
                                                      画像をサンプリング (円周マルチサンプル) → 800 LED
```

- 送信フォーマット: **equirectangular (正距円筒) パノラマ画像 320×160 の JPEG**
- **LED への色割当 (UV マッピング) はデバイス側** (`core/src/LEDManager.cpp` の `sphereToUV()`) で行う
- 800 点の RGB を直接送る経路は、MQTT `SET_LED {mode:"pixels", pixels:[...]}` (パターン制御用・低レート) のみ。映像ストリーミングには使わない
- **UI (ブラウザ) は映像フレームを受信していない** — UDP はデバイス宛のみ

この前提から、UI の LED マッピング表示は2段階に分ける:

| Phase | 内容 | 状態 |
|---|---|---|
| **A** | LED 物理配置 800 点を球体上に描画し、hue/brightness パラメータで着色 | **本仕様の必須スコープ** |
| **B** | サーバーがプレビューフレームを UI にも配信し、UI 側で同じ UV マッピングを再現して**実際の映像色**で 800 点を光らせる (デジタルツイン) | 任意・別タスク (§5 に設計指針のみ) |

---

## 1. データソース

`core/data/led_layouts-5strip.csv` (801 行 = ヘッダ + 800 LED):

```csv
FaceID,strip,strip_num,x,y,z
409,0,0,0.960038824,-0.273567911,0.059042825
627,0,1,0.988396116,-0.140200102,0.058455535
...
```

- `x,y,z`: **単位球面上**の LED 位置 (|v| ≈ 1.0)。そのまま Three.js 座標に使える
- `strip`: 0-4 (5 ストリップ)、`strip_num`: ストリップ内インデックス 0-159
- ファームウェアと同一のファイル (実機と同じ配置が UI に出る)

---

## 2. サーバー実装 — レイアウト配信エンドポイント

`server/app/api/endpoints/config.py` に追加する (config ルーターは `/api/config` プレフィックス済み):

```python
import csv
import os

# CONFIG_SEARCH_PATHS と同じ流儀 (サーバーは server/ から起動される前提)
LED_LAYOUT_SEARCH_PATHS = [
    "../core/data/led_layouts-5strip.csv",
    "data/led_layouts-5strip.csv",
]

@router.get("/led-layout")
async def get_led_layout():
    """LED 物理レイアウトをフラット配列で返す。

    返り値: {count, strips, positions: [x0,y0,z0, x1,y1,z1, ...], strip: [s0, s1, ...]}
    positions はそのまま Float32Array に流し込める形式。
    """
    for path in LED_LAYOUT_SEARCH_PATHS:
        if not os.path.exists(path):
            continue
        positions, strip_ids = [], []
        with open(path, newline="") as f:
            for row in csv.DictReader(f):
                positions.extend((float(row["x"]), float(row["y"]), float(row["z"])))
                strip_ids.append(int(row["strip"]))
        return {
            "count": len(strip_ids),
            "strips": (max(strip_ids) + 1) if strip_ids else 0,
            "positions": positions,
            "strip": strip_ids,
        }
    raise HTTPException(status_code=404, detail="LED layout CSV not found")
```

設計判断 (変更しないこと):

- **CSV を frontend にコピーしない** — core/data が単一ソース。基板改版でレイアウトが
  変わっても UI が自動追従する
- フラット配列 JSON (~30KB) で十分軽い。バイナリ化・キャッシュヘッダは不要
- レスポンスは初回 1 回だけ取得する想定 (UI 側でモジュールスコープにキャッシュ)

---

## 3. フロントエンド実装 — LED ポイントクラウド

対象: `server/frontend/src/components/sphere/HoloSphere.jsx` (Phase 1 で流用中のコンポーネント)

### 3.1 構造変更

現在は `<Sphere>`(wireframe) 単体に quaternion を適用している。これを **group 化**し、
ワイヤーフレーム球と LED 点群の両方が IMU で一緒に回るようにする:

```jsx
<group ref={groupRef} scale={2.2}>        {/* quaternion は group に適用 */}
    <Sphere args={[1, 32, 32]}>           {/* 既存ワイヤーフレーム: opacity を 0.6 → 0.12 に下げ、
                                              LED 点群を主役にする (骨格として薄く残す) */}
    <LedPointCloud hue={hue} brightness={brightness} />
</group>
```

### 3.2 `LedPointCloud` コンポーネント (新規)

- マウント時に `apiGet('/api/config/led-layout')` → `Float32Array(positions)` で
  `BufferGeometry` の `position` 属性を構築。**モジュールスコープで1回だけ fetch しキャッシュ**
  (シート開閉等の再マウントで再取得しない)
- 描画は `<points>` + `PointsMaterial`。800 点 = 1 ドローコールでスマホでも無負荷
- **丸い点**にするため、円形スプライトを `CanvasTexture` でコード生成して `map` に渡す
  (64×64 canvas に radial gradient の白丸。外部画像ファイルは使わない):

```jsx
const dotTexture = (() => {
    let tex = null;
    return () => {
        if (tex) return tex;
        const c = document.createElement('canvas');
        c.width = c.height = 64;
        const g = c.getContext('2d');
        const grad = g.createRadialGradient(32, 32, 0, 32, 32, 32);
        grad.addColorStop(0, 'rgba(255,255,255,1)');
        grad.addColorStop(0.4, 'rgba(255,255,255,.9)');
        grad.addColorStop(1, 'rgba(255,255,255,0)');
        g.fillStyle = grad;
        g.fillRect(0, 0, 64, 64);
        tex = new THREE.CanvasTexture(c);
        return tex;
    };
})();
```

- マテリアル設定:

```jsx
<pointsMaterial
    map={dotTexture()}
    color={sphereColor}                      /* 既存 hueToRgb(hue) を流用 */
    size={0.05}                              /* group scale 2.2 の球に対する見え幅。実機で微調整 */
    sizeAttenuation
    transparent
    depthWrite={false}                       /* 点同士の重なりで矩形が出るのを防ぐ */
    blending={THREE.AdditiveBlending}        /* LED の発光感 */
    opacity={0.35 + 0.65 * (brightness / 100)}
/>
```

- **brightness=0 でも 0.35 で薄く見える**(配置確認ができる) のは意図的仕様

### 3.3 ストリップ識別モード (デバッグ用、必須)

実機ブリングアップで「どのストリップがどの縦縞か」を確認する用途があるため、
点ごとの頂点色でストリップを塗り分けるモードを実装する:

- `LedPointCloud` に `colorMode: 'params' | 'strip'` prop
- `'strip'` 時は `color` 属性 (頂点色) を使い、strip 0-4 を
  `hsl(strip * 72, 85%, 60%)` で着色 (5 色が 72° ずつ離れ、確実に見分けられる)
- 切替 UI は CONTROL DRAWER の **ORIENTATION セクションにトグル** を置く (P4 で配線。
  P4 未実装の間は prop のデフォルト `'params'` のままでよい)

### 3.4 やらないこと

- ワイヤーフレーム球の削除 (薄く残す。完全に消すかは発注者の実機確認後)
- 点群の LOD・カリング (800 点では不要)
- OrbitControls の追加 (球体の回転は IMU のみ、現行方針を維持)

---

## 4. 受け入れ基準

1. STAGE の球体上に **800 点**が描画され、実機と同じ縦縞 5 ストリップの配置が視認できる
   (赤道付近に LED が無い千鳥配置、極に点が無い、が CSV 通り再現される)
2. IMU の quaternion で**ワイヤーフレームと点群が一体で回転**する
3. 左端エッジドラッグ (P3 実装済みなら) または `SET_PARAMS` で hue を変えると点群の色が追従する
4. brightness で点群の輝度が変わる
5. `colorMode='strip'` でストリップ 5 色の塗り分けが確認できる (トグル UI は P4 でよい)
6. スマホ実機 (iPhone Safari / Android Chrome) で 60fps を維持 (dev tools の FPS メータ目視で可)
7. `npm run build` が通り、FastAPI 配信 (`/`) で動作する
8. レイアウト CSV が無い環境ではエンドポイントが 404 を返し、**UI は点群なしで
   従来表示のまま動く** (fetch 失敗で UI を壊さない)

---

## 5. Phase B 設計指針 (今回は実装しない — 別タスク起票用)

UI で「実際に流れている映像の色」を 800 点に反映するデジタルツイン。実装する場合:

- **プレビュー配信**: `video_streamer.py` は既に 320×160 JPEG を持っている。
  同フレームを **2〜5fps に間引いて** WebSocket の新メッセージ
  `{"type":"FRAME_PREVIEW","payload":{"jpeg_b64":...}}` で配信する
  (毎フレーム送らない。UI プレビューに 30fps は不要で、WS と UI スレッドを圧迫しない)
- **UI 側サンプリング**: 受信 JPEG を `<img>` → offscreen canvas に描画し、
  LED 毎に UV サンプリング。**UV 変換式はファームウェア
  `core/src/LEDManager.cpp` の `sphereToUV()` と完全に一致させること**
  (equirect: u = atan2 系 / v = asin 系。円周マルチサンプリングまでは再現不要、中心 1 点で十分)
- 色は `BufferGeometry` の `color` 属性を毎プレビューフレーム更新 (`needsUpdate = true`)
- IMU 姿勢補正: 実機はデバイス側で回転補正した UV を使う。UI は点群自体が quaternion で
  回っているため、**画像サンプリングは無回転の素の UV で行う** (二重補正しない)

---

## 6. 実装手順 (Opus セッション向け)

```bash
git fetch && git switch feat/ui-v2
```

1. §2 のエンドポイントを `server/app/api/endpoints/config.py` に追加
2. `curl localhost:8000/api/config/led-layout | python3 -m json.tool | head` で
   count=800, strips=5 を確認
3. §3 の `LedPointCloud` を `HoloSphere.jsx` 内 (同ファイルでよい) に実装、group 化
4. 受け入れ基準 1-4, 7, 8 を確認 (Playwright + 実ブラウザ)
5. コミットは server / frontend を分けず 1 コミットでよい
   (`feat(frontend): 実機LEDレイアウト800点を3D球体にマッピング表示` 等)
