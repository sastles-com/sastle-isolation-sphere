import React, { useEffect, useState } from 'react';
import { apiGet, apiPost } from '../../lib/api';
import { GlassButton } from '../ui/GlassButton';
import { GlassToggle } from '../ui/GlassToggle';
import { Section } from './Section';

const API = '/api/config/';

const fieldStyle = {
    width: '100%', boxSizing: 'border-box',
    background: 'rgb(0 0 0 / .3)', color: 'var(--tx-1)',
    border: '1px solid var(--glass-stroke)', borderRadius: 10,
    padding: '8px 10px', fontFamily: 'var(--font-mono)', fontSize: 13,
};

/**
 * ConfigForm — CONFIG セクション。GET /api/config/ を読み、
 * セクションごとに POST /api/config/ {section, data} で保存する (現行 ConfigEditor 移植)。
 */
export const ConfigForm = () => {
    const [config, setConfig] = useState(null);
    const [status, setStatus] = useState('loading'); // loading | ready | error | saving | saved

    useEffect(() => {
        (async () => {
            try {
                const r = await apiGet(API);
                if (r.ok) { setConfig(await r.json()); setStatus('ready'); }
                else setStatus('error');
            } catch { setStatus('error'); }
        })();
    }, []);

    const change = (section, key, value) =>
        setConfig((prev) => ({ ...prev, [section]: { ...prev[section], [key]: value } }));

    const save = async () => {
        setStatus('saving');
        try {
            for (const section of Object.keys(config)) {
                await apiPost(API, { section, data: config[section] });
            }
            setStatus('saved');
            setTimeout(() => setStatus('ready'), 1500);
        } catch { setStatus('error'); }
    };

    if (status === 'loading') return <Section title="Config">読み込み中…</Section>;
    if (!config) return <Section title="Config"><span style={{ color: 'var(--err)' }}>設定の取得に失敗</span></Section>;

    const saveLabel = status === 'saving' ? 'SAVING…' : status === 'saved' ? 'SAVED ✓' : 'SAVE';

    return (
        <Section
            title="Config"
            action={
                <GlassButton variant="pill" title="設定を保存" onClick={save}
                    disabled={status === 'saving'}
                    style={{ minHeight: 30, height: 30, fontSize: 12 }}>
                    {saveLabel}
                </GlassButton>
            }
        >
            <div style={{ display: 'flex', flexDirection: 'column', gap: 14 }}>
                {Object.entries(config).map(([section, data]) => (
                    <div key={section}>
                        <div className="ui-label" style={{ marginBottom: 8 }}>{section}</div>
                        <div style={{ display: 'flex', flexDirection: 'column', gap: 8 }}>
                            {Object.entries(data).map(([key, value]) => {
                                if (value !== null && typeof value === 'object') {
                                    // ネストオブジェクト (例: PID) — サブキーを横並び
                                    return (
                                        <div key={key} style={{ paddingLeft: 10, borderLeft: '2px solid var(--glass-stroke)' }}>
                                            <div className="ui-label" style={{ marginBottom: 4 }}>{key}</div>
                                            <div style={{ display: 'flex', gap: 8 }}>
                                                {Object.entries(value).map(([sk, sv]) => (
                                                    <input key={sk} style={fieldStyle} value={sv}
                                                        onChange={(e) => change(section, key, { ...value, [sk]: e.target.value })}
                                                        placeholder={sk} aria-label={`${section}.${key}.${sk}`} />
                                                ))}
                                            </div>
                                        </div>
                                    );
                                }
                                if (typeof value === 'boolean') {
                                    return (
                                        <GlassToggle key={key} checked={value} label={key}
                                            onChange={(v) => change(section, key, v)} />
                                    );
                                }
                                return (
                                    <label key={key} style={{ display: 'flex', flexDirection: 'column', gap: 4 }}>
                                        <span className="ui-label">{key}</span>
                                        <input style={fieldStyle} value={value}
                                            onChange={(e) => change(section, key, e.target.value)}
                                            aria-label={`${section}.${key}`} />
                                    </label>
                                );
                            })}
                        </div>
                    </div>
                ))}
            </div>
        </Section>
    );
};
