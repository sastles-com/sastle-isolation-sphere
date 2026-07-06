import React, { useCallback, useEffect, useRef, useState } from 'react';
import { motion } from 'framer-motion';
import { useWebSocket } from '../contexts/WebSocketContext';
import { useStateUpdate } from '../hooks/useSphereState';
import { usePlayback } from '../hooks/usePlayback';
import { useDynamicAccent } from '../hooks/useDynamicAccent';
import { useParamSender } from '../hooks/useParamSender';
import { SphereStage } from '../components/stage/SphereStage';
import { StatusBar } from '../components/stage/StatusBar';
import { NowPlaying } from '../components/stage/NowPlaying';
import { TransportDock } from '../components/stage/TransportDock';
import { EdgeParamGauge } from '../components/stage/EdgeParamGauge';
import { GlassSheet } from '../components/sheets/GlassSheet';
import { LibrarySheet } from '../components/sheets/LibrarySheet';
import { ControlDrawer } from '../components/sheets/ControlDrawer';

const LIB_PEEK = 28;           // LIBRARY のグラブハンドル露出高さ
const LIB_DETENTS = [0.55, 1]; // half / full
const CTL_DETENTS = [0.75];    // CONTROL DRAWER は 1 デテント
const EDGE_RATIO = 0.2;        // 左右端 20% をエッジドラッグ領域とする (仕様 §2.4)

const clamp = (v, lo, hi) => Math.min(hi, Math.max(lo, v));

/**
 * SpherePlayer — UI v2 のルート。STAGE (常駐) + LIBRARY SHEET + CONTROL DRAWER。
 * タブバーは廃止し、画面遷移はすべてシートの出し入れで表現する。
 */
export const SpherePlayer = () => {
    const { isConnected } = useWebSocket();
    const pb = usePlayback();
    const sendParam = useParamSender();

    // ---- WS STATE_UPDATE からのライブ状態 ----
    const [brightness, setBrightness] = useState(80);
    const [hue, setHue] = useState(190);
    const [system, setSystem] = useState(null);
    const brtDragging = useRef(false);
    const hueDragging = useRef(false);

    // hue → --sphere-hue (UI アクセント / オーロラが即応)
    useDynamicAccent(hue);

    useStateUpdate((payload) => {
        if (payload.params) {
            // ドラッグ中はエコーバックで自己上書きしない (仕様 §5)
            if (!brtDragging.current && typeof payload.params.brightness === 'number') {
                setBrightness(payload.params.brightness);
            }
            if (!hueDragging.current && typeof payload.params.hue === 'number') {
                setHue(payload.params.hue);
            }
        }
        if (payload.system) setSystem(payload.system);
    });

    const handleBrightness = useCallback((v) => {
        setBrightness(v);
        sendParam('brightness', v);
    }, [sendParam]);
    const handleHue = useCallback((v) => {
        setHue(v);
        sendParam('hue', v);
    }, [sendParam]);

    // ---- シート開閉状態 (null=閉 / detent index) ----
    const [libOpen, setLibOpen] = useState(null);
    const [ctlOpen, setCtlOpen] = useState(null);
    const libRef = useRef(null);
    const ctlRef = useRef(null);
    const stageRef = useRef(null);

    // ---- エッジドラッグのゲージ表示 (右端=brightness / 左端=hue) ----
    const [gauge, setGauge] = useState(null); // { side, param, value, min, max, unit, visible }
    const gaugeFadeTimer = useRef(null);

    // ---- STAGE 上のパンをシート転送 / エッジパラメータ操作に振り分ける ----
    // 開始X位置で判定: 左端20%=hue / 右端20%=brightness / 中央60%=シート開閉。
    // (エッジ領域はシートフリックと競合しない, 仕様 §2.4)
    const panMode = useRef(null);      // 'lib' | 'ctl' | 'brightness' | 'hue' | 'ignore' | null
    const panStartVal = useRef(0);     // エッジドラッグ開始時のパラメータ値

    const handlePanStart = useCallback((_, info) => {
        panMode.current = null;
        if (libOpen !== null || ctlOpen !== null) { panMode.current = 'ignore'; return; }
        const rect = stageRef.current?.getBoundingClientRect();
        if (!rect) return;
        const relX = (info.point.x - rect.left) / rect.width;
        if (relX <= EDGE_RATIO) panMode.current = 'hue';
        else if (relX >= 1 - EDGE_RATIO) panMode.current = 'brightness';
        // 中央は最初の pan 移動で縦横を見て確定する (下で処理)
    }, [libOpen, ctlOpen]);

    const startEdgeGauge = useCallback((param) => {
        clearTimeout(gaugeFadeTimer.current);
        if (param === 'brightness') {
            panStartVal.current = brightness;
            brtDragging.current = true;
            setGauge({ side: 'right', param, value: brightness, min: 0, max: 100, unit: '%', visible: true });
        } else {
            panStartVal.current = hue;
            hueDragging.current = true;
            setGauge({ side: 'left', param, value: hue, min: 0, max: 360, unit: '°', visible: true });
        }
    }, [brightness, hue]);

    const handlePan = useCallback((_, info) => {
        if (panMode.current === 'ignore') return;

        // 中央領域: 縦優位ならシート、横優位は無視 (トラック送りは将来)
        if (panMode.current === null) {
            if (Math.abs(info.offset.y) <= Math.abs(info.offset.x)) return;
            const dir = info.offset.y < 0 ? 'lib' : 'ctl';
            panMode.current = dir;
            (dir === 'lib' ? libRef : ctlRef).current?.panStart();
        }

        if (panMode.current === 'lib' || panMode.current === 'ctl') {
            (panMode.current === 'lib' ? libRef : ctlRef).current?.panMove(info.offset.y);
            return;
        }

        // エッジドラッグ: 上方向で増加。ステージ高さいっぱいで全域変化。
        const rect = stageRef.current?.getBoundingClientRect();
        if (!rect) return;
        if (panMode.current === 'brightness') {
            if (!brtDragging.current) startEdgeGauge('brightness');
            const v = Math.round(clamp(panStartVal.current - (info.offset.y / rect.height) * 100, 0, 100));
            handleBrightness(v);
            setGauge((g) => (g ? { ...g, value: v, visible: true } : g));
        } else if (panMode.current === 'hue') {
            if (!hueDragging.current) startEdgeGauge('hue');
            const v = Math.round(clamp(panStartVal.current - (info.offset.y / rect.height) * 360, 0, 360));
            handleHue(v);
            setGauge((g) => (g ? { ...g, value: v, visible: true } : g));
        }
    }, [handleBrightness, handleHue, startEdgeGauge]);

    const handlePanEnd = useCallback((_, info) => {
        const mode = panMode.current;
        panMode.current = null;
        if (mode === 'lib' || mode === 'ctl') {
            (mode === 'lib' ? libRef : ctlRef).current?.panEnd(info.velocity.y);
        } else if (mode === 'brightness' || mode === 'hue') {
            brtDragging.current = false;
            hueDragging.current = false;
            // 離して1s後にゲージをフェードアウト (仕様 §2.4)
            clearTimeout(gaugeFadeTimer.current);
            gaugeFadeTimer.current = setTimeout(() => {
                setGauge((g) => (g ? { ...g, visible: false } : g));
            }, 1000);
        }
    }, []);
    useEffect(() => () => clearTimeout(gaugeFadeTimer.current), []);

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
                ref={stageRef}
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

            {/* エッジドラッグのゲージ (右端=brightness / 左端=hue) */}
            {gauge && (
                <EdgeParamGauge
                    side={gauge.side}
                    param={gauge.param}
                    value={gauge.value}
                    min={gauge.min}
                    max={gauge.max}
                    unit={gauge.unit}
                    visible={gauge.visible}
                />
            )}

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
