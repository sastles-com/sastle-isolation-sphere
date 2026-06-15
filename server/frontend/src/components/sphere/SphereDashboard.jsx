import React, { useState, useEffect, useCallback } from 'react';
import { Box, Typography, Chip, IconButton, Select, MenuItem, FormControl } from '@mui/material';
import PlayArrowIcon from '@mui/icons-material/PlayArrow';
import PauseIcon from '@mui/icons-material/Pause';
import StopIcon from '@mui/icons-material/Stop';
import RepeatIcon from '@mui/icons-material/Repeat';
import PlaylistPlayIcon from '@mui/icons-material/PlaylistPlay';
import { HoloSphere } from './HoloSphere';
import { CompactSlider } from '../ui/CompactSlider';
import { apiGet, apiPost } from '../../lib/api';

const PB = '/api/playlist';
const CFG = '/api/config/settings';

/**
 * SphereDashboard - SPHERE タブ(トップポータル)
 * config の事前設定(active playlist / loop)を読み、ライブ操作のみ提供:
 * プレイリスト選択・再生/停止/一時停止/ループ・明るさ調整・実再生状況の表示。
 */
export const SphereDashboard = ({ rotation, brightness, color, onParamChange }) => {
    const [playlists, setPlaylists] = useState([]);
    const [settings, setSettings] = useState({ playback: { active_playlist: null, loop: true } });
    const [playback, setPlayback] = useState({ status: 'stopped', playlist_id: null, video_id: null, loop: true });

    const activeId = settings.playback?.active_playlist ?? '';
    const isPlaying = playback.status === 'playing';
    const isStreaming = playback.status !== 'stopped';

    const fetchPlaylists = useCallback(async () => {
        try { const r = await apiGet(`${PB}/playlists`); if (r.ok) setPlaylists(await r.json()); } catch (e) { /* */ }
    }, []);
    const fetchSettings = useCallback(async () => {
        try { const r = await apiGet(CFG); if (r.ok) setSettings(await r.json()); } catch (e) { /* */ }
    }, []);
    const fetchPlayback = useCallback(async () => {
        try { const r = await apiGet(`${PB}/playback`); if (r.ok) setPlayback(await r.json()); } catch (e) { /* */ }
    }, []);

    useEffect(() => { fetchPlaylists(); fetchSettings(); }, [fetchPlaylists, fetchSettings]);
    useEffect(() => {
        fetchPlayback();
        const id = setInterval(fetchPlayback, 2000);
        return () => clearInterval(id);
    }, [fetchPlayback]);

    // プレイリスト選択 → config の active_playlist を更新(永続化)
    const handleSelect = async (e) => {
        const pid = e.target.value;
        setSettings(s => ({ ...s, playback: { ...s.playback, active_playlist: pid } }));
        await fetch(CFG, {
            method: 'PUT', headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ playback: { active_playlist: pid } })
        });
    };

    const handlePlay = async () => {
        const r = await apiPost(`${PB}/playback/start`, {});
        if (!r.ok) alert('再生開始に失敗: ' + (await r.text()));
        fetchPlayback();
    };
    const handleStop = async () => { await apiPost(`${PB}/playback/stop`, {}); fetchPlayback(); };
    const handlePauseToggle = async () => { await apiPost(`${PB}/playback/pause`, {}); fetchPlayback(); };

    // ループは config 設定。トグルで永続化 + ライブ反映(PUT side で streamer にも適用)。
    const handleLoopToggle = async () => {
        const next = !(settings.playback?.loop ?? true);
        setSettings(s => ({ ...s, playback: { ...s.playback, loop: next } }));
        await fetch(CFG, {
            method: 'PUT', headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ playback: { loop: next } })
        });
        fetchPlayback();
    };

    const activePlaylist = playlists.find(p => p.id === activeId);
    const currentName = (playlists.find(p => p.id === playback.playlist_id) || activePlaylist || {}).name || '—';
    const loopOn = settings.playback?.loop ?? true;

    return (
        <Box sx={{ height: '100%', position: 'relative', overflow: 'hidden', bgcolor: 'black' }}>
            {/* Layer 1: 球体可視化 */}
            <Box sx={{ position: 'absolute', inset: 0, zIndex: 0, '& canvas': { display: 'block' } }}>
                <HoloSphere rotation={rotation} brightness={brightness} color={color} />
            </Box>

            {/* Layer 2: ステータスHUD */}
            <Box sx={{ position: 'absolute', top: 10, left: 10, zIndex: 1, display: 'flex', flexDirection: 'column', gap: 0.5, pointerEvents: 'none' }}>
                <Chip
                    label={isPlaying ? 'STREAMING' : (playback.status === 'paused' ? 'PAUSED' : 'IDLE')}
                    size="small"
                    sx={{
                        bgcolor: isPlaying ? '#00ff00' : (playback.status === 'paused' ? '#FFB300' : '#555'),
                        color: isPlaying ? '#000' : '#111', fontWeight: 'bold', height: 20, fontSize: '0.65rem',
                        boxShadow: isPlaying ? '0 0 8px #00ff00' : 'none', transition: 'all 0.3s',
                    }}
                />
            </Box>

            {/* Layer 3: ライブ操作HUD */}
            <Box
                sx={{
                    position: 'absolute', bottom: 0, left: 0, right: 0, zIndex: 2, p: 2,
                    background: 'linear-gradient(to top, rgba(0,0,0,0.92) 0%, rgba(0,0,0,0.7) 60%, transparent 100%)',
                    display: 'flex', flexDirection: 'column', gap: 1.5, pb: 3,
                }}
                onMouseDown={(e) => e.stopPropagation()}
                onTouchStart={(e) => e.stopPropagation()}
            >
                {/* プレイリスト選択 */}
                <FormControl size="small" fullWidth data-no-swipe>
                    <Select
                        value={playlists.some(p => p.id === activeId) ? activeId : ''}
                        onChange={handleSelect}
                        displayEmpty
                        startAdornment={<PlaylistPlayIcon sx={{ color: 'primary.main', mr: 1 }} />}
                        sx={{
                            color: '#fff', bgcolor: 'rgba(0,0,0,0.5)', fontFamily: '"Source Code Pro", monospace',
                            '& .MuiOutlinedInput-notchedOutline': { borderColor: 'rgba(0,229,255,0.4)' },
                            '&:hover .MuiOutlinedInput-notchedOutline': { borderColor: 'primary.main' },
                            '& .MuiSvgIcon-root': { color: 'primary.main' },
                        }}
                    >
                        <MenuItem value="" disabled>プレイリストを選択</MenuItem>
                        {playlists.map(p => (
                            <MenuItem key={p.id} value={p.id}>{p.name} ({p.item_count ?? 0})</MenuItem>
                        ))}
                    </Select>
                </FormControl>

                {/* 再生状況 + 操作 */}
                <Box sx={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
                    <Box sx={{ minWidth: 0, flex: 1, mr: 1 }}>
                        <Typography variant="caption" sx={{ color: 'primary.main', display: 'block', lineHeight: 1 }}>
                            {isStreaming ? (playback.playlist_id ? 'PLAYLIST' : 'VIDEO') : 'READY'}
                        </Typography>
                        <Typography variant="body2" noWrap sx={{ color: '#fff', fontWeight: 600 }}>
                            {currentName}
                        </Typography>
                    </Box>
                    <Box sx={{ display: 'flex', gap: 1, alignItems: 'center' }}>
                        <IconButton onClick={handleLoopToggle} size="small" title={loopOn ? 'ループ ON' : 'ループ OFF'}
                            sx={{ color: loopOn ? '#00FF41' : 'text.secondary', border: '1px solid', borderColor: loopOn ? '#00FF41' : 'rgba(255,255,255,0.2)' }}>
                            <RepeatIcon fontSize="small" />
                        </IconButton>
                        {/* 再生 or 一時停止/再開 */}
                        {isStreaming ? (
                            <IconButton onClick={handlePauseToggle} size="small"
                                sx={{ color: isPlaying ? '#000' : 'primary.main', bgcolor: isPlaying ? 'primary.main' : 'rgba(0,229,255,0.1)', border: '1px solid', borderColor: 'primary.main' }}>
                                {isPlaying ? <PauseIcon fontSize="small" /> : <PlayArrowIcon fontSize="small" />}
                            </IconButton>
                        ) : (
                            <IconButton onClick={handlePlay} size="small" disabled={!activeId}
                                sx={{ color: 'primary.main', bgcolor: 'rgba(0,229,255,0.1)', border: '1px solid', borderColor: 'primary.main' }}>
                                <PlayArrowIcon fontSize="small" />
                            </IconButton>
                        )}
                        <IconButton onClick={handleStop} size="small" disabled={!isStreaming}
                            sx={{ color: 'error.main', border: '1px solid', borderColor: 'rgba(255,23,68,0.5)' }}>
                            <StopIcon fontSize="small" />
                        </IconButton>
                    </Box>
                </Box>

                {/* ライブ明るさ / 色相 */}
                <Box sx={{ display: 'flex', flexDirection: 'column', gap: 0 }}>
                    <CompactSlider label="BRIGHTNESS" value={brightness} min={0} max={100}
                        onChange={(val) => onParamChange('brightness', val)} />
                    <CompactSlider label="HUE" value={color} min={0} max={360} unit="°"
                        onChange={(val) => onParamChange('hue', val)} />
                </Box>
            </Box>
        </Box>
    );
};
