import React from 'react';
import { Box, Chip } from '@mui/material';
import WifiIcon from '@mui/icons-material/Wifi';
import WifiOffIcon from '@mui/icons-material/WifiOff';
import SpeedIcon from '@mui/icons-material/Speed';
import ThermostatIcon from '@mui/icons-material/Thermostat';
import KeyboardDoubleArrowLeftIcon from '@mui/icons-material/KeyboardDoubleArrowLeft';
import KeyboardDoubleArrowRightIcon from '@mui/icons-material/KeyboardDoubleArrowRight';

/**
 * StatusFooter - 画面下部のステータスバー
 * 左右スワイプによるタブ切替(swipeHandlers)と接続状態の表示を担う
 */
export const StatusFooter = ({ isConnected, swipeHandlers }) => (
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
            display: 'flex',
            flexDirection: 'column',
            gap: 0.5,
            flexShrink: 0,
        }}
    >
        {/* Swipe Indicators - Left and Right Arrows */}
        <Box sx={{
            display: 'flex',
            justifyContent: 'space-between',
            alignItems: 'center',
            px: 1.5,
        }}>
            {/* Left Arrow */}
            <Box sx={{
                display: 'flex',
                alignItems: 'center',
                opacity: 0.5,
                transition: 'opacity 0.3s',
                '&:hover': { opacity: 1 }
            }}>
                <KeyboardDoubleArrowLeftIcon sx={{
                    color: 'primary.main',
                    fontSize: '1.1rem',
                    filter: 'drop-shadow(0 0 4px rgba(0, 229, 255, 0.6))'
                }} />
            </Box>

            {/* Right Arrow */}
            <Box sx={{
                display: 'flex',
                alignItems: 'center',
                opacity: 0.5,
                transition: 'opacity 0.3s',
                '&:hover': { opacity: 1 }
            }}>
                <KeyboardDoubleArrowRightIcon sx={{
                    color: 'primary.main',
                    fontSize: '1.1rem',
                    filter: 'drop-shadow(0 0 4px rgba(0, 229, 255, 0.6))'
                }} />
            </Box>
        </Box>

        {/* Status Chips */}
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
);
