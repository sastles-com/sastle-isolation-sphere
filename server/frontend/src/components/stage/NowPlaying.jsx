import React from 'react';

/**
 * NowPlaying — アクティブプレイリスト名と再生中動画名。
 * 位置/長さの表示は P2 で配線 (現状 API に position が無いため名前のみ)。
 */
export const NowPlaying = ({ playlistName, videoName, isStreaming }) => (
    <div style={{ padding: '0 24px', minHeight: 56 }}>
        <div style={{
            fontSize: 20, fontWeight: 700, letterSpacing: '-0.01em',
            color: 'var(--tx-1)',
            whiteSpace: 'nowrap', overflow: 'hidden', textOverflow: 'ellipsis',
        }}>
            {playlistName || '—'}
        </div>
        <div style={{
            display: 'flex', alignItems: 'center', gap: 8,
            fontSize: 14, color: 'var(--tx-2)', marginTop: 2,
            whiteSpace: 'nowrap', overflow: 'hidden',
        }}>
            <span style={{ color: isStreaming ? 'var(--accent)' : 'var(--tx-3)' }}>▸</span>
            <span style={{ overflow: 'hidden', textOverflow: 'ellipsis' }}>
                {videoName || (isStreaming ? '…' : '停止中')}
            </span>
        </div>
    </div>
);
