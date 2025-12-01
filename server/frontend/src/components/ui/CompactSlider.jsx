import React from 'react';
import { Box, Typography, Slider } from '@mui/material';

/**
 * CompactSlider - Space-efficient parameter control
 * @param {string} label - Display label
 * @param {number} value - Current value
 * @param {number} min - Minimum value
 * @param {number} max - Maximum value
 * @param {Function} onChange - Value change handler
 * @param {string} unit - Unit suffix (%, °, etc.)
 */
export const CompactSlider = ({ label, value, min, max, onChange, unit = '%' }) => {
    return (
        <Box sx={{ mb: 2 }}>
            <Box sx={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', mb: 0.5 }}>
                <Typography
                    variant="caption"
                    sx={{
                        color: 'primary.main',
                        fontFamily: '"Source Code Pro", monospace',
                        fontSize: '0.75rem',
                        letterSpacing: '0.05em',
                    }}
                >
                    {label}
                </Typography>
                <Typography
                    variant="caption"
                    sx={{
                        color: 'primary.main',
                        fontFamily: '"Source Code Pro", monospace',
                        fontSize: '0.75rem',
                        fontWeight: 700,
                    }}
                >
                    {value}{unit}
                </Typography>
            </Box>
            <Box
                onMouseDown={(e) => e.stopPropagation()}
                onTouchStart={(e) => e.stopPropagation()}
            >
                <Slider
                    value={value}
                    min={min}
                    max={max}
                    onChange={(e, newValue) => onChange(newValue)}
                    sx={{
                        color: 'primary.main',
                        height: 4,
                        '& .MuiSlider-thumb': {
                            width: 16,
                            height: 16,
                            backgroundColor: 'primary.main',
                            boxShadow: '0 0 8px rgba(0, 229, 255, 0.8)',
                            '&:hover': {
                                boxShadow: '0 0 12px rgba(0, 229, 255, 1)',
                            },
                        },
                        '& .MuiSlider-track': {
                            border: 'none',
                            backgroundColor: 'primary.main',
                            boxShadow: '0 0 4px rgba(0, 229, 255, 0.5)',
                        },
                        '& .MuiSlider-rail': {
                            opacity: 0.3,
                            backgroundColor: '#bfbfbf',
                        },
                    }}
                />
            </Box>
        </Box>
    );
};
