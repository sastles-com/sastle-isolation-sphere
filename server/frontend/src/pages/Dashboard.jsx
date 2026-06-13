import React, { useState } from 'react';
import { Box, Typography } from '@mui/material';
import { useSwipeable } from 'react-swipeable';
import { SphereDashboard } from '../components/sphere/SphereDashboard';
import { StatusFooter } from '../components/layout/StatusFooter';
import { VerticalTabContainer } from '../components/ui/VerticalTabContainer';
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

export const Dashboard = () => {
    const { isConnected, sendMessage } = useWebSocket();

    const [currentTab, setCurrentTab] = useState(0);
    const [rotation, setRotation] = useState(0);
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

    const handleTogglePlay = () => {
        // Send toggle command to StateManager
        sendMessage('SET_PLAYBACK', { action: 'toggle' });
    };

    const handleStop = () => {
        sendMessage('SET_PLAYBACK', { action: 'stop' });
    };

    const handleParamChange = (key, value) => {
        // Optimistic update
        if (key === 'brightness') setBrightness(value);
        if (key === 'hue') setColor(value);

        // Send to backend
        sendMessage('SET_PARAMS', { [key]: value });
    };

    const handleSwipeLeft = () => {
        setCurrentTab((prev) => (prev + 1) % TAB_CONFIG.length);
        setRotation((prev) => prev - 90);
    };

    const handleSwipeRight = () => {
        setCurrentTab((prev) => (prev - 1 + TAB_CONFIG.length) % TAB_CONFIG.length);
        setRotation((prev) => prev + 90);
    };

    const swipeHandlers = useSwipeable({
        onSwipedLeft: handleSwipeLeft,
        onSwipedRight: handleSwipeRight,
        trackMouse: true,
        preventScrollOnSwipe: false, // Allow vertical scrolling
        delta: 50, // Increase threshold to avoid accidental swipes
        trackTouch: true,
    });

    // Render content for SPHERE (Integrated Dashboard)
    const renderSphereContent = () => (
        <SphereDashboard
            rotation={rotation}
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
        if (subTab.id === 'playlists') {
            return <PlaylistManager isPlaying={isPlaying} onTogglePlay={handleTogglePlay} onStop={handleStop} />;
        } else {
            return <VideoManager />;
        }
    };

    const renderParamsContent = (subTab) => {
        if (subTab.id === 'config') {
            return <ConfigEditor />;
        } else {
            return <ParamsEditor />;
        }
    };

    const renderControlContent = (subTab) => {
        if (subTab.id === 'sphere_control') {
            return <SphereControl />;
        } else if (subTab.id === 'debug_log') {
            return <LogPanel />;
        } else {
            return <PatternControl />;
        }
    };

    const contentRenderers = {
        sphere: renderSphereContent,
        playlist: renderPlaylistContent,
        params: renderParamsContent,
        control: renderControlContent,
    };

    return (
        <Box
            sx={{
                width: '100%',
                height: '100dvh',
                bgcolor: 'background.default',
                color: 'text.primary',
                display: 'flex',
                flexDirection: 'column',
                overflow: 'hidden',
            }}
        >
            {/* HEADER */}
            <Box
                {...swipeHandlers}
                sx={{
                    p: 2,
                    pt: 'max(20px, calc(env(safe-area-inset-top) + 8px))',
                    pb: 'max(12px, 8px)',
                    borderBottom: '2px solid',
                    borderColor: 'primary.main',
                    bgcolor: 'rgba(20, 27, 45, 0.98)',
                    boxShadow: '0 0 20px rgba(0, 229, 255, 0.3)',
                    position: 'sticky',
                    top: 0,
                    zIndex: 20,
                    cursor: 'grab',
                    touchAction: 'none',
                    userSelect: 'none',
                    '&:active': { cursor: 'grabbing' },
                    flexShrink: 0,
                }}
            >
                <Typography
                    variant="h6"
                    sx={{
                        color: 'primary.main',
                        textAlign: 'center',
                        fontWeight: 700,
                        letterSpacing: '0.15em',
                        textShadow: '0 0 10px rgba(0, 229, 255, 0.8)',
                        pointerEvents: 'none',
                    }}
                >
                    {TAB_CONFIG[currentTab].name} MODE
                </Typography>
            </Box>

            {/* BODY - Revolving Cylinder */}
            <Box
                sx={{
                    width: '100%',
                    flex: 1,
                    position: 'relative',
                    overflow: 'hidden',
                    perspective: '10000px',
                    perspectiveOrigin: '50% 50%',
                }}
            >
                {/* Rotating Container */}
                <Box
                    sx={{
                        position: 'absolute',
                        width: '100%',
                        height: '100%',
                        left: '50%',
                        top: '50%',
                        transform: `translate(-50%, -50%)`,
                    }}
                >
                    <Box
                        sx={{
                            width: '100%',
                            height: '100%',
                            transformStyle: 'preserve-3d',
                            transform: `rotateY(${rotation}deg)`,
                            transition: 'transform 0.5s cubic-bezier(0.4, 0, 0.2, 1)',
                        }}
                    >
                        {TAB_CONFIG.map((tab, index) => (
                            <Box
                                key={tab.id}
                                sx={{
                                    position: 'absolute',
                                    width: '100%',
                                    height: '100%',
                                    backfaceVisibility: 'hidden',
                                    transform: `rotateY(${tab.angle}deg) translateZ(50vw) scale(1.0)`,
                                    p: 2,
                                    boxSizing: 'border-box',
                                    bgcolor: 'background.default',
                                }}
                            >
                                {tab.subTabs.length > 0 ? (
                                    <VerticalTabContainer
                                        subTabs={tab.subTabs}
                                        renderContent={contentRenderers[tab.id]}
                                    />
                                ) : (
                                    contentRenderers[tab.id]()
                                )}
                            </Box>
                        ))}
                    </Box>
                </Box>
            </Box>

            {/* FOOTER */}
            <StatusFooter isConnected={isConnected} swipeHandlers={swipeHandlers} />
        </Box>
    );
};
