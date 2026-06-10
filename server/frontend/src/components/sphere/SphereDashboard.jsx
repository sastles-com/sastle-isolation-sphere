import React from 'react';
import { Box, Typography, Chip } from '@mui/material';
import PlayArrowIcon from '@mui/icons-material/PlayArrow';
import PauseIcon from '@mui/icons-material/Pause';
import StopIcon from '@mui/icons-material/Stop';
import { HoloSphere } from './HoloSphere';
import { CompactSlider } from '../ui/CompactSlider';

/**
 * SphereDashboard - SPHERE タブのメインビュー
 * 全画面の球体可視化 + ステータスHUD + 再生/パラメータ操作HUD
 */
export const SphereDashboard = ({
    rotation,
    brightness,
    color,
    playbackStatus,
    onTogglePlay,
    onStop,
    onParamChange,
}) => {
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
                            onClick={onTogglePlay}
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
                            onClick={onStop}
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
                        onChange={(val) => onParamChange('brightness', val)}
                    />
                    <CompactSlider
                        label="HUE"
                        value={color}
                        min={0}
                        max={360}
                        onChange={(val) => onParamChange('hue', val)}
                        unit="°"
                    />
                </Box>
            </Box>
        </Box>
    );
};
