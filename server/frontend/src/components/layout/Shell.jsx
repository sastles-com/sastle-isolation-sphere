import React from 'react';
import { Box, AppBar, Toolbar, Typography, Container } from '@mui/material';

export const Shell = ({ children }) => {
    return (
        <Box
            sx={{
                minHeight: '100vh',
                bgcolor: 'background.default',
                backgroundImage: `
          radial-gradient(circle at 50% 50%, rgba(0, 229, 255, 0.1) 0%, transparent 50%),
          repeating-linear-gradient(
            0deg,
            rgba(0, 229, 255, 0.03) 0px,
            transparent 1px,
            transparent 2px,
            rgba(0, 229, 255, 0.03) 3px
          ),
          repeating-linear-gradient(
            90deg,
            rgba(0, 229, 255, 0.03) 0px,
            transparent 1px,
            transparent 2px,
            rgba(0, 229, 255, 0.03) 3px
          )
        `,
                backgroundSize: '100% 100%, 30px 30px, 30px 30px',
                position: 'relative',
                overflow: 'auto',
            }}
        >
            {/* Animated scanline effect */}
            <Box
                sx={{
                    position: 'fixed',
                    top: 0,
                    left: 0,
                    right: 0,
                    bottom: 0,
                    pointerEvents: 'none',
                    background: 'repeating-linear-gradient(0deg, rgba(0, 0, 0, 0.15) 0px, transparent 1px, transparent 2px)',
                    animation: 'scanline 8s linear infinite',
                    '@keyframes scanline': {
                        '0%': { transform: 'translateY(0)' },
                        '100%': { transform: 'translateY(30px)' },
                    },
                }}
            />

            <AppBar
                position="static"
                elevation={0}
                sx={{
                    bgcolor: 'rgba(20, 27, 45, 0.9)',
                    borderBottom: '2px solid',
                    borderColor: 'primary.main',
                    boxShadow: '0 0 20px rgba(0, 229, 255, 0.3)',
                }}
            >
                <Toolbar>
                    <Typography
                        variant="h5"
                        component="h1"
                        sx={{
                            flexGrow: 1,
                            color: 'primary.main',
                            textShadow: '0 0 10px rgba(0, 229, 255, 0.8)',
                            fontWeight: 700,
                            letterSpacing: '0.15em',
                        }}
                    >
                        ISOLATION SPHERE // CONTROL
                    </Typography>
                    <Box
                        sx={{
                            display: 'flex',
                            alignItems: 'center',
                            gap: 2,
                            fontFamily: '"Source Code Pro", monospace',
                            fontSize: '0.875rem',
                            color: 'primary.main',
                        }}
                    >
                        <Box component="span" sx={{ '&::before': { content: '"●"', color: '#00ff00', mr: 0.5 } }}>
                            ONLINE
                        </Box>
                    </Box>
                </Toolbar>
            </AppBar>

            <Container maxWidth="xl" sx={{ py: 4 }}>
                {children}
            </Container>
        </Box>
    );
};
