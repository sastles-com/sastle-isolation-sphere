import React, { useRef, useEffect, useState, useMemo } from 'react';
import { Canvas, useFrame } from '@react-three/fiber';
import { Sphere } from '@react-three/drei';
import { useWebSocket } from '../../contexts/WebSocketContext';
import { apiGet } from '../../lib/api';
import * as THREE from 'three';

// HUE (0-360) to RGB hex conversion
const hueToRgb = (hue) => {
    // Normalize hue to 0-360 range
    const normalizedHue = ((hue % 360) + 360) % 360;
    const h = normalizedHue / 60;
    const c = 1;
    const x = c * (1 - Math.abs((h % 2) - 1));
    let r = 0, g = 0, b = 0;

    if (h >= 0 && h < 1) { r = c; g = x; b = 0; }
    else if (h >= 1 && h < 2) { r = x; g = c; b = 0; }
    else if (h >= 2 && h < 3) { r = 0; g = c; b = x; }
    else if (h >= 3 && h < 4) { r = 0; g = x; b = c; }
    else if (h >= 4 && h < 5) { r = x; g = 0; b = c; }
    else if (h >= 5 && h < 6) { r = c; g = 0; b = x; }

    const toHex = (val) => Math.round(val * 255).toString(16).padStart(2, '0');
    return `#${toHex(r)}${toHex(g)}${toHex(b)}`;
};

// ============================================================================
// ファーム描画パスの逐語移植 (core/src/LEDManager.cpp / core/src/FastMath.h)
// 「数学的に正しい形」への修正は禁止 — 実機と同じ絵が出ることが唯一の正解基準。
// ============================================================================

// FastMath.h `_atan2` は「度/180 (-1..1)」を返す規約。Math.atan2 で等価。
// (誤差 <0.2°、px 量子化後はほぼ同一。spec §5.2 参照)
const atan2n = (y, x) => Math.atan2(y, x) * (180 / Math.PI) / 180;

// LEDManager.cpp:274 rotateByQuaternion の逐語移植。
// v' = v + 2 * cross(q.xyz, cross(q.xyz, v) + q.w * v)。out に [x,y,z] を書く。
const rotateByQuaternion = (x, y, z, qw, qx, qy, qz, out) => {
    let cross1x = qy * z - qz * y;
    let cross1y = qz * x - qx * z;
    let cross1z = qx * y - qy * x;
    cross1x += qw * x;
    cross1y += qw * y;
    cross1z += qw * z;
    const cross2x = qy * cross1z - qz * cross1y;
    const cross2y = qz * cross1x - qx * cross1z;
    const cross2z = qx * cross1y - qy * cross1x;
    out[0] = x + 2.0 * cross2x;
    out[1] = y + 2.0 * cross2y;
    out[2] = z + 2.0 * cross2z;
};

// rotateByQuaternion の y 成分だけを返す軽量版 (前面判定 §5.2b 用)。
// 外側固定回転 Rx(-90°) が (x,y,z)→(x,z,-y) なので、合成後のワールド z は
// z_world = -( R(q)·p ).y。前面判定にはこの y 成分だけあれば足りる。
const rotatedY = (x, y, z, qw, qx, qy, qz) => {
    const cross1x = qy * z - qz * y + qw * x;
    const cross1z = qx * y - qy * x + qw * z;
    const cross2y = qz * cross1x - qx * cross1z;
    return y + 2.0 * cross2y;
};

// 正距円筒 (equirectangular) パノラマ → 球面 UV マッピング (ツイン先行 2026-07-07 → ファーム追従 2026-08-23)。
// 極軸=Z。out[0]=u→px(幅)=経度 -180..180 (全域)、out[1]=v→py(高さ)=極角 0..180 (全域)。
// ※ ファーム LEDManager.cpp sphereToUV() が同一式に是正され (旧実装は u=緯度/v=経度 の軸転置で
//   画像幅の右半分しか使っていなかった)、**逐語移植の不変条件は回復済み**。
//   以後この関数を変更する場合は必ずファーム側と同時に変更すること。
//   経度の継ぎ目/左右方向 (atan2n の引数順) は実機で要検証。
const sphereToUV = (x, y, z, out) => {
    const len = Math.sqrt(x * x + y * y + z * z);
    if (len < 0.0001) { out[0] = 0.5; out[1] = 0.5; return; }
    const nx = x / len, ny = y / len, nz = z / len;
    const horizontalDist = Math.sqrt(nx * nx + ny * ny);
    // 経度 (X-Y平面, -180..180) → 幅 全域 [0,1]
    let u = (atan2n(nx, ny) + 1.0) / 2.0;
    // 緯度 (Z極からの極角 0..180) → 高さ 全域 [0,1] (hd≥0 で atan2n∈[0,1]、(+1)/2 は付けない)
    let v = atan2n(horizontalDist, nz);
    if (u < 0.0) u = 0.0; else if (u > 1.0) u = 1.0;
    if (v < 0.0) v = 0.0; else if (v > 1.0) v = 1.0;
    out[0] = u; out[1] = v;
};

// ファーム updateLEDBuffer():436 は IMU quaternion の共役 (逆回転) でサンプリングする。
// spec §5.2 の擬似コードは q をそのまま使う形で書かれているため、ここを実機照合で
// 反転できるよう定数化する (静止時 = 恒等回転では両者は同一で、差は実機回転時のみ)。
const CONJUGATE_SAMPLING = true;

// LED 点サイズの物理由来 (φ100 球体 = 半径50mm、レイアウト座標は半径1に正規化)。
// sizeAttenuation の size はワールド単位・モデルスケール非依存なので GROUP_SCALE を掛け戻す。
const SPHERE_RADIUS_MM = 50;   // φ100 → 半径 50mm
const LED_SIZE_MM = 5;         // LED 相当の見た目 (好みで 1.5〜5 に調整可)
const GROUP_SCALE = 2.2;       // IMUControlledSphere の <group scale> と共有
const LED_POINT_SIZE = (LED_SIZE_MM / SPHERE_RADIUS_MM) * GROUP_SCALE;  // ≈ 0.088

// live 表示ルール (§5.2b, 2026-07-06 改訂)
const BACKFACE_COLOR = 0.0;              // 裏面 LED = 黒 (0,0,0)。AdditiveBlending で黒=完全不可視
const WIREFRAME_LIVE_OPACITY = 0.0;      // live 中はワイヤーフレーム非表示 (0.05 で薄表示に切替可)
const WIREFRAME_DEFAULT_OPACITY = 0.12;  // params / strip モード

// 円形スプライト (radial gradient の白丸) を一度だけ生成してキャッシュ。
// 外部画像ファイルを使わず points を丸ドットにする。
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

// LED レイアウトはモジュールスコープで1回だけ fetch しキャッシュする
// (シート開閉等の再マウントで再取得しない)。
let _layoutCache = null;      // { positions: Float32Array, strip: number[], count, strips }
let _layoutPromise = null;
const fetchLedLayout = () => {
    if (_layoutCache) return Promise.resolve(_layoutCache);
    if (_layoutPromise) return _layoutPromise;
    _layoutPromise = apiGet('/api/config/led-layout')
        .then((r) => {
            if (!r.ok) throw new Error(`led-layout ${r.status}`);
            return r.json();
        })
        .then((data) => {
            _layoutCache = {
                positions: new Float32Array(data.positions),
                strip: data.strip,
                count: data.count,
                strips: data.strips,
            };
            return _layoutCache;
        })
        .catch((err) => {
            // fetch 失敗で UI を壊さない (点群なしで従来表示のまま)。
            console.warn('LED layout unavailable:', err);
            _layoutPromise = null;   // 次回マウントで再試行を許す
            return null;
        });
    return _layoutPromise;
};

// strip 0-4 を hsl(strip * 72, 85%, 60%) で塗り分けた頂点色を生成
const buildStripColors = (stripIds) => {
    const colors = new Float32Array(stripIds.length * 3);
    const tmp = new THREE.Color();
    stripIds.forEach((s, i) => {
        tmp.setHSL(((s * 72) % 360) / 360, 0.85, 0.6);
        colors[i * 3] = tmp.r;
        colors[i * 3 + 1] = tmp.g;
        colors[i * 3 + 2] = tmp.b;
    });
    return colors;
};

// ============================================================================
// useLedLiveColors — FRAME_PREVIEW + IMU quaternion から 800 点の実発色を再計算する
// (デジタルツイン)。ファーム描画パスの逐語移植。spec §5.2 / §5.2b。
// ============================================================================
const FRAME_TIMEOUT_MS = 3000;   // これ以上フレームが来なければ params にフォールバック
const COMPUTE_MIN_INTERVAL_MS = 1000 / 15;   // 色計算は最大 15Hz

const useLedLiveColors = ({ layout, geomRef, quaternionRef, brightness, enabled }) => {
    let subscribeFrame = null;
    try {
        subscribeFrame = useWebSocket()?.subscribeFrame;
    } catch (e) {
        // WebSocketProvider が無い環境 (単体テスト等) — live は無効化される
    }

    const [active, setActive] = useState(false);
    // レイアウト毎に1回だけ確保する頂点色バッファ (in-place 更新)
    const colors = useMemo(
        () => (layout ? new Float32Array(layout.count * 3) : null),
        [layout]
    );
    const frameRef = useRef(null);        // { data: Uint8ClampedArray, w, h }
    const lastFrameAtRef = useRef(0);
    const lastComputeRef = useRef(0);
    const imgRef = useRef(null);
    const canvasRef = useRef(null);
    const brightnessRef = useRef(brightness);
    brightnessRef.current = brightness;

    // 最新フレーム + quaternion で colors を再計算し、geometry に needsUpdate を立てる
    const recompute = () => {
        const frame = frameRef.current;
        if (!layout || !colors || !frame) return;
        const now = (typeof performance !== 'undefined' ? performance.now() : 0);
        if (now - lastComputeRef.current < COMPUTE_MIN_INTERVAL_MS) return;
        lastComputeRef.current = now;

        const q = quaternionRef.current;
        const qw = q.w, qx = q.x, qy = q.y, qz = q.z;
        // サンプリング用は共役 (ファーム LEDManager.cpp:436)。前面判定は非共役 (表示と同じ)。
        const sx = CONJUGATE_SAMPLING ? -qx : qx;
        const sy = CONJUGATE_SAMPLING ? -qy : qy;
        const sz = CONJUGATE_SAMPLING ? -qz : qz;

        const pos = layout.positions;
        const { data, w, h } = frame;
        const bscale = (brightnessRef.current / 100) / 255;   // 0..255 → 0..1 + brightness
        const rot = [0, 0, 0];
        const uv = [0, 0];
        for (let i = 0; i < layout.count; i++) {
            const x = pos[i * 3], y = pos[i * 3 + 1], z = pos[i * 3 + 2];
            // 前面判定 (§5.2b): 表示用(非共役)回転 + 外側 Rx(-90°) 合成後のワールド z。
            // z_world = -( R(q)·p ).y。カメラは +Z 固定なので z_world > 0 が前面。
            if (-rotatedY(x, y, z, qw, qx, qy, qz) > 0) {
                // 前面 → 映像色 (サンプリングは共役回転)
                rotateByQuaternion(x, y, z, qw, sx, sy, sz, rot);
                sphereToUV(rot[0], rot[1], rot[2], uv);
                const px = Math.trunc(uv[0] * (w - 1));   // (uint16) 量子化 = 正数の trunc
                const py = Math.trunc(uv[1] * (h - 1));
                const idx = (py * w + px) * 4;
                colors[i * 3] = data[idx] * bscale;
                colors[i * 3 + 1] = data[idx + 1] * bscale;
                colors[i * 3 + 2] = data[idx + 2] * bscale;
            } else {
                // 裏面 → 黒 (AdditiveBlending で不可視)
                colors[i * 3] = colors[i * 3 + 1] = colors[i * 3 + 2] = BACKFACE_COLOR;
            }
        }
        const geom = geomRef.current;
        if (geom && geom.attributes.color) geom.attributes.color.needsUpdate = true;
    };

    // フレーム購読 + タイムアウト監視
    useEffect(() => {
        if (!enabled || !subscribeFrame) { setActive(false); return; }

        const onFrame = (payload) => {
            if (typeof document !== 'undefined' && document.hidden) return;  // バックグラウンドタブは停止
            lastFrameAtRef.current = performance.now();
            setActive(true);
            const img = imgRef.current || (imgRef.current = new Image());
            img.onload = () => {
                const w = payload.w, h = payload.h;
                let cv = canvasRef.current;
                if (!cv) cv = canvasRef.current = document.createElement('canvas');
                if (cv.width !== w) cv.width = w;
                if (cv.height !== h) cv.height = h;
                const g = cv.getContext('2d', { willReadFrequently: true });
                g.drawImage(img, 0, 0, w, h);
                frameRef.current = { data: g.getImageData(0, 0, w, h).data, w, h };
                recompute();
            };
            img.src = `data:image/jpeg;base64,${payload.jpeg_b64}`;
        };

        const unsub = subscribeFrame(onFrame);
        const iv = setInterval(() => {
            if (performance.now() - lastFrameAtRef.current > FRAME_TIMEOUT_MS) setActive(false);
        }, 1000);
        return () => { unsub(); clearInterval(iv); };
        // layout を含めるのは、レイアウト遅延ロード後に新しい closure で購読し直すため
        // eslint-disable-next-line react-hooks/exhaustive-deps
    }, [enabled, subscribeFrame, layout]);

    return { active, colors };
};

const LedPointCloud = ({ geomRef, layout, effectiveMode, liveColors, hue, brightness }) => {
    // 頂点属性を layout / effectiveMode に応じて構築
    useEffect(() => {
        if (!layout || !geomRef.current) return;
        const geom = geomRef.current;
        geom.setAttribute('position', new THREE.BufferAttribute(layout.positions, 3));
        if (effectiveMode === 'strip') {
            geom.setAttribute('color', new THREE.BufferAttribute(buildStripColors(layout.strip), 3));
        } else if (effectiveMode === 'live' && liveColors) {
            geom.setAttribute('color', new THREE.BufferAttribute(liveColors, 3));
        } else {
            geom.deleteAttribute('color');
        }
        geom.attributes.position.needsUpdate = true;
    }, [layout, effectiveMode, liveColors, geomRef]);

    if (!layout) return null;

    const isLive = effectiveMode === 'live';
    const useVertexColors = effectiveMode === 'strip' || isLive;
    const sphereColor = hueToRgb(hue);

    return (
        <points>
            <bufferGeometry ref={geomRef} />
            {/* effectiveMode を key に含め、vertexColors/blending 切替時にマテリアルを作り直す */}
            <pointsMaterial
                key={effectiveMode}
                map={dotTexture()}
                color={useVertexColors ? '#ffffff' : sphereColor}
                vertexColors={useVertexColors}
                size={LED_POINT_SIZE}
                sizeAttenuation
                transparent
                // 全モード AdditiveBlending。live の裏面は黒=不可視なので白濁しない (§5.2b 改訂)
                blending={THREE.AdditiveBlending}
                opacity={isLive ? 1.0 : 0.35 + 0.65 * (brightness / 100)}
            />
        </points>
    );
};

const IMUControlledSphere = ({ hue, brightness, colorMode }) => {
    const groupRef = useRef();
    const geomRef = useRef();
    const quaternionRef = useRef(new THREE.Quaternion(0, 0, 0, 1)); // x, y, z, w
    const [layout, setLayout] = useState(_layoutCache);

    // Safe WebSocket usage with error handling
    let lastMessage = null;
    let selectedDeviceId = null;
    try {
        const wsContext = useWebSocket();
        lastMessage = wsContext?.lastMessage;
        selectedDeviceId = wsContext?.selectedDeviceId;
    } catch (error) {
        console.warn('WebSocket context not available:', error);
    }

    useEffect(() => {
        if (layout) return;
        let alive = true;
        fetchLedLayout().then((l) => { if (alive && l) setLayout(l); });
        return () => { alive = false; };
    }, [layout]);

    const live = useLedLiveColors({
        layout, geomRef, quaternionRef, brightness, enabled: colorMode === 'live',
    });
    // live 選択中でもフレーム未着なら params 着色にフォールバック (spec §5.3)
    const effectiveMode = colorMode === 'live' ? (live.active ? 'live' : 'params') : colorMode;

    useEffect(() => {
        // Update quaternion when IMU data received.
        // 選択中の sphere があればそのデバイス別IMUを使い、無ければ後方互換の単一値にフォールバック。
        if (lastMessage && lastMessage.type === 'STATE_UPDATE') {
            const imu = (selectedDeviceId && lastMessage.payload?.devices?.[selectedDeviceId]?.imu)
                || lastMessage.payload?.imu;
            if (imu) {
                const { w, x, y, z } = imu;
                // Three.js uses (x, y, z, w) order
                quaternionRef.current.set(x, y, z, w);
            }
        }
    }, [lastMessage, selectedDeviceId]);

    useFrame(() => {
        if (groupRef.current) {
            // Apply quaternion rotation from IMU (wireframe + LED 点群を一体で回す)
            groupRef.current.quaternion.copy(quaternionRef.current);
        }
    });

    // Convert HUE to color
    const sphereColor = hueToRgb(hue);
    // Convert brightness (0-100) to emissive intensity (0-1)
    const emissiveIntensity = brightness / 100;
    // live 中はワイヤーフレーム非表示 (§5.2b)
    const wireframeOpacity = effectiveMode === 'live' ? WIREFRAME_LIVE_OPACITY : WIREFRAME_DEFAULT_OPACITY;

    return (
        // Z-up → Y-up 補正 (§5.2b): レイアウト CSV は CAD 由来 Z-up。この外側固定回転で
        // 映像の上=画面の上 (+Y) にする。IMU quaternion は内側 groupRef に適用 (無影響)。
        <group rotation={[-Math.PI / 2, 0, 0]}>
            <group ref={groupRef} scale={GROUP_SCALE}>
                {/* 骨格として薄く残すワイヤーフレーム球 (live 中は非表示)。LED 点群を主役にする。 */}
                <Sphere args={[1, 32, 32]} visible={wireframeOpacity > 0}>
                    <meshStandardMaterial
                        color={sphereColor}
                        wireframe={true}
                        emissive={sphereColor}
                        emissiveIntensity={emissiveIntensity}
                        transparent
                        opacity={wireframeOpacity}
                    />
                </Sphere>
                <LedPointCloud
                    geomRef={geomRef}
                    layout={layout}
                    effectiveMode={effectiveMode}
                    liveColors={live.colors}
                    hue={hue}
                    brightness={brightness}
                />
            </group>
        </group>
    );
};

export const HoloSphere = ({ color = 120, brightness = 80, colorMode = 'live' }) => {
    return (
        <div style={{ width: '100%', height: '100%', minHeight: '300px' }}>
            <Canvas camera={{ position: [0, 0, 6] }}>
                <ambientLight intensity={0.5} />
                <pointLight position={[10, 10, 10]} />
                <IMUControlledSphere hue={color} brightness={brightness} colorMode={colorMode} />
                {/* OrbitControls removed - sphere controlled by IMU only */}
            </Canvas>
        </div>
    );
};
