import React from 'react';
import { motion } from 'framer-motion';

/**
 * GlassButton — pill 型 / icon 型のグラスボタン。
 * variant: 'icon' (正円 44px タッチターゲット) | 'pill'
 * active: アクセント色のアクティブ表示 (ループトグル等)
 */
export const GlassButton = ({
    variant = 'icon',
    active = false,
    disabled = false,
    title,
    onClick,
    style,
    children,
}) => (
    <motion.button
        type="button"
        className="glass"
        title={title}
        aria-label={title}
        aria-pressed={active || undefined}
        disabled={disabled}
        onClick={onClick}
        whileTap={disabled ? undefined : { scale: 0.96 }}
        style={{
            display: 'inline-flex',
            alignItems: 'center',
            justifyContent: 'center',
            gap: 8,
            minWidth: 44,
            minHeight: 44,
            padding: variant === 'pill' ? '0 18px' : 0,
            borderRadius: 'var(--radius-pill)',
            color: active ? 'var(--accent)' : 'var(--tx-1)',
            borderColor: active ? 'var(--accent-soft)' : 'var(--glass-stroke)',
            background: active ? 'var(--accent-soft)' : 'var(--glass-bg)',
            opacity: disabled ? 0.35 : 1,
            cursor: disabled ? 'default' : 'pointer',
            touchAction: 'manipulation',
            ...style,
        }}
    >
        {children}
    </motion.button>
);
