import React from 'react';

/**
 * Section — ControlDrawer 内の 1 セクション枠 (ラベル + glass カード)。
 */
export const Section = ({ title, action, children }) => (
    <section style={{ marginBottom: 14 }}>
        <div style={{
            display: 'flex', alignItems: 'center', justifyContent: 'space-between',
            marginBottom: 6,
        }}>
            <span className="ui-label">{title}</span>
            {action}
        </div>
        <div className="glass" style={{ borderRadius: 'var(--radius)', padding: 14 }}>
            {children}
        </div>
    </section>
);
