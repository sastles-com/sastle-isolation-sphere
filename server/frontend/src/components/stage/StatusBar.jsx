import React from 'react';
import { GlassButton } from '../ui/GlassButton';
import { CoreIndicator } from '../ui/CoreIndicator';
import { IconGear } from '../ui/icons';

/**
 * StatusBar — STAGE 上部の glass pill。
 * 接続状態 ● (緑=WS+core / 黄=WSのみ / 赤=切断)、core ドット、再生状態、fps、temp。
 * ⚙︎ = CONTROL DRAWER を開くボタン。
 *
 * 先頭の ● はブラウザ↔サーバー間 (WebSocket) の状態、
 * core ドットはサーバー↔実機間の状態で、別の経路を表している。
 */
export const StatusBar = ({ isConnected, deviceOnline, playbackStatus, system,
                            spheres, online, target, presenceKnown, onOpenControl }) => {
    const dotColor = !isConnected ? 'var(--err)' : deviceOnline ? 'var(--ok)' : 'var(--warn)';
    const statusText = !isConnected
        ? 'OFFLINE'
        : playbackStatus === 'playing' ? 'STREAMING'
        : playbackStatus === 'paused' ? 'PAUSED'
        : 'IDLE';

    return (
        <div style={{
            display: 'flex', alignItems: 'center', gap: 8,
            padding: '0 12px',
            paddingTop: 'max(12px, env(safe-area-inset-top))',
        }}>
            <div className="glass" style={{
                display: 'flex', alignItems: 'center', gap: 10,
                height: 36, padding: '0 14px',
                borderRadius: 'var(--radius-pill)',
                flex: 1, minWidth: 0,
            }}>
                <span aria-label={`connection: ${statusText}`} style={{
                    width: 8, height: 8, borderRadius: '50%',
                    background: dotColor, flexShrink: 0,
                }} />
                <span className="ui-label" style={{ color: 'var(--tx-2)' }}>{statusText}</span>
                <CoreIndicator spheres={spheres} online={online} target={target}
                    presenceKnown={presenceKnown} compact />
                <span style={{ flex: 1 }} />
                <span className="mono" style={{ fontSize: 12, color: 'var(--tx-3)' }}>
                    {system?.fps != null ? `${system.fps}fps` : '—'}
                </span>
                <span className="mono" style={{ fontSize: 12, color: 'var(--tx-3)' }}>
                    {system?.temp != null ? `${Math.round(system.temp)}°C` : '—'}
                </span>
            </div>
            <GlassButton variant="icon" title="コントロールを開く" onClick={onOpenControl}
                style={{ minWidth: 36, minHeight: 36, width: 36, height: 36 }}>
                <IconGear size={18} />
            </GlassButton>
        </div>
    );
};
