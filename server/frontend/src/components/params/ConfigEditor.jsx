import React, { useState, useEffect } from 'react';
import { Box, Typography, Button, Accordion, AccordionSummary, AccordionDetails, TextField, Switch, FormControlLabel } from '@mui/material';
import ExpandMoreIcon from '@mui/icons-material/ExpandMore';
import SaveIcon from '@mui/icons-material/Save';
import SettingsIcon from '@mui/icons-material/Settings';

// API URL
const API_URL = 'http://localhost:8000/api/config';

export const ConfigEditor = () => {
    const [config, setConfig] = useState(null);
    const [loading, setLoading] = useState(true);

    useEffect(() => {
        fetchConfig();
    }, []);

    const fetchConfig = async () => {
        try {
            const response = await fetch(API_URL);
            if (response.ok) {
                const data = await response.json();
                setConfig(data);
            } else {
                console.error('Failed to fetch config');
            }
        } catch (error) {
            console.error('Error fetching config:', error);
        } finally {
            setLoading(false);
        }
    };

    const handleSave = async () => {
        console.log('Saving config:', config);
        // In a real app, we might want to save section by section or the whole thing
        // For this mock API, we'll just simulate a save or implement a bulk update if the API supported it
        // The current API supports updating by section. Let's update all sections.
        try {
            for (const section of Object.keys(config)) {
                await fetch(API_URL, {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ section, data: config[section] })
                });
            }
            alert('Configuration Saved!');
        } catch (error) {
            console.error('Error saving config:', error);
            alert('Failed to save configuration');
        }
    };

    const handleChange = (section, key, value, subKey = null) => {
        setConfig(prev => {
            const newConfig = { ...prev };
            if (subKey) {
                newConfig[section][key][subKey] = value;
            } else {
                newConfig[section][key] = value;
            }
            return newConfig;
        });
    };

    if (loading) return <Box sx={{ p: 2, color: '#fff' }}>Loading Config...</Box>;
    if (!config) return <Box sx={{ p: 2, color: 'error.main' }}>Error Loading Config</Box>;

    return (
        <Box sx={{ height: '100%', display: 'flex', flexDirection: 'column', p: 2, bgcolor: 'rgba(20, 27, 45, 0.9)' }}>
            {/* Header */}
            <Box sx={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', mb: 2 }}>
                <Typography variant="h6" sx={{ color: 'primary.main', letterSpacing: '0.1em' }}>
                    CONFIG
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

            {/* Accordion List */}
            <Box sx={{ flex: 1, overflow: 'auto' }}>
                {Object.entries(config).map(([section, data]) => (
                    <Accordion
                        key={section}
                        sx={{
                            bgcolor: 'rgba(0, 0, 0, 0.3)',
                            border: '1px solid rgba(0, 229, 255, 0.2)',
                            mb: 1,
                            '&:before': { display: 'none' },
                            '&.Mui-expanded': { margin: '0 0 8px 0' }
                        }}
                    >
                        <AccordionSummary
                            expandIcon={<ExpandMoreIcon sx={{ color: 'primary.main' }} />}
                            sx={{ color: '#fff', '& .MuiTypography-root': { fontFamily: '"Source Code Pro", monospace' } }}
                        >
                            <Box sx={{ display: 'flex', alignItems: 'center', gap: 1 }}>
                                <SettingsIcon fontSize="small" sx={{ color: 'primary.main' }} />
                                <Typography sx={{ textTransform: 'uppercase' }}>{section}</Typography>
                            </Box>
                        </AccordionSummary>
                        <AccordionDetails>
                            <Box sx={{ display: 'flex', flexDirection: 'column', gap: 2 }}>
                                {Object.entries(data).map(([key, value]) => {
                                    if (typeof value === 'object' && value !== null) {
                                        // Nested object (e.g., PID)
                                        return (
                                            <Box key={key} sx={{ pl: 2, borderLeft: '2px solid rgba(0, 229, 255, 0.2)' }}>
                                                <Typography variant="caption" sx={{ color: 'text.secondary', mb: 1, display: 'block' }}>
                                                    {key.toUpperCase()}
                                                </Typography>
                                                <Box sx={{ display: 'flex', gap: 1 }}>
                                                    {Object.entries(value).map(([subKey, subValue]) => (
                                                        <TextField
                                                            key={subKey}
                                                            label={subKey.toUpperCase()}
                                                            value={subValue}
                                                            onChange={(e) => handleChange(section, key, e.target.value, subKey)}
                                                            size="small"
                                                            variant="outlined"
                                                            sx={{
                                                                '& .MuiOutlinedInput-root': { color: '#fff', '& fieldset': { borderColor: 'rgba(255, 255, 255, 0.3)' } },
                                                                '& .MuiInputLabel-root': { color: 'text.secondary' }
                                                            }}
                                                        />
                                                    ))}
                                                </Box>
                                            </Box>
                                        );
                                    } else if (typeof value === 'boolean') {
                                        return (
                                            <FormControlLabel
                                                key={key}
                                                control={
                                                    <Switch
                                                        checked={value}
                                                        onChange={(e) => handleChange(section, key, e.target.checked)}
                                                        color="primary"
                                                    />
                                                }
                                                label={
                                                    <Typography sx={{ color: '#fff', fontFamily: '"Source Code Pro", monospace' }}>
                                                        {key}
                                                    </Typography>
                                                }
                                            />
                                        );
                                    } else {
                                        return (
                                            <TextField
                                                key={key}
                                                label={key}
                                                value={value}
                                                onChange={(e) => handleChange(section, key, e.target.value)}
                                                size="small"
                                                variant="outlined"
                                                fullWidth
                                                sx={{
                                                    '& .MuiOutlinedInput-root': { color: '#fff', '& fieldset': { borderColor: 'rgba(255, 255, 255, 0.3)' } },
                                                    '& .MuiInputLabel-root': { color: 'text.secondary' }
                                                }}
                                            />
                                        );
                                    }
                                })}
                            </Box>
                        </AccordionDetails>
                    </Accordion>
                ))}
            </Box>
        </Box>
    );
};
