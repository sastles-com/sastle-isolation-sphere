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
import { formatDuration, formatSize } from '../../lib/format';

export const VideoCard = ({
    video,
    selected = false,
    playing = false,
    onSelect,
    onInfo,
    onDelete,
    onPlay,
    mode = 'view' // 'view' | 'select'
}) => {
    return (
        <Card
            sx={{
                bgcolor: selected ? 'rgba(0, 229, 255, 0.1)' : 'rgba(20, 27, 45, 0.9)',
                border: playing ? '2px solid #00FF41' : (selected ? '2px solid #00E5FF' : '1px solid rgba(0, 229, 255, 0.2)'),
                borderRadius: 2,
                overflow: 'hidden',
                transition: 'all 0.2s ease',
                boxShadow: playing ? '0 0 20px rgba(0, 255, 65, 0.5)' : (selected ? '0 0 20px rgba(0, 229, 255, 0.4)' : 'none'),
                '&:active': {
                    transform: 'scale(0.98)'
                },
                position: 'relative',
                height: '100%',
                display: 'flex',
                flexDirection: 'column'
            }}
        >
            {/* Thumbnail */}
            <Box
                sx={{
                    position: 'relative',
                    width: '100%',
                    paddingTop: '100%', // 1:1 aspect ratio for grid
                    bgcolor: 'rgba(0, 0, 0, 0.5)',
                    cursor: 'pointer',
                    overflow: 'hidden',
                    '&:hover .play-overlay': {
                        opacity: 1
                    }
                }}
                onClick={onPlay || onInfo}
                title="再生"
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
                        <MovieIcon sx={{ fontSize: 48, color: 'rgba(0, 229, 255, 0.3)' }} />
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
                    <PlayArrowIcon sx={{ fontSize: 48, color: '#00E5FF' }} />
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

                {/* Selected indicator */}
                {selected && (
                    <Box
                        sx={{
                            position: 'absolute',
                            top: 8,
                            right: 8,
                            bgcolor: 'primary.main',
                            borderRadius: '50%',
                            width: 24,
                            height: 24,
                            display: 'flex',
                            alignItems: 'center',
                            justifyContent: 'center'
                        }}
                    >
                        <CheckCircleIcon sx={{ fontSize: 20, color: '#000' }} />
                    </Box>
                )}
            </Box>

            {/* Content */}
            <CardContent sx={{ p: 1, flex: 1, display: 'flex', flexDirection: 'column' }}>
                {/* Title */}
                <Typography
                    variant="body2"
                    sx={{
                        color: '#fff',
                        fontWeight: 600,
                        mb: 0.5,
                        fontSize: '0.75rem',
                        fontFamily: '"Source Code Pro", monospace',
                        overflow: 'hidden',
                        textOverflow: 'ellipsis',
                        whiteSpace: 'nowrap'
                    }}
                >
                    {video.title}
                </Typography>

                {/* Metadata - compact */}
                <Box sx={{ display: 'flex', gap: 1, mb: 0.5, flexWrap: 'wrap' }}>
                    <Typography variant="caption" sx={{ color: 'text.secondary', fontSize: '0.65rem' }}>
                        {formatDuration(video.duration_ms)}
                    </Typography>
                    <Typography variant="caption" sx={{ color: 'text.secondary', fontSize: '0.65rem' }}>
                        {formatSize(video.size_bytes)}
                    </Typography>
                </Box>

                {/* Action buttons - compact */}
                <Box sx={{ display: 'flex', gap: 0.5, mt: 'auto' }}>
                    <IconButton
                        size="small"
                        onClick={onSelect}
                        sx={{
                            flex: 1,
                            borderRadius: 1,
                            bgcolor: selected ? 'rgba(0, 229, 255, 0.2)' : 'transparent',
                            border: '1px solid',
                            borderColor: selected ? 'primary.main' : 'rgba(0, 229, 255, 0.3)',
                            color: selected ? 'primary.main' : 'text.secondary',
                            '&:hover': {
                                bgcolor: 'rgba(0, 229, 255, 0.1)'
                            }
                        }}
                    >
                        {selected ? <CheckCircleIcon fontSize="small" /> : <RadioButtonUncheckedIcon fontSize="small" />}
                    </IconButton>
                    <IconButton
                        size="small"
                        onClick={onInfo}
                        sx={{
                            borderRadius: 1,
                            border: '1px solid rgba(0, 229, 255, 0.3)',
                            color: 'primary.main',
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
                            borderRadius: 1,
                            border: '1px solid rgba(255, 23, 68, 0.3)',
                            color: 'error.main',
                            '&:hover': {
                                bgcolor: 'rgba(255, 23, 68, 0.1)'
                            }
                        }}
                    >
                        <DeleteIcon fontSize="small" />
                    </IconButton>
                </Box>
            </CardContent>
        </Card>
    );
};
