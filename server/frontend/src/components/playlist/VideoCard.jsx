import React from 'react';
import { Box, Card, CardContent, Typography, Chip, IconButton, Button } from '@mui/material';
import PlayArrowIcon from '@mui/icons-material/PlayArrow';
import CheckCircleIcon from '@mui/icons-material/CheckCircle';
import RadioButtonUncheckedIcon from '@mui/icons-material/RadioButtonUnchecked';
import InfoIcon from '@mui/icons-material/Info';
import DeleteIcon from '@mui/icons-material/Delete';
import AccessTimeIcon from '@mui/icons-material/AccessTime';
import StorageIcon from '@mui/icons-material/Storage';
import AspectRatioIcon from '@mui/icons-material/AspectRatio';
import MovieIcon from '@mui/icons-material/Movie';
import CalendarTodayIcon from '@mui/icons-material/CalendarToday';

const formatDuration = (ms) => {
    const seconds = Math.floor(ms / 1000);
    const minutes = Math.floor(seconds / 60);
    const secs = seconds % 60;
    return `${minutes}:${secs.toString().padStart(2, '0')}`;
};

const formatSize = (bytes) => {
    if (!bytes) return '0 B';
    const mb = bytes / (1024 * 1024);
    return mb.toFixed(1) + ' MB';
};

const formatDate = (dateStr) => {
    const date = new Date(dateStr);
    return date.toLocaleString('ja-JP', { 
        month: '2-digit', 
        day: '2-digit', 
        hour: '2-digit', 
        minute: '2-digit' 
    });
};

export const VideoCard = ({ 
    video, 
    selected = false, 
    onSelect, 
    onInfo, 
    onDelete,
    mode = 'view' // 'view' | 'select'
}) => {
    return (
        <Card
            sx={{
                bgcolor: selected ? 'rgba(0, 229, 255, 0.1)' : 'rgba(20, 27, 45, 0.9)',
                border: selected ? '2px solid #00E5FF' : '1px solid rgba(0, 229, 255, 0.2)',
                borderRadius: 2,
                overflow: 'hidden',
                transition: 'all 0.2s ease',
                boxShadow: selected ? '0 0 20px rgba(0, 229, 255, 0.4)' : 'none',
                mb: 2,
                '&:active': {
                    transform: 'scale(0.98)'
                },
                position: 'relative'
            }}
        >
            {/* Thumbnail */}
            <Box
                sx={{
                    position: 'relative',
                    width: '100%',
                    paddingTop: '56.25%', // 16:9 aspect ratio
                    bgcolor: 'rgba(0, 0, 0, 0.5)',
                    cursor: 'pointer',
                    overflow: 'hidden',
                    '&:hover .play-overlay': {
                        opacity: 1
                    }
                }}
                onClick={onInfo}
            >
                {/* Thumbnail image (if available) */}
                {video.thumbnail_path ? (
                    <Box
                        component="img"
                        src={video.thumbnail_path}
                        alt={video.title}
                        sx={{
                            position: 'absolute',
                            top: 0,
                            left: 0,
                            width: '100%',
                            height: '100%',
                            objectFit: 'cover'
                        }}
                    />
                ) : (
                    <Box
                        sx={{
                            position: 'absolute',
                            top: 0,
                            left: 0,
                            width: '100%',
                            height: '100%',
                            display: 'flex',
                            alignItems: 'center',
                            justifyContent: 'center'
                        }}
                    >
                        <MovieIcon sx={{ fontSize: 64, color: 'rgba(0, 229, 255, 0.3)' }} />
                    </Box>
                )}

                {/* Play overlay */}
                <Box
                    className="play-overlay"
                    sx={{
                        position: 'absolute',
                        top: 0,
                        left: 0,
                        width: '100%',
                        height: '100%',
                        bgcolor: 'rgba(0, 0, 0, 0.5)',
                        display: 'flex',
                        alignItems: 'center',
                        justifyContent: 'center',
                        opacity: 0,
                        transition: 'opacity 0.2s'
                    }}
                >
                    <PlayArrowIcon sx={{ fontSize: 64, color: '#00E5FF' }} />
                </Box>

                {/* Scanline effect */}
                <Box
                    sx={{
                        position: 'absolute',
                        top: 0,
                        left: '-100%',
                        width: '100%',
                        height: '100%',
                        background: 'linear-gradient(90deg, transparent, rgba(0, 229, 255, 0.3), transparent)',
                        animation: selected ? 'scan 2s infinite' : 'none',
                        '@keyframes scan': {
                            '0%': { left: '-100%' },
                            '100%': { left: '100%' }
                        }
                    }}
                />
            </Box>

            {/* Content */}
            <CardContent sx={{ p: 2 }}>
                {/* Title */}
                <Typography
                    variant="h6"
                    sx={{
                        color: '#fff',
                        fontWeight: 700,
                        letterSpacing: '0.05em',
                        mb: 1.5,
                        textTransform: 'uppercase',
                        fontSize: '1rem',
                        fontFamily: '"Source Code Pro", monospace',
                        borderBottom: '2px solid rgba(0, 229, 255, 0.3)',
                        pb: 0.5
                    }}
                >
                    {video.title}
                </Typography>

                {/* Metadata row with icons */}
                <Box sx={{ display: 'flex', gap: 2, flexWrap: 'wrap', mb: 1.5 }}>
                    <Box sx={{ display: 'flex', alignItems: 'center', gap: 0.5 }}>
                        <AccessTimeIcon sx={{ fontSize: 16, color: 'primary.main' }} />
                        <Typography variant="caption" sx={{ color: 'text.secondary' }}>
                            {formatDuration(video.duration_ms)}
                        </Typography>
                    </Box>
                    <Box sx={{ display: 'flex', alignItems: 'center', gap: 0.5 }}>
                        <StorageIcon sx={{ fontSize: 16, color: 'primary.main' }} />
                        <Typography variant="caption" sx={{ color: 'text.secondary' }}>
                            {formatSize(video.size_bytes)}
                        </Typography>
                    </Box>
                    <Box sx={{ display: 'flex', alignItems: 'center', gap: 0.5 }}>
                        <AspectRatioIcon sx={{ fontSize: 16, color: 'primary.main' }} />
                        <Typography variant="caption" sx={{ color: 'text.secondary' }}>
                            {video.width}x{video.height}
                        </Typography>
                    </Box>
                    <Box sx={{ display: 'flex', alignItems: 'center', gap: 0.5 }}>
                        <MovieIcon sx={{ fontSize: 16, color: 'primary.main' }} />
                        <Typography variant="caption" sx={{ color: 'text.secondary' }}>
                            {video.fps || 30}fps
                        </Typography>
                    </Box>
                </Box>

                {/* Tags */}
                {video.tags && video.tags.length > 0 && (
                    <Box sx={{ display: 'flex', gap: 0.5, flexWrap: 'wrap', mb: 1.5 }}>
                        {video.tags.map((tag, index) => (
                            <Chip
                                key={index}
                                label={`#${tag}`}
                                size="small"
                                sx={{
                                    bgcolor: 'rgba(0, 229, 255, 0.1)',
                                    color: 'primary.main',
                                    border: '1px solid rgba(0, 229, 255, 0.3)',
                                    fontSize: '0.7rem',
                                    height: 20,
                                    fontFamily: '"Source Code Pro", monospace'
                                }}
                            />
                        ))}
                    </Box>
                )}

                {/* Action buttons */}
                <Box sx={{ display: 'flex', gap: 1, mb: 1 }}>
                    <Button
                        variant="outlined"
                        size="small"
                        startIcon={selected ? <CheckCircleIcon /> : <RadioButtonUncheckedIcon />}
                        onClick={onSelect}
                        sx={{
                            flex: 1,
                            borderColor: selected ? 'primary.main' : 'rgba(0, 229, 255, 0.3)',
                            color: selected ? 'primary.main' : 'text.secondary',
                            bgcolor: selected ? 'rgba(0, 229, 255, 0.1)' : 'transparent',
                            '&:hover': {
                                borderColor: 'primary.main',
                                bgcolor: 'rgba(0, 229, 255, 0.2)'
                            }
                        }}
                    >
                        {selected ? 'Selected' : 'Select'}
                    </Button>
                    <IconButton
                        size="small"
                        onClick={onInfo}
                        sx={{
                            color: 'primary.main',
                            border: '1px solid rgba(0, 229, 255, 0.3)',
                            '&:hover': {
                                bgcolor: 'rgba(0, 229, 255, 0.1)'
                            }
                        }}
                    >
                        <InfoIcon fontSize="small" />
                    </IconButton>
                    <IconButton
                        size="small"
                        onClick={onDelete}
                        sx={{
                            color: 'error.main',
                            border: '1px solid rgba(255, 23, 68, 0.3)',
                            '&:hover': {
                                bgcolor: 'rgba(255, 23, 68, 0.1)'
                            }
                        }}
                    >
                        <DeleteIcon fontSize="small" />
                    </IconButton>
                </Box>

                {/* Upload timestamp */}
                <Box sx={{ display: 'flex', alignItems: 'center', gap: 0.5 }}>
                    <CalendarTodayIcon sx={{ fontSize: 12, color: 'text.disabled' }} />
                    <Typography variant="caption" sx={{ color: 'text.disabled', fontSize: '0.65rem' }}>
                        {formatDate(video.uploaded_at)}
                    </Typography>
                </Box>
            </CardContent>
        </Card>
    );
};
