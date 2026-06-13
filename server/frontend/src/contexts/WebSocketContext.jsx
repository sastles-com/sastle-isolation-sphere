import React, { createContext, useContext, useEffect, useState, useRef, useCallback } from 'react';

const WebSocketContext = createContext(null);

// 開発サーバー(vite等)でポートが取れない場合のフォールバック先
const DEFAULT_WS_PORT = '9000';

export const WebSocketProvider = ({ children }) => {
    const [isConnected, setIsConnected] = useState(false);
    const [lastMessage, setLastMessage] = useState(null);
    const [logs, setLogs] = useState([]);
    const ws = useRef(null);
    const reconnectTimeout = useRef(null);

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

    const sendMessage = useCallback((type, payload) => {
        if (ws.current && ws.current.readyState === WebSocket.OPEN) {
            ws.current.send(JSON.stringify({ type, payload }));
        } else {
            console.warn('WebSocket not connected, cannot send message:', type);
        }
    }, []);

    return (
        <WebSocketContext.Provider value={{ isConnected, lastMessage, sendMessage, logs, clearLogs }}>
            {children}
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
