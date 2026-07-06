import React from 'react';
import { motion } from 'framer-motion';

/**
 * SegmentControl — 2-3 セグメント、アクセント色のスライドインジケータ。
 * options: [{ value, label }]
 */
export const SegmentControl = ({ options, value, onChange, style }) => {
    const activeIndex = Math.max(0, options.findIndex((o) => o.value === value));
    return (
        <div
            className="glass"
            role="tablist"
            style={{
                position: 'relative',
                display: 'grid',
                gridTemplateColumns: `repeat(${options.length}, 1fr)`,
                padding: 4,
                borderRadius: 'var(--radius-pill)',
                ...style,
            }}
        >
            {/* スライドインジケータ */}
            <motion.div
                aria-hidden="true"
                animate={{ left: `calc(${(activeIndex / options.length) * 100}% + 4px)` }}
                transition={{ type: 'spring', stiffness: 400, damping: 40 }}
                style={{
                    position: 'absolute', top: 4, bottom: 4,
                    width: `calc(${100 / options.length}% - 8px)`,
                    borderRadius: 'var(--radius-pill)',
                    background: 'var(--accent-soft)',
                    border: '1px solid var(--accent)',
                }}
            />
            {options.map((o) => {
                const selected = o.value === value;
                return (
                    <button
                        key={o.value}
                        type="button"
                        role="tab"
                        aria-selected={selected}
                        onClick={() => onChange(o.value)}
                        style={{
                            position: 'relative', zIndex: 1,
                            height: 36, borderRadius: 'var(--radius-pill)',
                            fontFamily: 'var(--font-ui)', fontWeight: 500, fontSize: 13,
                            color: selected ? 'var(--accent)' : 'var(--tx-2)',
                            transition: 'color .2s',
                        }}
                    >
                        {o.label}
                    </button>
                );
            })}
        </div>
    );
};
