import React, { useState, useEffect, useRef, useCallback } from 'react';
import { Box, Typography, Button, TextField, InputAdornment, IconButton, Chip, CircularProgress, LinearProgress } from '@mui/material';
import AddIcon from '@mui/icons-material/Add';
import SearchIcon from '@mui/icons-material/Search';
import MovieIcon from '@mui/icons-material/Movie';
import StopIcon from '@mui/icons-material/Stop';
import PauseIcon from '@mui/icons-material/Pause';
import PlayArrowIcon from '@mui/icons-material/PlayArrow';
import { VideoCard } from './VideoCard';
import { apiGet, apiPost } from '../../lib/api';

const VIDEOS_URL = '/api/playlist/videos';
const PLAYBACK_URL = '/api/playlist/playback';

export const VideoManager = () => {
    const [videos, setVideos] = useState([]);
    const [selectedVideos, setSelectedVideos] = useState([]);
    const [searchQuery, setSearchQuery] = useState('');
    const [loading, setLoading] = useState(true);
    const [uploading, setUploading] = useState(false);
    const [playback, setPlayback] = useState({ status: 'stopped', video_id: null });
    const fileInputRef = useRef(null);

    const fetchVideos = useCallback(async () => {
        try {
            const res = await apiGet(VIDEOS_URL);
            if (res.ok) setVideos(await res.json());
        } catch (e) {
            console.error('fetch videos failed', e);
        } finally {
            setLoading(false);
        }
    }, []);

    const fetchPlayback = useCallback(async () => {
        try {
            const res = await apiGet(PLAYBACK_URL);
            if (res.ok) setPlayback(await res.json());
        } catch (e) { /* ignore polling errors */ }
    }, []);

    useEffect(() => { fetchVideos(); }, [fetchVideos]);
    useEffect(() => {
        fetchPlayback();
        const id = setInterval(fetchPlayback, 2000);
        return () => clearInterval(id);
    }, [fetchPlayback]);

    const handleSelect = (videoId) => {
        setSelectedVideos(prev =>
            prev.includes(videoId) ? prev.filter(id => id !== videoId) : [...prev, videoId]
        );
    };

    const handleDelete = async (videoId) => {
        if (!window.confirm('この動画を削除しますか?')) return;
        try {
            const res = await fetch(`${VIDEOS_URL}/${videoId}`, { method: 'DELETE' });
            if (res.ok) {
                setVideos(prev => prev.filter(v => v.id !== videoId));
                setSelectedVideos(prev => prev.filter(id => id !== videoId));
            } else {
                alert('削除に失敗しました');
            }
        } catch (e) {
            alert('削除エラー: ' + e);
        }
    };

    const handleInfo = (video) => {
        alert(
            `タイトル: ${video.title}\n` +
            `長さ: ${Math.floor((video.duration_ms || 0) / 1000)}s\n` +
            `解像度: ${video.width}x${video.height} @ ${video.fps}fps\n` +
            `サイズ: ${((video.size_bytes || 0) / 1024 / 1024).toFixed(2)}MB\n` +
            `形式: ${video.codec || '-'}`
        );
    };

    const handlePlay = async (video) => {
        try {
            const res = await apiPost(`/api/playlist/play/${video.id}`, {});
            if (res.ok) {
                fetchPlayback();
            } else {
                alert('再生開始に失敗しました: ' + (await res.text()));
            }
        } catch (e) {
            alert('再生エラー: ' + e);
        }
    };

    const handleStop = async () => {
        try { await apiPost(`${PLAYBACK_URL}/stop`, {}); } finally { fetchPlayback(); }
    };

    const handlePauseToggle = async () => {
        try { await apiPost(`${PLAYBACK_URL}/pause`, {}); } finally { fetchPlayback(); }
    };

    const handleUploadClick = () => fileInputRef.current?.click();

    const handleFileChange = async (e) => {
        const file = e.target.files?.[0];
        if (!file) return;
        const fd = new FormData();
        fd.append('file', file);
        fd.append('title', file.name);
        setUploading(true);
        try {
            // FormData は Content-Type を自動設定させるため fetch を直接使う
            const res = await fetch(VIDEOS_URL, { method: 'POST', body: fd });
            if (res.ok) {
                await fetchVideos();
            } else {
                alert('アップロード失敗: ' + (await res.text()));
            }
        } catch (err) {
            alert('アップロードエラー: ' + err);
        } finally {
            setUploading(false);
            e.target.value = '';
        }
    };

    const totalSize = videos.reduce((sum, v) => sum + (v.size_bytes || 0), 0);
    const filteredVideos = videos.filter(v =>
        (v.title || '').toLowerCase().includes(searchQuery.toLowerCase())
    );
    const playingId = playback.status !== 'stopped' ? playback.video_id : null;

    return (
        <Box sx={{ width: '100%', height: '100%', display: 'flex', flexDirection: 'column', bgcolor: 'rgba(20, 27, 45, 0.9)', position: 'relative' }}>
            {/* 非表示の file input */}
            <input ref={fileInputRef} type="file" accept="video/*" hidden onChange={handleFileChange} />

            {/* Header */}
            <Box sx={{ p: 2, borderBottom: '1px solid rgba(0, 229, 255, 0.2)', bgcolor: 'rgba(20, 27, 45, 0.95)', flexShrink: 0 }}>
                <Box sx={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', mb: 2 }}>
                    <Typography variant="h5" sx={{ color: 'primary.main', letterSpacing: '0.15em', fontWeight: 700, fontFamily: '"Source Code Pro", monospace', display: 'flex', alignItems: 'center', gap: 1 }}>
                        <Box component="span" sx={{ fontSize: '1.5rem' }}>▤</Box>
                        VIDEO LIBRARY
                    </Typography>
                    <IconButton
                        onClick={handleUploadClick}
                        disabled={uploading}
                        sx={{
                            bgcolor: 'rgba(0, 229, 255, 0.1)', border: '2px solid #00E5FF', color: 'primary.main',
                            '&:hover': { bgcolor: 'rgba(0, 229, 255, 0.2)', boxShadow: '0 0 15px rgba(0, 229, 255, 0.5)' }
                        }}
                    >
                        {uploading ? <CircularProgress size={20} sx={{ color: 'primary.main' }} /> : <AddIcon />}
                    </IconButton>
                </Box>

                {uploading && <LinearProgress sx={{ mb: 1 }} />}

                {/* Search Bar */}
                <TextField
                    fullWidth
                    placeholder="動画を検索..."
                    value={searchQuery}
                    onChange={(e) => setSearchQuery(e.target.value)}
                    size="small"
                    InputProps={{
                        startAdornment: (
                            <InputAdornment position="start">
                                <SearchIcon sx={{ color: 'primary.main' }} />
                            </InputAdornment>
                        ),
                    }}
                    sx={{
                        '& .MuiOutlinedInput-root': {
                            bgcolor: 'rgba(20, 27, 45, 0.5)', color: '#fff',
                            '& fieldset': { borderColor: 'rgba(0, 229, 255, 0.3)' },
                            '&:hover fieldset': { borderColor: 'rgba(0, 229, 255, 0.5)' },
                            '&.Mui-focused fieldset': { borderColor: 'primary.main' }
                        }
                    }}
                />
            </Box>

            {/* NOW STREAMING バー */}
            {playback.status !== 'stopped' && (
                <Box sx={{
                    px: 2, py: 1, display: 'flex', alignItems: 'center', gap: 1,
                    bgcolor: 'rgba(0, 255, 65, 0.08)', borderBottom: '1px solid rgba(0, 255, 65, 0.3)', flexShrink: 0
                }}>
                    <Box component="span" sx={{
                        width: 8, height: 8, borderRadius: '50%',
                        bgcolor: playback.status === 'playing' ? '#00FF41' : '#FFB300',
                        animation: playback.status === 'playing' ? 'blink 1s infinite' : 'none',
                        '@keyframes blink': { '50%': { opacity: 0.3 } }
                    }} />
                    <Typography variant="caption" sx={{ color: '#00FF41', fontFamily: '"Source Code Pro", monospace', flex: 1 }}>
                        {playback.status === 'playing' ? 'STREAMING' : 'PAUSED'}
                        {' · '}
                        {(videos.find(v => v.id === playback.video_id) || {}).title || `#${playback.video_id ?? '-'}`}
                    </Typography>
                    <IconButton size="small" onClick={handlePauseToggle} sx={{ color: 'primary.main' }}>
                        {playback.status === 'playing' ? <PauseIcon fontSize="small" /> : <PlayArrowIcon fontSize="small" />}
                    </IconButton>
                    <IconButton size="small" onClick={handleStop} sx={{ color: 'error.main' }}>
                        <StopIcon fontSize="small" />
                    </IconButton>
                </Box>
            )}

            {/* Video List */}
            <Box
                sx={{
                    flex: 1, overflow: 'auto', p: 2, touchAction: 'pan-y',
                    '&::-webkit-scrollbar': { width: '8px' },
                    '&::-webkit-scrollbar-track': { background: 'rgba(0, 0, 0, 0.2)' },
                    '&::-webkit-scrollbar-thumb': { background: 'rgba(0, 229, 255, 0.3)', borderRadius: '4px', '&:hover': { background: 'rgba(0, 229, 255, 0.5)' } }
                }}
                onTouchStart={(e) => e.stopPropagation()}
                onTouchMove={(e) => e.stopPropagation()}
            >
                {loading ? (
                    <Box sx={{ textAlign: 'center', py: 8, color: 'text.secondary' }}>
                        <CircularProgress sx={{ color: 'primary.main' }} />
                    </Box>
                ) : filteredVideos.length === 0 ? (
                    <Box sx={{ textAlign: 'center', py: 8, color: 'text.secondary' }}>
                        <MovieIcon sx={{ fontSize: 80, opacity: 0.2, mb: 2 }} />
                        <Typography variant="h6" sx={{ mb: 1 }}>
                            {searchQuery ? '該当する動画がありません' : 'まだ動画がありません'}
                        </Typography>
                        <Typography variant="body2" sx={{ mb: 3 }}>
                            {searchQuery ? 'キーワードを変えてください' : '右上の + から動画をアップロード'}
                        </Typography>
                        {!searchQuery && (
                            <Button
                                variant="outlined" startIcon={<AddIcon />} onClick={handleUploadClick} disabled={uploading}
                                sx={{ borderColor: 'primary.main', color: 'primary.main', '&:hover': { borderColor: '#fff', bgcolor: 'rgba(0, 229, 255, 0.1)' } }}
                            >
                                動画をアップロード
                            </Button>
                        )}
                    </Box>
                ) : (
                    <Box sx={{
                        display: 'grid',
                        gridTemplateColumns: 'repeat(auto-fill, minmax(140px, 1fr))', gap: 2,
                        '@media (min-width: 600px)': { gridTemplateColumns: 'repeat(auto-fill, minmax(160px, 1fr))' }
                    }}>
                        {filteredVideos.map((video) => (
                            <VideoCard
                                key={video.id}
                                video={video}
                                selected={selectedVideos.includes(video.id)}
                                playing={video.id === playingId}
                                onSelect={() => handleSelect(video.id)}
                                onPlay={() => handlePlay(video)}
                                onInfo={() => handleInfo(video)}
                                onDelete={() => handleDelete(video.id)}
                            />
                        ))}
                    </Box>
                )}
            </Box>

            {/* Footer */}
            <Box sx={{ p: 2, borderTop: '1px solid rgba(0, 229, 255, 0.2)', bgcolor: 'rgba(20, 27, 45, 0.95)', flexShrink: 0 }}>
                <Box sx={{ display: 'flex', gap: 1, flexWrap: 'wrap' }}>
                    <Chip
                        icon={<MovieIcon sx={{ fontSize: 16 }} />}
                        label={`${videos.length} 本`}
                        size="small"
                        sx={{ bgcolor: 'rgba(0, 229, 255, 0.1)', color: 'primary.main', border: '1px solid rgba(0, 229, 255, 0.3)', fontFamily: '"Source Code Pro", monospace' }}
                    />
                    <Chip
                        label={`${(totalSize / 1024 / 1024).toFixed(1)} MB`}
                        size="small"
                        sx={{ bgcolor: 'rgba(0, 229, 255, 0.1)', color: 'primary.main', border: '1px solid rgba(0, 229, 255, 0.3)', fontFamily: '"Source Code Pro", monospace' }}
                    />
                </Box>
            </Box>
        </Box>
    );
};
