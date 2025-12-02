import React, { useState, useEffect } from 'react';
import { Box, Typography, Chip } from '@mui/material';
import { useSwipeable } from 'react-swipeable';
import WifiIcon from '@mui/icons-material/Wifi';
import SpeedIcon from '@mui/icons-material/Speed';
import ThermostatIcon from '@mui/icons-material/Thermostat';
import PlayArrowIcon from '@mui/icons-material/PlayArrow';
import PauseIcon from '@mui/icons-material/Pause';
import StopIcon from '@mui/icons-material/Stop';
import WifiOffIcon from '@mui/icons-material/WifiOff';
import { HoloSphere } from '../components/sphere/HoloSphere';
import { NeonDial } from '../components/ui/NeonDial';
import { VerticalTabContainer } from '../components/ui/VerticalTabContainer';
import { CompactSlider } from '../components/ui/CompactSlider';
import { PlaylistManager } from '../components/playlist/PlaylistManager';
import { VideoManager } from '../components/playlist/VideoManager';
import { SphereControl } from '../components/control/SphereControl';
import { PatternControl } from '../components/control/PatternControl';
import { ConfigEditor } from '../components/params/ConfigEditor';
import { ParamsEditor } from '../components/params/ParamsEditor';
import { TAB_CONFIG } from '../config/tabConfig';
import { useWebSocket } from '../contexts/WebSocketContext';

export const Dashboard = () => {
    const { isConnected, lastMessage, sendMessage } = useWebSocket();

    const [currentTab, setCurrentTab] = useState(0);
    const [rotation, setRotation] = useState(0);
    const [brightness, setBrightness] = useState(80);
    const [color, setColor] = useState(120);
    const [playbackStatus, setPlaybackStatus] = useState('stopped'); // "playing" | "paused" | "stopped"

    // Sync state from WebSocket
    useEffect(() => {
        if (lastMessage && lastMessage.type === 'STATE_UPDATE') {
            const { playback, params } = lastMessage.payload;
            if (playback) {
                setPlaybackStatus(playback.status || 'stopped');
            }
            if (params) {
                setBrightness(params.brightness);
                setColor(params.hue);
            }
        }
    }, [lastMessage]);

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
        preventScrollOnSwipe: true,
        delta: 10,
        trackTouch: true,
    });

    // Render content for SPHERE (Integrated Dashboard)
    const renderSphereContent = () => {
        return (
            <Box
                sx={{
                    height: '100%',
                    position: 'relative',
                    overflow: 'hidden',
                    bgcolor: 'black',
                }}
            >
                {/* Layer 1: Full Screen Sphere Visualization */}
                <Box
                    sx={{
                        position: 'absolute',
                        inset: 0,
                        zIndex: 0,
                        '& canvas': {
                            display: 'block',
                        }
                    }}
                >
                    <HoloSphere
                        rotation={rotation}
                        brightness={brightness}
                        color={color}
                    />
                </Box>

                {/* Layer 2: Top Status HUD */}
                <Box
                    sx={{
                        position: 'absolute',
                        top: 10,
                        left: 10,
                        zIndex: 1,
                        display: 'flex',
                        flexDirection: 'column',
                        gap: 0.5,
                        pointerEvents: 'none',
                    }}
                >
                    <Chip
                        label={playbackStatus === "playing" ? "ONLINE (PLAYING)" : "ONLINE (IDLE)"}
                        size="small"
                        sx={{
                            bgcolor: playbackStatus === "playing" ? '#00ff00' : '#555',
                            color: playbackStatus === "playing" ? '#000' : '#ccc',
                            fontWeight: 'bold',
                            height: 20,
                            fontSize: '0.65rem',
                            border: 'none',
                            boxShadow: playbackStatus === "playing" ? '0 0 8px #00ff00' : 'none',
                            transition: 'all 0.3s',
                        }}
                    />
                    <Typography
                        variant="caption"
                        sx={{
                            color: 'primary.main',
                            fontFamily: '"Source Code Pro", monospace',
                            fontSize: '0.7rem',
                            textShadow: '0 0 5px rgba(0, 229, 255, 0.5)',
                            bgcolor: 'rgba(0,0,0,0.5)',
                            px: 0.5,
                            borderRadius: 0.5,
                        }}
                    >
                        FPS: 60
                    </Typography>
                </Box>

                {/* Layer 3: Bottom Control HUD */}
                <Box
                    sx={{
                        position: 'absolute',
                        bottom: 0,
                        left: 0,
                        right: 0,
                        zIndex: 2,
                        p: 2,
                        background: 'linear-gradient(to top, rgba(0,0,0,0.9) 0%, rgba(0,0,0,0.7) 60%, transparent 100%)',
                        display: 'flex',
                        flexDirection: 'column',
                        gap: 2,
                        pb: 3,
                    }}
                    onMouseDown={(e) => e.stopPropagation()}
                    onTouchStart={(e) => e.stopPropagation()}
                >
                    {/* Playlist Controls */}
                    <Box sx={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', px: 1 }}>
                        <Box>
                            <Typography variant="caption" sx={{ color: 'primary.main', display: 'block', lineHeight: 1 }}>
                                TRACK 1/5
                            </Typography>
                            <Typography variant="body2" sx={{ color: '#fff', fontWeight: 600, textShadow: '0 0 10px rgba(0,0,0,0.5)' }}>
                                Isolation Theme.mp3
                            </Typography>
                        </Box>
                        <Box sx={{ display: 'flex', gap: 1 }}>
                            <Box
                                onClick={handleTogglePlay}
                                sx={{
                                    p: 1,
                                    border: '1px solid',
                                    borderColor: 'primary.main',
                                    borderRadius: '50%',
                                    cursor: 'pointer',
                                    bgcolor: playbackStatus === "playing" ? 'primary.main' : 'rgba(0, 229, 255, 0.1)',
                                    color: playbackStatus === "playing" ? 'black' : 'primary.main',
                                    boxShadow: playbackStatus === "playing" ? '0 0 15px rgba(0, 229, 255, 0.6)' : 'none',
                                    transition: 'all 0.3s',
                                    '&:active': { transform: 'scale(0.95)' }
                                }}
                            >
                                {playbackStatus === "playing" ? <PauseIcon fontSize="small" sx={{ color: 'inherit' }} /> : <PlayArrowIcon fontSize="small" sx={{ color: 'inherit' }} />}
                            </Box>
                            <Box
                                onClick={handleStop}
                                sx={{
                                    p: 1,
                                    border: '1px solid',
                                    borderColor: 'error.main',
                                    borderRadius: '50%',
                                    cursor: 'pointer',
                                    bgcolor: 'rgba(255, 23, 68, 0.1)',
                                    '&:active': { bgcolor: 'error.main', color: 'white' }
                                }}
                            >
                                <StopIcon fontSize="small" sx={{ color: 'error.main' }} />
                            </Box>
                        </Box>
                    </Box>

                    {/* Sliders */}
                    <Box sx={{ display: 'flex', flexDirection: 'column', gap: 0 }}>
                        <CompactSlider
                            label="BRIGHTNESS"
                            value={brightness}
                            min={0}
                            max={100}
                            onChange={(val) => handleParamChange('brightness', val)}
                        />
                        <CompactSlider
                            label="HUE"
                            value={color}
                            min={0}
                            max={360}
                            onChange={(val) => handleParamChange('hue', val)}
                            unit="°"
                        />
                    </Box>
                </Box>
            </Box>
        );
    };

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
                    pt: 'max(12px, env(safe-area-inset-top))',
                    borderBottom: '2px solid',
                    borderColor: 'primary.main',
                    bgcolor: 'rgba(20, 27, 45, 0.95)',
                    boxShadow: '0 0 20px rgba(0, 229, 255, 0.3)',
                    zIndex: 20,
                    cursor: 'grab',
                    touchAction: 'none',
                    userSelect: 'none',
                    '&:active': { cursor: 'grabbing' },
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
            <Box
                {...swipeHandlers}
                sx={{
                    p: 1.5,
                    pb: 'max(12px, env(safe-area-inset-bottom))',
                    borderTop: '2px solid',
                    borderColor: 'primary.main',
                    bgcolor: 'rgba(20, 27, 45, 0.95)',
                    boxShadow: '0 -2px 20px rgba(0, 229, 255, 0.3)',
                    zIndex: 20,
                    cursor: 'grab',
                    touchAction: 'none',
                    userSelect: 'none',
                    '&:active': { cursor: 'grabbing' },
                }}
            >
                <Box sx={{ display: 'flex', justifyContent: 'space-around', alignItems: 'center', pointerEvents: 'none' }}>
                    <Chip
                        icon={isConnected ? <WifiIcon /> : <WifiOffIcon />}
                        label={isConnected ? "CONNECTED" : "DISCONNECTED"}
                        size="small"
                        sx={{
                            bgcolor: isConnected ? 'rgba(0, 255, 0, 0.1)' : 'rgba(255, 0, 0, 0.1)',
                            border: '1px solid',
                            borderColor: isConnected ? '#00ff00' : '#ff0000',
                            color: isConnected ? '#00ff00' : '#ff0000',
                            fontSize: '0.7rem',
                            fontFamily: '"Source Code Pro", monospace',
                            '& .MuiChip-icon': { color: isConnected ? '#00ff00' : '#ff0000' },
                        }}
                    />
                    <Chip
                        icon={<SpeedIcon />}
                        label="60 FPS"
                        size="small"
                        sx={{
                            bgcolor: 'rgba(0, 229, 255, 0.1)',
                            border: '1px solid',
                            borderColor: 'primary.main',
                            color: 'primary.main',
                            fontSize: '0.7rem',
                            fontFamily: '"Source Code Pro", monospace',
                        }}
                    />
                    <Chip
                        icon={<ThermostatIcon />}
                        label="42°C"
                        size="small"
                        sx={{
                            bgcolor: 'rgba(0, 229, 255, 0.1)',
                            border: '1px solid',
                            borderColor: 'primary.main',
                            color: 'primary.main',
                            fontSize: '0.7rem',
                            fontFamily: '"Source Code Pro", monospace',
                        }}
                    />
                </Box>
            </Box>
        </Box>
    );
};
