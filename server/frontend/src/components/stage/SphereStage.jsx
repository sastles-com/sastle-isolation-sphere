import React from 'react';
import { HoloSphere } from '../sphere/HoloSphere';

/**
 * SphereStage — 背景全面の 3D 球体 + hue 追従のオーロラグロー。
 * エッジドラッグ (右端=明るさ / 左端=hue) は P3 で追加。
 */
export const SphereStage = ({ hue, brightness }) => (
    <div style={{ position: 'absolute', inset: 0, zIndex: 0 }}>
        {/* オーロラグロー (球体背後、--sphere-hue 追従) */}
        <div style={{
            position: 'absolute', inset: 0,
            background: 'var(--aurora)',
            pointerEvents: 'none',
        }} />
        <HoloSphere color={hue} brightness={brightness} />
    </div>
);
