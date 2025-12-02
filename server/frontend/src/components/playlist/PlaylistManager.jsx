import React, { useState, useEffect } from 'react';
import { Box, Typography, Button, IconButton, List, ListItem, ListItemText, ListItemIcon, Chip } from '@mui/material';
import PlaylistPlayIcon from '@mui/icons-material/PlaylistPlay';
import AddIcon from '@mui/icons-material/Add';
import EditIcon from '@mui/icons-material/Edit';
import PlayArrowIcon from '@mui/icons-material/PlayArrow';
import PauseIcon from '@mui/icons-material/Pause';
import StopIcon from '@mui/icons-material/Stop';

// API URL - Use relative path to work with current host and port
const API_URL = '/api/playlist/playlists';

export const PlaylistManager = ({ isPlaying, onTogglePlay, onStop }) => {
    const [playlists, setPlaylists] = useState([]);
    const [loading, setLoading] = useState(true);

    useEffect(() => {
        fetchPlaylists();
    }, []);

    const fetchPlaylists = async () => {
        try {
            const response = await fetch(API_URL);
            if (response.ok) {
                const data = await response.json();
                // Map API data to UI format if needed, or ensure API returns compatible format
                // API returns { id, name, videos: [] }
                // UI expects { id, name, count, duration }
                const uiPlaylists = data.map(p => ({
                    ...p,
                    count: p.videos.length,
                    duration: '0:00' // Placeholder as API doesn't return duration yet
                }));
                setPlaylists(uiPlaylists);
            }
        } catch (error) {
            console.error('Error fetching playlists:', error);
        } finally {
            setLoading(false);
        }
    };

    const handleCreate = async () => {
        const newPlaylist = {
            id: `p${Date.now()}`,
            name: `New Playlist ${playlists.length + 1}`,
            videos: []
        };

        try {
            const response = await fetch(API_URL, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(newPlaylist)
            });

            if (response.ok) {
                const savedPlaylist = await response.json();
                setPlaylists([{ ...savedPlaylist, count: 0, duration: '0:00' }, ...playlists]);
            }
        } catch (error) {
            console.error('Error creating playlist:', error);
        }
    };

    return (
        <Box sx={{ height: '100%', display: 'flex', flexDirection: 'column', p: 2, bgcolor: 'rgba(20, 27, 45, 0.9)' }}>
            {/* Header & Controls */}
            <Box sx={{ mb: 2 }}>
                <Box sx={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', mb: 2 }}>
                    <Typography variant="h6" sx={{ color: 'primary.main', letterSpacing: '0.1em' }}>
                        PLAYLISTS
                    </Typography>
                    <Button
                        variant="contained"
                        startIcon={<AddIcon />}
                        onClick={handleCreate}
                        sx={{
                            bgcolor: 'primary.main',
                            color: '#000',
                            fontWeight: 'bold',
                            '&:hover': { bgcolor: '#00b8cc' }
                        }}
                    >
                        NEW
                    </Button>
                </Box>

                {/* Global Playback Controls */}
                <Box sx={{
                    display: 'flex',
                    gap: 2,
                    p: 1.5,
                    bgcolor: 'rgba(0,0,0,0.3)',
                    borderRadius: 2,
                    border: '1px solid rgba(0, 229, 255, 0.2)'
                }}>
                    <Button
                        fullWidth
                        variant={isPlaying ? "contained" : "outlined"}
                        startIcon={isPlaying ? <PauseIcon /> : <PlayArrowIcon />}
                        onClick={onTogglePlay}
                        sx={{
                            borderColor: 'primary.main',
                            color: isPlaying ? '#000' : 'primary.main',
                            bgcolor: isPlaying ? 'primary.main' : 'transparent',
                            '&:hover': {
                                bgcolor: isPlaying ? '#00b8cc' : 'rgba(0, 229, 255, 0.1)',
                                borderColor: 'primary.main'
                            }
                        }}
                    >
                        {isPlaying ? "PAUSE" : "PLAY"}
                    </Button>
                    <Button
                        variant="outlined"
                        onClick={onStop}
                        sx={{
                            minWidth: 60,
                            borderColor: 'error.main',
                            color: 'error.main',
                            '&:hover': { bgcolor: 'rgba(255, 23, 68, 0.1)', borderColor: 'error.main' }
                        }}
                    >
                        <StopIcon />
                    </Button>
                </Box>
            </Box>

            {/* List */}
            <List sx={{ flex: 1, overflow: 'auto' }}>
                {playlists.map((playlist) => (
                    <ListItem
                        key={playlist.id}
                        sx={{
                            mb: 1,
                            border: '1px solid rgba(0, 229, 255, 0.3)',
                            borderRadius: 1,
                            bgcolor: 'rgba(0, 0, 0, 0.3)',
                            transition: 'all 0.2s',
                            '&:hover': {
                                bgcolor: 'rgba(0, 229, 255, 0.05)',
                                borderColor: 'primary.main',
                                transform: 'translateX(4px)'
                            }
                        }}
                        secondaryAction={
                            <Box>
                                <IconButton size="small" sx={{ color: 'primary.main', mr: 1 }}>
                                    <EditIcon fontSize="small" />
                                </IconButton>
                                <IconButton size="small" sx={{ color: '#00ff00', border: '1px solid #00ff00' }}>
                                    <PlayArrowIcon fontSize="small" />
                                </IconButton>
                            </Box>
                        }
                    >
                        <ListItemIcon sx={{ minWidth: 40 }}>
                            <PlaylistPlayIcon sx={{ color: 'primary.main' }} />
                        </ListItemIcon>
                        <ListItemText
                            primary={playlist.name}
                            secondary={
                                <Box component="span" sx={{ display: 'flex', gap: 1, mt: 0.5 }}>
                                    <Chip label={`${playlist.count} Videos`} size="small" sx={{ height: 20, fontSize: '0.65rem', bgcolor: 'rgba(255,255,255,0.1)', color: '#ccc' }} />
                                    <Chip label={playlist.duration} size="small" sx={{ height: 20, fontSize: '0.65rem', bgcolor: 'rgba(255,255,255,0.1)', color: '#ccc' }} />
                                </Box>
                            }
                            primaryTypographyProps={{ color: '#fff', fontWeight: 600, fontFamily: '"Source Code Pro", monospace' }}
                        />
                    </ListItem>
                ))}
            </List>
        </Box>
    );
};
