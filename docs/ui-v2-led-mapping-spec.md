> **English** · [日本語](ui-v2-led-mapping-spec.ja.md)

# UI v2 supplementary spec — 3D sphere mapping of the real LED layout

Parent spec: [`ui-v2-design-spec.md`](ui-v2-design-spec.md) / Branch: `feat/ui-v2`
Created: 2026-07-06 / An instruction sheet for the implementer (Opus session). Implementation can begin from this document alone.

---

## 0. Background — current state of the streaming design (prerequisite knowledge)

**The data flowing to the sphere is a "compressed image", not the RGB of 800 LEDs.**

```
[Server] video_streamer.py                          [Device] ESP32 (AtomS3R)
 decode the video with OpenCV                        ImageManager: UDP receive → TJpg_Decoder
 → resize to 320x160 → JPEG compression              → RGB565 frame buffer
 → split-send to UDP :8889 (MAGIC "JPEG")  ────▶      LEDManager: per-LED 3D-coord → UV conversion
                                                      to sample the image (circular multisample) → 800 LEDs
```

- Send format: **an equirectangular panorama image, 320×160 JPEG**
- **Color assignment to LEDs (UV mapping) is done on the device side** (`sphereToUV()` in `core/src/LEDManager.cpp`)
- The path that sends the 800 RGB values directly is only MQTT `SET_LED {mode:"pixels", pixels:[...]}` (for pattern control, low rate). It is not used for video streaming.
- **The UI (browser) does not receive the video frames** — UDP goes only to the device

From this premise, the UI's LED mapping display is split into two phases:

| Phase | Content | State |
|---|---|---|
| **A** | Draw the 800 physical LED positions on the sphere and color them with the hue/brightness parameters | **implemented** (LedPointCloud in HoloSphere.jsx) |
| **B** | The server also distributes preview frames to the UI, and the UI **reproduces the same rotation + UV conversion as the firmware using the IMU quaternion**, lighting the 800 points with the **actual video colors** (digital twin) | **implementation target** (§5 is this spec) |

---

## 1. Data source

`core/data/led_layouts-5strip.csv` (801 rows = header + 800 LEDs):

```csv
FaceID,strip,strip_num,x,y,z
409,0,0,0.960038824,-0.273567911,0.059042825
627,0,1,0.988396116,-0.140200102,0.058455535
...
```

- `x,y,z`: LED positions **on the unit sphere** (|v| ≈ 1.0). Usable directly as Three.js coordinates
- `strip`: 0-4 (5 strips), `strip_num`: index within the strip 0-159
- The same file as the firmware (the same layout as the real device appears in the UI)

---

## 2. Server implementation — layout distribution endpoint

Add to `server/app/api/endpoints/config.py` (the config router is already prefixed with `/api/config`):

```python
import csv
import os

# Same style as CONFIG_SEARCH_PATHS (assuming the server is started from server/)
LED_LAYOUT_SEARCH_PATHS = [
    "../core/data/led_layouts-5strip.csv",
    "data/led_layouts-5strip.csv",
]

@router.get("/led-layout")
async def get_led_layout():
    """Return the LED physical layout as flat arrays.

    Return value: {count, strips, positions: [x0,y0,z0, x1,y1,z1, ...], strip: [s0, s1, ...]}
    positions is in a form that can be poured straight into a Float32Array.
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

Design decisions (do not change):

- **Do not copy the CSV into the frontend** — core/data is the single source. Even if the layout
  changes on a board revision, the UI follows automatically
- A flat-array JSON (~30KB) is light enough. Binarization or cache headers are unnecessary
- The response is expected to be fetched only once at first (cached in module scope on the UI side)

---

## 3. Frontend implementation — LED point cloud

Target: `server/frontend/src/components/sphere/HoloSphere.jsx` (the component being reused in Phase 1)

### 3.1 Structural change

Currently a quaternion is applied to a single `<Sphere>` (wireframe). Turn this into a **group** so that
both the wireframe sphere and the LED point cloud rotate together with the IMU:

```jsx
<group ref={groupRef} scale={2.2}>        {/* the quaternion is applied to the group */}
    <Sphere args={[1, 32, 32]}>           {/* existing wireframe: lower opacity from 0.6 → 0.12 to
                                              make the LED point cloud the star (keep the skeleton faintly) */}
    <LedPointCloud hue={hue} brightness={brightness} />
</group>
```

### 3.2 `LedPointCloud` component (new)

- On mount, `apiGet('/api/config/led-layout')` → build the `position` attribute of `BufferGeometry`
  from `Float32Array(positions)`. **Fetch only once in module scope and cache it**
  (do not re-fetch on remounts caused by sheet open/close, etc.)
- Rendered with `<points>` + `PointsMaterial`. 800 points = 1 draw call, no load even on a smartphone
- To make them **round dots**, generate a circular sprite by code with a `CanvasTexture` and pass it to `map`
  (a white circle with a radial gradient on a 64×64 canvas. No external image files):

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

- Material settings:

```jsx
<pointsMaterial
    map={dotTexture()}
    color={sphereColor}                      /* reuse the existing hueToRgb(hue) */
    size={0.05}                              /* apparent width for the sphere at group scale 2.2. Fine-tune on the real device */
    sizeAttenuation
    transparent
    depthWrite={false}                       /* prevents rectangles from appearing where points overlap */
    blending={THREE.AdditiveBlending}        /* the emissive feel of LEDs */
    opacity={0.35 + 0.65 * (brightness / 100)}
/>
```

- **Being faintly visible at 0.35 even at brightness=0** (so the layout can be checked) is an intentional spec

### 3.3 Strip identification mode (for debugging, required)

Because during real-device bring-up there is a use for checking "which strip is which vertical stripe",
implement a mode that color-codes the strips using per-point vertex colors:

- A `colorMode: 'params' | 'strip'` prop on `LedPointCloud`
- In `'strip'` mode, use the `color` attribute (vertex color) and color strips 0-4 with
  `hsl(strip * 72, 85%, 60%)` (5 colors 72° apart, reliably distinguishable)
- Place the toggle UI as a **toggle in the ORIENTATION section** of the CONTROL DRAWER (wired in P4;
  while P4 is unimplemented, leaving the prop at its default `'params'` is fine)

### 3.4 Non-goals

- Removing the wireframe sphere (keep it faint. Whether to hide it completely is after the client checks the real device)
- LOD / culling of the point cloud (unnecessary at 800 points)
- Adding OrbitControls (the sphere's rotation is IMU-only; maintain the current policy)

---

## 4. Acceptance criteria (Phase A — implemented, maintained as a regression baseline)

1. **800 points** are drawn on the STAGE sphere, and the same 5-strip vertical-stripe layout as the real device is visible
   (a staggered arrangement with no LEDs near the equator, no points at the poles — reproduced exactly as in the CSV)
2. The **wireframe and the point cloud rotate as one** driven by the IMU quaternion
3. Changing hue via left-edge drag (if P3 is implemented) or via `SET_PARAMS` makes the point cloud's color follow
4. brightness changes the point cloud's brightness
5. With `colorMode='strip'`, the 5-color coding of the strips can be confirmed (the toggle UI can wait for P4)
6. On a real smartphone (iPhone Safari / Android Chrome), 60fps is maintained (checking the dev tools FPS meter by eye is fine)
7. `npm run build` passes and it works under FastAPI distribution (`/`)
8. In an environment without the layout CSV, the endpoint returns 404, and **the UI keeps working with the conventional
   display without the point cloud** (a fetch failure does not break the UI)

---

## 5. Phase B implementation spec — 800-point RGB computation using the IMU (digital twin)

**Goal**: from the playing video and the IMU quaternion, recompute on the UI side **the color that each real LED is displaying right now**
and reflect it on the 800 points. A faithful JS port of the real firmware's rendering path
(`LEDManager.cpp` renderFrame: rotation → UV conversion → image sampling).

### 5.1 Server: FRAME_PREVIEW distribution (new WS message)

`video_streamer.py` holds a decoded 320×160 JPEG every frame. Thin it out and
distribute it to WebSocket observers:

- Message: `{"type": "FRAME_PREVIEW", "payload": {"jpeg_b64": "<base64>", "w": 320, "h": 160, "seq": n}}`
- **Distribution rate: 5fps** (200ms throttle, keep it as a constant). Do not send while stopped
- Implementation location: inside `video_streamer`'s send loop, do the throttle check → add a broadcast method
  to `state_manager` (e.g. `notify_frame(jpeg_bytes)`) and call it.
  Since the streamer is a separate thread, submit it to the event loop with **the same `run_coroutine_threadsafe` pattern
  as mqtt_service** (refer to `_submit_coroutine`)
- **Do not put it in `_state`** (no retention needed, do not mix it with STATE_UPDATE — same style as LOG_LINE)
- Bandwidth: ~10KB × 5fps = 50KB/s/client. No problem on the LAN premise

### 5.2 UI: the same conversion pipeline as the firmware (verbatim port)

Create a new hook `useLedLiveColors(layout)` and use it in `LedPointCloud`'s `colorMode='live'`.

**Inputs (all available on existing channels):**

| Data | Provider | Rate |
|---|---|---|
| 800 LED coordinates | `/api/config/led-layout` (implemented in Phase A) | once at first |
| IMU quaternion | WS `STATE_UPDATE` payload.imu | **10Hz** (firmware `IMU_PUBLISH_INTERVAL`) |
| Video frame | WS `FRAME_PREVIEW` (§5.1) | 5fps |

**Frame reception processing**: `jpeg_b64` → `Image` → drawImage onto an offscreen canvas →
hold the `Uint8ClampedArray` via `getImageData()` (only the latest one).

**Color computation — the reference is the firmware's implementation (this is canonical, taking precedence over this document's prose):**

The conversion "320×160 image → 800 LED RGB" has a complete implementation in the firmware.
**Port the functions in the table below to JS verbatim.** Correcting or optimizing them into a "mathematically correct form" is prohibited
(the sole criterion of correctness is that the same picture as the real device appears. Some comments in the code are inaccurate, so treat the behavior as canonical):

| Firmware reference (core/src/) | Role | Notes when porting |
|---|---|---|
| `LEDManager.cpp` **`updateLEDBuffer()`** (~L422) | **The main loop.** The overall processing order follows this function | At the start, **conjugate the quaternion (`qx=-qx; qy=-qy; qz=-qz`) before** using it for rotation. Do the same in the UI (easy to miss) |
| `LEDManager.cpp` `rotateByQuaternion()` (L274) | Rotation of LED body coordinates: `v' = v + 2*cross(q.xyz, cross(q.xyz,v) + q.w*v)` | Port as-is |
| `LEDManager.cpp` `sphereToUV()` (L242) | Rotated coordinates → (u,v) | The internal `_sqrt`/`_atan2` refer to `FastMath.h`. `_atan2` is a **polynomial approximation returning degrees/180 (-1..1)**. In JS, `Math.atan2(y,x)/Math.PI` is equivalent (error <0.2°, virtually identical after quantization) |
| Quantization inside `updateLEDBuffer()` (L452) | `px = trunc(u*(W-1))`, `py = trunc(v*(H-1))` | W,H are **the device's decode resolution** (see ⚠️ below) |
| `LEDManager.cpp` **`sampleAveraged()`** (L495) + `setMultisample()` (L476) | **The average of the center + 6 points on a circle of radius 2.0px (K=7).** Offsets are precomputed. x wraps (`sx %= w`), y is clamped | Port with the same behavior as config `led.multisample` (default ON/2.0px/6 points). Precompute the offsets with the same formula too |
| `ImageManager.cpp` `getPixel()` (L255) | Get RGB from the frame buffer. **RGB565 → RGB888** (`r=5bit<<3, g=6bit<<2, b=5bit<<3`) | Since the UI takes from a full-color JPEG, to increase fidelity apply an RGB565 quantization after fetching (`r&0xF8, g&0xFC, b&0xF8`) (optional but recommended) |

Processing order (same as updateLEDBuffer):
`conjugate quaternion → per LED [rotate → sphereToUV → quantize px,py → sampleAveraged(K7)] → multiply by brightness → into the color attribute`

> ⚠️ **Decode resolution**: the device decodes at a reduced **160×80** with `ImageManager._jpegScale = 2`,
> and px/py quantization is also done at that resolution. To match exactly in the UI, make the offscreen canvas
> **160×80** and quantize with the same W,H (recommended). Leaving it at 320×160 has almost no visible difference, but
> if a discrepancy appears, suspect this first.

> ⚠️ **Distribution of u**: since `_atan2(hd, ny)`'s first argument is ≥0, it only returns 0..180°, so
> **u ∈ [0.5, 1.0] → px uses only the right half of the image width.** Because the video content
> is made on this premise, do not arbitrarily correct it in the UI.

**Rotation consistency (sorting out the double correction)**: the firmware "rotates the LED body coordinates by the (conjugate)
quaternion, then samples" = a world-fixed video is projected onto the LEDs. In the UI, the point-cloud group itself is also
rotated by the quaternion, but **sampling is also done exactly as in the firmware, on 'the coordinates after rotation by the conjugate quaternion'.**
As a result, the UI's sphere becomes "the picture of the real device seen from outside in that orientation". This is the correct twin.
Do not stop the group rotation, and do not sample with unrotated UV.

**Out of scope (not ported)**: `overlayAxisIndicator()` (XYZ axis-marker overlay) is not ported in Phase B
(a real-device debugging overlay. If needed, refer to the same function and add it).

### 5.2b Display rule — drawing front/back faces distinctly (spec addition 2026-07-06, revised twice the same day)

In live mode, lean toward the "looking at the real device from outside" appearance:

1. **Apply the video RGB only to front-facing LEDs.** **Hide the back face (whose normal points away from the camera)**
   (revised: the original "50% gray" is abolished)
   - Implementation: keep the blending as **`AdditiveBlending`** (same as params mode), and
     make the back-facing vertex color **black `(0,0,0)`**. In additive blending, black = nothing drawn = completely invisible.
     Changing to `depthWrite` or `NormalBlending` is **unnecessary** (because the gray-muddiness problem itself disappears;
     item 4 of the old spec is withdrawn)
2. **The wireframe sphere is hidden during live** (opacity 0). Make it switchable to "faint display (around 0.05)"
   via a single constant. In params / strip modes, keep it at 0.12 as before
3. **Display coordinate transform (Z-up → Y-up)**: the layout CSV is CAD-derived **Z-up**
   (equator on the z=0 plane, poles at ±Z). Since three.js's screen is Y-up, placing it as-is makes
   **the video's up direction appear to point toward +Z (toward the viewer)**.
   Interpose one fixed rotation **outside the quaternion-rotation group** to correct it:

   ```jsx
   <group rotation={[-Math.PI / 2, 0, 0]}>   {/* Z-up → Y-up: video up = screen up */}
       <group ref={groupRef} scale={2.2}> ... </group>   {/* the IMU-quaternion-applied side */}
   </group>
   ```

   - This is a **display-only transform** and must have **no effect at all** on §5.2's sampling
     (the firmware verbatim-port path) (the real device's colors do not change; only the UI's appearance is fixed)
4. **Front-face judgment**: the camera is fixed at +Z ([0,0,6]). The judgment is made by **the z component of the composited world coordinates.**
   The outer fixed rotation Rx(-90°) maps (x,y,z)→(x,z,-y), so
   `z_world = -( R(q)·p ).y` (q = the **non-conjugate** quaternion used for display).
   `z_world > 0` → front face (video color) / `z_world ≤ 0` → back face (black = hidden)

> ⚠️ **Be careful which rotation you use**: sampling (§5.2) uses the **conjugate** quaternion,
> while the front-face judgment uses the **non-conjugate** (same as display) quaternion + the outer fixed rotation composited.
> If you reuse the coordinates rotated for sampling, the judgment is inverted.
> Even 800 points × 2 rotations is no problem for computation

### 5.3 colorMode extension and fallback

- Extend to `colorMode: 'params' | 'strip' | 'live'`
- `'live'`: uses vertexColors, material color=#fff. Since brightness is baked into the vertex color,
  opacity is fixed (1.0). The handling of blending, sphere mesh, and coordinate system **follows §5.2b's display rules**
  (stay Additive, front only shows video color / back is black = hidden, wireframe hidden, Z-up→Y-up transform)
- **If frames do not arrive for 3 seconds or more, automatically drop to the `'params'` display** (on playback stop / disconnect).
  When it recovers, return to `'live'`. No user action needed
- While `document.hidden`, stop the color computation (do not waste computation on a background tab)

### 5.4 Performance budget

- Color computation: 800 × (quat rotation + atan2×2 + array lookup) ≈ 0.1ms/pass, at most 15Hz → negligible
- JPEG decode: browser-native (Image) at 5fps → negligible
- **Maintaining 60fps is an acceptance condition** (do not do the color computation inside useFrame. Frame/IMU event-driven)

### 5.5 Phase B acceptance criteria

1. Play a test video (top half red / bottom half blue, or a longitude grid) and **the pattern of the UI's front 800 points
   matches the color emission of the real device's LEDs** (down to orientation and boundary position)
2. When you rotate the real device by hand, the point cloud (sphere) rotates in the UI while **the video's pattern appears world-fixed**
   — the same behavior as the real device's appearance
3. **The video's up direction appears at the top of the screen (+Y)** (confirming the Z-up→Y-up transform.
   e.g. with a test video of sky at top / ground at bottom, the sky comes to the top of the screen)
4. **Only front-facing LEDs (normal toward the camera) show video color**, and **back-facing LEDs are invisible.**
   As you rotate, the visible/invisible boundary moves along the sphere's outline (silhouette edge)
5. During live, **the wireframe sphere is invisible** (switchable to faint display by changing a constant)
6. Playback stop → fall back to params coloring in 3 seconds; playback resume → automatically return to live
   (in params / strip modes, the conventional display stays as before)
7. STATE_UPDATE / commands on the WS are not delayed even during FRAME_PREVIEW distribution
8. On a real smartphone, 60fps is maintained; it does not consume CPU on a background tab

---

## 6. Implementation steps (for the Opus session)

```bash
git fetch && git switch feat/ui-v2
```

**Phase A (implemented)**: §2 endpoint + §3 LedPointCloud → already reflected in HoloSphere.jsx.

**Phase B (this implementation target)**:

1. §5.1 server-side FRAME_PREVIEW distribution (`video_streamer.py` + `state_manager.py`).
   In the style of `verify_server_comm.py`, it would be good to script-confirm that FRAME_PREVIEW arrives at 5fps by
   opening a WS (video playback is `POST /api/playlist/playback/start`)
2. §5.2 `useLedLiveColors` hook + `LedPointCloud` `colorMode='live'` support.
   The **verbatim port of the firmware formulas** (`rotateByQuaternion` / `sphereToUV`) is the core of this task.
   Always refer to the originals `core/src/LEDManager.cpp:242-297` and `core/src/FastMath.h`
3. §5.3 fallback (3 seconds → params) and `document.hidden` support
4. Confirm the §5.5 acceptance criteria. Since 1 and 2 require cross-checking against the real device's LEDs,
   if there is no real device, do "test video + UI-standalone pattern verification" and ask the client for the real-device comparison
5. Example commit: `feat(server+frontend): FRAME_PREVIEW streaming and IMU-linked digital twin (live coloring of 800 LEDs)`
</content>
