import React, { useState, useEffect, useCallback } from 'react';
import {
    Box, Typography, Button, IconButton, List, ListItem, ListItemText, ListItemIcon,
    Chip, Collapse, Divider, Menu, MenuItem, CircularProgress
} from '@mui/material';
import PlaylistPlayIcon from '@mui/icons-material/PlaylistPlay';
import AddIcon from '@mui/icons-material/Add';
import PlayArrowIcon from '@mui/icons-material/PlayArrow';
import StopIcon from '@mui/icons-material/Stop';
import PauseIcon from '@mui/icons-material/Pause';
import RepeatIcon from '@mui/icons-material/Repeat';
import DeleteIcon from '@mui/icons-material/Delete';
import ExpandMoreIcon from '@mui/icons-material/ExpandMore';
import ExpandLessIcon from '@mui/icons-material/ExpandLess';
import ArrowUpwardIcon from '@mui/icons-material/ArrowUpward';
import ArrowDownwardIcon from '@mui/icons-material/ArrowDownward';
import MovieIcon from '@mui/icons-material/Movie';
import { apiGet, apiPost } from '../../lib/api';
import { formatDuration } from '../../lib/format';

const BASE = '/api/playlist';

export const PlaylistManager = () => {
    const [playlists, setPlaylists] = useState([]);
    const [loading, setLoading] = useState(true);
    const [expanded, setExpanded] = useState(null);      // playlist_id
    const [items, setItems] = useState([]);              // 展開中PLのアイテム詳細
    const [videos, setVideos] = useState([]);            // 追加用の素材一覧
    const [playback, setPlayback] = useState({ status: 'stopped', playlist_id: null });
    const [addAnchor, setAddAnchor] = useState(null);

    const fetchPlaylists = useCallback(async () => {
        try {
            const res = await apiGet(`${BASE}/playlists`);
            if (res.ok) setPlaylists(await res.json());
        } catch (e) { console.error(e); } finally { setLoading(false); }
    }, []);

    const fetchPlayback = useCallback(async () => {
        try {
            const res = await apiGet(`${BASE}/playback`);
            if (res.ok) setPlayback(await res.json());
        } catch (e) { /* ignore */ }
    }, []);

    const fetchItems = useCallback(async (pid) => {
        try {
            const res = await apiGet(`${BASE}/playlists/${pid}`);
            if (res.ok) setItems((await res.json()).items || []);
        } catch (e) { console.error(e); }
    }, []);

    useEffect(() => { fetchPlaylists(); fetchVideos(); }, [fetchPlaylists]);
    useEffect(() => {
        fetchPlayback();
        const id = setInterval(fetchPlayback, 2000);
        return () => clearInterval(id);
    }, [fetchPlayback]);

    const fetchVideos = async () => {
        try {
            const res = await apiGet(`${BASE}/videos`);
            if (res.ok) setVideos(await res.json());
        } catch (e) { console.error(e); }
    };

    const handleCreate = async () => {
        const name = window.prompt('プレイリスト名', `Playlist ${playlists.length + 1}`);
        if (!name) return;
        const res = await apiPost(`${BASE}/playlists`, { name, loop: true });
        if (res.ok) fetchPlaylists();
    };

    const handleDeletePlaylist = async (pid, e) => {
        e.stopPropagation();
        if (!window.confirm('このプレイリストを削除しますか?')) return;
        const res = await fetch(`${BASE}/playlists/${pid}`, { method: 'DELETE' });
        if (res.ok) { if (expanded === pid) setExpanded(null); fetchPlaylists(); }
    };

    const toggleExpand = (pid) => {
        if (expanded === pid) { setExpanded(null); return; }
        setExpanded(pid); setItems([]); fetchItems(pid);
    };

    const handlePlayPlaylist = async (pid, e) => {
        e?.stopPropagation();
        const res = await apiPost(`${BASE}/playlists/${pid}/play`, {});
        if (res.ok) fetchPlayback();
        else alert('再生失敗: ' + (await res.text()));
    };

    const handleStop = async () => { await apiPost(`${BASE}/playback/stop`, {}); fetchPlayback(); };
    const handlePauseToggle = async () => { await apiPost(`${BASE}/playback/pause`, {}); fetchPlayback(); };
    const handleLoopToggle = async () => { await apiPost(`${BASE}/playback/loop`, { loop: !playback.loop }); fetchPlayback(); };

    const handleAddVideo = async (videoId) => {
        setAddAnchor(null);
        const res = await apiPost(`${BASE}/playlists/${expanded}/items`, { video_id: videoId });
        if (res.ok) { setItems((await res.json()).items || []); fetchPlaylists(); }
    };

    const handleRemoveItem = async (itemId) => {
        const res = await fetch(`${BASE}/playlists/${expanded}/items/${itemId}`, { method: 'DELETE' });
        if (res.ok) { setItems((await res.json()).items || []); fetchPlaylists(); }
    };

    const handleMove = async (index, dir) => {
        const next = index + dir;
        if (next < 0 || next >= items.length) return;
        const order = items.map(it => it.id);            // 動画ID列 (id = videos.id)
        [order[index], order[next]] = [order[next], order[index]];
        const res = await fetch(`${BASE}/playlists/${expanded}/items`, {
            method: 'PUT', headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ video_ids: order })
        });
        if (res.ok) setItems((await res.json()).items || []);
    };

    const isStreaming = playback.status !== 'stopped';

    return (
        <Box sx={{ height: '100%', display: 'flex', flexDirection: 'column', bgcolor: 'rgba(20, 27, 45, 0.9)' }}
            onTouchStart={(e) => e.stopPropagation()} onTouchMove={(e) => e.stopPropagation()}>
            {/* Header */}
            <Box sx={{ p: 2, flexShrink: 0 }}>
                <Box sx={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', mb: 2 }}>
                    <Typography variant="h6" sx={{ color: 'primary.main', letterSpacing: '0.1em', fontFamily: '"Source Code Pro", monospace' }}>
                        PLAYLISTS
                    </Typography>
                    <Button variant="contained" startIcon={<AddIcon />} onClick={handleCreate}
                        sx={{ bgcolor: 'primary.main', color: '#000', fontWeight: 'bold', '&:hover': { bgcolor: '#00b8cc' } }}>
                        NEW
                    </Button>
                </Box>

                {/* NOW STREAMING バー */}
                {isStreaming && (
                    <Box sx={{ display: 'flex', alignItems: 'center', gap: 1, p: 1.5, mb: 1,
                        bgcolor: 'rgba(0, 255, 65, 0.08)', borderRadius: 2, border: '1px solid rgba(0, 255, 65, 0.3)' }}>
                        <Box component="span" sx={{ width: 8, height: 8, borderRadius: '50%',
                            bgcolor: playback.status === 'playing' ? '#00FF41' : '#FFB300',
                            animation: playback.status === 'playing' ? 'blink 1s infinite' : 'none',
                            '@keyframes blink': { '50%': { opacity: 0.3 } } }} />
                        <Typography variant="caption" sx={{ color: '#00FF41', fontFamily: '"Source Code Pro", monospace', flex: 1 }}>
                            {playback.status === 'playing' ? 'STREAMING' : 'PAUSED'}
                            {playback.playlist_id ? ` · PL#${playback.playlist_id}` : ` · #${playback.video_id ?? '-'}`}
                        </Typography>
                        <IconButton size="small" onClick={handleLoopToggle}
                            title={playback.loop ? 'ループ ON' : 'ループ OFF'}
                            sx={{ color: playback.loop ? '#00FF41' : 'text.secondary' }}>
                            <RepeatIcon fontSize="small" />
                        </IconButton>
                        <IconButton size="small" onClick={handlePauseToggle} sx={{ color: 'primary.main' }}>
                            {playback.status === 'playing' ? <PauseIcon fontSize="small" /> : <PlayArrowIcon fontSize="small" />}
                        </IconButton>
                        <IconButton size="small" onClick={handleStop} sx={{ color: 'error.main' }}>
                            <StopIcon fontSize="small" />
                        </IconButton>
                    </Box>
                )}
            </Box>

            {/* List */}
            <Box sx={{ flex: 1, overflow: 'auto', px: 2, pb: 2 }}>
                {loading ? (
                    <Box sx={{ textAlign: 'center', py: 6 }}><CircularProgress sx={{ color: 'primary.main' }} /></Box>
                ) : playlists.length === 0 ? (
                    <Box sx={{ textAlign: 'center', py: 6, color: 'text.secondary' }}>
                        <PlaylistPlayIcon sx={{ fontSize: 64, opacity: 0.2, mb: 1 }} />
                        <Typography variant="body2">NEW からプレイリストを作成</Typography>
                    </Box>
                ) : (
                    <List sx={{ p: 0 }}>
                        {playlists.map((pl) => {
                            const isOpen = expanded === pl.id;
                            const isPlayingThis = playback.playlist_id === pl.id && isStreaming;
                            return (
                                <Box key={pl.id} sx={{ mb: 1, border: '1px solid', borderRadius: 1,
                                    borderColor: isPlayingThis ? '#00FF41' : 'rgba(0, 229, 255, 0.3)',
                                    bgcolor: 'rgba(0, 0, 0, 0.3)', overflow: 'hidden' }}>
                                    <ListItem
                                        onClick={() => toggleExpand(pl.id)}
                                        sx={{ cursor: 'pointer', '&:hover': { bgcolor: 'rgba(0, 229, 255, 0.05)' } }}
                                        secondaryAction={
                                            <Box>
                                                <IconButton size="small" onClick={(e) => handlePlayPlaylist(pl.id, e)}
                                                    sx={{ color: '#00FF41', border: '1px solid #00FF41', mr: 0.5 }}>
                                                    <PlayArrowIcon fontSize="small" />
                                                </IconButton>
                                                <IconButton size="small" onClick={(e) => handleDeletePlaylist(pl.id, e)} sx={{ color: 'error.main' }}>
                                                    <DeleteIcon fontSize="small" />
                                                </IconButton>
                                            </Box>
                                        }
                                    >
                                        <ListItemIcon sx={{ minWidth: 36 }}>
                                            {isOpen ? <ExpandLessIcon sx={{ color: 'primary.main' }} /> : <ExpandMoreIcon sx={{ color: 'primary.main' }} />}
                                        </ListItemIcon>
                                        <ListItemText
                                            primary={pl.name}
                                            secondary={<Chip label={`${pl.item_count ?? 0} 本`} size="small"
                                                sx={{ height: 18, fontSize: '0.6rem', bgcolor: 'rgba(255,255,255,0.1)', color: '#ccc' }} />}
                                            primaryTypographyProps={{ color: '#fff', fontWeight: 600, fontFamily: '"Source Code Pro", monospace', fontSize: '0.85rem' }}
                                        />
                                    </ListItem>

                                    {/* アイテム編集 */}
                                    <Collapse in={isOpen} unmountOnExit>
                                        <Divider sx={{ borderColor: 'rgba(0, 229, 255, 0.15)' }} />
                                        <Box sx={{ p: 1 }}>
                                            {items.length === 0 ? (
                                                <Typography variant="caption" sx={{ color: 'text.secondary', display: 'block', textAlign: 'center', py: 1 }}>
                                                    動画がありません
                                                </Typography>
                                            ) : items.map((it, idx) => (
                                                <Box key={it.item_id} sx={{ display: 'flex', alignItems: 'center', gap: 0.5, py: 0.5 }}>
                                                    <Typography variant="caption" sx={{ color: 'primary.main', width: 18, textAlign: 'right' }}>{idx + 1}.</Typography>
                                                    <MovieIcon sx={{ fontSize: 16, color: 'rgba(0,229,255,0.5)' }} />
                                                    <Typography variant="caption" sx={{ color: '#fff', flex: 1, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>
                                                        {it.title} <Box component="span" sx={{ color: 'text.secondary' }}>({formatDuration(it.duration_ms)})</Box>
                                                    </Typography>
                                                    <IconButton size="small" disabled={idx === 0} onClick={() => handleMove(idx, -1)} sx={{ color: 'primary.main', p: 0.25 }}><ArrowUpwardIcon sx={{ fontSize: 16 }} /></IconButton>
                                                    <IconButton size="small" disabled={idx === items.length - 1} onClick={() => handleMove(idx, 1)} sx={{ color: 'primary.main', p: 0.25 }}><ArrowDownwardIcon sx={{ fontSize: 16 }} /></IconButton>
                                                    <IconButton size="small" onClick={() => handleRemoveItem(it.item_id)} sx={{ color: 'error.main', p: 0.25 }}><DeleteIcon sx={{ fontSize: 16 }} /></IconButton>
                                                </Box>
                                            ))}
                                            <Button fullWidth size="small" startIcon={<AddIcon />}
                                                onClick={(e) => setAddAnchor(e.currentTarget)}
                                                sx={{ mt: 1, color: 'primary.main', border: '1px dashed rgba(0,229,255,0.4)' }}>
                                                動画を追加
                                            </Button>
                                        </Box>
                                    </Collapse>
                                </Box>
                            );
                        })}
                    </List>
                )}
            </Box>

            {/* 動画追加メニュー */}
            <Menu anchorEl={addAnchor} open={Boolean(addAnchor)} onClose={() => setAddAnchor(null)}
                PaperProps={{ sx: { bgcolor: 'rgba(20,27,45,0.98)', border: '1px solid rgba(0,229,255,0.3)', maxHeight: 300 } }}>
                {videos.length === 0 ? (
                    <MenuItem disabled sx={{ color: 'text.secondary' }}>素材動画がありません</MenuItem>
                ) : videos.map(v => (
                    <MenuItem key={v.id} onClick={() => handleAddVideo(v.id)} sx={{ color: '#fff', fontSize: '0.8rem' }}>
                        <MovieIcon sx={{ fontSize: 16, mr: 1, color: 'primary.main' }} /> {v.title}
                    </MenuItem>
                ))}
            </Menu>
        </Box>
    );
};
