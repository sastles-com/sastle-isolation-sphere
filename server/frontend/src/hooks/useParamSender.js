import { useCallback, useEffect, useRef } from 'react';
import { useWebSocket } from '../contexts/WebSocketContext';

const DEBOUNCE_MS = 60; // SET_PARAMS 送信デバウンス (仕様 §2.4 / §5)

/**
 * useParamSender — SET_PARAMS を key ごとに 60ms デバウンスで送信する共通フック。
 * エッジドラッグ・TUNE スライダーなど連続操作の送信間引きに使う。
 */
export const useParamSender = () => {
    const { sendMessage } = useWebSocket();
    const timers = useRef({});
    const pending = useRef({});
    useEffect(() => () => Object.values(timers.current).forEach(clearTimeout), []);
    return useCallback((key, value) => {
        pending.current[key] = value;
        if (!timers.current[key]) {
            timers.current[key] = setTimeout(() => {
                timers.current[key] = null;
                sendMessage('SET_PARAMS', { [key]: pending.current[key] });
            }, DEBOUNCE_MS);
        }
    }, [sendMessage]);
};
