import React from 'react';
import { MotionConfig } from 'framer-motion';
import { WebSocketProvider } from './contexts/WebSocketContext';
import { SpherePlayer } from './pages/SpherePlayer';

function App() {
  return (
    // reducedMotion="user" で prefers-reduced-motion を尊重し、
    // transform 系アニメーションを無効化 (opacity は維持, 仕様 §3.3)
    <MotionConfig reducedMotion="user">
      <WebSocketProvider>
        <SpherePlayer />
      </WebSocketProvider>
    </MotionConfig>
  );
}

export default App;
