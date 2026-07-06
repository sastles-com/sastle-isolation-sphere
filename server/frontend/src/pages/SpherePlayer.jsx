import React, { useCallback, useEffect, useRef, useState } from 'react';
import { motion } from 'framer-motion';
import { useWebSocket } from '../contexts/WebSocketContext';
import { useStateUpdate } from '../hooks/useSphereState';
import { usePlayback } from '../hooks/usePlayback';
import { SphereStage } from '../components/stage/SphereStage';
import { StatusBar } from '../components/stage/StatusBar';
import { NowPlaying } from '../components/stage/NowPlaying';
import { TransportDock } from '../components/stage/TransportDock';
import { GlassSheet } from '../components/sheets/GlassSheet';
import { LibrarySheet } from '../components/sheets/LibrarySheet';
import { ControlDrawer } from '../components/sheets/ControlDrawer';

const LIB_PEEK = 28;           // LIBRARY のグラブハンドル露出高さ
const LIB_DETENTS = [0.55, 1]; // half / full
const CTL_DETENTS = [0.75];    // CONTROL DRAWER は 1 デテント
const PARAM_DEBOUNCE_MS = 60;  // SET_PARAMS 送信デバウンス (仕様 §2.4)

/**
 * SpherePlayer — UI v2 のルート。STAGE (常駐) + LIBRARY SHEET + CONTROL DRAWER。
 * タブバーは廃止し、画面遷移はすべてシートの出し入れで表現する。
 */
export const SpherePlayer = () => {
    const { isConnected, sendMessage } = useWebSocket();
    const pb = usePlayback();

    // ---- WS STATE_UPDATE からのライブ状態 ----
    const [brightness, setBrightness] = useState(80);
    const [hue, setHue] = useState(190);
    const [system, setSystem] = useState(null);
    const brtDragging = useRef(false);

    useStateUpdate((payload) => {
        if (payload.params) {
            // ドラッグ中はエコーバックで自己上書きしない (仕様 §5)
            if (!brtDragging.current && typeof payload.params.brightness === 'number') {
                setBrightness(payload.params.brightness);
            }
            if (typeof payload.params.hue === 'number') setHue(payload.params.hue);
        }
        if (payload.system) setSystem(payload.system);
    });

    // ---- 明るさ: 60ms デバウンスで SET_PARAMS 送信 ----
    const sendTimer = useRef(null);
    const pendingBrt = useRef(null);
    useEffect(() => () => clearTimeout(sendTimer.current), []);
    const handleBrightness = useCallback((v) => {
        setBrightness(v);
        pendingBrt.current = v;
        if (!sendTimer.current) {
            sendTimer.current = setTimeout(() => {
                sendTimer.current = null;
                sendMessage('SET_PARAMS', { brightness: pendingBrt.current });
            }, PARAM_DEBOUNCE_MS);
        }
    }, [sendMessage]);

    // ---- シート開閉状態 (null=閉 / detent index) ----
    const [libOpen, setLibOpen] = useState(null);
    const [ctlOpen, setCtlOpen] = useState(null);
    const libRef = useRef(null);
    const ctlRef = useRef(null);

    // ---- STAGE 上の縦パンをシートへ転送 (物理追従) ----
    // 上フリック→LIBRARY / 下フリック→CONTROL。両シート閉時のみ (開時はシート自身+scrimが担当)。
    const panTarget = useRef(null);
    const handlePanStart = useCallback(() => { panTarget.current = null; }, []);
    const handlePan = useCallback((_, info) => {
        if (libOpen !== null || ctlOpen !== null) return;
        if (!panTarget.current) {
            if (Math.abs(info.offset.y) <= Math.abs(info.offset.x)) return; // 横フリックは P2 (トラック送り)
            const dir = info.offset.y < 0 ? 'lib' : 'ctl';
            panTarget.current = dir;
            (dir === 'lib' ? libRef : ctlRef).current?.panStart();
        }
        (panTarget.current === 'lib' ? libRef : ctlRef).current?.panMove(info.offset.y);
    }, [libOpen, ctlOpen]);
    const handlePanEnd = useCallback((_, info) => {
        if (!panTarget.current) return;
        (panTarget.current === 'lib' ? libRef : ctlRef).current?.panEnd(info.velocity.y);
        panTarget.current = null;
    }, []);

    // ---- PC キーボードフォールバック (Space / L / ,) ----
    const togglePlayRef = useRef(pb.togglePlay);
    togglePlayRef.current = pb.togglePlay;
    useEffect(() => {
        const onKey = (e) => {
            if (e.target.closest?.('input, textarea, select, [role="slider"]')) return;
            if (e.code === 'Space') { e.preventDefault(); togglePlayRef.current(); }
            else if (e.key === 'l' || e.key === 'L') setLibOpen((o) => (o === null ? 0 : null));
            else if (e.key === ',') setCtlOpen((o) => (o === null ? 0 : null));
        };
        window.addEventListener('keydown', onKey);
        return () => window.removeEventListener('keydown', onKey);
    }, []);

    // 再生中の動画名: video_id からタイトル解決、無ければパス末尾
    const videoName = pb.currentVideo?.title
        || (pb.playback.path ? pb.playback.path.split('/').pop() : null);

    return (
        <div style={{
            position: 'fixed', inset: 0,
            height: '100dvh',
            background: 'var(--ground)',
            overflow: 'hidden',
        }}>
            {/* Layer 0: 3D球体 + オーロラ (常に見えている) */}
            <SphereStage hue={hue} brightness={brightness} />

            {/* Layer 1: STAGE オーバーレイ (720px 以上では中央 640px カラム) */}
            <motion.div
                onPanStart={handlePanStart}
                onPan={handlePan}
                onPanEnd={handlePanEnd}
                style={{
                    position: 'absolute', inset: 0, zIndex: 1,
                    display: 'flex', flexDirection: 'column',
                    maxWidth: 640, margin: '0 auto',
                    paddingBottom: `calc(${LIB_PEEK}px + env(safe-area-inset-bottom))`,
                    touchAction: 'none',
                }}
            >
                <StatusBar
                    isConnected={isConnected}
                    deviceOnline={system != null}
                    playbackStatus={pb.playback.status}
                    system={system}
                    onOpenControl={() => setCtlOpen(0)}
                />
                <div style={{ flex: 1 }} />
                <NowPlaying
                    playlistName={pb.currentPlaylist?.name}
                    videoName={videoName}
                    isStreaming={pb.isStreaming}
                />
                <div style={{ height: 12 }} />
                <TransportDock
                    isPlaying={pb.isPlaying}
                    isStreaming={pb.isStreaming}
                    loopOn={pb.loopOn}
                    onTogglePlay={pb.togglePlay}
                    onStop={pb.stop}
                    onToggleLoop={pb.toggleLoop}
                    brightness={brightness}
                    onBrightnessChange={handleBrightness}
                    onBrightnessDragState={(d) => { brtDragging.current = d; }}
                />
            </motion.div>

            {/* LIBRARY SHEET (下端から上フリック) */}
            <GlassSheet
                ref={libRef}
                side="bottom"
                detents={LIB_DETENTS}
                peek={LIB_PEEK}
                open={libOpen}
                onOpenChange={setLibOpen}
                zIndex={20}
            >
                <LibrarySheet pb={pb} />
            </GlassSheet>

            {/* CONTROL DRAWER (上端から下フリック / ⚙︎) */}
            <GlassSheet
                ref={ctlRef}
                side="top"
                detents={CTL_DETENTS}
                peek={0}
                open={ctlOpen}
                onOpenChange={setCtlOpen}
                zIndex={30}
            >
                <ControlDrawer />
            </GlassSheet>
        </div>
    );
};
