import React from 'react';
import { ThemeProvider, CssBaseline, GlobalStyles } from '@mui/material';
import { darkTechTheme } from './theme';
import { Dashboard } from './pages/Dashboard';

function App() {
  return (
    <ThemeProvider theme={darkTechTheme}>
      <CssBaseline />
      <GlobalStyles
        styles={{
          html: {
            margin: 0,
            padding: 0,
            height: '100%',
            overflow: 'hidden',
          },
          body: {
            margin: 0,
            padding: 0,
            height: '100%',
            overflow: 'hidden',
          },
          '#root': {
            height: '100%',
            overflow: 'hidden',
          },
        }}
      />
      <Dashboard />
    </ThemeProvider>
  );
}

export default App;
