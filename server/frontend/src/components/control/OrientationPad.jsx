import React, { useEffect, useRef, useState, useCallback } from 'react';
import { useStateUpdate } from '../../hooks/useSphereState';
import { apiPost } from '../../lib/api';
import { GlassButton } from '../ui/GlassButton';
import { GlassToggle } from '../ui/GlassToggle';
import { Section } from './Section';

const SIZE = 150;
const RADIUS = SIZE / 2;
const KNOB = SIZE * 0.3;
const MAX_DIST = RADIUS - KNOB / 2;

/**
 * OrientationPad — ORIENTATION セクション。
 * XYZ軸表示トグル (POST /api/command/led {axis}) + 姿勢オフセットジョイスティック (glass)。
 * ジョイスティックはレート制御でオフセットを積算 (現行 SphereControl 踏襲)。
 */
export const OrientationPad = () => {
    const [axisOn, setAxisOn] = useState(false);
    const [offset, setOffset] = useState({ pitch: 0, roll: 0 });
    const vector = useRef({ x: 0, y: 0 });
    const raf = useRef();
    const padRef = useRef(null);
    const [knob, setKnob] = useState({ x: 0, y: 0 });
    const [dragging, setDragging] = useState(false);

    useStateUpdate((payload) => {
        if (payload.led && typeof payload.led.axis === 'boolean') setAxisOn(payload.led.axis);
    });

    const handleAxisToggle = async (on) => {
        setAxisOn(on);
        try { await apiPost('/api/command/led', { axis: on }); }
        catch (e) { console.error('axis toggle failed:', e); }
    };

    // レート制御ループ: スティックの倒し量に比例してオフセットを積算
    useEffect(() => {
        const tick = () => {
            const { x, y } = vector.current;
            if (Math.abs(x) > 0.01 || Math.abs(y) > 0.01) {
                setOffset((p) => ({
                    pitch: Number((p.pitch - y * 0.5).toFixed(1)),
                    roll: Number((p.roll + x * 0.5).toFixed(1)),
                }));
            }
            raf.current = requestAnimationFrame(tick);
        };
        raf.current = requestAnimationFrame(tick);
        return () => cancelAnimationFrame(raf.current);
    }, []);

    const updateFromPointer = useCallback((clientX, clientY) => {
        const rect = padRef.current.getBoundingClientRect();
        let dx = clientX - (rect.left + RADIUS);
        let dy = clientY - (rect.top + RADIUS);
        const dist = Math.hypot(dx, dy);
        if (dist > MAX_DIST) { dx *= MAX_DIST / dist; dy *= MAX_DIST / dist; }
        setKnob({ x: dx, y: dy });
        vector.current = { x: dx / MAX_DIST, y: dy / MAX_DIST };
    }, []);

    const onDown = (e) => {
        e.currentTarget.setPointerCapture(e.pointerId);
        setDragging(true);
        updateFromPointer(e.clientX, e.clientY);
    };
    const onMove = (e) => { if (dragging) updateFromPointer(e.clientX, e.clientY); };
    const onUp = () => { setDragging(false); setKnob({ x: 0, y: 0 }); vector.current = { x: 0, y: 0 }; };

    return (
        <Section title="Orientation">
            <GlassToggle
                checked={axisOn}
                onChange={handleAxisToggle}
                label="XYZ軸表示 (X/Y/Z = R/G/B)"
            />

            <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', gap: 14, marginTop: 12 }}>
                <div
                    ref={padRef}
                    onPointerDown={onDown}
                    onPointerMove={onMove}
                    onPointerUp={onUp}
                    onPointerCancel={onUp}
                    style={{
                        position: 'relative', width: SIZE, height: SIZE, borderRadius: '50%',
                        background: 'rgb(0 0 0 / .3)', border: '1px solid var(--glass-stroke)',
                        touchAction: 'none', cursor: 'pointer',
                    }}
                >
                    <span style={{
                        position: 'absolute', top: '50%', left: '50%', width: 4, height: 4,
                        borderRadius: '50%', background: 'var(--tx-3)', transform: 'translate(-50%,-50%)',
                    }} />
                    <span style={{
                        position: 'absolute', top: '50%', left: '50%',
                        width: KNOB, height: KNOB, borderRadius: '50%',
                        background: 'var(--accent)',
                        transform: `translate(calc(-50% + ${knob.x}px), calc(-50% + ${knob.y}px))`,
                        transition: dragging ? 'none' : 'transform .2s cubic-bezier(0.175,0.885,0.32,1.275)',
                        pointerEvents: 'none',
                    }} />
                </div>

                <div style={{ display: 'flex', gap: 32 }}>
                    <div style={{ textAlign: 'center' }}>
                        <div className="ui-label">Pitch</div>
                        <div className="mono" style={{ fontSize: 18, color: 'var(--accent)' }}>
                            {offset.pitch > 0 ? '+' : ''}{offset.pitch}°
                        </div>
                    </div>
                    <div style={{ textAlign: 'center' }}>
                        <div className="ui-label">Roll</div>
                        <div className="mono" style={{ fontSize: 18, color: 'var(--accent)' }}>
                            {offset.roll > 0 ? '+' : ''}{offset.roll}°
                        </div>
                    </div>
                </div>

                <GlassButton variant="pill" title="オフセットをリセット"
                    onClick={() => setOffset({ pitch: 0, roll: 0 })}
                    style={{ width: '100%', fontSize: 13 }}>
                    RESET OFFSET
                </GlassButton>
            </div>
        </Section>
    );
};
