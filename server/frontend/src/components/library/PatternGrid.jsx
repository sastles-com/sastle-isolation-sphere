import React, { useRef, useState } from 'react';
import { VideoCard } from './VideoCard';
import { GlassButton } from '../ui/GlassButton';
import { IconUpload } from '../ui/icons';

// Pattern Studio (P3, 別Web) の起動先。同一オリジンで配信予定。
const STUDIO_URL = '/studio';

/**
 * PatternGrid — パターン動画 (kind='pattern') の 2 列グリッド。
 * Studio 起動リンク + アップロード / 削除 / 単発再生。
 * パターンは実体が equirectangular 動画なので再生/削除は素材動画と同じ経路を使う。
 */
export const PatternGrid = ({ patterns, currentVideoId, isStreaming, onPlay, onDelete, onUpload }) => {
    const fileRef = useRef(null);
    const [uploading, setUploading] = useState(false);

    const handleFile = async (e) => {
        const file = e.target.files?.[0];
        if (!file) return;
        setUploading(true);
        try { await onUpload(file, 'pattern'); } finally {
            setUploading(false);
            e.target.value = '';
        }
    };

    return (
        <div style={{ display: 'flex', flexDirection: 'column', gap: 10 }}>
            <input ref={fileRef} type="file" accept="video/*" hidden onChange={handleFile} />
            <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', gap: 8 }}>
                <span className="ui-label">Patterns ({patterns.length})</span>
                <div style={{ display: 'flex', gap: 6 }}>
                    <GlassButton variant="pill" title="Pattern Studio を開く"
                        onClick={() => window.open(STUDIO_URL, '_blank', 'noopener')}
                        style={{ minHeight: 32, height: 32, fontSize: 13, gap: 6 }}>
                        STUDIO ↗
                    </GlassButton>
                    <GlassButton variant="pill" title="パターン動画をアップロード" disabled={uploading}
                        onClick={() => fileRef.current?.click()}
                        style={{ minHeight: 32, height: 32, fontSize: 13, gap: 6 }}>
                        <IconUpload size={16} />
                        {uploading ? 'UP...' : 'UPLOAD'}
                    </GlassButton>
                </div>
            </div>

            {patterns.length === 0 ? (
                <div className="glass" style={{
                    borderRadius: 'var(--radius)', padding: 24,
                    color: 'var(--tx-3)', textAlign: 'center', fontSize: 14,
                }}>
                    STUDIO で作成、または UPLOAD からパターンを追加
                </div>
            ) : (
                <div style={{
                    display: 'grid', gridTemplateColumns: 'repeat(2, 1fr)', gap: 10,
                }}>
                    {patterns.map((v) => (
                        <VideoCard
                            key={v.id}
                            video={v}
                            playing={isStreaming && v.id === currentVideoId}
                            onPlay={() => onPlay(v)}
                            onDelete={() => onDelete(v)}
                        />
                    ))}
                </div>
            )}
        </div>
    );
};
