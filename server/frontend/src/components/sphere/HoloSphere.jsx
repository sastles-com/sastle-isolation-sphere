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

// LEDManager.cpp:242 sphereToUV の逐語移植。out に [u,v] を書く。
const sphereToUV = (x, y, z, out) => {
    const len = Math.sqrt(x * x + y * y + z * z);
    if (len < 0.0001) { out[0] = 0.5; out[1] = 0.5; return; }
    const nx = x / len, ny = y / len, nz = z / len;
    const horizontalDist = Math.sqrt(nx * nx + nz * nz);
    let u = (atan2n(horizontalDist, ny) + 1.0) / 2.0;  // hd≥0 なので u ∈ [0.5, 1.0] (仕様)
    let v = (atan2n(nx, nz) + 1.0) / 2.0;
    if (u < 0.0) u = 0.0; else if (u > 1.0) u = 1.0;
    if (v < 0.0) v = 0.0; else if (v > 1.0) v = 1.0;
    out[0] = u; out[1] = v;
};

// ファーム updateLEDBuffer():436 は IMU quaternion の共役 (逆回転) でサンプリングする。
// spec §5.2 の擬似コードは q をそのまま使う形で書かれているため、ここを実機照合で
// 反転できるよう定数化する (静止時 = 恒等回転では両者は同一で、差は実機回転時のみ)。
const CONJUGATE_SAMPLING = true;

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
// (デジタルツイン)。ファーム描画パスの逐語移植。spec §5.2。
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
        let qw = q.w, qx = q.x, qy = q.y, qz = q.z;
        if (CONJUGATE_SAMPLING) { qx = -qx; qy = -qy; qz = -qz; }

        const pos = layout.positions;
        const { data, w, h } = frame;
        const bscale = (brightnessRef.current / 100) / 255;   // 0..255 → 0..1 + brightness
        const rot = [0, 0, 0];
        const uv = [0, 0];
        for (let i = 0; i < layout.count; i++) {
            rotateByQuaternion(pos[i * 3], pos[i * 3 + 1], pos[i * 3 + 2], qw, qx, qy, qz, rot);
            sphereToUV(rot[0], rot[1], rot[2], uv);
            const px = Math.trunc(uv[0] * (w - 1));   // (uint16) 量子化 = 正数の trunc
            const py = Math.trunc(uv[1] * (h - 1));
            const idx = (py * w + px) * 4;
            colors[i * 3] = data[idx] * bscale;
            colors[i * 3 + 1] = data[idx + 1] * bscale;
            colors[i * 3 + 2] = data[idx + 2] * bscale;
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

const LedPointCloud = ({ hue, brightness, colorMode = 'params', quaternionRef }) => {
    const geomRef = useRef();
    const [layout, setLayout] = useState(_layoutCache);

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

    // 頂点属性を layout / effectiveMode に応じて構築
    useEffect(() => {
        if (!layout || !geomRef.current) return;
        const geom = geomRef.current;
        geom.setAttribute('position', new THREE.BufferAttribute(layout.positions, 3));
        if (effectiveMode === 'strip') {
            geom.setAttribute('color', new THREE.BufferAttribute(buildStripColors(layout.strip), 3));
        } else if (effectiveMode === 'live' && live.colors) {
            geom.setAttribute('color', new THREE.BufferAttribute(live.colors, 3));
        } else {
            geom.deleteAttribute('color');
        }
        geom.attributes.position.needsUpdate = true;
    }, [layout, effectiveMode, live.colors]);

    if (!layout) return null;

    const useVertexColors = effectiveMode === 'strip' || effectiveMode === 'live';
    const sphereColor = hueToRgb(hue);
    const opacity = effectiveMode === 'live' ? 0.9 : 0.35 + 0.65 * (brightness / 100);

    return (
        <points>
            {/* effectiveMode を key に含め、vertexColors 切替時にマテリアルを作り直す */}
            <bufferGeometry ref={geomRef} />
            <pointsMaterial
                key={effectiveMode}
                map={dotTexture()}
                color={useVertexColors ? '#ffffff' : sphereColor}
                vertexColors={useVertexColors}
                size={0.05}
                sizeAttenuation
                transparent
                depthWrite={false}
                blending={THREE.AdditiveBlending}
                opacity={opacity}
            />
        </points>
    );
};

const IMUControlledSphere = ({ hue, brightness, colorMode }) => {
    const groupRef = useRef();
    const quaternionRef = useRef(new THREE.Quaternion(0, 0, 0, 1)); // x, y, z, w

    // Safe WebSocket usage with error handling
    let lastMessage = null;
    try {
        const wsContext = useWebSocket();
        lastMessage = wsContext?.lastMessage;
    } catch (error) {
        console.warn('WebSocket context not available:', error);
    }

    useEffect(() => {
        // Update quaternion when IMU data received
        if (lastMessage && lastMessage.type === 'STATE_UPDATE' && lastMessage.payload?.imu) {
            const { w, x, y, z } = lastMessage.payload.imu;
            // Three.js uses (x, y, z, w) order
            quaternionRef.current.set(x, y, z, w);
        }
    }, [lastMessage]);

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

    return (
        <group ref={groupRef} scale={2.2}>
            {/* 骨格として薄く残すワイヤーフレーム球。LED 点群を主役にする。 */}
            <Sphere args={[1, 32, 32]}>
                <meshStandardMaterial
                    color={sphereColor}
                    wireframe={true}
                    emissive={sphereColor}
                    emissiveIntensity={emissiveIntensity}
                    transparent
                    opacity={0.12}
                />
            </Sphere>
            <LedPointCloud
                hue={hue}
                brightness={brightness}
                colorMode={colorMode}
                quaternionRef={quaternionRef}
            />
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
