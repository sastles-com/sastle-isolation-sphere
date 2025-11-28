import React, { useState } from 'react';
import { Box, Typography, FormControlLabel, Switch, Select, MenuItem, FormControl, InputLabel } from '@mui/material';
import LightModeIcon from '@mui/icons-material/LightMode';
import ShuffleIcon from '@mui/icons-material/Shuffle';
import PaletteIcon from '@mui/icons-material/Palette';
import { CompactSlider } from '../ui/CompactSlider';

const PATTERNS = [
    { id: 'solid', name: 'Solid Color' },
    { id: 'pulse', name: 'Pulse' },
    { id: 'rainbow', name: 'Rainbow Wave' },
    { id: 'noise', name: 'Perlin Noise' },
    { id: 'sparkle', name: 'Sparkle' },
];

export const PatternControl = () => {
    const [lightsOn, setLightsOn] = useState(true);
    const [randomMode, setRandomMode] = useState(false);
    const [selectedPattern, setSelectedPattern] = useState('solid');
    const [hue, setHue] = useState(180);
    const [speed, setSpeed] = useState(50);

    return (
        <Box sx={{ height: '100%', display: 'flex', flexDirection: 'column', p: 2, bgcolor: 'rgba(20, 27, 45, 0.9)' }}>
            {/* Header */}
            <Typography variant="h6" sx={{ color: 'primary.main', letterSpacing: '0.1em', mb: 3, textAlign: 'center' }}>
                PATTERN CONTROL
            </Typography>

            {/* Toggles */}
            <Box sx={{ display: 'flex', justifyContent: 'space-between', mb: 3, px: 1 }}>
                <FormControlLabel
                    control={
                        <Switch
                            checked={lightsOn}
                            onChange={(e) => setLightsOn(e.target.checked)}
                            color="primary"
                        />
                    }
                    label={
                        <Box sx={{ display: 'flex', alignItems: 'center', gap: 1, color: '#fff' }}>
                            <LightModeIcon fontSize="small" sx={{ color: lightsOn ? 'primary.main' : '#666' }} />
                            <Typography variant="body2">LIGHTS</Typography>
                        </Box>
                    }
                />
                <FormControlLabel
                    control={
                        <Switch
                            checked={randomMode}
                            onChange={(e) => setRandomMode(e.target.checked)}
                            color="secondary"
                        />
                    }
                    label={
                        <Box sx={{ display: 'flex', alignItems: 'center', gap: 1, color: '#fff' }}>
                            <ShuffleIcon fontSize="small" sx={{ color: randomMode ? 'secondary.main' : '#666' }} />
                            <Typography variant="body2">RANDOM</Typography>
                        </Box>
                    }
                />
            </Box>

            {/* Pattern Selection */}
            <Box sx={{ mb: 4 }}>
                <FormControl fullWidth variant="outlined" size="small">
                    <InputLabel sx={{ color: 'primary.main' }}>PATTERN</InputLabel>
                    <Select
                        value={selectedPattern}
                        label="PATTERN"
                        onChange={(e) => setSelectedPattern(e.target.value)}
                        sx={{
                            color: '#fff',
                            '.MuiOutlinedInput-notchedOutline': { borderColor: 'rgba(0, 229, 255, 0.3)' },
                            '&:hover .MuiOutlinedInput-notchedOutline': { borderColor: 'primary.main' },
                            '&.Mui-focused .MuiOutlinedInput-notchedOutline': { borderColor: 'primary.main' },
                            '.MuiSvgIcon-root': { color: 'primary.main' }
                        }}
                        MenuProps={{
                            PaperProps: {
                                sx: {
                                    bgcolor: 'rgba(20, 27, 45, 0.95)',
                                    border: '1px solid rgba(0, 229, 255, 0.2)',
                                    '& .MuiMenuItem-root': {
                                        color: '#fff',
                                        '&:hover': { bgcolor: 'rgba(0, 229, 255, 0.1)' },
                                        '&.Mui-selected': { bgcolor: 'rgba(0, 229, 255, 0.2)', '&:hover': { bgcolor: 'rgba(0, 229, 255, 0.3)' } }
                                    }
                                }
                            }
                        }}
                    >
                        {PATTERNS.map((pattern) => (
                            <MenuItem key={pattern.id} value={pattern.id}>{pattern.name}</MenuItem>
                        ))}
                    </Select>
                </FormControl>
            </Box>

            {/* Parameters */}
            <Box sx={{ display: 'flex', flexDirection: 'column', gap: 1 }}>
                <Box sx={{ display: 'flex', alignItems: 'center', gap: 1, mb: 1, color: 'primary.main' }}>
                    <PaletteIcon fontSize="small" />
                    <Typography variant="subtitle2">PARAMETERS</Typography>
                </Box>

                <CompactSlider
                    label="HUE"
                    value={hue}
                    min={0}
                    max={360}
                    onChange={setHue}
                    unit="°"
                />
                <CompactSlider
                    label="SPEED"
                    value={speed}
                    min={0}
                    max={100}
                    onChange={setSpeed}
                />
            </Box>
        </Box>
    );
};
