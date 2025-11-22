import React, { useState } from 'react';
import { Box, Typography, Chip } from '@mui/material';
import { useSwipeable } from 'react-swipeable';
import WifiIcon from '@mui/icons-material/Wifi';
import SpeedIcon from '@mui/icons-material/Speed';
import ThermostatIcon from '@mui/icons-material/Thermostat';
import PlayArrowIcon from '@mui/icons-material/PlayArrow';
import StopIcon from '@mui/icons-material/Stop';
import { HoloSphere } from '../components/sphere/HoloSphere';
import { NeonDial } from '../components/ui/NeonDial';
import { VerticalTabContainer } from '../components/ui/VerticalTabContainer';
import { TAB_CONFIG } from '../config/tabConfig';

export const Dashboard = () => {
    const [currentTab, setCurrentTab] = useState(0);
    const [rotation, setRotation] = useState(0);
    const [brightness, setBrightness] = useState(80);
    const [speed, setSpeed] = useState(50);
    const [color, setColor] = useState(120);

    const handleSwipeLeft = () => {
        setCurrentTab((prev) => (prev + 1) % TAB_CONFIG.length);
        setRotation((prev) => prev - 120);
    };

    const handleSwipeRight = () => {
        setCurrentTab((prev) => (prev - 1 + TAB_CONFIG.length) % TAB_CONFIG.length);
        setRotation((prev) => prev + 120);
    };

    const swipeHandlers = useSwipeable({
        onSwipedLeft: handleSwipeLeft,
        onSwipedRight: handleSwipeRight,
        trackMouse: true,
        preventScrollOnSwipe: true,
    });

    // Render content for SPHERE sub-tabs
    const renderSphereContent = (subTab) => {
        if (subTab.id === 'view') {
            return (
                <Box
                    sx={{
                        height: '100%',
                        bgcolor: 'rgba(0, 229, 255, 0.02)',
                        border: '2px solid',
                        borderColor: 'primary.main',
                        borderRadius: 2,
                        boxShadow: '0 0 30px rgba(0, 229, 255, 0.2)',
                        position: 'relative',
                        overflow: 'hidden',
                        p: 2,
                    }}
                >
                    <HoloSphere />
                    <Box
                        sx={{
                            position: 'absolute',
                            top: 10,
                            left: 10,
                            fontFamily: '"Source Code Pro", monospace',
                            fontSize: '0.75rem',
                            color: 'primary.main',
                            zIndex: 1,
                        }}
                    >
                        <Box sx={{ display: 'flex', alignItems: 'center', mb: 0.5 }}>
                            <Box
                                sx={{
                                    width: 6,
                                    height: 6,
                                    borderRadius: '50%',
                                    bgcolor: '#00ff00',
                                    mr: 1,
                                    boxShadow: '0 0 8px #00ff00',
                                }}
                            />
                            ONLINE
                        </Box>
                    </Box>
                </Box>
            );
        } else {
            // Settings sub-tab
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
                        SPHERE SETTINGS
                    </Typography>
                    <Typography sx={{ color: 'text.secondary', fontSize: '0.9rem' }}>
                        Configuration options for sphere visualization...
                    </Typography>
                </Box>
            );
        }
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

    const contentRenderers = {
        sphere: renderSphereContent,
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
                sx={{
                    p: 2,
                    borderBottom: '2px solid',
                    borderColor: 'primary.main',
                    bgcolor: 'rgba(20, 27, 45, 0.95)',
                    boxShadow: '0 2px 20px rgba(0, 229, 255, 0.3)',
                    zIndex: 10,
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
                    }}
                >
                    {TAB_CONFIG[currentTab].name} MODE
                </Typography>
            </Box>

            {/* BODY - Revolving Cylinder */}
            <Box
                {...swipeHandlers}
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
                                    transform: `rotateY(${tab.angle}deg) translateZ(50px) scale(1.0)`,
                                    p: 2,
                                    boxSizing: 'border-box',
                                }}
                            >
                                <VerticalTabContainer
                                    subTabs={tab.subTabs}
                                    renderContent={contentRenderers[tab.id]}
                                />
                            </Box>
                        ))}
                    </Box>
                </Box>
            </Box>

            {/* FOOTER */}
            <Box
                sx={{
                    p: 1.5,
                    borderTop: '2px solid',
                    borderColor: 'primary.main',
                    bgcolor: 'rgba(20, 27, 45, 0.95)',
                    boxShadow: '0 -2px 20px rgba(0, 229, 255, 0.3)',
                    zIndex: 10,
                }}
            >
                <Box sx={{ display: 'flex', justifyContent: 'space-around', alignItems: 'center' }}>
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
