import React, { useRef, useState } from 'react';
import { VideoCard } from './VideoCard';
import { GlassButton } from '../ui/GlassButton';
import { IconUpload } from '../ui/icons';

/**
 * VideoGrid — 素材動画の 2 列グリッド。アップロード / 削除 / 単発再生。
 */
export const VideoGrid = ({ videos, currentVideoId, isStreaming, onPlay, onDelete, onAdd, onUpload }) => {
    const fileRef = useRef(null);
    const [uploading, setUploading] = useState(false);

    const handleFile = async (e) => {
        const file = e.target.files?.[0];
        if (!file) return;
        setUploading(true);
        try { await onUpload(file); } finally {
            setUploading(false);
            e.target.value = '';
        }
    };

    return (
        <div style={{ display: 'flex', flexDirection: 'column', gap: 10 }}>
            <input ref={fileRef} type="file" accept="video/*" hidden onChange={handleFile} />
            <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
                <span className="ui-label">Videos ({videos.length})</span>
                <GlassButton variant="pill" title="動画をアップロード" disabled={uploading}
                    onClick={() => fileRef.current?.click()}
                    style={{ minHeight: 32, height: 32, fontSize: 13, gap: 6 }}>
                    <IconUpload size={16} />
                    {uploading ? 'UP...' : 'UPLOAD'}
                </GlassButton>
            </div>

            {videos.length === 0 ? (
                <div className="glass" style={{
                    borderRadius: 'var(--radius)', padding: 24,
                    color: 'var(--tx-3)', textAlign: 'center', fontSize: 14,
                }}>
                    UPLOAD から動画を追加
                </div>
            ) : (
                <div style={{
                    display: 'grid', gridTemplateColumns: 'repeat(2, 1fr)', gap: 10,
                }}>
                    {videos.map((v) => (
                        <VideoCard
                            key={v.id}
                            video={v}
                            playing={isStreaming && v.id === currentVideoId}
                            onPlay={() => onPlay(v)}
                            onDelete={() => onDelete(v)}
                            onAdd={onAdd ? () => onAdd(v) : undefined}
                        />
                    ))}
                </div>
            )}
        </div>
    );
};
