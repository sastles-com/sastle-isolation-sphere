import React from 'react';
import { motion } from 'framer-motion';
import { formatDuration, formatSize } from '../../lib/format';
import { GlassButton } from '../ui/GlassButton';
import { IconPlay, IconTrash } from '../ui/icons';

/**
 * VideoCard (glass 版) — サムネイル + タイトル + メタ + 再生/削除。
 * サムネイルタップ or 再生ボタンで単発再生。
 */
export const VideoCard = ({ video, playing, onPlay, onDelete }) => (
    <motion.div
        className="glass"
        whileTap={{ scale: 0.98 }}
        style={{
            display: 'flex', flexDirection: 'column',
            borderRadius: 'var(--radius)', overflow: 'hidden',
            borderColor: playing ? 'var(--accent)' : 'var(--glass-stroke)',
        }}
    >
        {/* サムネイル (1:1) */}
        <div
            onClick={onPlay}
            title="再生"
            style={{
                position: 'relative', width: '100%', paddingTop: '100%',
                background: 'rgb(0 0 0 / .35)', cursor: 'pointer',
            }}
        >
            {video.thumbnail_path ? (
                <img
                    src={video.thumbnail_path}
                    alt={video.title}
                    style={{
                        position: 'absolute', inset: 0,
                        width: '100%', height: '100%', objectFit: 'cover',
                    }}
                />
            ) : (
                <div style={{
                    position: 'absolute', inset: 0,
                    display: 'flex', alignItems: 'center', justifyContent: 'center',
                    color: 'var(--tx-3)',
                }}>
                    <IconPlay size={32} />
                </div>
            )}
            {playing && (
                <div className="ui-label" style={{
                    position: 'absolute', top: 6, left: 6,
                    padding: '2px 8px', borderRadius: 'var(--radius-pill)',
                    background: 'var(--accent-soft)', border: '1px solid var(--accent)',
                    color: 'var(--accent)',
                }}>
                    ● PLAYING
                </div>
            )}
        </div>

        {/* メタ + アクション */}
        <div style={{ padding: 10, display: 'flex', flexDirection: 'column', gap: 6 }}>
            <div style={{
                fontSize: 13, fontWeight: 600, color: 'var(--tx-1)',
                whiteSpace: 'nowrap', overflow: 'hidden', textOverflow: 'ellipsis',
            }}>
                {video.title}
            </div>
            <div className="mono" style={{ fontSize: 11, color: 'var(--tx-3)', display: 'flex', gap: 8 }}>
                <span>{formatDuration(video.duration_ms)}</span>
                <span>{formatSize(video.size_bytes)}</span>
            </div>
            <div style={{ display: 'flex', gap: 6, marginTop: 2 }}>
                <GlassButton title="再生" onClick={onPlay} active={playing}
                    style={{ flex: 1, minHeight: 36, height: 36 }}>
                    <IconPlay size={16} />
                </GlassButton>
                <GlassButton title="削除" onClick={onDelete}
                    style={{ minWidth: 36, minHeight: 36, width: 36, height: 36, color: 'var(--err)' }}>
                    <IconTrash size={16} />
                </GlassButton>
            </div>
        </div>
    </motion.div>
);
