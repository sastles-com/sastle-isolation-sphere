import React, { useState } from 'react';
import { motion } from 'framer-motion';
import { SegmentControl } from '../ui/SegmentControl';
import { PlaylistList } from '../library/PlaylistList';
import { VideoGrid } from '../library/VideoGrid';

const SEGMENTS = [
    { value: 'playlists', label: 'Playlists' },
    { value: 'videos', label: 'Videos' },
];

/**
 * LibrarySheet — Playlists / Videos セグメント。
 * セグメント切替はコントロールのタップ、または内容の横フリック。
 */
export const LibrarySheet = ({ pb }) => {
    const [seg, setSeg] = useState('playlists');

    // 横フリックでセグメント切替 (縦優位のジェスチャーは無視 = シート縦ドラッグを妨げない)
    const handlePanEnd = (_, info) => {
        if (Math.abs(info.offset.x) < 60 || Math.abs(info.offset.x) <= Math.abs(info.offset.y)) return;
        setSeg((s) => (info.offset.x < 0
            ? (s === 'playlists' ? 'videos' : s)
            : (s === 'videos' ? 'playlists' : s)));
    };

    const handleActivatePlay = async (pl) => {
        if (!window.confirm(`「${pl.name}」を再生しますか?`)) {
            pb.setActivePlaylist(pl.id); // 再生はしないがアクティブ化はする
            return;
        }
        await pb.activateAndPlay(pl.id);
    };

    const handleDeletePlaylist = async (pl) => {
        if (window.confirm(`「${pl.name}」を削除しますか?`)) await pb.deletePlaylist(pl.id);
    };

    const handleCreatePlaylist = async () => {
        const name = window.prompt('プレイリスト名', `Playlist ${pb.playlists.length + 1}`);
        if (name) await pb.createPlaylist(name);
    };

    const handleDeleteVideo = async (v) => {
        if (window.confirm(`「${v.title}」を削除しますか?`)) await pb.deleteVideo(v.id);
    };

    return (
        <div style={{ display: 'flex', flexDirection: 'column', flex: 1, minHeight: 0 }}>
            <div style={{ padding: '4px 20px 10px', flexShrink: 0 }}>
                <SegmentControl options={SEGMENTS} value={seg} onChange={setSeg} />
            </div>
            <motion.div
                onPanEnd={handlePanEnd}
                style={{
                    flex: 1, minHeight: 0, overflowY: 'auto',
                    padding: '0 20px',
                    paddingBottom: 'max(20px, env(safe-area-inset-bottom))',
                }}
            >
                {seg === 'playlists' ? (
                    <PlaylistList
                        playlists={pb.playlists}
                        currentPlaylistId={pb.currentPlaylist?.id}
                        isStreaming={pb.isStreaming}
                        onActivatePlay={handleActivatePlay}
                        onCreate={handleCreatePlaylist}
                        onDelete={handleDeletePlaylist}
                    />
                ) : (
                    <VideoGrid
                        videos={pb.videos}
                        currentVideoId={pb.currentVideo?.id}
                        isStreaming={pb.isStreaming}
                        onPlay={(v) => pb.playVideo(v.id)}
                        onDelete={handleDeleteVideo}
                        onUpload={pb.uploadVideo}
                    />
                )}
            </motion.div>
        </div>
    );
};
