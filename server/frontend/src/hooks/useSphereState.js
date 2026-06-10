import { useEffect } from 'react';
import { useWebSocket } from '../contexts/WebSocketContext';

// StateManager からの STATE_UPDATE メッセージを購読する共通フック。
// 受信のたびに onUpdate(payload) を呼ぶ。どの値をどう state に反映するかは
// 呼び出し側に委ねる(コンポーネントごとに必要な項目が異なるため)。
export const useStateUpdate = (onUpdate) => {
    const { lastMessage } = useWebSocket();

    useEffect(() => {
        if (lastMessage && lastMessage.type === 'STATE_UPDATE') {
            onUpdate(lastMessage.payload);
        }
        // 発火タイミングを従来実装([lastMessage] のみ)と同一に保つ
        // eslint-disable-next-line react-hooks/exhaustive-deps
    }, [lastMessage]);
};
