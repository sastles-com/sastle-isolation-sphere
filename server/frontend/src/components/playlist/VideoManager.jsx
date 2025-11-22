import React, { useState } from 'react';
import { Box, Typography, Button, IconButton, List, ListItem, ListItemText, ListItemAvatar, Avatar } from '@mui/material';
import UploadFileIcon from '@mui/icons-material/UploadFile';
import DeleteIcon from '@mui/icons-material/Delete';
import MovieIcon from '@mui/icons-material/Movie';

// Mock Data
const INITIAL_VIDEOS = [
    { id: 'v1', title: 'Isolation Theme', duration: '3:45', size: '12MB' },
    { id: 'v2', title: 'Ambient Rain', duration: '10:00', size: '45MB' },
    { id: 'v3', title: 'Cyber City', duration: '5:20', size: '28MB' },
    { id: 'v4', title: 'Abstract Loop', duration: '1:30', size: '8MB' },
];

export const VideoManager = () => {
    const [videos, setVideos] = useState(INITIAL_VIDEOS);

    const handleDelete = (id) => {
        setVideos(videos.filter(v => v.id !== id));
    };

    const handleUpload = () => {
        // Mock upload
        const newVideo = {
            id: `v${Date.now()}`,
            title: `New Video ${videos.length + 1}`,
            duration: '0:00',
            size: '0MB'
        };
        setVideos([newVideo, ...videos]);
    };

    return (
        <Box sx={{ height: '100%', display: 'flex', flexDirection: 'column', p: 2, bgcolor: 'rgba(20, 27, 45, 0.9)' }}>
            {/* Header */}
            <Box sx={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', mb: 2 }}>
                <Typography variant="h6" sx={{ color: 'primary.main', letterSpacing: '0.1em' }}>
                    VIDEO LIBRARY
                </Typography>
                <Button
                    variant="outlined"
                    startIcon={<UploadFileIcon />}
                    onClick={handleUpload}
                    sx={{
                        borderColor: 'primary.main',
                        color: 'primary.main',
                        '&:hover': { borderColor: '#fff', bgcolor: 'rgba(0, 229, 255, 0.1)' }
                    }}
                >
                    UPLOAD
                </Button>
            </Box>

            {/* List */}
            <List sx={{ flex: 1, overflow: 'auto', border: '1px solid rgba(0, 229, 255, 0.2)', borderRadius: 1 }}>
                {videos.map((video) => (
                    <ListItem
                        key={video.id}
                        secondaryAction={
                            <IconButton edge="end" aria-label="delete" onClick={() => handleDelete(video.id)} sx={{ color: 'error.main' }}>
                                <DeleteIcon />
                            </IconButton>
                        }
                        sx={{
                            borderBottom: '1px solid rgba(255, 255, 255, 0.05)',
                            '&:hover': { bgcolor: 'rgba(0, 229, 255, 0.05)' }
                        }}
                    >
                        <ListItemAvatar>
                            <Avatar sx={{ bgcolor: 'rgba(0, 229, 255, 0.1)', color: 'primary.main' }}>
                                <MovieIcon />
                            </Avatar>
                        </ListItemAvatar>
                        <ListItemText
                            primary={video.title}
                            secondary={`${video.duration} • ${video.size}`}
                            primaryTypographyProps={{ color: '#fff', fontFamily: '"Source Code Pro", monospace' }}
                            secondaryTypographyProps={{ color: 'text.secondary', fontSize: '0.8rem' }}
                        />
                    </ListItem>
                ))}
            </List>
        </Box>
    );
};
