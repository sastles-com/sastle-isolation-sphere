import React, { useState, useRef } from 'react';
import { useSwipeable } from 'react-swipeable';
import { Box, Typography, BottomNavigation, BottomNavigationAction, Tabs, Tab } from '@mui/material';
import PublicIcon from '@mui/icons-material/Public';
import PlaylistPlayIcon from '@mui/icons-material/PlaylistPlay';
import TuneIcon from '@mui/icons-material/Tune';
import SportsEsportsIcon from '@mui/icons-material/SportsEsports';
import { SphereDashboard } from '../components/sphere/SphereDashboard';
import { StatusFooter } from '../components/layout/StatusFooter';
import { PlaylistManager } from '../components/playlist/PlaylistManager';
import { VideoManager } from '../components/playlist/VideoManager';
import { SphereControl } from '../components/control/SphereControl';
import { PatternControl } from '../components/control/PatternControl';
import { ConfigEditor } from '../components/params/ConfigEditor';
import { ParamsEditor } from '../components/params/ParamsEditor';
import { LogPanel } from '../components/debug/LogPanel';
import { TAB_CONFIG } from '../config/tabConfig';
import { useWebSocket } from '../contexts/WebSocketContext';
import { useStateUpdate } from '../hooks/useSphereState';

// 下部タブのアイコン (TAB_CONFIG の id に対応)
const TAB_ICONS = {
    sphere: <PublicIcon />,
    playlist: <PlaylistPlayIcon />,
    params: <TuneIcon />,
    control: <SportsEsportsIcon />,
};

export const Dashboard = () => {
    const { isConnected, sendMessage } = useWebSocket();

    const [currentTab, setCurrentTab] = useState(0);
    const [subTabIndex, setSubTabIndex] = useState(0);
    const [brightness, setBrightness] = useState(80);
    const [color, setColor] = useState(120);
    const [playbackStatus, setPlaybackStatus] = useState('stopped'); // "playing" | "paused" | "stopped"

    // Sync state from WebSocket
    useStateUpdate(({ playback, params }) => {
        if (playback) {
            setPlaybackStatus(playback.status || 'stopped');
        }
        if (params) {
            setBrightness(params.brightness);
            setColor(params.hue);
        }
    });

    const handleTogglePlay = () => sendMessage('SET_PLAYBACK', { action: 'toggle' });
    const handleStop = () => sendMessage('SET_PLAYBACK', { action: 'stop' });

    const handleParamChange = (key, value) => {
        if (key === 'brightness') setBrightness(value);
        if (key === 'hue') setColor(value);
        sendMessage('SET_PARAMS', { [key]: value });
    };

    const changeTab = (index) => {
        const n = TAB_CONFIG.length;
        setCurrentTab(((index % n) + n) % n);  // ラップ
        setSubTabIndex(0);  // タブ切替時はサブタブを先頭へ
    };

    // 横スワイプで主タブ移動 (ボトムタブバーと併用)。縦スクロールは妨げない。
    // スライダ/ジョイスティック等の操作部品の上で始まったジェスチャーは
    // タブ移動として拾わない (誤作動防止)。
    const swipeBlocked = useRef(false);
    const swipeHandlers = useSwipeable({
        onTouchStartOrOnMouseDown: ({ event }) => {
            const el = event.target;
            swipeBlocked.current = !!(el && el.closest && el.closest(
                '[data-no-swipe], .MuiSlider-root, [role="slider"], input, textarea, button, a'
            ));
        },
        onSwipedLeft: () => { if (!swipeBlocked.current) changeTab(currentTab + 1); },
        onSwipedRight: () => { if (!swipeBlocked.current) changeTab(currentTab - 1); },
        trackMouse: true,
        preventScrollOnSwipe: false,
        delta: 50,
    });

    // --- 各タブの内容レンダラー (表示中のものだけ呼ばれる=マウントされる) ---
    const renderSphereContent = () => (
        <SphereDashboard
            rotation={0}
            brightness={brightness}
            color={color}
            playbackStatus={playbackStatus}
            onTogglePlay={handleTogglePlay}
            onStop={handleStop}
            onParamChange={handleParamChange}
        />
    );

    const renderPlaylistContent = (subTab) => {
        const isPlaying = playbackStatus === 'playing';
        return subTab.id === 'playlists'
            ? <PlaylistManager isPlaying={isPlaying} onTogglePlay={handleTogglePlay} onStop={handleStop} />
            : <VideoManager />;
    };

    const renderParamsContent = (subTab) =>
        subTab.id === 'config' ? <ConfigEditor /> : <ParamsEditor />;

    const renderControlContent = (subTab) => {
        if (subTab.id === 'sphere_control') return <SphereControl />;
        if (subTab.id === 'debug_log') return <LogPanel />;
        return <PatternControl />;
    };

    const contentRenderers = {
        sphere: renderSphereContent,
        playlist: renderPlaylistContent,
        params: renderParamsContent,
        control: renderControlContent,
    };

    const tab = TAB_CONFIG[currentTab];
    const hasSubTabs = tab.subTabs.length > 0;
    const activeSubTab = hasSubTabs ? tab.subTabs[Math.min(subTabIndex, tab.subTabs.length - 1)] : null;
    const renderer = contentRenderers[tab.id];

    return (
        <Box sx={{
            width: '100%', height: '100dvh', bgcolor: 'background.default', color: 'text.primary',
            display: 'flex', flexDirection: 'column', overflow: 'hidden',
        }}>
            {/* HEADER */}
            <Box sx={{
                px: 2, pt: 'max(16px, calc(env(safe-area-inset-top) + 6px))', pb: 1,
                borderBottom: '2px solid', borderColor: 'primary.main',
                bgcolor: 'rgba(20, 27, 45, 0.98)', boxShadow: '0 0 16px rgba(0, 229, 255, 0.25)',
                flexShrink: 0, zIndex: 20,
            }}>
                <Typography variant="h6" sx={{
                    color: 'primary.main', textAlign: 'center', fontWeight: 700,
                    letterSpacing: '0.15em', textShadow: '0 0 10px rgba(0, 229, 255, 0.8)',
                }}>
                    {tab.name} MODE
                </Typography>

                {/* サブタブ (上部セグメント。サブタブを持つタブのみ) */}
                {hasSubTabs && (
                    <Tabs
                        value={Math.min(subTabIndex, tab.subTabs.length - 1)}
                        onChange={(_, v) => setSubTabIndex(v)}
                        variant="fullWidth"
                        sx={{
                            minHeight: 36, mt: 0.5,
                            '& .MuiTab-root': { minHeight: 36, color: 'text.secondary', fontSize: '0.75rem' },
                            '& .Mui-selected': { color: 'primary.main' },
                            '& .MuiTabs-indicator': { backgroundColor: 'primary.main' },
                        }}
                    >
                        {tab.subTabs.map((st) => <Tab key={st.id} label={st.name} />)}
                    </Tabs>
                )}
            </Box>

            {/* BODY - 表示中タブの内容のみマウント。横スワイプで主タブ移動可。 */}
            <Box {...swipeHandlers} sx={{ flex: 1, minHeight: 0, overflowY: 'auto', overflowX: 'hidden' }}>
                {hasSubTabs ? renderer(activeSubTab) : renderer()}
            </Box>

            {/* BOTTOM TAB BAR */}
            <BottomNavigation
                value={currentTab}
                onChange={(_, v) => changeTab(v)}
                showLabels
                sx={{
                    flexShrink: 0, bgcolor: 'rgba(20, 27, 45, 0.98)',
                    borderTop: '2px solid', borderColor: 'primary.main',
                    pb: 'env(safe-area-inset-bottom)',
                    '& .MuiBottomNavigationAction-root': { color: 'text.secondary' },
                    '& .Mui-selected': { color: 'primary.main' },
                }}
            >
                {TAB_CONFIG.map((t) => (
                    <BottomNavigationAction key={t.id} label={t.name} icon={TAB_ICONS[t.id]} />
                ))}
            </BottomNavigation>

            {/* 接続ステータス (フリック撤去のためスワイプハンドラなし) */}
            <StatusFooter isConnected={isConnected} swipeHandlers={{}} />
        </Box>
    );
};
