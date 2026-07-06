import React, { useRef, useState, useCallback } from 'react';

/**
 * GlassSlider — 横型スライダー。
 * - 44px タッチターゲット (視覚トラックは細く、当たり判定は太く)
 * - ドラッグ中は値ツールチップ表示
 * - role="slider" + 矢印キー操作
 * onChange はドラッグ中連続で呼ばれる (デバウンスは呼び出し側の責務)。
 */
export const GlassSlider = ({
    value,
    min = 0,
    max = 100,
    step = 1,
    label,
    unit = '',
    onChange,
    onDragStateChange,
    style,
}) => {
    const trackRef = useRef(null);
    const [dragging, setDragging] = useState(false);
    const ratio = Math.min(1, Math.max(0, (value - min) / (max - min)));

    const setDragState = useCallback((d) => {
        setDragging(d);
        onDragStateChange?.(d);
    }, [onDragStateChange]);

    const valueFromPointer = useCallback((clientX) => {
        const rect = trackRef.current.getBoundingClientRect();
        const r = Math.min(1, Math.max(0, (clientX - rect.left) / rect.width));
        const raw = min + r * (max - min);
        return Math.round(raw / step) * step;
    }, [min, max, step]);

    const handlePointerDown = (e) => {
        e.currentTarget.setPointerCapture(e.pointerId);
        setDragState(true);
        onChange?.(valueFromPointer(e.clientX));
    };
    const handlePointerMove = (e) => {
        if (dragging) onChange?.(valueFromPointer(e.clientX));
    };
    const handlePointerUp = () => setDragState(false);

    const handleKeyDown = (e) => {
        let next = null;
        if (e.key === 'ArrowRight' || e.key === 'ArrowUp') next = Math.min(max, value + step);
        if (e.key === 'ArrowLeft' || e.key === 'ArrowDown') next = Math.max(min, value - step);
        if (e.key === 'Home') next = min;
        if (e.key === 'End') next = max;
        if (next !== null) {
            e.preventDefault();
            onChange?.(next);
        }
    };

    return (
        <div
            style={{
                display: 'flex', alignItems: 'center', gap: 10,
                minHeight: 44, ...style,
            }}
        >
            {label && <span className="ui-label" style={{ flexShrink: 0 }}>{label}</span>}
            <div
                ref={trackRef}
                role="slider"
                tabIndex={0}
                aria-label={label}
                aria-valuemin={min}
                aria-valuemax={max}
                aria-valuenow={value}
                onPointerDown={handlePointerDown}
                onPointerMove={handlePointerMove}
                onPointerUp={handlePointerUp}
                onPointerCancel={handlePointerUp}
                onKeyDown={handleKeyDown}
                style={{
                    position: 'relative', flex: 1,
                    // 44px の当たり判定の中に 4px の視覚トラックを置く
                    height: 44, display: 'flex', alignItems: 'center',
                    touchAction: 'none', cursor: 'pointer',
                }}
            >
                <div style={{
                    position: 'relative', width: '100%', height: 4,
                    borderRadius: 2, background: 'rgb(255 255 255 / .12)',
                }}>
                    <div style={{
                        position: 'absolute', inset: 0, width: `${ratio * 100}%`,
                        borderRadius: 2, background: 'var(--accent)',
                    }} />
                    <div style={{
                        position: 'absolute', top: '50%', left: `${ratio * 100}%`,
                        transform: 'translate(-50%, -50%)',
                        width: 14, height: 14, borderRadius: '50%',
                        background: 'var(--tx-1)',
                        border: '1px solid var(--glass-stroke)',
                    }} />
                    {/* ドラッグ中の値ツールチップ */}
                    <div className="glass mono" style={{
                        position: 'absolute', bottom: 18, left: `${ratio * 100}%`,
                        transform: 'translateX(-50%)',
                        padding: '2px 8px', borderRadius: 8, fontSize: 12,
                        color: 'var(--tx-1)', whiteSpace: 'nowrap',
                        opacity: dragging ? 1 : 0,
                        transition: 'opacity .15s',
                        pointerEvents: 'none',
                    }}>
                        {value}{unit}
                    </div>
                </div>
            </div>
        </div>
    );
};
