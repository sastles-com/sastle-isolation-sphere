import React from 'react';

const SECTIONS = ['DEVICE', 'ORIENTATION', 'TUNE', 'PATTERN', 'CONFIG', 'LOGS'];

/**
 * ControlDrawer — システム/IMU/ログのコントロール内容。
 * P1 ではセクション枠のみのプレースホルダ (P4 で現行機能を移植)。
 */
export const ControlDrawer = () => (
    <div style={{
        flex: 1, minHeight: 0, overflowY: 'auto',
        padding: '4px 20px 12px',
        paddingTop: 'max(12px, env(safe-area-inset-top))',
    }}>
        {SECTIONS.map((name) => (
            <section key={name} style={{ marginBottom: 14 }}>
                <div className="ui-label" style={{ marginBottom: 6 }}>{name}</div>
                <div className="glass" style={{
                    borderRadius: 'var(--radius)', padding: 16,
                    color: 'var(--tx-3)', fontSize: 13,
                }}>
                    Phase 4 で移植
                </div>
            </section>
        ))}
    </div>
);
