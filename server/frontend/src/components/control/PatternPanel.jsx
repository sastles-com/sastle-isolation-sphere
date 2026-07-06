import React, { useState } from 'react';
import { useWebSocket } from '../../contexts/WebSocketContext';
import { useStateUpdate } from '../../hooks/useSphereState';
import { GlassButton } from '../ui/GlassButton';
import { Section } from './Section';

// LED モード (仕様 §5 SET_LED の契約: sphere | pixels | off)
const MODES = [
    { value: 'sphere', label: 'SPHERE' },
    { value: 'off', label: 'OFF' },
];

/**
 * PatternPanel — PATTERN セクション。LED モード切替 (SET_LED)。
 * pixels 単位エディタはスコープ外 (仕様 §7) のため sphere/off のみ。
 */
export const PatternPanel = () => {
    const { sendMessage } = useWebSocket();
    const [mode, setMode] = useState('sphere');

    useStateUpdate((payload) => {
        if (payload.led && typeof payload.led.mode === 'string') setMode(payload.led.mode);
    });

    const selectMode = (m) => {
        setMode(m);
        sendMessage('SET_LED', { mode: m });
    };

    return (
        <Section title="Pattern">
            <div style={{ display: 'flex', gap: 10 }}>
                {MODES.map((m) => (
                    <GlassButton
                        key={m.value}
                        variant="pill"
                        active={mode === m.value}
                        title={m.label}
                        onClick={() => selectMode(m.value)}
                        style={{ flex: 1, fontSize: 13 }}
                    >
                        {m.label}
                    </GlassButton>
                ))}
            </div>
        </Section>
    );
};
