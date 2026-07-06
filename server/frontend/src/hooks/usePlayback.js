import { useCallback, useEffect, useState } from 'react';
import { apiGet, apiPost } from '../lib/api';

const PB = '/api/playlist';
const CFG = '/api/config/settings';

/**
 * usePlayback — 再生状態のポーリングと再生操作、ライブラリ (playlists/videos) を集約するフック。
 * playback の真値は MQTT 側 (`GET /playback`) のため 2s ポーリング (現行踏襲)。
 * コンポーネントから直接 fetch しないこと (仕様 §5)。
 */
export const usePlayback = () => {
    const [playback, setPlayback] = useState({
        status: 'stopped', playlist_id: null, video_id: null, loop: true,
    });
    const [playlists, setPlaylists] = useState([]);
    const [videos, setVideos] = useState([]);
    const [settings, setSettings] = useState({ playback: { active_playlist: null, loop: true } });

    const fetchPlayback = useCallback(async () => {
        try { const r = await apiGet(`${PB}/playback`); if (r.ok) setPlayback(await r.json()); } catch { /* offline */ }
    }, []);
    const fetchPlaylists = useCallback(async () => {
        try { const r = await apiGet(`${PB}/playlists`); if (r.ok) setPlaylists(await r.json()); } catch { /* offline */ }
    }, []);
    const fetchVideos = useCallback(async () => {
        try { const r = await apiGet(`${PB}/videos`); if (r.ok) setVideos(await r.json()); } catch { /* offline */ }
    }, []);
    const fetchSettings = useCallback(async () => {
        try { const r = await apiGet(CFG); if (r.ok) setSettings(await r.json()); } catch { /* offline */ }
    }, []);

    useEffect(() => {
        fetchPlaylists(); fetchVideos(); fetchSettings();
    }, [fetchPlaylists, fetchVideos, fetchSettings]);
    useEffect(() => {
        fetchPlayback();
        const id = setInterval(fetchPlayback, 2000);
        return () => clearInterval(id);
    }, [fetchPlayback]);

    const isPlaying = playback.status === 'playing';
    const isStreaming = playback.status !== 'stopped';
    const loopOn = settings.playback?.loop ?? true;
    const activeId = settings.playback?.active_playlist ?? null;

    // アクティブプレイリスト名 (再生中はそちらを優先) / 再生中の動画
    const activePlaylist = playlists.find((p) => p.id === activeId);
    const currentPlaylist =
        playlists.find((p) => p.id === playback.playlist_id) || activePlaylist || null;
    const currentVideo = videos.find((v) => v.id === playback.video_id) || null;

    // ---- 再生制御 ----
    const play = useCallback(async () => {
        const r = await apiPost(`${PB}/playback/start`, {});
        if (!r.ok) console.warn('playback start failed:', await r.text());
        fetchPlayback();
    }, [fetchPlayback]);

    const pauseToggle = useCallback(async () => {
        await apiPost(`${PB}/playback/pause`, {});
        fetchPlayback();
    }, [fetchPlayback]);

    const togglePlay = useCallback(async () => {
        if (playback.status === 'stopped') await play();
        else await pauseToggle();
    }, [playback.status, play, pauseToggle]);

    const stop = useCallback(async () => {
        await apiPost(`${PB}/playback/stop`, {});
        fetchPlayback();
    }, [fetchPlayback]);

    const toggleLoop = useCallback(async () => {
        const next = !loopOn;
        setSettings((s) => ({ ...s, playback: { ...s.playback, loop: next } }));
        await fetch(CFG, {
            method: 'PUT', headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ playback: { loop: next } }),
        });
        fetchPlayback();
    }, [loopOn, fetchPlayback]);

    const setActivePlaylist = useCallback(async (pid) => {
        setSettings((s) => ({ ...s, playback: { ...s.playback, active_playlist: pid } }));
        await fetch(CFG, {
            method: 'PUT', headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ playback: { active_playlist: pid } }),
        });
    }, []);

    // プレイリストをアクティブ化してそのまま再生 (LIBRARY のカードタップ)
    const activateAndPlay = useCallback(async (pid) => {
        await setActivePlaylist(pid);
        const r = await apiPost(`${PB}/playlists/${pid}/play`, {});
        if (!r.ok) console.warn('playlist play failed:', await r.text());
        fetchPlayback();
        return r.ok;
    }, [setActivePlaylist, fetchPlayback]);

    // 素材動画の単発再生 (Videos グリッド)
    const playVideo = useCallback(async (videoId) => {
        const r = await apiPost(`${PB}/play/${videoId}`, {});
        if (!r.ok) console.warn('video play failed:', await r.text());
        fetchPlayback();
        return r.ok;
    }, [fetchPlayback]);

    // ---- ライブラリ CRUD (成功時に一覧を再取得) ----
    const createPlaylist = useCallback(async (name) => {
        const r = await apiPost(`${PB}/playlists`, { name, loop: true });
        if (r.ok) fetchPlaylists();
        return r.ok;
    }, [fetchPlaylists]);

    const deletePlaylist = useCallback(async (pid) => {
        const r = await fetch(`${PB}/playlists/${pid}`, { method: 'DELETE' });
        if (r.ok) fetchPlaylists();
        return r.ok;
    }, [fetchPlaylists]);

    const uploadVideo = useCallback(async (file) => {
        const fd = new FormData();
        fd.append('file', file);
        fd.append('title', file.name);
        // FormData は Content-Type を自動設定させるため fetch を直接使う
        const r = await fetch(`${PB}/videos`, { method: 'POST', body: fd });
        if (r.ok) fetchVideos();
        return r.ok;
    }, [fetchVideos]);

    const deleteVideo = useCallback(async (videoId) => {
        const r = await fetch(`${PB}/videos/${videoId}`, { method: 'DELETE' });
        if (r.ok) { fetchVideos(); fetchPlayback(); }
        return r.ok;
    }, [fetchVideos, fetchPlayback]);

    return {
        playback, playlists, videos, settings,
        isPlaying, isStreaming, loopOn, activeId, currentPlaylist, currentVideo,
        play, pauseToggle, togglePlay, stop, toggleLoop, setActivePlaylist,
        activateAndPlay, playVideo,
        createPlaylist, deletePlaylist, uploadVideo, deleteVideo,
        refresh: fetchPlayback, refreshPlaylists: fetchPlaylists, refreshVideos: fetchVideos,
    };
};
