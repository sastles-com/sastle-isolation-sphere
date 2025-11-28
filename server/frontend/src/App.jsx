import React from 'react';
import { BrowserRouter as Router, Routes, Route } from 'react-router-dom';
import { ThemeProvider, createTheme, CssBaseline, GlobalStyles } from '@mui/material';
import { Dashboard } from './pages/Dashboard';
import { WebSocketProvider } from './contexts/WebSocketContext';
import './App.css';

const theme = createTheme({
  palette: {
    mode: 'dark',
    primary: {
      main: '#00e5ff',
    },
    secondary: {
      main: '#ff00aa',
    },
    background: {
      default: '#0a0f1c',
      paper: '#141b2d',
    },
  },
  typography: {
    fontFamily: '"Inter", "Roboto", "Helvetica", "Arial", sans-serif',
  },
});

const globalStyles = (
  <GlobalStyles
    styles={{
      'html, body, #root': {
        height: '100%',
        width: '100%',
        margin: 0,
        padding: 0,
        overflow: 'hidden', // Prevent scrolling on body
        overscrollBehavior: 'none', // Prevent browser navigation gestures
      },
    }}
  />
);

function App() {
  return (
    <ThemeProvider theme={theme}>
      <CssBaseline />
      {globalStyles}
      <WebSocketProvider>
        <Router>
          <Routes>
            <Route path="/" element={<Dashboard />} />
          </Routes>
        </Router>
      </WebSocketProvider>
    </ThemeProvider>
  );
}

export default App;
