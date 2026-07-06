import React from 'react';

/**
 * LibrarySheet — Playlists / Videos のライブラリ内容。
 * P1 ではプレースホルダ (P2 で PlaylistList / VideoGrid を実装)。
 */
export const LibrarySheet = () => (
    <div style={{
        flex: 1, minHeight: 0, overflowY: 'auto',
        padding: '4px 20px',
        paddingBottom: 'max(20px, env(safe-area-inset-bottom))',
    }}>
        <div className="ui-label" style={{ marginBottom: 12 }}>Library</div>
        <div className="glass" style={{
            borderRadius: 'var(--radius)', padding: 24,
            color: 'var(--tx-3)', textAlign: 'center', fontSize: 14,
        }}>
            Playlists / Videos は Phase 2 で実装
        </div>
    </div>
);
