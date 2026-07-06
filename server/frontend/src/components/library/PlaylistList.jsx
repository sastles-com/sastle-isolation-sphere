import React from 'react';
import { motion } from 'framer-motion';
import { GlassButton } from '../ui/GlassButton';
import { IconPlay, IconTrash } from '../ui/icons';

/**
 * PlaylistList — プレイリストのカード縦リスト。
 * カード = 名前 + 曲数。タップ → アクティブ化 + 再生 (確認あり)。
 * 再生中プレイリストには --accent のインジケータ。
 */
export const PlaylistList = ({ playlists, currentPlaylistId, isStreaming, onActivatePlay, onCreate, onDelete }) => (
    <div style={{ display: 'flex', flexDirection: 'column', gap: 10 }}>
        <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
            <span className="ui-label">Playlists</span>
            <GlassButton variant="pill" title="新規プレイリスト" onClick={onCreate}
                style={{ minHeight: 32, height: 32, fontSize: 13 }}>
                + NEW
            </GlassButton>
        </div>

        {playlists.length === 0 ? (
            <div className="glass" style={{
                borderRadius: 'var(--radius)', padding: 24,
                color: 'var(--tx-3)', textAlign: 'center', fontSize: 14,
            }}>
                NEW からプレイリストを作成
            </div>
        ) : playlists.map((pl) => {
            const playingThis = isStreaming && currentPlaylistId === pl.id;
            return (
                <motion.div
                    key={pl.id}
                    className="glass"
                    whileTap={{ scale: 0.98 }}
                    onClick={() => onActivatePlay(pl)}
                    style={{
                        display: 'flex', alignItems: 'center', gap: 12,
                        padding: '12px 14px', borderRadius: 'var(--radius)',
                        cursor: 'pointer',
                        borderColor: playingThis ? 'var(--accent)' : 'var(--glass-stroke)',
                    }}
                >
                    {/* サムネイル枠 (プレイリストにサムネ API が無いためアイコンプレースホルダ) */}
                    <div style={{
                        width: 44, height: 44, flexShrink: 0,
                        borderRadius: 12, background: 'rgb(255 255 255 / .06)',
                        display: 'flex', alignItems: 'center', justifyContent: 'center',
                        color: playingThis ? 'var(--accent)' : 'var(--tx-3)',
                    }}>
                        <IconPlay size={18} />
                    </div>
                    <div style={{ flex: 1, minWidth: 0 }}>
                        <div style={{
                            fontSize: 16, fontWeight: 700, color: 'var(--tx-1)',
                            whiteSpace: 'nowrap', overflow: 'hidden', textOverflow: 'ellipsis',
                        }}>
                            {pl.name}
                        </div>
                        <div className="ui-label" style={{ marginTop: 2 }}>
                            {pl.item_count ?? 0} 本
                            {playingThis && <span style={{ color: 'var(--accent)' }}>  ● 再生中</span>}
                        </div>
                    </div>
                    <GlassButton title="削除" onClick={(e) => { e.stopPropagation(); onDelete(pl); }}
                        style={{ minWidth: 40, minHeight: 40, width: 40, height: 40, color: 'var(--err)' }}>
                        <IconTrash size={18} />
                    </GlassButton>
                </motion.div>
            );
        })}
    </div>
);
