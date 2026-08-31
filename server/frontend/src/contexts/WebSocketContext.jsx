import React, { createContext, useContext, useEffect, useState, useRef, useCallback, useMemo } from 'react';

const WebSocketContext = createContext(null);
// ログは高頻度更新のため別コンテキストに分離。useWebSocket() の購読者(Dashboard等)が
// ログ受信のたびに再レンダーされるのを防ぐ(LogPanel だけが useLogs() で購読する)。
const LogContext = createContext(null);

// 開発サーバー(vite等)でポートが取れない場合のフォールバック先
const DEFAULT_WS_PORT = '9000';

export const WebSocketProvider = ({ children }) => {
    const [isConnected, setIsConnected] = useState(false);
    const [lastMessage, setLastMessage] = useState(null);
    const [logs, setLogs] = useState([]);
    // WebUI で操作/表示対象として選んでいる sphere の device_id。
    // "all" = 全 core にブロードキャスト。null は未選択 (サーバー側の既定対象に従う)。
    const [selectedDeviceId, setSelectedDeviceId] = useState(null);
    // sendMessage の identity を変えずに最新の選択値を読むための ref
    // (sendMessage を再生成すると、これを deps に持つフック側で送信処理が作り直される)
    const selectedDeviceRef = useRef(null);
    const ws = useRef(null);
    const reconnectTimeout = useRef(null);
    // FRAME_PREVIEW は 5fps の base64 JPEG。React state に載せると購読者が毎フレーム
    // 再レンダーされるため、LOG_LINE と同様に別経路 (ref ベースの購読) で流す。
    const frameSubscribers = useRef(new Set());

    const MAX_LOG_LINES = 500; // 古い行は破棄してメモリ肥大を防ぐ
    const clearLogs = useCallback(() => setLogs([]), []);

    const connect = useCallback(() => {
        // Use hostname from window.location to support mobile testing on LAN
        const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
        const host = window.location.hostname;
        const port = window.location.port || DEFAULT_WS_PORT; // Use same port as current page
        const url = `${protocol}//${host}:${port}/ws`;

        console.log(`Connecting to WebSocket: ${url}`);
        ws.current = new WebSocket(url);

        ws.current.onopen = () => {
            console.log('WebSocket Connected');
            setIsConnected(true);
            if (reconnectTimeout.current) {
                clearTimeout(reconnectTimeout.current);
                reconnectTimeout.current = null;
            }
        };

        ws.current.onmessage = (event) => {
            try {
                const data = JSON.parse(event.data);
                if (data.type === 'LOG_LINE') {
                    // デバッグログはバッファに追記 (状態更新とは別経路)
                    const entry = { line: data.payload?.line ?? '', ts: Date.now() };
                    setLogs((prev) => {
                        const next = [...prev, entry];
                        return next.length > MAX_LOG_LINES
                            ? next.slice(next.length - MAX_LOG_LINES)
                            : next;
                    });
                } else if (data.type === 'FRAME_PREVIEW') {
                    // 映像プレビューは購読者へ直接渡す (state 経由の再レンダーを避ける)
                    frameSubscribers.current.forEach((cb) => {
                        try { cb(data.payload); } catch (e) { console.error('frame subscriber failed:', e); }
                    });
                } else {
                    setLastMessage(data);
                }
            } catch (e) {
                console.error('Failed to parse WebSocket message:', e);
            }
        };

        ws.current.onclose = () => {
            console.log('WebSocket Disconnected');
            setIsConnected(false);
            // Auto reconnect
            reconnectTimeout.current = setTimeout(() => {
                console.log('Attempting to reconnect...');
                connect();
            }, 3000);
        };

        ws.current.onerror = (error) => {
            console.error('WebSocket Error:', error);
            ws.current.close();
        };
    }, []);

    useEffect(() => {
        connect();
        return () => {
            if (ws.current) {
                ws.current.close();
            }
            if (reconnectTimeout.current) {
                clearTimeout(reconnectTimeout.current);
            }
        };
    }, [connect]);

    useEffect(() => { selectedDeviceRef.current = selectedDeviceId; }, [selectedDeviceId]);

    const sendMessage = useCallback((type, payload) => {
        if (ws.current && ws.current.readyState === WebSocket.OPEN) {
            // device = 宛先 core。サーバーは sphere/<device>/command/* へ転送する
            // (未指定ならサーバー保持の操作対象。"all" は全 core ブロードキャスト)
            const device = selectedDeviceRef.current;
            ws.current.send(JSON.stringify(device ? { type, payload, device } : { type, payload }));
        } else {
            console.warn('WebSocket not connected, cannot send message:', type);
        }
    }, []);

    // FRAME_PREVIEW の購読 (返り値は解除関数)。identity は不変なので再レンダーを誘発しない。
    const subscribeFrame = useCallback((cb) => {
        frameSubscribers.current.add(cb);
        return () => frameSubscribers.current.delete(cb);
    }, []);

    // main値はメモ化し、logs更新では identity を変えない(購読者を再レンダーさせない)
    const wsValue = useMemo(
        () => ({ isConnected, lastMessage, sendMessage, subscribeFrame, selectedDeviceId, setSelectedDeviceId }),
        [isConnected, lastMessage, sendMessage, subscribeFrame, selectedDeviceId]
    );
    const logValue = useMemo(() => ({ logs, clearLogs }), [logs, clearLogs]);

    return (
        <WebSocketContext.Provider value={wsValue}>
            <LogContext.Provider value={logValue}>
                {children}
            </LogContext.Provider>
        </WebSocketContext.Provider>
    );
};

export const useWebSocket = () => {
    const context = useContext(WebSocketContext);
    if (!context) {
        throw new Error('useWebSocket must be used within a WebSocketProvider');
    }
    return context;
};

// デバッグログ専用フック (LogPanel のみが使用)。これを使う側だけがログ更新で再レンダーされる。
export const useLogs = () => {
    const context = useContext(LogContext);
    if (!context) {
        throw new Error('useLogs must be used within a WebSocketProvider');
    }
    return context;
};
