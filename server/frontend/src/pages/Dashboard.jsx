import React, { useState } from 'react';
import { Box, Typography, Chip } from '@mui/material';
import { useSwipeable } from 'react-swipeable';
import WifiIcon from '@mui/icons-material/Wifi';
import SpeedIcon from '@mui/icons-material/Speed';
import ThermostatIcon from '@mui/icons-material/Thermostat';
import PlayArrowIcon from '@mui/icons-material/PlayArrow';
import PauseIcon from '@mui/icons-material/Pause';
import StopIcon from '@mui/icons-material/Stop';
import { HoloSphere } from '../components/sphere/HoloSphere';
import { NeonDial } from '../components/ui/NeonDial';
import { VerticalTabContainer } from '../components/ui/VerticalTabContainer';
import { CompactSlider } from '../components/ui/CompactSlider';
import { PlaylistManager } from '../components/playlist/PlaylistManager';
import { VideoManager } from '../components/playlist/VideoManager';
import { TAB_CONFIG } from '../config/tabConfig';

export const Dashboard = () => {
    const [currentTab, setCurrentTab] = useState(0);
    const [rotation, setRotation] = useState(0);
    const [brightness, setBrightness] = useState(80);
    const [speed, setSpeed] = useState(50);
    const [color, setColor] = useState(120);

    const [isPlaying, setIsPlaying] = useState(false);

    const handleTogglePlay = () => {
        setIsPlaying(!isPlaying);
    };

    const handleStop = () => {
        setIsPlaying(false);
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
        delta: 10, // Lower delta for easier triggering on small areas
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
                    bgcolor: 'black', // Ensure dark background
                }}
            >
                {/* Layer 1: Full Screen Sphere Visualization */}
                <Box
                    sx={{
                        position: 'absolute',
                        inset: 0,
                        zIndex: 0,
                        '& canvas': {
                            display: 'block', // Remove inline-block spacing
                        }
                    }}
                >
                    <HoloSphere />
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
                        pointerEvents: 'none', // Allow clicks to pass through to sphere if needed
                    }}
                >
                    <Chip
                        label={isPlaying ? "ONLINE (PLAYING)" : "ONLINE (IDLE)"}
                        size="small"
                        sx={{
                            bgcolor: isPlaying ? '#00ff00' : '#555',
                            color: isPlaying ? '#000' : '#ccc',
                            fontWeight: 'bold',
                            height: 20,
                            fontSize: '0.65rem',
                            border: 'none',
                            boxShadow: isPlaying ? '0 0 8px #00ff00' : 'none',
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
                        IMU: W:1.00 X:0.00 Y:0.00 Z:0.00
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
                        pb: 3, // Extra padding for bottom
                    }}
                    // CRITICAL: Stop propagation to prevent swipe gestures when interacting with controls
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
                                    bgcolor: isPlaying ? 'primary.main' : 'rgba(0, 229, 255, 0.1)',
                                    color: isPlaying ? 'black' : 'primary.main',
                                    boxShadow: isPlaying ? '0 0 15px rgba(0, 229, 255, 0.6)' : 'none',
                                    transition: 'all 0.3s',
                                    '&:active': { transform: 'scale(0.95)' }
                                }}
                            >
                                {isPlaying ? <PauseIcon fontSize="small" sx={{ color: 'inherit' }} /> : <PlayArrowIcon fontSize="small" sx={{ color: 'inherit' }} />}
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
                            onChange={setBrightness}
                        />
                        <CompactSlider
                            label="SPEED"
                            value={speed}
                            min={0}
                            max={100}
                            onChange={setSpeed}
                        />
                        <CompactSlider
                            label="HUE"
                            value={color}
                            min={0}
                            max={360}
                            onChange={setColor}
                            unit="°"
                        />
                    </Box>
                </Box>
            </Box>
        );
    };

    // Render content for PARAMS sub-tabs
    const renderParamsContent = (subTab) => {
        if (subTab.id === 'controls') {
            return (
                <Box
                    sx={{
                        height: '100%',
                        p: 2,
                        border: '2px solid',
                        borderColor: 'primary.main',
                        borderRadius: 2,
                        boxShadow: '0 0 20px rgba(0, 229, 255, 0.2)',
                        bgcolor: 'rgba(20, 27, 45, 0.9)',
                        overflow: 'auto',
                    }}
                >
                    <Typography
                        variant="h6"
                        sx={{
                            mb: 3,
                            pb: 1,
                            borderBottom: '2px solid',
                            borderColor: 'primary.main',
                            color: 'primary.main',
                            textAlign: 'center',
                            letterSpacing: '0.1em',
                        }}
                    >
                        PARAMETERS
                    </Typography>
                    <Box sx={{ display: 'flex', flexDirection: 'column', alignItems: 'center', gap: 3 }}>
                        <NeonDial label="Brightness" value={brightness} min={0} max={100} onChange={setBrightness} />
                        <NeonDial label="Speed" value={speed} min={0} max={100} onChange={setSpeed} />
                        <NeonDial label="Hue" value={color} min={0} max={360} onChange={setColor} />
                    </Box>
                </Box>
            );
        } else {
            // Presets sub-tab
            return (
                <Box
                    sx={{
                        height: '100%',
                        p: 2,
                        border: '2px solid',
                        borderColor: 'primary.main',
                        borderRadius: 2,
                        bgcolor: 'rgba(20, 27, 45, 0.9)',
                        overflow: 'auto',
                    }}
                >
                    <Typography
                        variant="h6"
                        sx={{
                            mb: 2,
                            color: 'primary.main',
                            textAlign: 'center',
                            letterSpacing: '0.1em',
                        }}
                    >
                        PRESETS
                    </Typography>
                    <Typography sx={{ color: 'text.secondary', fontSize: '0.9rem' }}>
                        Saved parameter presets will appear here...
                    </Typography>
                </Box>
            );
        }
    };

    // Render content for CONTROL sub-tabs
    const renderControlContent = (subTab) => {
        if (subTab.id === 'actions') {
            return (
                <Box
                    sx={{
                        height: '100%',
                        p: 2,
                        border: '2px solid',
                        borderColor: 'primary.main',
                        borderRadius: 2,
                        boxShadow: '0 0 20px rgba(0, 229, 255, 0.2)',
                        bgcolor: 'rgba(20, 27, 45, 0.9)',
                        display: 'flex',
                        flexDirection: 'column',
                        overflow: 'auto',
                    }}
                >
                    <Typography
                        variant="h6"
                        sx={{
                            mb: 3,
                            pb: 1,
                            borderBottom: '2px solid',
                            borderColor: 'primary.main',
                            color: 'primary.main',
                            textAlign: 'center',
                            letterSpacing: '0.1em',
                        }}
                    >
                        ACTIONS
                    </Typography>
                    <Box sx={{ display: 'flex', flexDirection: 'column', gap: 2 }}>
                        <Box
                            onClick={() => console.log('START')}
                            sx={{
                                p: 1.5,
                                border: '2px solid',
                                borderColor: 'primary.main',
                                borderRadius: 1,
                                cursor: 'pointer',
                                transition: 'all 0.3s',
                                '&:hover': {
                                    boxShadow: '0 0 20px rgba(0, 229, 255, 0.5)',
                                    transform: 'scale(1.02)',
                                },
                            }}
                        >
                            <Box sx={{ display: 'flex', alignItems: 'center', justifyContent: 'center' }}>
                                <PlayArrowIcon sx={{ color: 'primary.main', mr: 1 }} />
                                <Typography sx={{ color: 'primary.main', fontWeight: 700, letterSpacing: '0.1em' }}>
                                    START
                                </Typography>
                            </Box>
                        </Box>
                        <Box
                            onClick={() => console.log('STOP')}
                            sx={{
                                p: 1.5,
                                border: '2px solid',
                                borderColor: 'error.main',
                                borderRadius: 1,
                                cursor: 'pointer',
                                transition: 'all 0.3s',
                                '&:hover': {
                                    boxShadow: '0 0 20px rgba(255, 23, 68, 0.5)',
                                    transform: 'scale(1.02)',
                                },
                            }}
                        >
                            <Box sx={{ display: 'flex', alignItems: 'center', justifyContent: 'center' }}>
                                <StopIcon sx={{ color: 'error.main', mr: 1 }} />
                                <Typography sx={{ color: 'error.main', fontWeight: 700, letterSpacing: '0.1em' }}>
                                    STOP
                                </Typography>
                            </Box>
                        </Box>
                    </Box>
                </Box>
            );
        } else {
            // System sub-tab
            return (
                <Box
                    sx={{
                        height: '100%',
                        p: 2,
                        border: '2px solid',
                        borderColor: 'primary.main',
                        borderRadius: 2,
                        bgcolor: 'rgba(20, 27, 45, 0.9)',
                        overflow: 'auto',
                    }}
                >
                    <Typography
                        variant="h6"
                        sx={{
                            mb: 2,
                            pb: 1,
                            borderBottom: '2px solid',
                            borderColor: 'primary.main',
                            color: 'primary.main',
                            textAlign: 'center',
                            letterSpacing: '0.1em',
                        }}
                    >
                        SYSTEM
                    </Typography>
                    <Box
                        sx={{
                            fontFamily: '"Source Code Pro", monospace',
                            fontSize: '0.85rem',
                            color: 'text.secondary',
                            '& > div': { mb: 1 },
                        }}
                    >
                        <Box>CPU: 12.5%</Box>
                        <Box>MEM: 256MB/4GB</Box>
                        <Box>TEMP: 42.5°C</Box>
                        <Box sx={{ mt: 2, pt: 2, borderTop: '1px solid', borderColor: 'primary.main' }}>
                            UPTIME: 24h 15m
                        </Box>
                    </Box>
                </Box>
            );
        }
    };

    // Render content for PLAYLIST sub-tabs
    const renderPlaylistContent = (subTab) => {
        if (subTab.id === 'playlists') {
            return <PlaylistManager />;
        } else {
            return <VideoManager />;
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
                display: 'flex',
                flexDirection: 'column',
                bgcolor: 'background.default',
            }}
        >
            {/* HEADER */}
            <Box
                {...swipeHandlers}
                sx={{
                    p: 2,
                    pt: 'max(16px, env(safe-area-inset-top))', // Handle notch/safe area
                    pb: 2,
                    borderBottom: '2px solid',
                    borderColor: 'primary.main',
                    bgcolor: 'rgba(20, 27, 45, 0.95)',
                    boxShadow: '0 2px 20px rgba(0, 229, 255, 0.3)',
                    zIndex: 20, // Increase z-index
                    cursor: 'grab',
                    touchAction: 'none',
                    userSelect: 'none', // Prevent text selection
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
                        pointerEvents: 'none', // Pass events to parent Box
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
                                    bgcolor: 'background.default', // Ensure background opacity to hide back faces
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
                    pb: 'max(12px, env(safe-area-inset-bottom))', // Handle home bar area
                    borderTop: '2px solid',
                    borderColor: 'primary.main',
                    bgcolor: 'rgba(20, 27, 45, 0.95)',
                    boxShadow: '0 -2px 20px rgba(0, 229, 255, 0.3)',
                    zIndex: 20, // Increase z-index
                    cursor: 'grab',
                    touchAction: 'none',
                    userSelect: 'none',
                    '&:active': { cursor: 'grabbing' },
                }}
            >
                <Box sx={{ display: 'flex', justifyContent: 'space-around', alignItems: 'center', pointerEvents: 'none' }}>
                    <Chip
                        icon={<WifiIcon />}
                        label="CONNECTED"
                        size="small"
                        sx={{
                            bgcolor: 'rgba(0, 255, 0, 0.1)',
                            border: '1px solid #00ff00',
                            color: '#00ff00',
                            fontSize: '0.7rem',
                            fontFamily: '"Source Code Pro", monospace',
                            '& .MuiChip-icon': { color: '#00ff00' },
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
