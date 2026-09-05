import React, { useState } from 'react';
import { motion } from 'framer-motion';
import { SegmentControl } from '../ui/SegmentControl';
import { PlaylistList } from '../library/PlaylistList';
import { VideoGrid } from '../library/VideoGrid';
import { PatternGrid } from '../library/PatternGrid';

const SEGMENTS = [
    { value: 'playlists', label: 'Playlists' },
    { value: 'videos', label: 'Videos' },
    { value: 'patterns', label: 'Patterns' },
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
        const dir = info.offset.x < 0 ? 1 : -1; // 左フリック=次 / 右フリック=前
        setSeg((s) => {
            const i = SEGMENTS.findIndex((o) => o.value === s);
            const next = Math.min(SEGMENTS.length - 1, Math.max(0, i + dir));
            return SEGMENTS[next].value;
        });
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

    // 動画/パターンをプレイリストへ追加。追加先はアクティブなプレイリスト。
    // 未設定でプレイリストが 1 本ならそれ、複数なら番号で選ぶ (UI v2 はまだ
    // ピッカーを持たないので prompt で代替)。0 本なら作成を促す。
    const handleAddToPlaylist = async (v) => {
        const pls = pb.playlists;
        if (pls.length === 0) {
            if (!window.confirm('プレイリストがありません。作成しますか?')) return;
            const name = window.prompt('プレイリスト名', 'Playlist 1');
            if (!name || !(await pb.createPlaylist(name))) return;
            // 作成直後の一覧は再取得待ちなので、ユーザーにもう一度押してもらう
            window.alert(`「${name}」を作成しました。もう一度 +PL を押して追加してください。`);
            return;
        }
        let target = pb.currentPlaylist ?? (pls.length === 1 ? pls[0] : null);
        if (!target) {
            const menu = pls.map((pl, i) => `${i + 1}: ${pl.name} (${pl.item_count ?? 0}本)`).join('\n');
            const ans = window.prompt(`追加先のプレイリスト番号を入力\n${menu}`, '1');
            const idx = parseInt(ans, 10) - 1;
            if (Number.isNaN(idx) || idx < 0 || idx >= pls.length) return;
            target = pls[idx];
        } else if (!window.confirm(`「${v.title}」を「${target.name}」に追加しますか?`)) {
            return;
        }
        const ok = await pb.addToPlaylist(target.id, v.id);
        if (!ok) window.alert('追加に失敗しました');
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
                {seg === 'playlists' && (
                    <PlaylistList
                        playlists={pb.playlists}
                        currentPlaylistId={pb.currentPlaylist?.id}
                        isStreaming={pb.isStreaming}
                        onActivatePlay={handleActivatePlay}
                        onCreate={handleCreatePlaylist}
                        onDelete={handleDeletePlaylist}
                    />
                )}
                {seg === 'videos' && (
                    <VideoGrid
                        videos={pb.materialVideos}
                        currentVideoId={pb.currentVideo?.id}
                        isStreaming={pb.isStreaming}
                        onPlay={(v) => pb.playVideo(v.id)}
                        onDelete={handleDeleteVideo}
                        onAdd={handleAddToPlaylist}
                        onUpload={pb.uploadVideo}
                    />
                )}
                {seg === 'patterns' && (
                    <PatternGrid
                        patterns={pb.patterns}
                        currentVideoId={pb.currentVideo?.id}
                        isStreaming={pb.isStreaming}
                        onPlay={(v) => pb.playVideo(v.id)}
                        onDelete={handleDeleteVideo}
                        onAdd={handleAddToPlaylist}
                        onUpload={pb.uploadVideo}
                    />
                )}
            </motion.div>
        </div>
    );
};
