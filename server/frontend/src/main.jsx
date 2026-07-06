import { StrictMode } from 'react'
import { createRoot } from 'react-dom/client'
// フォントは npm パッケージで同梱 (オフライン LAN 配信のため外部 CDN 参照禁止)
import '@fontsource-variable/manrope'
import '@fontsource/jetbrains-mono'
import './theme/tokens.css'
import App from './App.jsx'

createRoot(document.getElementById('root')).render(
  <StrictMode>
    <App />
  </StrictMode>,
)
