import { createTheme } from '@mui/material/styles';

export const darkTechTheme = createTheme({
    palette: {
        mode: 'dark',
        primary: {
            main: '#00e5ff', // Cyan
            light: '#6effff',
            dark: '#00b2cc',
        },
        secondary: {
            main: '#ff4081', // Pink accent
            light: '#ff79b0',
            dark: '#c60055',
        },
        background: {
            default: '#0a0e27',
            paper: '#141b2d',
        },
        text: {
            primary: '#e0e0e0',
            secondary: '#00e5ff',
        },
        error: {
            main: '#ff1744',
        },
    },
    typography: {
        fontFamily: '"Titillium Web", "Roboto", sans-serif',
        h1: {
            fontFamily: '"Titillium Web", sans-serif',
            fontWeight: 700,
            letterSpacing: '0.1em',
        },
        h2: {
            fontFamily: '"Titillium Web", sans-serif',
            fontWeight: 600,
            letterSpacing: '0.08em',
        },
        h3: {
            fontFamily: '"Titillium Web", sans-serif',
            fontWeight: 600,
            letterSpacing: '0.05em',
        },
        body1: {
            fontFamily: '"Titillium Web", sans-serif',
        },
        button: {
            fontFamily: '"Titillium Web", sans-serif',
            fontWeight: 700,
            letterSpacing: '0.1em',
        },
    },
    components: {
        MuiPaper: {
            styleOverrides: {
                root: {
                    backgroundImage: 'linear-gradient(rgba(255, 255, 255, 0.05), rgba(255, 255, 255, 0.05))',
                    border: '1px solid rgba(0, 229, 255, 0.2)',
                },
            },
        },
        MuiButton: {
            styleOverrides: {
                root: {
                    borderRadius: 0,
                    border: '2px solid',
                    transition: 'all 0.3s',
                    '&:hover': {
                        boxShadow: '0 0 20px rgba(0, 229, 255, 0.5)',
                        transform: 'translateY(-2px)',
                    },
                },
            },
        },
    },
});
