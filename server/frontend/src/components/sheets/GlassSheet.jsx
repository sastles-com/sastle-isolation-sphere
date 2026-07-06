import React, {
    forwardRef, useCallback, useEffect, useImperativeHandle, useMemo, useRef, useState,
} from 'react';
import { motion, useMotionValue, useTransform, animate } from 'framer-motion';

// 仕様 §3.3: シートはオーバーシュートなしの spring
const SPRING = { type: 'spring', stiffness: 400, damping: 40 };
// full デテント時に残す画面端の余白 (safe-area ぶんの逃げ)
const FULL_INSET = 48;
// オーバードラッグ時に地が見えないよう余分に描画する高さ
const OVERDRAG_SLACK = 80;

/**
 * GlassSheet — 汎用シート (bottom/top、デテント、drag 追従、スナップ、scrim)。
 *
 * props:
 * - side: 'bottom' | 'top'
 * - detents: 開時の画面高さ比の配列 (昇順)。例 [0.55, 1]
 * - peek: 閉状態で見せるハンドル高さ(px)
 * - open: null(閉) | detents の index — 制御された開閉状態
 * - onOpenChange(next): スナップ/scrim タップ/ハンドルで状態が変わる時に呼ぶ
 * - zIndex: scrim がこの値、シート本体が +1
 *
 * ref (imperative): STAGE 上のパンをシートに転送して物理的に追従させるための
 * panStart() / panMove(dy) / panEnd(velocityY)
 */
export const GlassSheet = forwardRef(function GlassSheet(
    { side = 'bottom', detents = [1], peek = 0, open = null, onOpenChange, zIndex = 20, children },
    ref
) {
    const [viewH, setViewH] = useState(() => window.innerHeight);
    useEffect(() => {
        const onResize = () => setViewH(window.innerHeight);
        window.addEventListener('resize', onResize);
        return () => window.removeEventListener('resize', onResize);
    }, []);

    // bottom は画面下端(top:100dvh)に、top は画面上端の上(bottom:100dvh基準)に
    // アンカーし、y の符号だけで開閉方向を統一する (bottom: 負で開く / top: 正で開く)
    const sign = side === 'bottom' ? -1 : 1;
    const detentPx = useCallback(
        (f) => Math.min(f * viewH, viewH - FULL_INSET),
        [viewH]
    );
    const offsetFor = useCallback(
        (o) => (o === null ? sign * peek : sign * detentPx(detents[o])),
        [sign, peek, detents, detentPx]
    );

    const maxOpenPx = detentPx(detents[detents.length - 1]);
    const renderH = maxOpenPx + OVERDRAG_SLACK;

    const y = useMotionValue(offsetFor(open));

    // 制御 prop の変化に追従 (スナップ後の同値 animate は実質 no-op)
    useEffect(() => {
        const anim = animate(y, offsetFor(open), SPRING);
        return () => anim.stop();
    }, [open, offsetFor, y]);

    // ドラッグ可動域
    const dragConstraints = useMemo(() => {
        const closedY = sign * peek;
        const fullY = sign * maxOpenPx;
        return side === 'bottom'
            ? { top: fullY, bottom: closedY }
            : { top: closedY, bottom: fullY };
    }, [side, sign, peek, maxOpenPx]);

    // 離した位置 + 速度からデテント (または閉) にスナップ
    const snap = useCallback((velocityY) => {
        const candidates = [
            { key: null, off: offsetFor(null) },
            ...detents.map((_, i) => ({ key: i, off: offsetFor(i) })),
        ];
        const projected = y.get() + velocityY * 0.2;
        let best = candidates[0];
        for (const c of candidates) {
            if (Math.abs(c.off - projected) < Math.abs(best.off - projected)) best = c;
        }
        animate(y, best.off, SPRING);
        if (best.key !== open) onOpenChange?.(best.key);
    }, [detents, offsetFor, y, open, onOpenChange]);

    // STAGE からのパン転送 (シートが指に物理的に追従する)
    const panBase = useRef(0);
    useImperativeHandle(ref, () => ({
        panStart: () => { panBase.current = y.get(); },
        panMove: (dy) => {
            const lo = dragConstraints.top;
            const hi = dragConstraints.bottom;
            y.set(Math.min(hi, Math.max(lo, panBase.current + dy)));
        },
        panEnd: (velocityY) => snap(velocityY),
    }), [y, dragConstraints, snap]);

    // scrim: 開き具合に応じてフェード (useTransform の入力レンジは昇順にする)
    const scrimOpacity = useTransform(
        y,
        side === 'bottom'
            ? [sign * maxOpenPx, offsetFor(null)]
            : [offsetFor(null), sign * maxOpenPx],
        side === 'bottom' ? [1, 0] : [0, 1]
    );

    const isOpen = open !== null;
    const handleBar = (
        <div
            role="button"
            tabIndex={0}
            aria-label={isOpen ? 'シートを閉じる' : 'シートを開く'}
            onClick={() => onOpenChange?.(isOpen ? null : 0)}
            onKeyDown={(e) => {
                if (e.key === 'Enter' || e.key === ' ') {
                    e.preventDefault();
                    onOpenChange?.(isOpen ? null : 0);
                }
            }}
            style={{
                display: 'flex', alignItems: 'center', justifyContent: 'center',
                height: Math.max(peek, 28), flexShrink: 0, cursor: 'grab',
            }}
        >
            <div style={{
                width: 44, height: 4, borderRadius: 2,
                background: 'rgb(255 255 255 / .28)',
            }} />
        </div>
    );

    return (
        <>
            <motion.div
                aria-hidden={!isOpen}
                onClick={() => isOpen && onOpenChange?.(null)}
                style={{
                    position: 'fixed', inset: 0, zIndex,
                    background: 'rgb(0 0 0 / .45)',
                    opacity: scrimOpacity,
                    pointerEvents: isOpen ? 'auto' : 'none',
                }}
            />
            <motion.div
                className="glass"
                drag="y"
                dragConstraints={dragConstraints}
                dragElastic={0.04}
                dragMomentum={false}
                onDragEnd={(_, info) => snap(info.velocity.y)}
                style={{
                    position: 'fixed', left: 0, right: 0,
                    maxWidth: 640, marginInline: 'auto', // 広い画面ではステージと同じ中央カラム
                    top: side === 'bottom' ? '100dvh' : undefined,
                    bottom: side === 'top' ? '100dvh' : undefined,
                    height: renderH,
                    y,
                    zIndex: zIndex + 1,
                    display: 'flex',
                    flexDirection: side === 'bottom' ? 'column' : 'column-reverse',
                    justifyContent: 'flex-start',
                    // glass 面ベース + ground-2 の下地 (blur 越しに球体が透ける)
                    background: 'rgb(10 16 32 / .72)',
                    borderRadius: side === 'bottom'
                        ? 'var(--radius) var(--radius) 0 0'
                        : '0 0 var(--radius) var(--radius)',
                    borderWidth: side === 'bottom' ? '1px 0 0 0' : '0 0 1px 0',
                    touchAction: 'none',
                }}
            >
                {handleBar}
                <div style={{
                    flex: 1, minHeight: 0, overflow: 'hidden',
                    display: 'flex', flexDirection: 'column',
                    // オーバードラッグ用スラック領域ぶんを除いた実表示域
                    maxHeight: maxOpenPx - Math.max(peek, 28),
                }}>
                    {children}
                </div>
            </motion.div>
        </>
    );
});
