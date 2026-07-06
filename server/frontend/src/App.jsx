import React from 'react';
import { WebSocketProvider } from './contexts/WebSocketContext';
import { SpherePlayer } from './pages/SpherePlayer';

function App() {
  return (
    <WebSocketProvider>
      <SpherePlayer />
    </WebSocketProvider>
  );
}

export default App;
