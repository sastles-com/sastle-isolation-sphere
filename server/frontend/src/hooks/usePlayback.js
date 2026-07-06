import { useCallback, useEffect, useState } from 'react';
import { apiGet, apiPost } from '../lib/api';

const PB = '/api/playlist';
const CFG = '/api/config/settings';

/**
 * usePlayback — 再生状態のポーリングと再生操作を集約するフック。
 * playback の真値は MQTT 側 (`GET /playback`) のため 2s ポーリング (現行踏襲)。
 * コンポーネントから直接 fetch しないこと (仕様 §5)。
 */
export const usePlayback = () => {
    const [playback, setPlayback] = useState({
        status: 'stopped', playlist_id: null, video_id: null, loop: true,
    });
    const [playlists, setPlaylists] = useState([]);
    const [settings, setSettings] = useState({ playback: { active_playlist: null, loop: true } });

    const fetchPlayback = useCallback(async () => {
        try { const r = await apiGet(`${PB}/playback`); if (r.ok) setPlayback(await r.json()); } catch { /* offline */ }
    }, []);
    const fetchPlaylists = useCallback(async () => {
        try { const r = await apiGet(`${PB}/playlists`); if (r.ok) setPlaylists(await r.json()); } catch { /* offline */ }
    }, []);
    const fetchSettings = useCallback(async () => {
        try { const r = await apiGet(CFG); if (r.ok) setSettings(await r.json()); } catch { /* offline */ }
    }, []);

    useEffect(() => { fetchPlaylists(); fetchSettings(); }, [fetchPlaylists, fetchSettings]);
    useEffect(() => {
        fetchPlayback();
        const id = setInterval(fetchPlayback, 2000);
        return () => clearInterval(id);
    }, [fetchPlayback]);

    const isPlaying = playback.status === 'playing';
    const isStreaming = playback.status !== 'stopped';
    const loopOn = settings.playback?.loop ?? true;
    const activeId = settings.playback?.active_playlist ?? null;

    // アクティブプレイリスト名 (再生中はそちらを優先)
    const activePlaylist = playlists.find((p) => p.id === activeId);
    const currentPlaylist =
        playlists.find((p) => p.id === playback.playlist_id) || activePlaylist || null;

    const play = useCallback(async () => {
        const r = await apiPost(`${PB}/playback/start`, {});
        if (!r.ok) console.warn('playback start failed:', await r.text());
        fetchPlayback();
    }, [fetchPlayback]);

    const pauseToggle = useCallback(async () => {
        await apiPost(`${PB}/playback/pause`, {});
        fetchPlayback();
    }, [fetchPlayback]);

    // 停止中は再生開始、再生/一時停止中はトグル
    const togglePlay = useCallback(async () => {
        if (playback.status === 'stopped') await play();
        else await pauseToggle();
    }, [playback.status, play, pauseToggle]);

    const stop = useCallback(async () => {
        await apiPost(`${PB}/playback/stop`, {});
        fetchPlayback();
    }, [fetchPlayback]);

    // ループは config 設定。トグルで永続化 + ライブ反映 (現行踏襲)
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

    return {
        playback, playlists, settings,
        isPlaying, isStreaming, loopOn, activeId, currentPlaylist,
        play, pauseToggle, togglePlay, stop, toggleLoop, setActivePlaylist,
        refresh: fetchPlayback,
    };
};
