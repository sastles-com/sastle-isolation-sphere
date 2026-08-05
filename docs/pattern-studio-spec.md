> **English** · [日本語](pattern-studio-spec.ja.md)

# Pattern Studio spec & implementation plan

> Status: **draft / for discussion**. Last updated: 2026-07-08
> Related: [playlist_system_design.md](./playlist_system_design.md) /
> [ui-v2-design-spec.md](./ui-v2-design-spec.md) / VideoStreamer (320x160 equirectangular JPEG/UDP)

---

## 0. Background and decisions

- The sphere's display content is **equirectangular 320×160, 2:1, JPEG, ≤20fps**.
  The server (`VideoStreamer`) decodes with OpenCV → `cv2.resize(320,160)` → JPEG → UDP send.
  **No projection transform is applied; the uploaded video itself is the equirectangular map.** The core is display-only.
- The **final deliverable of a pattern (handwritten text + effects, etc.) is "an ordinary video"**,
  which can ride directly on the existing video/playlist pipeline.
- Therefore, do not cram editing into the operation app; **separate it into a dedicated web "Pattern Studio".**
  Add to the operation app a **"Pattern" tab = material list + a link to launch the Studio (launcher)**.
- A pattern is **a kind of video** (distinguished by `kind`). Inserting it between videos in a playlist is still possible as an existing function.
- **No core/firmware changes at all.** Only minimal additions to the DB.

---

## 1. Scope

### In scope (initial)
- Handwritten characters/text, basic shapes, color, background.
- Effects: spherical spin (longitude scroll) / in-plane rotation / zoom pulse / fade.
- Duration / fps / loop-count settings. **Enforcement of the loop invariants** (§2).
- WYSIWYG preview of equirectangular 2:1 (loop playback to check the seam) + an optional spherical preview.
- Client-side rendering → video export → upload to the existing `POST /api/playlist/videos`.

### Out of scope (initial; future options)
- Server-side live generation (rendering parameters in real time). A promotion candidate in P5.
- Core/firmware changes.
- A full-feature timeline / keyframe editor (initially about numbers + easing).

---

## 2. Output spec (device conformance = invariants)

- **Aspect 2:1 (equirectangular) required.** The working resolution may be 2× such as 640×320
  (the server resizes to 320×160).
- **fps ≤ 20** (recommended 15–20. The device's decode limit is ~20fps).
- **Codec**: existing videos are mp4 (H.264). Since the device decodes with `cv2.VideoCapture`,
  **make mp4/H.264 the first choice** (see the webm risk in §10).
- **Loop invariants (loopy panorama + loop playback)**:
  1. **Longitude wrap**: whatever spills past the right edge (+180°) wraps around to the left edge (−180°).
     Ensured by a horizontal roll of a full-width layer, or by horizontal tile compositing (x, x−W, x+W) when straddling the seam.
  2. **Time loop**: make the head ↔ tail continuous. **Total movement = an integer multiple of the width W**,
     **all variable parameters have integer periods within the duration** (in-plane rotation = integer 360° turns, zoom = sin pulse, spin = integer revolutions).
  3. **Do not duplicate the last frame (t=T)** (t=T ≡ t=0). N frames output phases 0..(N−1)/N.
- Place text in the **equatorial band** (in-plane rotation in equirectangular distorts near the poles).

---

## 3. UX / editing features

- **Handwriting canvas** (touch/mouse, transparent). **Text input** (font selection). Basic shapes.
- Layers (initially 1 to a few), color/background/opacity.
- **Effects**: spherical spin (longitude scroll) / in-plane rotation (integer 360°) / zoom pulse (sin) / fade.
- Duration / fps / loop revolutions (integer) / speed.
- **Preview**: a 2:1 canvas (loop playback to check the seam) + an optional spherical preview
  (the existing `HoloSphere`/three.js can be reused).
- **Save**: "Save to Sphere" = video export → upload. Specify a title.

---

## 4. Integration contract (server coordination = minimal)

- **Format retrieval**: `GET /api/config/settings` (image width/height/fps limit).
  If absent, add a lightweight `GET /api/config/image`.
- **Upload**: the existing `POST /api/playlist/videos` (multipart: file + title).
  To pass **`kind=pattern`**, add `kind` to the Form (default 'video').
- The result is registered in `videos` and can be listed in the Pattern tab and inserted into a playlist.

---

## 5. Changes on the operation app side (Pattern tab = launcher)

- Add `patterns` to `LibrarySheet`'s `SEGMENTS` → three segments: **Playlists / Videos / Patterns**.
- Patterns segment: list materials with `kind==='pattern'` like `VideoGrid`
  (`PatternGrid`/`PatternCard` reuse `VideoCard` with minor modifications).
  - Each card has an "**Edit in Studio**" link (`/studio?video={id}`).
  - A "**Create new**" button (`/studio`).
- Since materials are videos, whether to also show them in the Videos segment is a filter choice (default: separated display).
- **Terminology-collision caution**: the existing `control/PatternPanel.jsx` (LED display mode SPHERE/OFF) is a different thing.
  To avoid confusion, consider renaming the LED side to "Display / LED Mode" (optional).

---

## 6. Data model changes (minimal)

- Add **`kind TEXT DEFAULT 'video'`** to `videos` ('video' | 'pattern').
  - Migration: `ALTER TABLE videos ADD COLUMN kind TEXT DEFAULT 'video'` (existing rows become 'video').
- Add `create_video(..., kind='video')`. Add narrowing to `get_videos(kind=None)`.
- Filter with `GET /api/playlist/videos?kind=pattern`. `upload_video` receives the Form `kind`.

---

## 7. Thumbnail fix (a separate request, handled at the same time)

**Cause**: `upload_video` does not generate a thumbnail, so `thumbnail_path` is always Null.
In addition, there is no HTTP distribution path for media (only `/assets` is mounted). Because `VideoCard`
uses `video.thumbnail_path` for `<img src>`, it always shows a placeholder.

**Response**:
1. In `upload_video`, extract a representative frame (a non-black frame at the 0–10% position) with OpenCV →
   save to `data/thumbnails/{uuid}.jpg` → store `thumbnail_path`. 1:1 crop (the card is 1:1).
2. Distribution: add `GET /api/playlist/videos/{id}/thumbnail` (FileResponse).
   Attach **`thumbnail_url`** (= the above URL) to the API response.
3. `VideoCard`: `src = video.thumbnail_url || video.thumbnail_path` (backward compatible).
4. Backfill for existing videos: lazy generation on first request, or a batch script.
- Pattern videos also get the same thumbnail (also displayed on the Pattern card).

---

## 8. Technology selection / hosting

- **Studio**: a separate Vite + React app (the existing three.js can be reused). Rendering Canvas2D/WebGL.
  Export is **mp4 (H.264)**. Method: `ffmpeg.wasm` (reliable) first, `MediaRecorder` (webm) after §10 verification.
- **Distribution options**:
  - **A) A separate route `/studio` on the same FastAPI** (mount `frontend-studio/dist` with StaticFiles).
    → Same origin, so **upload is CORS-free**, single-package distribution. **Recommended.**
  - B) A fully independent static app (separate port/host). Fast to develop but requires CORS handling.
- **Caution**: currently the catch-all `@app.get("/{full_path:path}")` picks up everything, so
  place the `/studio` mount **before the catch-all** (same as the existing `/assets` mount).

---

## 9. Implementation plan (phases)

| P | Content | Depends on | Size |
|---|------|------|------|
| **P0** | **Thumbnail fix** (upload generation + distribution + VideoCard) | none | small, immediate effect |
| P1 | Data model: add `videos.kind` + API filter + upload's kind | P0 | small |
| P2 | Operation app Pattern tab (list + Studio link, new/edit) | P1 | medium |
| P3 | Pattern Studio minimal version: handwriting + text + spherical spin + loop invariants + mp4 export + upload. Same-origin `/studio` distribution | P1 | medium–large |
| P4 | Studio expansion: in-plane rotation / zoom pulse / fade, color/background, spherical preview, multiple layers, duration/fps/revolutions | P3 | large |
| P5 | (future) promote to server-side live generation (reuse the same rendering logic, no core modification) | P3 | large |

- **P0 is independent** and can be implemented ahead (thumbnail display has value that takes effect right now).
- The Studio (P3) is loosely coupled to the operation app (the only contact point is the upload contract), so it can be developed in parallel.

---

## 10. Risks / to be verified

1. **Codec compatibility (most important)**: device distribution opens with `cv2.VideoCapture`.
   **Whether webm (VP8/9) can be decoded by the OpenCV build is unconfirmed.** webm is in ALLOWED_EXT, but
   whether it actually plays needs verification. **If not, have the Studio output mp4 (H.264/ffmpeg.wasm)**,
   or normalize-transcode on the server side at upload time (also useful for general uploads, scope increase).
2. **Pole distortion of equirectangular in-plane rotation** → guide text to equatorial-band operation in the UI.
3. **The look of 320×160 / ≤20fps** needs real-device checking (breakdown of thin text / fast rotation).
4. **Terminology collision**: the existing `PatternPanel` (LED mode) and the Pattern tab/Studio. Sort it out with a rename (optional).
5. **Loop-end continuity**: enforce total movement = integer multiple of W, integer periods, non-duplicated tail on the Studio side (auto-snap).

---

## 11. Open issues (next discussion)

- Studio export method: `ffmpeg.wasm` (mp4 reliable, heavy) or `MediaRecorder` (webm light, needs verification).
- Studio distribution: same-origin `/studio` (recommended) or an independent app.
- `videos.kind` separated display, or integrated display in Videos + a badge.
- Thumbnail distribution: a dedicated endpoint or a `data/` static mount.
- Whether the existing LED `PatternPanel` can be renamed.
</content>
