import React, { useState } from 'react';
import { useStateUpdate } from '../../hooks/useSphereState';
import { GlassButton } from '../ui/GlassButton';
import { Section } from './Section';

const Row = ({ k, v }) => (
    <div style={{
        display: 'flex', justifyContent: 'space-between', alignItems: 'baseline',
        padding: '4px 0',
    }}>
        <span className="ui-label">{k}</span>
        <span className="mono" style={{ fontSize: 13, color: 'var(--tx-1)' }}>{v}</span>
    </div>
);

/**
 * DevicePanel — DEVICE セクション。STATE_UPDATE の system/playback から読み取り表示。
 * 再起動等の system コマンドは将来枠 (仕様 §2.3)。
 */
export const DevicePanel = () => {
    const [system, setSystem] = useState(null);
    const [playbackStatus, setPlaybackStatus] = useState('stopped');

    useStateUpdate((payload) => {
        if (payload.system) setSystem(payload.system);
        if (payload.playback?.status) setPlaybackStatus(payload.playback.status);
    });

    return (
        <Section title="Device">
            <Row k="Status" v={playbackStatus} />
            <Row k="FPS" v={system?.fps != null ? system.fps : '—'} />
            <Row k="Temp" v={system?.temp != null ? `${Math.round(system.temp)}°C` : '—'} />
            <Row k="Host" v={window.location.hostname} />
            <div style={{ marginTop: 10 }}>
                <GlassButton variant="pill" disabled title="再起動 (将来対応)"
                    style={{ width: '100%', color: 'var(--err)', borderColor: 'rgb(248 113 113 / .3)' }}>
                    REBOOT (将来対応)
                </GlassButton>
            </div>
        </Section>
    );
};
