import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'

// https://vite.dev/config/
export default defineConfig({
  plugins: [react()],
  server: {
    port: 5173,
    proxy: {
      // Proxy API requests to Flask backend
      '/health': 'http://localhost:5001',
      '/aggregated': 'http://localhost:5001',
      '/aggregation': 'http://localhost:5001',
      '/export': 'http://localhost:5001',
      '/commands': 'http://localhost:5001',
      '/config': 'http://localhost:5001',
      '/ota': 'http://localhost:5001',
      '/diagnostics': 'http://localhost:5001',
      '/security': 'http://localhost:5001',
      '/compression': 'http://localhost:5001',
      '/fault': 'http://localhost:5001',
      '/devices': 'http://localhost:5001',
      '/utilities': 'http://localhost:5001',
      '/socket.io': {
        target: 'http://localhost:5001',
        ws: true,
      }
    }
  },
  build: {
    outDir: '../flask/static/frontend',
    emptyOutDir: true,
  },
  test: {
    globals: true,
    environment: 'jsdom',
    setupFiles: './tests/setup.js',
  },
})

