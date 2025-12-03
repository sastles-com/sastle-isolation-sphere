import React, { useState } from 'react';
import { Box, Typography, Button, TextField, InputAdornment, IconButton, Chip } from '@mui/material';
import AddIcon from '@mui/icons-material/Add';
import SearchIcon from '@mui/icons-material/Search';
import FilterListIcon from '@mui/icons-material/FilterList';
import MovieIcon from '@mui/icons-material/Movie';
import { VideoCard } from './VideoCard';

// Mock Data (モバイルテック感)
const INITIAL_VIDEOS = [
    { 
        id: 1,
        uuid: 'v_001',
        title: 'Cyber City',
        description: 'Neon-lit cityscape at night',
        thumbnail_path: null,
        duration_ms: 320000, // 5:20
        size_bytes: 29360128, // 28MB
        width: 320,
        height: 160,
        fps: 30,
        tags: ['cyberpunk', 'neon', 'city'],
        uploaded_at: '2025-12-02T10:30:00Z'
    },
    { 
        id: 2,
        uuid: 'v_002',
        title: 'Ambient Rain',
        description: 'Calming rain sounds with visuals',
        thumbnail_path: null,
        duration_ms: 600000, // 10:00
        size_bytes: 47185920, // 45MB
        width: 320,
        height: 160,
        fps: 30,
        tags: ['ambient', 'nature', 'rain'],
        uploaded_at: '2025-12-01T15:20:00Z'
    },
    { 
        id: 3,
        uuid: 'v_003',
        title: 'Sunrise',
        description: 'Morning sunrise timelapse',
        thumbnail_path: null,
        duration_ms: 150000, // 2:30
        size_bytes: 15728640, // 15MB
        width: 320,
        height: 160,
        fps: 30,
        tags: ['nature', 'sunrise', 'timelapse'],
        uploaded_at: '2025-12-01T08:00:00Z'
    },
    { 
        id: 4,
        uuid: 'v_004',
        title: 'Coffee Brewing',
        description: 'Coffee making process',
        thumbnail_path: null,
        duration_ms: 225000, // 3:45
        size_bytes: 20971520, // 20MB
        width: 320,
        height: 160,
        fps: 30,
        tags: ['coffee', 'morning', 'process'],
        uploaded_at: '2025-11-30T09:15:00Z'
    }
];

export const VideoManager = () => {
    const [videos, setVideos] = useState(INITIAL_VIDEOS);
    const [selectedVideos, setSelectedVideos] = useState([]);
    const [searchQuery, setSearchQuery] = useState('');

    const handleSelect = (videoId) => {
        setSelectedVideos(prev => {
            if (prev.includes(videoId)) {
                return prev.filter(id => id !== videoId);
            } else {
                return [...prev, videoId];
            }
        });
    };

    const handleDelete = (videoId) => {
        if (window.confirm('Delete this video?')) {
            setVideos(videos.filter(v => v.id !== videoId));
            setSelectedVideos(selectedVideos.filter(id => id !== videoId));
        }
    };

    const handleInfo = (video) => {
        alert(`Video Info:\n\nTitle: ${video.title}\nDuration: ${Math.floor(video.duration_ms/1000)}s\nSize: ${(video.size_bytes/1024/1024).toFixed(1)}MB`);
    };

    const handleUpload = () => {
        alert('Upload functionality will be implemented in Phase 2');
    };

    const handleCreatePlaylist = () => {
        const selected = videos.filter(v => selectedVideos.includes(v.id));
        alert(`Create Playlist with ${selected.length} videos:\n\n${selected.map(v => v.title).join('\n')}`);
    };

    const totalSize = videos.reduce((sum, v) => sum + v.size_bytes, 0);
    const filteredVideos = videos.filter(v => 
        v.title.toLowerCase().includes(searchQuery.toLowerCase()) ||
        v.tags.some(tag => tag.toLowerCase().includes(searchQuery.toLowerCase()))
    );


    return (
        <Box 
            sx={{ 
                width: '100%',
                height: '100%', 
                display: 'flex', 
                flexDirection: 'column',
                bgcolor: 'rgba(20, 27, 45, 0.9)',
                position: 'relative'
            }}
        >
            {/* Header */}
            <Box 
                sx={{ 
                    p: 2, 
                    borderBottom: '1px solid rgba(0, 229, 255, 0.2)',
                    bgcolor: 'rgba(20, 27, 45, 0.95)',
                    flexShrink: 0
                }}
            >
                <Box sx={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', mb: 2 }}>
                    <Typography 
                        variant="h5" 
                        sx={{ 
                            color: 'primary.main', 
                            letterSpacing: '0.15em',
                            fontWeight: 700,
                            fontFamily: '"Source Code Pro", monospace',
                            display: 'flex',
                            alignItems: 'center',
                            gap: 1
                        }}
                    >
                        <Box component="span" sx={{ fontSize: '1.5rem' }}>▤</Box>
                        VIDEO LIBRARY
                    </Typography>
                    <IconButton
                        onClick={handleUpload}
                        sx={{
                            bgcolor: 'rgba(0, 229, 255, 0.1)',
                            border: '2px solid #00E5FF',
                            color: 'primary.main',
                            '&:hover': {
                                bgcolor: 'rgba(0, 229, 255, 0.2)',
                                boxShadow: '0 0 15px rgba(0, 229, 255, 0.5)'
                            }
                        }}
                    >
                        <AddIcon />
                    </IconButton>
                </Box>

                {/* Search Bar */}
                <TextField
                    fullWidth
                    placeholder="Search videos..."
                    value={searchQuery}
                    onChange={(e) => setSearchQuery(e.target.value)}
                    size="small"
                    InputProps={{
                        startAdornment: (
                            <InputAdornment position="start">
                                <SearchIcon sx={{ color: 'primary.main' }} />
                            </InputAdornment>
                        ),
                        endAdornment: (
                            <InputAdornment position="end">
                                <IconButton size="small" sx={{ color: 'primary.main' }}>
                                    <FilterListIcon />
                                </IconButton>
                            </InputAdornment>
                        )
                    }}
                    sx={{
                        '& .MuiOutlinedInput-root': {
                            bgcolor: 'rgba(20, 27, 45, 0.5)',
                            color: '#fff',
                            '& fieldset': {
                                borderColor: 'rgba(0, 229, 255, 0.3)'
                            },
                            '&:hover fieldset': {
                                borderColor: 'rgba(0, 229, 255, 0.5)'
                            },
                            '&.Mui-focused fieldset': {
                                borderColor: 'primary.main'
                            }
                        }
                    }}
                />
            </Box>

            {/* Video List - Grid Layout */}
            <Box 
                sx={{ 
                    flex: 1, 
                    overflow: 'auto',
                    p: 2,
                    // Prevent parent swipe handlers
                    touchAction: 'pan-y', // Only allow vertical panning
                    // Custom scrollbar
                    '&::-webkit-scrollbar': {
                        width: '8px'
                    },
                    '&::-webkit-scrollbar-track': {
                        background: 'rgba(0, 0, 0, 0.2)'
                    },
                    '&::-webkit-scrollbar-thumb': {
                        background: 'rgba(0, 229, 255, 0.3)',
                        borderRadius: '4px',
                        '&:hover': {
                            background: 'rgba(0, 229, 255, 0.5)'
                        }
                    }
                }}
                onTouchStart={(e) => {
                    // Stop event propagation to parent swipe handlers
                    e.stopPropagation();
                }}
                onTouchMove={(e) => {
                    // Allow vertical scrolling, prevent horizontal swipe
                    e.stopPropagation();
                }}
            >
                {filteredVideos.length === 0 ? (
                    <Box 
                        sx={{ 
                            textAlign: 'center', 
                            py: 8,
                            color: 'text.secondary'
                        }}
                    >
                        <MovieIcon sx={{ fontSize: 80, opacity: 0.2, mb: 2 }} />
                        <Typography variant="h6" sx={{ mb: 1 }}>
                            {searchQuery ? 'No videos found' : 'No videos uploaded yet'}
                        </Typography>
                        <Typography variant="body2" sx={{ mb: 3 }}>
                            {searchQuery ? 'Try different keywords' : 'Upload your first video to get started'}
                        </Typography>
                        {!searchQuery && (
                            <Button
                                variant="outlined"
                                startIcon={<AddIcon />}
                                onClick={handleUpload}
                                sx={{
                                    borderColor: 'primary.main',
                                    color: 'primary.main',
                                    '&:hover': {
                                        borderColor: '#fff',
                                        bgcolor: 'rgba(0, 229, 255, 0.1)'
                                    }
                                }}
                            >
                                Upload First Video
                            </Button>
                        )}
                    </Box>
                ) : (
                    <Box
                        sx={{
                            display: 'grid',
                            gridTemplateColumns: 'repeat(auto-fill, minmax(140px, 1fr))',
                            gap: 2,
                            '@media (min-width: 600px)': {
                                gridTemplateColumns: 'repeat(auto-fill, minmax(160px, 1fr))'
                            }
                        }}
                    >
                        {filteredVideos.map((video) => (
                            <VideoCard
                                key={video.id}
                                video={video}
                                selected={selectedVideos.includes(video.id)}
                                onSelect={() => handleSelect(video.id)}
                                onInfo={() => handleInfo(video)}
                                onDelete={() => handleDelete(video.id)}
                            />
                        ))}
                    </Box>
                )}
            </Box>

            {/* Footer */}
            <Box 
                sx={{ 
                    p: 2, 
                    borderTop: '1px solid rgba(0, 229, 255, 0.2)',
                    bgcolor: 'rgba(20, 27, 45, 0.95)',
                    flexShrink: 0
                }}
            >
                <Box sx={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', mb: 1 }}>
                    <Box sx={{ display: 'flex', gap: 1, flexWrap: 'wrap' }}>
                        <Chip
                            icon={<MovieIcon sx={{ fontSize: 16 }} />}
                            label={`${videos.length} videos`}
                            size="small"
                            sx={{
                                bgcolor: 'rgba(0, 229, 255, 0.1)',
                                color: 'primary.main',
                                border: '1px solid rgba(0, 229, 255, 0.3)',
                                fontFamily: '"Source Code Pro", monospace'
                            }}
                        />
                        <Chip
                            label={`${(totalSize / 1024 / 1024).toFixed(1)} MB`}
                            size="small"
                            sx={{
                                bgcolor: 'rgba(0, 229, 255, 0.1)',
                                color: 'primary.main',
                                border: '1px solid rgba(0, 229, 255, 0.3)',
                                fontFamily: '"Source Code Pro", monospace'
                            }}
                        />
                        {selectedVideos.length > 0 && (
                            <Chip
                                label={`${selectedVideos.length} selected`}
                                size="small"
                                sx={{
                                    bgcolor: 'rgba(0, 229, 255, 0.2)',
                                    color: '#00FF41',
                                    border: '1px solid #00FF41',
                                    fontFamily: '"Source Code Pro", monospace',
                                    animation: 'pulse 2s infinite',
                                    '@keyframes pulse': {
                                        '0%, 100%': { opacity: 1 },
                                        '50%': { opacity: 0.7 }
                                    }
                                }}
                            />
                        )}
                    </Box>
                </Box>
                {selectedVideos.length > 0 && (
                    <Button
                        fullWidth
                        variant="contained"
                        onClick={handleCreatePlaylist}
                        sx={{
                            bgcolor: 'primary.main',
                            color: '#000',
                            fontWeight: 700,
                            letterSpacing: '0.1em',
                            fontFamily: '"Source Code Pro", monospace',
                            '&:hover': {
                                bgcolor: '#fff',
                                boxShadow: '0 0 20px rgba(0, 229, 255, 0.6)'
                            }
                        }}
                    >
                        Create Playlist →
                    </Button>
                )}
            </Box>
        </Box>
    );
};
