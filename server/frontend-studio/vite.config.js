import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'

// Pattern Studio は FastAPI の /studio 配下に mount されるため base を固定する。
// (dist/index.html と assets の参照が /studio/... になる)
export default defineConfig({
  base: '/studio/',
  plugins: [react()],
  build: {
    chunkSizeWarningLimit: 1100,
    rollupOptions: {
      output: {
        manualChunks: {
          three: ['three'],
        },
      },
    },
  },
})
