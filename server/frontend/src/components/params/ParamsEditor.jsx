import React, { useState } from 'react';
import { Box, Typography, Button, List, ListItem, ListItemText, Slider, TextField } from '@mui/material';
import TuneIcon from '@mui/icons-material/Tune';
import SaveIcon from '@mui/icons-material/Save';

// Mock Data
const INITIAL_PARAMS = [
    { id: 'p1', name: 'Motor Max Speed', value: 120, min: 0, max: 200, unit: 'rpm' },
    { id: 'p2', name: 'Acceleration', value: 50, min: 0, max: 100, unit: '%' },
    { id: 'p3', name: 'LED Brightness Limit', value: 80, min: 0, max: 100, unit: '%' },
    { id: 'p4', name: 'Idle Timeout', value: 300, min: 0, max: 3600, unit: 's' },
    { id: 'p5', name: 'Temperature Limit', value: 60, min: 40, max: 90, unit: '°C' },
];

export const ParamsEditor = () => {
    const [params, setParams] = useState(INITIAL_PARAMS);

    const handleSave = () => {
        console.log('Saving parameters:', params);
        setTimeout(() => alert('Parameters Saved!'), 500);
    };

    const handleChange = (id, newValue) => {
        setParams(params.map(p => p.id === id ? { ...p, value: newValue } : p));
    };

    return (
        <Box sx={{ height: '100%', display: 'flex', flexDirection: 'column', p: 2, bgcolor: 'rgba(20, 27, 45, 0.9)' }}>
            {/* Header */}
            <Box sx={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', mb: 2 }}>
                <Typography variant="h6" sx={{ color: 'primary.main', letterSpacing: '0.1em' }}>
                    PARAMETERS
                </Typography>
                <Button
                    variant="contained"
                    startIcon={<SaveIcon />}
                    onClick={handleSave}
                    sx={{
                        bgcolor: 'primary.main',
                        color: '#000',
                        fontWeight: 'bold',
                        '&:hover': { bgcolor: '#00b8cc' }
                    }}
                >
                    SAVE
                </Button>
            </Box>

            {/* List */}
            <List sx={{ flex: 1, overflow: 'auto' }}>
                {params.map((param) => (
                    <ListItem
                        key={param.id}
                        sx={{
                            flexDirection: 'column',
                            alignItems: 'stretch',
                            mb: 2,
                            p: 2,
                            bgcolor: 'rgba(0, 0, 0, 0.3)',
                            border: '1px solid rgba(0, 229, 255, 0.2)',
                            borderRadius: 1
                        }}
                    >
                        <Box sx={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', mb: 1 }}>
                            <Box sx={{ display: 'flex', alignItems: 'center', gap: 1 }}>
                                <TuneIcon fontSize="small" sx={{ color: 'primary.main' }} />
                                <Typography sx={{ color: '#fff', fontFamily: '"Source Code Pro", monospace' }}>
                                    {param.name}
                                </Typography>
                            </Box>
                            <Typography sx={{ color: 'primary.main', fontWeight: 'bold' }}>
                                {param.value}{param.unit}
                            </Typography>
                        </Box>

                        <Box sx={{ display: 'flex', alignItems: 'center', gap: 2 }}>
                            <Slider
                                value={param.value}
                                min={param.min}
                                max={param.max}
                                onChange={(_, val) => handleChange(param.id, val)}
                                sx={{
                                    color: 'primary.main',
                                    '& .MuiSlider-thumb': {
                                        boxShadow: '0 0 10px rgba(0, 229, 255, 0.5)',
                                    }
                                }}
                            />
                            <TextField
                                value={param.value}
                                onChange={(e) => handleChange(param.id, Number(e.target.value))}
                                type="number"
                                size="small"
                                variant="outlined"
                                sx={{
                                    width: 80,
                                    '& .MuiOutlinedInput-root': {
                                        color: '#fff',
                                        '& fieldset': { borderColor: 'rgba(255, 255, 255, 0.3)' }
                                    }
                                }}
                            />
                        </Box>
                    </ListItem>
                ))}
            </List>
        </Box>
    );
};
