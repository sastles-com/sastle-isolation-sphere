> **English** · [日本語](ui-v2-design-spec.ja.md)

# UI v2 Design Specification — Sphere Player

Target: complete overhaul of `server/frontend/`
Created: 2026-07-06 / Direction confirmed by the client: **sphere-centric player × glassmorphism**
For implementers: this document is written so that implementation can begin from it alone. Do not guess about unclear points — confirm them with the client.

> **Recovery point**: the current UI (cyber-neon 4-tab) is saved on GitHub under the git tag **`ui-v1-cyber-neon`**.
> This work is done on the branch **`feat/ui-v2`** and replaces the current UI without touching it at all.

---

## 0. Summary

Renew the operation WebUI of the Isolation Sphere (an 800-LED spherical display) from a **tab-switching dashboard**
into **"a player where the sphere is always the star"**. Smartphone (portrait) is the main target, with full operation on PC as well.
Make flick (swipe) a first-class navigation mechanism, and provide every gesture with a visible
affordance and a PC fallback (click / keyboard).

The design language is glassmorphism. The **UI's ambient light following the sphere's hue parameter** —
the "dynamic accent" — is the signature of this design.

---

## 1. Big picture — mental model

Borrow the mental model of a "music player". Spotify's Now Playing screen is the closest analogue.

```
                    ┌──────────────────────┐
                    │   CONTROL DRAWER     │  ← flick down to bring it in
                    │ (system/IMU/logs)     │
                    └──────────▲───────────┘
                               │ flick down from the top edge
┌──────────────────────────────┴───────────────────────────────┐
│                                                              │
│                        STAGE (persistent)                    │
│    3D sphere + status + Now Playing + transport              │
│                                                              │
│   ← horizontal flick: skip track forward/back →              │
│   right-edge vertical drag: brightness / left-edge vertical drag: hue │
│                                                              │
└──────────────────────────────▲───────────────────────────────┘
                               │ flick up from the bottom edge
                    ┌──────────┴───────────┐
                    │   LIBRARY SHEET      │  ← flick up to raise it
                    │ (playlists/videos)    │
                    └──────────────────────┘
```

- **STAGE**: the only persistent screen. The 3D sphere (`HoloSphere`) is drawn full-bleed in the background and rotates with the IMU.
- **LIBRARY SHEET**: a bottom sheet that appears with an upward flick from the bottom edge (two detents: half / full).
- **CONTROL DRAWER**: a top drawer that appears with a downward flick from the top edge.
- The tab bar is abolished. All screen transitions are expressed as "showing/hiding a sheet".

---

## 2. Screen specifications

### 2.1 STAGE (persistent main screen)

```
┌─────────────────────────────┐
│ ● STREAMING      28fps 42°C │ ← status bar (glass pill)
│                          ⚙︎ │    ⚙︎ = button to open the CONTROL DRAWER
│                             │
│         ╭─────────╮         │
│        ╱           ╲        │
│       │  3D sphere │       │ ← HoloSphere (IMU rotation, full-bleed canvas)
│        ╲           ╱        │    hue-following aurora glow behind it
│         ╰─────────╯         │
│                             │
│  morning_mix                │ ← Now Playing (playlist / video name)
│  ▸ demo01.mp4    1:23/2:04  │
│                             │
│ ┌─────────────────────────┐ │
│ │  ⏮   ▶ / ⏸   ⏭    ⟳    │ │ ← transport dock (glass)
│ │ ────────●──────  BRT 64%│ │    persistent brightness mini-slider
│ └─────────────────────────┘ │
│        ── (handle) ──       │ ← grab handle for the LIBRARY
└─────────────────────────────┘
```

Components:

| Element | Content | Data source |
|---|---|---|
| Status bar | Connection state ●(green = WS + device online / yellow = WS only / red = disconnected), playback state (STREAMING/PAUSED/IDLE), fps, temp | WS `STATE_UPDATE` payload.system / `/api/playlist/playback` |
| 3D sphere | Reuse the existing `HoloSphere.jsx`. Rotation = IMU quaternion, color/brightness = params | WS `STATE_UPDATE` payload.imu / payload.params |
| Aurora glow | radial-gradient behind the sphere. Color is `--accent` (hue-following) | payload.params.hue |
| Now Playing | Active playlist name, name of the video being played, position/length | `/api/playlist/playback` (2s polling, following the current approach) |
| Transport | ⏮ previous / ▶⏸ play/pause / ⏭ next / ⟳ loop + brightness slider | API mapping table in §5 |

### 2.2 LIBRARY SHEET (flick up)

- A bottom sheet. Detents: **half (55%)** / **full (screen − safe-area)**. It follows the drag continuously and
  snaps to a detent (spring) based on release velocity and position.
- Internally two segments: **Playlists / Videos** (glass segment control, also switchable by horizontal flick).
- **Playlists**: a vertical list of cards. Card = name + track count + thumbnail.
  - Tap → activate (`PUT /api/config/settings` playback.active_playlist) + playback-start confirmation
  - The currently playing playlist gets an `--accent` indicator
- **Videos**: a 2-column grid (following the current VideoCard). Upload / delete / one-shot playback (`POST /api/playlist/play/{video_id}`).
- Close the sheet: flick down / tap the handle / tap the background (the visible part of STAGE).

### 2.3 CONTROL DRAWER (flick down)

- A top drawer, 1 detent (75%). Internal vertical scroll. Section order:
  1. **DEVICE** — device_status, fps, temp, IP, `sphere/{id}/status`. System commands such as reboot are a future slot
  2. **ORIENTATION** — axis-display toggle, orientation-offset joystick (a glass version of the current `NeonJoystick`)
  3. **TUNE** — speed / saturation sliders (brightness/hue may overlap with the STAGE edge drag)
  4. **PATTERN** — equivalent of the current PatternControl (LED modes such as solid/off)
  5. **CONFIG** — equivalent of the current ConfigEditor (a form. For the first phase, porting the existing implementation is fine)
  6. **LOGS** — tap to go to a full-screen log viewer (monospace font, auto-following scroll, pause)

### 2.4 Edge drag (direct parameter operation on STAGE)

Borrow the convention of video players (right edge = brightness):

| Gesture | Parameter | Feedback |
|---|---|---|
| **Vertical drag on the right 20%** | brightness (0-100) | A vertical gauge + value appears on the right of the screen, fading out 1s after release |
| **Vertical drag on the left 20%** | hue (0-360) | A hue gradient gauge appears on the left of the screen; the sphere and UI accent respond immediately |

- During the drag, send `SET_PARAMS` with 60ms debounce (following the current 100ms of SphereControl, but 60ms to prioritize responsiveness).
- The edge-drag start region does not conflict with sheet flicks (sheet open/close is judged in the central 60%).

### 2.5 Horizontal flick (STAGE) — track skip

- A horizontal flick in the central region of STAGE = **skip to the next/previous video**.
- A micro-motion where the sphere momentarily flows in the flick direction with inertia and returns (a physical "sent it" feel).
- API: next = advance to the next item of `POST /api/playlist/playback/start`. **If the server has no skip API,
  the first phase substitutes "directly play the next video_id in the playlist via `/play/{video_id}`"**
  (playlist items can be obtained via `/api/playlist/playlists/{id}`). Adding a skip API is filed as a server-side task.

### 2.6 PC (mouse/keyboard) fallback

Every gesture must also have a non-gesture means:

| Operation | Touch | PC |
|---|---|---|
| Open/close LIBRARY | flick up | click the handle / `L` key |
| Open/close CONTROL | flick down | click ⚙︎ / `,` key |
| Play/pause | tap ▶ | click / `Space` |
| Track skip | horizontal flick | click ⏮⏭ / `←` `→` |
| Brightness | right-edge drag | slider / `↑` `↓` |
| Drag inside a sheet | drag | mouse drag also works (framer-motion drag supports the mouse) |

- On hover-capable devices, define a hover state for interactive elements (`@media (hover: hover)`).
- At 720px and above, constrain the layout to a centered 640px column (place the smartphone version as-is in the center.
  Do not create a PC-only layout such as a two-column layout — prioritize consistency of the operation model).

---

## 3. Visual design specification

### 3.1 Design tokens (CSS custom properties)

```css
:root {
  /* ground */
  --ground: #05080f;            /* deep space. Not pure black but blue-biased */
  --ground-2: #0a1020;          /* a slight step inside the sheet */

  /* dynamic accent — made to follow the sphere's hue parameter via JS */
  --sphere-hue: 190;                                   /* updated by STATE_UPDATE params.hue */
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

  /* semantic (independent of the accent) */
  --ok: #34d399;  --warn: #fbbf24;  --err: #f87171;
}
```

- A glass surface must always use the three-piece set `background: var(--glass-bg); backdrop-filter: var(--glass-blur);
  border: 1px solid var(--glass-stroke);`. **Overuse of glow or drop shadow is prohibited.**
  The only things allowed to emit light are the sphere, the aurora, and accent indicators.
- Because this app's main use case is "operating an LED sphere in the dark", the **dark theme is fixed as the single theme**
  (no light theme is made. This is a choice, not an omission).

### 3.2 Typography

| Role | Face | Usage |
|---|---|---|
| General UI | **Manrope Variable** (`@fontsource-variable/manrope`) | 400/500/700. Headings are 700 + `letter-spacing: -0.01em` |
| Numbers / logs | **JetBrains Mono** (`@fontsource/jetbrains-mono`) | fps/temperature/time/logs. `font-variant-numeric: tabular-nums` |
| Labels | Manrope 500 uppercase | 11px, `letter-spacing: .08em`, `--tx-3` |

Type scale: `12 / 14 / 16 / 20 / 28px`. Now Playing track name = 20px, playlist name = 16px.

> ⚠️ **Fonts must always be bundled as npm packages** (`@fontsource-*`). Because this server is
> distributed over an offline LAN such as the ESP32's P2P AP, **any reference to an external CDN (Google Fonts, etc.) is strictly prohibited.**
> This also applies to emoji icons, icon CDNs, and analytics scripts.

### 3.3 Motion

- Library: **framer-motion**. The sheet is `drag="y"` + spring (`stiffness: 400, damping: 40`).
- Sheet appearance is a spring with no overshoot. Micro-motions (button-press scale .96,
  sphere inertia on track skip) are 150-250ms.
- Under `prefers-reduced-motion: reduce`, drop the sheet to a fade transition and stop the sphere's auto-rotation effect.

---

## 4. Technical design

### 4.1 Stack changes

| Item | Current | v2 |
|---|---|---|
| Navigation | MUI Tabs + BottomNavigation + react-swipeable | **custom Stage/Sheet + framer-motion drag** |
| UI components | MUI (Box/Chip/Slider...) | **custom glass components** (§4.3). MUI is phased out entirely and gradually |
| Gestures | react-swipeable | **framer-motion** (drag / pan / snap are all unified on this. react-swipeable is removed) |
| 3D | react-three-fiber + HoloSphere | **reuse as-is** |
| State/communication | WebSocketContext + useStateUpdate + lib/api | **reuse as-is** (the contract in §5 is unchanged) |
| Routing | react-router (essentially unused) | **removed** (single screen) |
| Fonts | no CDN dependency (system) | @fontsource-variable/manrope + @fontsource/jetbrains-mono |

Added dependencies: `framer-motion`, `@fontsource-variable/manrope`, `@fontsource/jetbrains-mono`
Removed dependencies (final phase): `@mui/material`, `@mui/icons-material`, `@emotion/*`, `react-swipeable`, `react-router-dom`, `react-knob-headless`

Icons: instead of MUI icons, **author inline SVGs ourselves** (collected in `components/ui/icons.jsx`,
unified as 1.5px stroke line drawings). About 15 are needed (play/pause/stop/skip/loop/gear/chevron/upload/trash/...).

### 4.2 Directory structure (v2)

```
server/frontend/src/
├── App.jsx                     # only WebSocketProvider + <SpherePlayer/>
├── theme/tokens.css            # tokens from §3.1 (single source)
├── pages/SpherePlayer.jsx      # assembly of STAGE + the 2 sheets, gesture wiring
├── components/
│   ├── stage/
│   │   ├── SphereStage.jsx     # HoloSphere + aurora + edge drag
│   │   ├── StatusBar.jsx       # connection/playback state/fps/temp
│   │   ├── NowPlaying.jsx
│   │   ├── TransportDock.jsx
│   │   └── EdgeParamGauge.jsx  # gauge display for right-edge/left-edge drag
│   ├── sheets/
│   │   ├── GlassSheet.jsx      # generic sheet (bottom/top, detents, drag)
│   │   ├── LibrarySheet.jsx    # Playlists/Videos segments
│   │   └── ControlDrawer.jsx   # DEVICE/ORIENTATION/TUNE/PATTERN/CONFIG/LOGS
│   ├── library/                # PlaylistList / VideoGrid / VideoCard (ported from current)
│   ├── control/                # OrientationPad / PatternPanel / ConfigForm / LogViewer
│   └── ui/                     # GlassButton / GlassSlider / SegmentControl / icons.jsx
├── contexts/WebSocketContext.jsx   # reused (do not change)
├── hooks/
│   ├── useSphereState.js       # reused
│   ├── usePlayback.js          # consolidates playback polling + operations (extracted from the current SphereDashboard)
│   └── useDynamicAccent.js     # reflects params.hue → --sphere-hue onto the document
├── lib/{api.js, format.js}     # reused
└── components/sphere/HoloSphere.jsx  # reused (maintain props compatibility)
```

### 4.3 Minimal set of custom UI components

| Component | Requirements |
|---|---|
| `GlassSheet` | direction (bottom/top), detents array, drag following, snap, background scrim (tap to close), safe-area support |
| `GlassSlider` | 44px touch target, value tooltip during drag, `role="slider"` + keyboard operation |
| `GlassButton` | pill type / icon type. Press scale. `:focus-visible` ring required |
| `SegmentControl` | 2-3 segments, accent-colored sliding indicator |
| `EdgeParamGauge` | vertical gauge + current value. Show/fade implemented with opacity only (avoid layout thrash) |

### 4.4 Mandatory mobile-support implementation

```html
<meta name="viewport" content="width=device-width, initial-scale=1,
      viewport-fit=cover, user-scalable=no">
<meta name="theme-color" content="#05080f">
```

- Height is `100dvh`. The bottom dock uses `padding-bottom: env(safe-area-inset-bottom)`.
- `html, body { overscroll-behavior: none; }` (prevent pull-to-refresh and back-gesture misfires, following the current approach).
- Conflict between sheet-internal scroll and sheet drag: only when the sheet is full and internal scroll is at the top,
  pass the downward drag to the sheet (control via framer-motion's `dragListener`, or by switching touch-action).
- 3D rendering: cap `HoloSphere`'s DPR at 2. Making it `frameloop="demand"` is optional (driven by IMU updates).

---

## 5. Server communication contract (unchangeable — the UI conforms to this contract)

### WebSocket `/ws`

| Direction | type | payload | Use in the UI |
|---|---|---|---|
| ← receive | `STATE_UPDATE` | full state `{imu, playback, params, led, system, seq, timestamp}` | sphere rotation / status / slider synchronization / `--sphere-hue` |
| ← receive | `LOG_LINE` | `{line}` | LogViewer (a 500-line ring buffer already on the Context side) |
| → send | `SET_PARAMS` | `{brightness?, speed?, hue?, saturation?}` (0-100, hue 0-360) | edge drag / TUNE sliders |
| → send | `SET_PLAYBACK` | `{action: "play"\|"pause"\|"stop"\|"toggle"}` | transport |
| → send | `SET_LED` | `{mode: "sphere"\|"pixels"\|"off", pixels?}` | PATTERN panel |

### REST (via `lib/api.js`)

| Endpoint | Use |
|---|---|
| `GET/POST/DELETE /api/playlist/videos[/{id}]` | video list / upload / delete |
| `GET/POST/DELETE /api/playlist/playlists[/{id}]` | playlist CRUD |
| `POST/DELETE/PUT /api/playlist/playlists/{id}/items[/{item_id}]` | item editing / reordering |
| `POST /api/playlist/play/{video_id}` | one-shot playback (also used for the horizontal-flick substitute implementation) |
| `POST /api/playlist/playback/{start\|pause\|stop\|loop}` / `GET /api/playlist/playback` | playback control / actual state (2s polling) |
| `GET/PUT /api/config/settings` | persistence of active_playlist / loop |
| `GET/POST /api/config/` | CONFIG form |

Notes (lessons from the current implementation):
- For sliders such as brightness, always include **60ms send debounce** + suppression of self-overwrite by the
  `STATE_UPDATE` echo-back (ignore received values during the drag).
- The true value of playback is on the MQTT side (`GET /playback`), which is a dual source with `STATE_UPDATE`.
  In v2, consolidate it into the `usePlayback` hook and do not fetch directly from components.

---

## 6. Implementation phases and acceptance criteria

Branch: `feat/ui-v2`. Commit per phase, and verify operation in an environment with `npm run dev` (Vite) +
a real server or `verify_server_comm.sh`.

| Phase | Content | Acceptance criteria |
|---|---|---|
| **P1 shell** | tokens.css / GlassSheet / Stage skeleton / transport / gesture open-close of the 2 sheets (placeholder content is fine) | On a real smartphone: up/down flicks make the sheet physically follow and snap, the STAGE sphere is always visible, and play/stop work |
| **P2 library** | LibrarySheet implementation (Playlists/Videos), Now Playing wiring | The sequence of selecting a playlist → playing → reflecting it in Now Playing is completed on a smartphone |
| **P3 parameters** | edge drag + EdgeParamGauge + useDynamicAccent | A right-edge drag changes the brightness of the real LEDs, and a left-edge drag changes hue and the UI accent color simultaneously |
| **P4 control** | all ControlDrawer sections (porting ORIENTATION/PATTERN/CONFIG/LOGS) | All functions of the current CONTROL tab can be executed from the new UI |
| **P5 finishing** | PC keyboard / hover / reduced-motion / removal of MUI, react-swipeable, router / removal of old components / `npm run build` → update dist | The removed dependencies from §4.1 are gone from `package.json`, the build passes without warnings, and all functions work under FastAPI distribution (`/`) |

Cross-cutting acceptance criteria:
- Works on iPhone Safari / Android Chrome / PC Chrome (portrait priority).
- All assets are distributed on an offline LAN (external CDN unreachable).
- On Lighthouse mobile, CLS ≈ 0 and time-to-interactive < 3s (LAN).
- Reverting to the old UI is possible with `git checkout ui-v1-cyber-neon -- server/frontend` (do not touch outside frontend).

---

## 7. Non-goals (out of scope)

- Changing the server API / MQTT contract (adding a skip API is only filed as a separate task)
- Light theme, i18n, PWA manifest/Service Worker (future candidates)
- New development of a joystick physical-controller UI or a per-pixel LED editor (only porting the current equivalents)
</content>
</invoke>
