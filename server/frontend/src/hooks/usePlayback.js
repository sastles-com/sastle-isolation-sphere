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

    // kind でライブラリを分割 (素材動画 / パターン動画)。旧レコードは kind 未設定でも 'video' 扱い。
    const materialVideos = videos.filter((v) => (v.kind || 'video') === 'video');
    const patterns = videos.filter((v) => v.kind === 'pattern');

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
        // playing → 一時停止 / paused → 再開 (どちらも /playback/pause の toggle で処理)。
        // それ以外 (stopped / error / 未知) はクリーンに /playback/start で開始する。
        // 以前は stopped 以外を一律 pauseToggle に流していたため、error 状態では
        // toggle() が無反応となり再生ボタンが死ぬ (core に何も送出されない) 不具合があった。
        if (playback.status === 'playing' || playback.status === 'paused') await pauseToggle();
        else await play();
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

    // トラック送り/戻し。サーバーに skip API が無いため、アクティブ (再生中) プレイリストの
    // items を取得し、現在の video_id の前後を /play/{id} で直接再生する (仕様 §2.5 の代替実装)。
    const skip = useCallback(async (dir) => {
        const pid = playback.playlist_id ?? activeId;
        if (!pid) return false;
        let items;
        try {
            const r = await apiGet(`${PB}/playlists/${pid}`);
            if (!r.ok) return false;
            items = (await r.json()).items || [];
        } catch { return false; }
        if (items.length === 0) return false;
        const ids = items.map((it) => it.id); // items[].id = videos.id
        const cur = ids.indexOf(playback.video_id);
        // 現在位置不明なら先頭/末尾から。ループ考慮で剰余送り。
        const base = cur >= 0 ? cur : (dir > 0 ? -1 : 0);
        const next = ((base + dir) % ids.length + ids.length) % ids.length;
        return playVideo(ids[next]);
    }, [playback.playlist_id, playback.video_id, activeId, playVideo]);

    const skipNext = useCallback(() => skip(1), [skip]);
    const skipPrev = useCallback(() => skip(-1), [skip]);

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

    // 動画/パターンをプレイリスト末尾に追加 (POST /playlists/{pid}/items)
    const addToPlaylist = useCallback(async (pid, videoId) => {
        const r = await apiPost(`${PB}/playlists/${pid}/items`, { video_id: videoId });
        if (r.ok) fetchPlaylists();  // item_count を更新
        return r.ok;
    }, [fetchPlaylists]);

    const uploadVideo = useCallback(async (file, kind = 'video') => {
        const fd = new FormData();
        fd.append('file', file);
        fd.append('title', file.name);
        if (kind && kind !== 'video') fd.append('kind', kind);
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
        playback, playlists, videos, materialVideos, patterns, settings,
        isPlaying, isStreaming, loopOn, activeId, currentPlaylist, currentVideo,
        play, pauseToggle, togglePlay, stop, toggleLoop, setActivePlaylist,
        activateAndPlay, playVideo, skipNext, skipPrev,
        createPlaylist, deletePlaylist, addToPlaylist, uploadVideo, deleteVideo,
        refresh: fetchPlayback, refreshPlaylists: fetchPlaylists, refreshVideos: fetchVideos,
    };
};
