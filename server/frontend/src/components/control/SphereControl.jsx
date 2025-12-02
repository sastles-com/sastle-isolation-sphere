import React, { useState, useEffect, useRef } from 'react';
import { Box, Typography, Button, Paper, Grid } from '@mui/material';
import RestartAltIcon from '@mui/icons-material/RestartAlt';
import CompassCalibrationIcon from '@mui/icons-material/CompassCalibration';
import PowerSettingsNewIcon from '@mui/icons-material/PowerSettingsNew';
import { CompactSlider } from '../ui/CompactSlider';
import { NeonJoystick } from '../ui/NeonJoystick';
import { useWebSocket } from '../../contexts/WebSocketContext';

export const SphereControl = () => {
    const [brightness, setBrightness] = useState(80);
    const [offset, setOffset] = useState({ pitch: 0, roll: 0 });
    const joystickVector = useRef({ x: 0, y: 0 });
    const requestRef = useRef();
    const brightnessTimeoutRef = useRef(null);
    const { lastMessage, isConnected, sendMessage } = useWebSocket();

    // Listen for state updates from StateManager
    useEffect(() => {
        if (lastMessage && lastMessage.type === 'STATE_UPDATE') {
            const state = lastMessage.payload;
            if (state.params && state.params.brightness !== undefined) {
                console.log('[SphereControl] Received state update, brightness:', state.params.brightness);
                setBrightness(state.params.brightness);
            }
        }
    }, [lastMessage]);

    // Send brightness command to MQTT (debounced)
    const handleBrightnessChange = (value) => {
        console.log('[SphereControl] Brightness slider changed to:', value);
        setBrightness(value);
        
        // Clear previous timeout
        if (brightnessTimeoutRef.current) {
            clearTimeout(brightnessTimeoutRef.current);
        }
        
        // Debounce: send command after 300ms of inactivity
        brightnessTimeoutRef.current = setTimeout(() => {
            console.log('[SphereControl] Sending brightness command via WebSocket:', value);
            sendMessage('COMMAND', {
                command: 'params',
                params: { brightness: value }
            });
        }, 300);
    };

    // Rate control loop
    const updateOffset = () => {
        const { x, y } = joystickVector.current;
        if (Math.abs(x) > 0.01 || Math.abs(y) > 0.01) {
            setOffset(prev => ({
                pitch: Number((prev.pitch - y * 0.5).toFixed(1)), // Up/Down controls Pitch (inverted Y)
                roll: Number((prev.roll + x * 0.5).toFixed(1))     // Left/Right controls Roll
            }));
        }
        requestRef.current = requestAnimationFrame(updateOffset);
    };

    useEffect(() => {
        requestRef.current = requestAnimationFrame(updateOffset);
        return () => cancelAnimationFrame(requestRef.current);
    }, []);

    const handleJoystickChange = (vector) => {
        joystickVector.current = vector;
    };

    const handleResetOrientation = () => {
        setOffset({ pitch: 0, roll: 0 });
        console.log('Resetting Orientation...');
    };

    const handleSystemReset = () => {
        console.log('System Reset...');
    };

    return (
        <Box sx={{ height: '100%', display: 'flex', flexDirection: 'column', p: 2, bgcolor: 'rgba(20, 27, 45, 0.9)' }}>
            {/* Header */}
            <Typography variant="h6" sx={{ color: 'primary.main', letterSpacing: '0.1em', mb: 3, textAlign: 'center' }}>
                SPHERE CONTROL
            </Typography>

            {/* IMU Controls */}
            <Paper sx={{ p: 2, mb: 3, bgcolor: 'rgba(0, 0, 0, 0.3)', border: '1px solid rgba(0, 229, 255, 0.2)' }}>
                <Typography variant="subtitle2" sx={{ color: '#ccc', mb: 2, display: 'flex', alignItems: 'center', gap: 1 }}>
                    <CompassCalibrationIcon fontSize="small" sx={{ color: 'primary.main' }} />
                    IMU OFFSET ADJUSTMENT
                </Typography>

                <Box sx={{ display: 'flex', flexDirection: 'column', alignItems: 'center', gap: 2 }}>
                    <NeonJoystick onChange={handleJoystickChange} />

                    <Box sx={{ display: 'flex', gap: 4, width: '100%', justifyContent: 'center' }}>
                        <Box sx={{ textAlign: 'center' }}>
                            <Typography variant="caption" sx={{ color: 'text.secondary' }}>PITCH</Typography>
                            <Typography variant="h6" sx={{ color: 'primary.main', fontFamily: '"Source Code Pro", monospace' }}>
                                {offset.pitch > 0 ? '+' : ''}{offset.pitch}°
                            </Typography>
                        </Box>
                        <Box sx={{ textAlign: 'center' }}>
                            <Typography variant="caption" sx={{ color: 'text.secondary' }}>ROLL</Typography>
                            <Typography variant="h6" sx={{ color: 'primary.main', fontFamily: '"Source Code Pro", monospace' }}>
                                {offset.roll > 0 ? '+' : ''}{offset.roll}°
                            </Typography>
                        </Box>
                    </Box>

                    <Button
                        variant="outlined"
                        startIcon={<RestartAltIcon />}
                        onClick={handleResetOrientation}
                        fullWidth
                        size="small"
                        sx={{
                            mt: 1,
                            borderColor: 'rgba(0, 229, 255, 0.5)',
                            color: 'primary.main',
                            '&:hover': { borderColor: '#fff', bgcolor: 'rgba(0, 229, 255, 0.1)' }
                        }}
                    >
                        RESET OFFSET
                    </Button>
                </Box>
            </Paper>

            {/* System Controls */}
            <Paper sx={{ p: 2, bgcolor: 'rgba(0, 0, 0, 0.3)', border: '1px solid rgba(0, 229, 255, 0.2)' }}>
                <Typography variant="subtitle2" sx={{ color: '#ccc', mb: 2, display: 'flex', alignItems: 'center', gap: 1 }}>
                    <PowerSettingsNewIcon fontSize="small" sx={{ color: 'primary.main' }} />
                    SYSTEM
                </Typography>

                <Box sx={{ mb: 3 }}>
                    <CompactSlider
                        label="GLOBAL BRIGHTNESS"
                        value={brightness}
                        min={0}
                        max={100}
                        onChange={handleBrightnessChange}
                    />
                </Box>

                <Button
                    variant="outlined"
                    startIcon={<PowerSettingsNewIcon />}
                    onClick={handleSystemReset}
                    fullWidth
                    sx={{
                        borderColor: 'error.main',
                        color: 'error.main',
                        '&:hover': { borderColor: '#ff5252', bgcolor: 'rgba(255, 23, 68, 0.1)' }
                    }}
                >
                    REBOOT SYSTEM
                </Button>
            </Paper>
        </Box>
    );
};
