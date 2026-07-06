import React, { useEffect, useRef, useState } from 'react';
import { createPortal } from 'react-dom';
import { motion } from 'framer-motion';
import { useLogs, useWebSocket } from '../../contexts/WebSocketContext';
import { GlassButton } from '../ui/GlassButton';
import { GlassToggle } from '../ui/GlassToggle';
import { IconChevronDown } from '../ui/icons';

const fmtTime = (ts) => {
    const d = new Date(ts);
    return d.toLocaleTimeString('ja-JP', { hour12: false }) +
        '.' + String(d.getMilliseconds()).padStart(3, '0');
};

/**
 * LogViewer — フルスクリーンのデバイスログビューア。
 * 等幅フォント、自動追従スクロール (一時停止可)、クリア (仕様 §2.3 LOGS)。
 */
export const LogViewer = ({ onClose }) => {
    const { logs, clearLogs } = useLogs();
    const { isConnected } = useWebSocket();
    const [follow, setFollow] = useState(true);
    const endRef = useRef(null);

    useEffect(() => {
        if (follow && endRef.current) endRef.current.scrollIntoView({ behavior: 'auto' });
    }, [logs, follow]);

    // 親の GlassSheet は transform(y) を持つため position:fixed の基準が
    // ビューポートにならない。フルスクリーン化するには body 直下へ portal する。
    return createPortal(
        <motion.div
            initial={{ opacity: 0 }} animate={{ opacity: 1 }} exit={{ opacity: 0 }}
            style={{
                position: 'fixed', inset: 0, zIndex: 50,
                background: 'var(--ground)',
                display: 'flex', flexDirection: 'column',
                paddingTop: 'env(safe-area-inset-top)',
                paddingBottom: 'env(safe-area-inset-bottom)',
            }}
        >
            {/* ツールバー */}
            <div style={{
                display: 'flex', alignItems: 'center', gap: 12,
                padding: '10px 16px', flexShrink: 0,
                borderBottom: '1px solid var(--glass-stroke)',
            }}>
                <GlassButton title="閉じる" onClick={onClose}
                    style={{ minWidth: 36, minHeight: 36, width: 36, height: 36 }}>
                    <IconChevronDown size={18} />
                </GlassButton>
                <span className="ui-label" style={{ flex: 1 }}>
                    Device Log · {logs.length}{isConnected ? '' : ' (disconnected)'}
                </span>
                <div style={{ width: 130 }}>
                    <GlassToggle checked={follow} onChange={setFollow} label="Follow" />
                </div>
                <GlassButton variant="pill" title="クリア" onClick={clearLogs}
                    style={{ minHeight: 34, height: 34, fontSize: 12 }}>
                    CLEAR
                </GlassButton>
            </div>

            {/* ログ本文 */}
            <div
                onWheel={() => setFollow(false)}
                onTouchMove={() => setFollow(false)}
                style={{
                    flex: 1, minHeight: 0, overflowY: 'auto',
                    padding: 12, fontFamily: 'var(--font-mono)', fontSize: 12,
                    lineHeight: 1.5, whiteSpace: 'pre-wrap', wordBreak: 'break-word',
                }}
            >
                {logs.length === 0 ? (
                    <span style={{ color: 'var(--tx-3)' }}>
                        ログ待機中… (デバイスが sphere/&lt;id&gt;/log に publish するとここに表示されます)
                    </span>
                ) : logs.map((e, i) => (
                    <div key={i} style={{ color: 'var(--tx-2)' }}>
                        <span style={{ color: 'var(--accent)', marginRight: 8 }}>{fmtTime(e.ts)}</span>
                        {e.line}
                    </div>
                ))}
                <div ref={endRef} />
            </div>
        </motion.div>,
        document.body
    );
};
