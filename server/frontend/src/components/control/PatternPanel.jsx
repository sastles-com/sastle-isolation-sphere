import React, { useState } from 'react';
import { useWebSocket } from '../../contexts/WebSocketContext';
import { useStateUpdate } from '../../hooks/useSphereState';
import { GlassButton } from '../ui/GlassButton';
import { Section } from './Section';

// LED モード (仕様 §5 SET_LED の契約: sphere | pixels | off | test)。
// test は点灯/配線確認用: strip index から core 内部で生成し IMU/画像を通さない。
// XYZ は映像表示のまま ±X/±Y/±Z 軸マーカー (R/G/B) をオーバーレイする。
// 他のパターンを選ぶと axis:false でオーバーレイを消す。
// key は UI 選択状態の識別子で、SET_LED に送る payload を持つ。
const OPTIONS = [
    { key: 'sphere', label: 'SPHERE', payload: { mode: 'sphere', axis: false } },
    { key: 'off', label: 'OFF', payload: { mode: 'off', axis: false } },
    { key: 'strip_id', label: 'STRIP', payload: { mode: 'test', pattern: 'strip_id', axis: false } },
    { key: 'chase', label: 'CHASE', payload: { mode: 'test', pattern: 'chase', width: 5, axis: false } },
    { key: 'xyz', label: 'XYZ', payload: { mode: 'sphere', axis: true } },
];

/**
 * PatternPanel — PATTERN セクション。LED モード切替 (SET_LED)。
 * sphere/off に加え、点灯確認用の test パターン (STRIP=識別色ベタ塗り /
 * CHASE=5連LEDが移動) を選べる。pixels 単位エディタはスコープ外 (仕様 §7)。
 */
export const PatternPanel = () => {
    const { sendMessage } = useWebSocket();
    const [sel, setSel] = useState('sphere');

    useStateUpdate((payload) => {
        // デバイスが報告する led.mode は sphere/off/test/pixels。test の場合は
        // パターンまで判別できないので、ローカル選択を尊重して上書きしない。
        // sphere + axis:true は XYZ (軸オーバーレイ) として表示する。
        const m = payload.led?.mode;
        if (m === 'sphere') setSel(payload.led?.axis ? 'xyz' : 'sphere');
        else if (m === 'off') setSel(m);
    });

    const select = (opt) => {
        setSel(opt.key);
        sendMessage('SET_LED', opt.payload);
    };

    return (
        <Section title="Pattern">
            <div style={{ display: 'flex', gap: 10, flexWrap: 'wrap' }}>
                {OPTIONS.map((opt) => (
                    <GlassButton
                        key={opt.key}
                        variant="pill"
                        active={sel === opt.key}
                        title={opt.label}
                        onClick={() => select(opt)}
                        style={{ flex: '1 1 40%', fontSize: 13 }}
                    >
                        {opt.label}
                    </GlassButton>
                ))}
            </div>
        </Section>
    );
};
