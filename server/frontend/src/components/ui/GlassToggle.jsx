import React from 'react';
import { motion } from 'framer-motion';

/**
 * GlassToggle — オン/オフのスイッチ。オンでアクセント色。
 */
export const GlassToggle = ({ checked, onChange, label, disabled = false }) => (
    <label style={{
        display: 'flex', alignItems: 'center', justifyContent: 'space-between',
        gap: 12, minHeight: 44, cursor: disabled ? 'default' : 'pointer',
        opacity: disabled ? 0.4 : 1,
    }}>
        {label && <span style={{ fontSize: 14, color: 'var(--tx-1)' }}>{label}</span>}
        <button
            type="button"
            role="switch"
            aria-checked={checked}
            aria-label={typeof label === 'string' ? label : undefined}
            disabled={disabled}
            onClick={() => onChange(!checked)}
            style={{
                position: 'relative', flexShrink: 0,
                width: 46, height: 28, borderRadius: 'var(--radius-pill)',
                background: checked ? 'var(--accent-soft)' : 'rgb(255 255 255 / .1)',
                border: `1px solid ${checked ? 'var(--accent)' : 'var(--glass-stroke)'}`,
                transition: 'background .2s, border-color .2s',
            }}
        >
            <motion.span
                animate={{ x: checked ? 20 : 2 }}
                transition={{ type: 'spring', stiffness: 500, damping: 32 }}
                style={{
                    position: 'absolute', top: 2, left: 0,
                    width: 22, height: 22, borderRadius: '50%',
                    background: checked ? 'var(--accent)' : 'var(--tx-2)',
                }}
            />
        </button>
    </label>
);
