import React from 'react';

// hue バーの縦グラデーション (下=0° → 上=360°)
const HUE_GRADIENT =
    'linear-gradient(to top, ' +
    'hsl(0 85% 60%), hsl(60 85% 60%), hsl(120 85% 60%), hsl(180 85% 60%), ' +
    'hsl(240 85% 60%), hsl(300 85% 60%), hsl(360 85% 60%))';

/**
 * EdgeParamGauge — 右端(brightness)/左端(hue)ドラッグ時の縦ゲージ + 現在値。
 * 表示/フェードは opacity のみで実装 (layout thrash 回避, 仕様 §4.3)。
 */
export const EdgeParamGauge = ({ side, param, value, min = 0, max = 100, unit = '', visible }) => {
    const ratio = Math.min(1, Math.max(0, (value - min) / (max - min)));
    const isHue = param === 'hue';

    return (
        <div
            aria-hidden={!visible}
            style={{
                position: 'fixed', top: '50%', [side]: 16,
                transform: 'translateY(-50%)',
                zIndex: 6, pointerEvents: 'none',
                display: 'flex', flexDirection: 'column', alignItems: 'center', gap: 10,
                opacity: visible ? 1 : 0,
                transition: 'opacity .35s ease',
            }}
        >
            <div className="glass mono" style={{
                padding: '4px 10px', borderRadius: 8, fontSize: 13, color: 'var(--tx-1)',
            }}>
                {Math.round(value)}{unit}
            </div>

            {/* トラック + ノブ (ノブがクリップされないよう relative 枠は幅広に) */}
            <div style={{
                position: 'relative', width: 18, height: '42vh',
                display: 'flex', justifyContent: 'center',
            }}>
                <div style={{
                    width: 8, height: '100%', borderRadius: 999, overflow: 'hidden',
                    background: isHue ? HUE_GRADIENT : 'rgb(255 255 255 / .12)',
                }}>
                    {!isHue && (
                        <div style={{
                            position: 'absolute', bottom: 0, left: 5, right: 5,
                            height: `${ratio * 100}%`, borderRadius: 999,
                            background: 'var(--accent)',
                        }} />
                    )}
                </div>
                {/* ノブ */}
                <div style={{
                    position: 'absolute', bottom: `calc(${ratio * 100}% - 8px)`,
                    left: '50%', transform: 'translateX(-50%)',
                    width: 16, height: 16, borderRadius: '50%',
                    background: isHue ? `hsl(${value} 85% 62%)` : 'var(--tx-1)',
                    border: '2px solid rgb(255 255 255 / .55)',
                }} />
            </div>

            <div className="ui-label">{isHue ? 'HUE' : 'BRT'}</div>
        </div>
    );
};
