import React, { useEffect, useRef, useState } from 'react';
import { Box, Typography, Button, FormControlLabel, Switch } from '@mui/material';
import { useWebSocket, useLogs } from '../../contexts/WebSocketContext';

/**
 * デバイスのデバッグログ (MQTT topic sphere/sphere001/log) を表示するパネル。
 * USB が届かない封止状態でも、WebSocket 経由でリアルタイムにログを確認できる。
 */
export const LogPanel = () => {
    const { logs, clearLogs } = useLogs();
    const { isConnected } = useWebSocket();
    const [autoScroll, setAutoScroll] = useState(true);
    const endRef = useRef(null);

    useEffect(() => {
        if (autoScroll && endRef.current) {
            endRef.current.scrollIntoView({ behavior: 'auto' });
        }
    }, [logs, autoScroll]);

    const fmtTime = (ts) => {
        const d = new Date(ts);
        return d.toLocaleTimeString('ja-JP', { hour12: false }) +
            '.' + String(d.getMilliseconds()).padStart(3, '0');
    };

    return (
        <Box sx={{ display: 'flex', flexDirection: 'column', height: '100%', p: 1 }}>
            {/* ツールバー */}
            <Box sx={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', mb: 1 }}>
                <Typography variant="subtitle2" sx={{ color: 'primary.main', letterSpacing: '0.1em' }}>
                    DEVICE LOG {isConnected ? '' : '(disconnected)'} · {logs.length}
                </Typography>
                <Box sx={{ display: 'flex', alignItems: 'center', gap: 1 }}>
                    <FormControlLabel
                        control={<Switch size="small" checked={autoScroll} onChange={(e) => setAutoScroll(e.target.checked)} />}
                        label={<Typography variant="caption">Auto-scroll</Typography>}
                        sx={{ mr: 0 }}
                    />
                    <Button size="small" variant="outlined" onClick={clearLogs}>Clear</Button>
                </Box>
            </Box>

            {/* ログ表示領域 */}
            <Box
                sx={{
                    flex: 1,
                    minHeight: 0,
                    overflowY: 'auto',
                    bgcolor: 'rgba(0, 0, 0, 0.6)',
                    border: '1px solid',
                    borderColor: 'rgba(0, 229, 255, 0.25)',
                    borderRadius: 1,
                    p: 1,
                    fontFamily: 'monospace',
                    fontSize: '0.72rem',
                    lineHeight: 1.5,
                    whiteSpace: 'pre-wrap',
                    wordBreak: 'break-word',
                }}
            >
                {logs.length === 0 ? (
                    <Typography variant="caption" sx={{ color: 'text.secondary' }}>
                        ログ待機中… (デバイスが sphere/sphere001/log に publish するとここに表示されます)
                    </Typography>
                ) : (
                    logs.map((entry, i) => (
                        <Box key={i} sx={{ color: 'grey.300' }}>
                            <Box component="span" sx={{ color: 'primary.dark', mr: 1 }}>
                                {fmtTime(entry.ts)}
                            </Box>
                            {entry.line}
                        </Box>
                    ))
                )}
                <div ref={endRef} />
            </Box>
        </Box>
    );
};
