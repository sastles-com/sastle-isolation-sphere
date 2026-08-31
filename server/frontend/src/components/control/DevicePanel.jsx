import React, { useCallback, useEffect, useState } from 'react';
import { useStateUpdate } from '../../hooks/useSphereState';
import { useCorePresence } from '../../hooks/useCorePresence';
import { useWebSocket } from '../../contexts/WebSocketContext';
import { apiPut } from '../../lib/api';
import { TARGET_ALL } from '../../lib/selectDeviceImu';
import { CoreIndicator } from '../ui/CoreIndicator';
import { GlassButton } from '../ui/GlassButton';
import { SegmentControl } from '../ui/SegmentControl';
import { Section } from './Section';

// "sphere001" → "#001" (セグメント幅に収める)。想定外の id はそのまま出す
const shortLabel = (id) => {
    const m = /^sphere0*(\d+)$/i.exec(id);
    return m ? `#${m[1].padStart(3, '0')}` : id;
};

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
 *
 * 操作対象 core の切替もここで行う。選択肢は config.json の spheres[] (= STATE_UPDATE
 * の spheres。オフラインの core も含む) で、今応答している core は online (サーバーの
 * 死活判定) として区別する。切替は 2 系統に効く:
 *   - WebSocket 経由のコマンド: sendMessage が device を載せる (WebSocketContext)
 *   - サーバー保持の操作対象:   PUT /api/config/spheres/active で MQTT/UDP の宛先ごと切替
 * 再起動等の system コマンドは将来枠 (仕様 §2.3)。
 */
export const DevicePanel = () => {
    const [system, setSystem] = useState(null);
    const [playbackStatus, setPlaybackStatus] = useState('stopped');
    const { selectedDeviceId, setSelectedDeviceId } = useWebSocket();

    // spheres = config 登録済み (オフライン含む) / online = 今応答している core
    const { spheres, online: onlineIds, target: serverTarget, presenceKnown } = useCorePresence();

    useStateUpdate((payload) => {
        if (payload.system) setSystem(payload.system);
        if (payload.playback?.status) setPlaybackStatus(payload.playback.status);
    });

    // 選択肢: config の spheres[] を優先。未登録なら実際に見えている core で代替する
    const sphereIds = spheres.map((s) => s.id).filter(Boolean);
    const ids = sphereIds.length > 0 ? sphereIds : onlineIds;

    // まだ選んでいなければ、サーバー保持の操作対象 (config の active_sphere) に合わせる。
    // deps には ids ではなく join した文字列を渡す: ids は毎レンダー作り直される
    // 配列なので、identity で比較すると毎回このエフェクトが再実行される。
    const idsKey = ids.join(',');
    useEffect(() => {
        if (selectedDeviceId) return;
        const list = idsKey ? idsKey.split(',') : [];
        const initial = (serverTarget && (serverTarget === TARGET_ALL || list.includes(serverTarget)))
            ? serverTarget
            : list[0];
        if (initial) setSelectedDeviceId(initial);
    }, [idsKey, serverTarget, selectedDeviceId, setSelectedDeviceId]);

    // 操作対象の切替。WS コマンドは即座に新しい device で飛ぶが、MQTT/UDP の
    // サーバー側宛先も合わせる必要があるため API も叩く (config.json に永続化)。
    const changeTarget = useCallback(async (id) => {
        setSelectedDeviceId(id);
        try {
            const r = await apiPut('/api/config/spheres/active', { id });
            if (!r.ok) console.warn('target switch failed:', await r.text());
        } catch (e) {
            console.warn('target switch failed:', e);
        }
    }, [setSelectedDeviceId]);

    const options = [
        ...ids.map((id) => ({
            value: id,
            label: onlineIds.includes(id) ? shortLabel(id) : `${shortLabel(id)}·off`,
        })),
        // 2台以上登録されているときだけ「全 core」を選べるようにする
        ...(ids.length > 1 ? [{ value: TARGET_ALL, label: 'ALL' }] : []),
    ];
    const current = selectedDeviceId ?? serverTarget ?? ids[0];

    return (
        <Section title="Device">
            {options.length > 1 && (
                <div style={{ marginBottom: 10 }}>
                    <SegmentControl options={options} value={current} onChange={changeTarget} />
                </div>
            )}
            <Row k="Target" v={current ?? '—'} />
            <Row
                k="Cores"
                v={
                    <span style={{ display: 'inline-flex', alignItems: 'center', gap: 8 }}>
                        <CoreIndicator spheres={spheres} online={onlineIds} target={current}
                            presenceKnown={presenceKnown} />
                        <span>
                            {!presenceKnown ? 'unknown'
                                : onlineIds.length > 0 ? onlineIds.join(', ') : 'none'}
                        </span>
                    </span>
                }
            />
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
