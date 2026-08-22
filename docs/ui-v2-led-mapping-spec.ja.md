> [English](ui-v2-led-mapping-spec.md) · **日本語**

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
| **A** | LED 物理配置 800 点を球体上に描画し、hue/brightness パラメータで着色 | **実装済み** (HoloSphere.jsx の LedPointCloud) |
| **B** | サーバーがプレビューフレームを UI にも配信し、UI 側で **IMU quaternion を使ってファームと同一の回転+UV変換** を再現、**実際の映像色**で 800 点を光らせる (デジタルツイン) | **実装対象** (§5 が本仕様) |

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

## 4. 受け入れ基準 (Phase A — 実装済み、リグレッション基準として維持)

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

## 5. Phase B 実装仕様 — IMU を使った 800 点 RGB 算出 (デジタルツイン)

**目的**: 再生中の映像と IMU quaternion から、**実機の各 LED が今まさに表示している色**を
UI 側で再計算し、800 点に反映する。実機ファームの描画パス
(`LEDManager.cpp` renderFrame: 回転 → UV 変換 → 画像サンプリング) の忠実な JS 移植。

### 5.1 サーバー: FRAME_PREVIEW 配信 (WS 新メッセージ)

`video_streamer.py` はデコード済み 320×160 JPEG を毎フレーム持っている。これを間引いて
WebSocket observer へ配信する:

- メッセージ: `{"type": "FRAME_PREVIEW", "payload": {"jpeg_b64": "<base64>", "w": 320, "h": 160, "seq": n}}`
- **配信レート: 5fps** (200ms スロットル、定数化しておく)。停止中は送らない
- 実装位置: `video_streamer` の送信ループ内でスロットル判定 → `state_manager` に
  ブロードキャスト用メソッド (例 `notify_frame(jpeg_bytes)`) を追加して呼ぶ。
  streamer は別スレッドなので **mqtt_service と同じ `run_coroutine_threadsafe` パターン**で
  イベントループに投入する (`_submit_coroutine` を参考)
- `_state` には**入れない** (retained 不要、STATE_UPDATE と混ぜない — LOG_LINE と同じ流儀)
- 帯域: ~10KB × 5fps = 50KB/s/クライアント。LAN 前提で問題なし

### 5.2 UI: ファームと同一の変換パイプライン (逐語移植)

新規フック `useLedLiveColors(layout)` を作り、`LedPointCloud` の `colorMode='live'` で使う。

**入力 (すべて既存チャネルで揃う):**

| データ | 供給元 | レート |
|---|---|---|
| LED 座標 800 点 | `/api/config/led-layout` (Phase A 実装済み) | 初回1回 |
| IMU quaternion | WS `STATE_UPDATE` payload.imu | **10Hz** (ファーム `IMU_PUBLISH_INTERVAL`) |
| 映像フレーム | WS `FRAME_PREVIEW` (§5.1) | 5fps |

**フレーム受信処理**: `jpeg_b64` → `Image` → offscreen canvas に drawImage →
`getImageData()` で `Uint8ClampedArray` を保持 (最新1枚のみ)。

**色計算 — 参照元はファームウェアの実装 (これが正、本仕様の文章より優先):**

「320×160 画像 → 800 LED の RGB」の変換はファームウェアに完全な実装が存在する。
**下表の関数を JS へ逐語移植する**こと。「数学的に正しい形」への修正・最適化を禁止する
(実機と同じ絵が出ることが唯一の正解基準。コード中のコメントは一部不正確なので挙動を正とする):

| ファームウェア参照 (core/src/) | 役割 | 移植時の注意 |
|---|---|---|
| `LEDManager.cpp` **`updateLEDBuffer()`** (~L422) | **メインループ**。全体の処理順序はこの関数に従う | 冒頭で quaternion を **共役化 (`qx=-qx; qy=-qy; qz=-qz`) してから**回転に使う。UI も同じにすること (見落としやすい) |
| `LEDManager.cpp` `rotateByQuaternion()` (L274) | LED body座標の回転: `v' = v + 2*cross(q.xyz, cross(q.xyz,v) + q.w*v)` | そのまま移植 |
| `LEDManager.cpp` `sphereToUV()` (L242) | 回転後座標 → (u,v) | 内部の `_sqrt`/`_atan2` は `FastMath.h` 参照。`_atan2` は**度/180 (-1..1) を返す多項式近似**。JS では `Math.atan2(y,x)/Math.PI` で等価 (誤差<0.2°、量子化後ほぼ同一) |
| `updateLEDBuffer()` 内の量子化 (L452) | `px = trunc(u*(W-1))`, `py = trunc(v*(H-1))` | W,H は**デバイスのデコード解像度** (下記⚠️) |
| `LEDManager.cpp` **`sampleAveraged()`** (L495) + `setMultisample()` (L476) | **中心 + 半径2.0pxの円周6点 (K=7) の平均**。オフセットは前計算。x はラップ (`sx %= w`)、y はクランプ | config `led.multisample` (既定 ON/2.0px/6点) と同じ挙動で移植する。オフセットも同式で前計算 |
| `ImageManager.cpp` `getPixel()` (L255) | フレームバッファから RGB 取得。**RGB565 → RGB888** (`r=5bit<<3, g=6bit<<2, b=5bit<<3`) | UI は fullcolor JPEG から取るため、忠実度を上げるなら取得後に RGB565 量子化 (`r&0xF8, g&0xFC, b&0xF8`) を掛ける (任意だが推奨) |

処理順序 (updateLEDBuffer と同一):
`quaternion共役化 → LED毎に[回転 → sphereToUV → px,py量子化 → sampleAveraged(K7)] → brightness乗算 → color属性へ`

> ⚠️ **デコード解像度**: デバイスは JPEG を **320×160 のフル解像度でデコード**する。
> [`ImageManager.cpp:53`](../core/src/ImageManager.cpp#L53) が `_jpegScale = 1` を強制するため、
> `_width/_height` は config の元サイズと等しい。px/py の量子化も **W=320, H=160** で行われる。
> UI の offscreen canvas も 320×160 にして同じ W,H で量子化すること。
> (旧版の本書は「`_jpegScale = 2` で 160×80 に縮小デコード」と記載していたが、これは古い情報)

> ✅ **UV規約 (2026-08-23 是正)**: ファーム `sphereToUV()` は**極軸=Z の標準正距円筒**を実装しており、
> ツインと完全に同一:
> `u = (_atan2(nx, ny) + 1)/2` (経度 −180..+180° → 画像幅**全域**、継ぎ目で wrap)、
> `v = _atan2(√(nx²+ny²), nz)` (極角 0°=+Z=北極=画像上端 .. 180°=−Z=南極=画像下端、clamp)。
> `v` 側は第1引数が ≥0 なので `_atan2 ∈ [0,1]` に収まる — **`(+1)/2` は付けない**。
> 以前は 2軸が転置 (`u`=緯度 / `v`=経度) しており、`u` が構造的に `[0.5, 1.0]` に閉じ込められて
> **画像幅の右半分しかサンプリングしない**状態だった (映像が崩れる原因)。
> 現在はファームとツインの双方が上記の式を使う。片方だけを変えないこと。

**回転の整合 (二重補正の整理)**: ファームは「LED body 座標を (共役) quaternion で回してから
サンプリング」する = LED には世界固定の映像が映る。UI では点群グループ自体も quaternion で
回転しているが、**サンプリングもファームと同一に『共役 quaternion で回転後の座標』で行う**。
結果として UI の球体は「その姿勢の実機を外から見た絵」になる。これが正しいツイン。
グループ回転を止めたり、無回転 UV でサンプリングしたりしないこと。

**スコープ外 (移植しない)**: `overlayAxisIndicator()` (XYZ軸マーカー重畳) は Phase B では
移植しない (実機デバッグ用オーバーレイ。必要になれば同関数を参照して追加)。

### 5.2b 表示ルール — 前面/裏面の描き分け (2026-07-06 仕様追加、同日2回改訂)

live モードでは「実機を外から見ている」見え方に寄せる:

1. **前面の LED のみ映像 RGB を適用**する。**裏面 (法線がカメラと逆向き) は非表示**
   (改訂: 当初の「50% グレー」は廃止)
   - 実装: ブレンディングを **`AdditiveBlending` のまま**とし (params モードと同じ)、
     裏面の頂点色を **黒 `(0,0,0)`** にする。加算合成では黒 = 何も描かれない = 完全不可視。
     `depthWrite` や `NormalBlending` への変更は**不要** (グレー混濁問題自体が消えるため、
     旧仕様の項目 4 は撤回)
2. **ワイヤーフレーム球は live 中は非表示** (opacity 0)。定数 1 つで「薄表示 (0.05 程度)」に
   切替できるようにしておく。params / strip モードでは従来どおり 0.12
3. **表示座標系の変換 (Z-up → Y-up)**: レイアウト CSV は CAD 由来の **Z-up**
   (赤道が z=0 平面、極が ±Z)。three.js の画面は Y-up のため、そのまま置くと
   **映像の上方向が +Z (視聴者方向) を向いて見える**。
   **quaternion 回転グループのさらに外側**に固定回転を1段かませて補正する:

   ```jsx
   <group rotation={[-Math.PI / 2, 0, 0]}>   {/* Z-up → Y-up: 映像の上=画面の上 */}
       <group ref={groupRef} scale={2.2}> ... </group>   {/* IMU quaternion 適用側 */}
   </group>
   ```

   - **表示専用の変換**であり、§5.2 のサンプリング (ファーム逐語移植パス) には
     **一切影響させない** (実機の発色は変わらないので UI 側の見た目だけ直す)
4. **前面判定**: カメラは +Z 固定 ([0,0,6])。判定は**合成後のワールド座標の z 成分**で行う。
   外側固定回転 Rx(-90°) は (x,y,z)→(x,z,-y) なので、
   `z_world = -( R(q)·p ).y` (q = 表示用の**非共役** quaternion)。
   `z_world > 0` → 前面 (映像色) / `z_world ≤ 0` → 裏面 (黒=非表示)

> ⚠️ **回転の使い分けに注意**: サンプリング (§5.2) は**共役** quaternion、
> 前面判定は**非共役** (表示と同じ) quaternion + 外側固定回転の合成。
> サンプリング用に回した座標を流用すると判定が逆になる。
> 800 点 × 回転 2 回でも計算量は問題ない

### 5.3 colorMode の拡張とフォールバック

- `colorMode: 'params' | 'strip' | 'live'` に拡張
- `'live'`: vertexColors 使用、material color=#fff、brightness は頂点色に焼き込み済みのため
  opacity は固定 (1.0)。ブレンディング・球メッシュ・座標系の扱いは **§5.2b の表示ルールに従う**
  (Additive のまま、前面のみ映像色 / 裏面は黒=非表示、ワイヤーフレーム非表示、Z-up→Y-up 変換)
- **フレームが 3 秒以上届かない場合は自動で `'params'` 表示に落とす** (再生停止・切断時)。
  復帰したら `'live'` に戻る。ユーザー操作は不要
- `document.hidden` 中は色計算を停止 (バックグラウンドタブで無駄な計算をしない)

### 5.4 性能予算

- 色計算: 800 × (quat回転 + atan2×2 + 配列参照) ≈ 0.1ms/回、最大 15Hz → 無視できる
- JPEG デコード: ブラウザネイティブ (Image) 5fps → 無視できる
- **60fps 維持が受け入れ条件** (色計算は useFrame 内でやらない。フレーム/IMU イベント駆動)

### 5.5 Phase B 受け入れ基準

1. テスト動画 (上半分赤/下半分青、または経度グリッド) を再生し、**UI の前面 800 点の模様が
   実機 LED の発色と一致**する (向き・境界位置まで)
2. 実機を手で回転させると、UI 上で点群 (球) が回る一方、**映像の模様は世界固定に見える**
   — 実機の見た目と同じ挙動
3. **映像の上方向が画面の上 (+Y) に見える** (Z-up→Y-up 変換の確認。
   例: 上=空/下=地面のテスト映像で空が画面上に来る)
4. **前面 (法線がカメラ向き) の LED だけが映像色**になり、**裏面の LED は見えない**。
   回転すると可視/不可視の境界が球の輪郭 (シルエットエッジ) に沿って移動する
5. live 中は**ワイヤーフレーム球が見えない** (定数変更で薄表示に切替できる)
6. 再生停止 → 3 秒で params 着色へフォールバック、再生再開で live に自動復帰
   (params / strip モードでは従来どおりの表示のまま)
7. FRAME_PREVIEW 配信中も WS の STATE_UPDATE / コマンドが遅延しない
8. スマホ実機で 60fps 維持、バックグラウンドタブで CPU を食わない

---

## 6. 実装手順 (Opus セッション向け)

```bash
git fetch && git switch feat/ui-v2
```

**Phase A (実装済み)**: §2 エンドポイント + §3 LedPointCloud → HoloSphere.jsx に反映済み。

**Phase B (今回の実装対象)**:

1. §5.1 サーバー側 FRAME_PREVIEW 配信 (`video_streamer.py` + `state_manager.py`)。
   `verify_server_comm.py` の流儀で、WS を張って FRAME_PREVIEW が 5fps で届くことを
   スクリプト確認できるとよい (動画再生は `POST /api/playlist/playback/start`)
2. §5.2 `useLedLiveColors` フック + `LedPointCloud` の `colorMode='live'` 対応。
   **ファーム式の逐語移植** (`rotateByQuaternion` / `sphereToUV`) がこのタスクの核心。
   `core/src/LEDManager.cpp:242-297` と `core/src/FastMath.h` を必ず原文参照すること
3. §5.3 フォールバック (3秒 → params) と `document.hidden` 対応
4. §5.5 受け入れ基準を確認。1 と 2 は実機 LED との突き合わせが必要なため、
   実機がない場合は「テスト動画 + UI 単体の模様検証」まで行い、実機照合は発注者に依頼
5. コミット例: `feat(server+frontend): FRAME_PREVIEW配信とIMU連動デジタルツイン (LED 800点のライブ発色)`
