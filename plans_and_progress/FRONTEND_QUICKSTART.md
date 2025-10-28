# EcoWatt Frontend Quick Start Guide

## Technology Stack (CONFIRMED)

- **Framework**: React 18 with Vite
- **UI Library**: Material-UI (MUI) v5
- **State Management**: React Context API + React Query
- **Charts**: Recharts
- **HTTP Client**: Axios
- **Real-time**: Socket.IO Client
- **Testing**: Jest + React Testing Library + Cypress
- **Routing**: React Router v6
- **Deployment**: Served from Flask static folder

---

## Installation Steps

### Step 1: Create Vite Project

```bash
cd /home/akitha/Desktop/EcoWatt_TeamPowerPort
npm create vite@latest frontend -- --template react
cd frontend
```

### Step 2: Install Core Dependencies

```bash
# UI Framework
npm install @mui/material @emotion/react @emotion/styled @mui/icons-material

# Routing
npm install react-router-dom

# HTTP & WebSocket
npm install axios socket.io-client react-query

# Charts
npm install recharts

# Date utilities
npm install date-fns

# Forms
npm install react-hook-form yup @hookform/resolvers
```

### Step 3: Install Dev Dependencies

```bash
# Testing
npm install -D @testing-library/react @testing-library/jest-dom @testing-library/user-event
npm install -D vitest jsdom @vitest/ui
npm install -D cypress

# Code Quality
npm install -D eslint prettier eslint-config-prettier eslint-plugin-react
```

---

## Project Structure Setup

Create the following folder structure:

```
frontend/
├── public/
│   └── favicon.ico
├── src/
│   ├── api/
│   │   ├── axios.js
│   │   ├── websocket.js
│   │   ├── devices.js
│   │   ├── aggregation.js
│   │   ├── commands.js
│   │   ├── config.js
│   │   ├── ota.js
│   │   ├── diagnostics.js
│   │   ├── security.js
│   │   └── fault.js
│   ├── components/
│   │   ├── common/
│   │   │   ├── Navbar.jsx
│   │   │   ├── Sidebar.jsx
│   │   │   ├── LoadingSpinner.jsx
│   │   │   └── ErrorBoundary.jsx
│   │   ├── dashboard/
│   │   │   ├── MetricsCard.jsx
│   │   │   ├── TimeSeriesChart.jsx
│   │   │   └── DeviceSelector.jsx
│   │   ├── config/
│   │   │   ├── ConfigForm.jsx
│   │   │   └── ConfigHistory.jsx
│   │   ├── commands/
│   │   │   ├── CommandBuilder.jsx
│   │   │   ├── CommandQueue.jsx
│   │   │   └── CommandHistory.jsx
│   │   ├── ota/
│   │   │   ├── FirmwareUpload.jsx
│   │   │   ├── OTAProgress.jsx
│   │   │   └── FirmwareList.jsx
│   │   ├── logs/
│   │   │   ├── LogViewer.jsx
│   │   │   └── LogFilters.jsx
│   │   ├── utilities/
│   │   │   ├── FirmwarePrep.jsx
│   │   │   ├── KeyGenerator.jsx
│   │   │   └── CompressionBench.jsx
│   │   └── testing/
│   │       ├── FaultInjection.jsx
│   │       ├── SecurityTests.jsx
│   │       ├── UploadTests.jsx
│   │       └── OTATests.jsx
│   ├── pages/
│   │   ├── Dashboard.jsx
│   │   ├── Configuration.jsx
│   │   ├── Commands.jsx
│   │   ├── FOTA.jsx
│   │   ├── Logs.jsx
│   │   ├── Utilities.jsx
│   │   └── Testing.jsx
│   ├── contexts/
│   │   ├── DeviceContext.jsx
│   │   └── WebSocketContext.jsx
│   ├── hooks/
│   │   ├── useDevices.js
│   │   ├── useCommands.js
│   │   ├── useWebSocket.js
│   │   └── useNotification.js
│   ├── utils/
│   │   ├── formatters.js
│   │   ├── validators.js
│   │   └── constants.js
│   ├── theme/
│   │   └── theme.js
│   ├── App.jsx
│   ├── main.jsx
│   └── index.css
├── cypress/
│   ├── e2e/
│   └── support/
├── tests/
│   └── unit/
├── .eslintrc.cjs
├── .prettierrc
├── cypress.config.js
├── vite.config.js
├── package.json
└── README.md
```

---

## Flask Backend Updates Needed

### 1. Add Flask-CORS Support

```bash
cd /home/akitha/Desktop/EcoWatt_TeamPowerPort/flask
pip install flask-cors flask-socketio
```

Update `flask_server_modular.py`:

```python
from flask_cors import CORS
from flask_socketio import SocketIO

app = Flask(__name__)
CORS(app)  # Enable CORS for React dev server
socketio = SocketIO(app, cors_allowed_origins="*")

# ... existing code ...

if __name__ == '__main__':
    socketio.run(app, host='0.0.0.0', port=5001, debug=True)
```

### 2. Add WebSocket Events

Create `flask/handlers/websocket_handler.py`:

```python
from flask_socketio import emit
import logging

logger = logging.getLogger(__name__)

def register_websocket_events(socketio):
    @socketio.on('connect')
    def handle_connect():
        logger.info("Client connected")
        emit('connection_response', {'status': 'connected'})
    
    @socketio.on('disconnect')
    def handle_disconnect():
        logger.info("Client disconnected")
    
    @socketio.on('subscribe_device')
    def handle_subscribe(data):
        device_id = data.get('device_id')
        logger.info(f"Client subscribed to device: {device_id}")
        # Join a room for this device
        from flask_socketio import join_room
        join_room(device_id)
```

### 3. Add Missing Endpoints

Create `flask/routes/device_routes.py`:

```python
from flask import Blueprint, jsonify, request
import logging

device_bp = Blueprint('devices', __name__)
logger = logging.getLogger(__name__)

# Simple in-memory storage (replace with DB in production)
devices = {}

@device_bp.route('/devices', methods=['GET'])
def get_devices():
    """Get all registered devices"""
    return jsonify({
        'success': True,
        'devices': list(devices.values())
    }), 200

@device_bp.route('/devices/<device_id>', methods=['GET'])
def get_device(device_id: str):
    """Get specific device details"""
    if device_id in devices:
        return jsonify({
            'success': True,
            'device': devices[device_id]
        }), 200
    else:
        return jsonify({
            'success': False,
            'error': 'Device not found'
        }), 404
```

Add to `flask_server_modular.py`:

```python
from routes.device_routes import device_bp
app.register_blueprint(device_bp)
```

---

## Development Workflow

### 1. Start Flask Backend (Terminal 1)

```bash
cd /home/akitha/Desktop/EcoWatt_TeamPowerPort/flask
python3 flask_server_modular.py
```

Flask will run on: `http://localhost:5001`

### 2. Start Frontend Dev Server (Terminal 2)

```bash
cd /home/akitha/Desktop/EcoWatt_TeamPowerPort/frontend
npm run dev
```

Vite will run on: `http://localhost:5173`

### 3. Access Application

Open browser: `http://localhost:5173`

---

## Key Configuration Files

### `vite.config.js`

```javascript
import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'

export default defineConfig({
  plugins: [react()],
  server: {
    port: 5173,
    proxy: {
      '/api': {
        target: 'http://localhost:5001',
        changeOrigin: true,
      },
      '/socket.io': {
        target: 'http://localhost:5001',
        ws: true,
      }
    }
  },
  build: {
    outDir: '../flask/static/frontend',
    emptyOutDir: true,
  }
})
```

### `src/api/axios.js`

```javascript
import axios from 'axios';

const api = axios.create({
  baseURL: import.meta.env.DEV ? 'http://localhost:5001' : '',
  timeout: 10000,
  headers: {
    'Content-Type': 'application/json',
  },
});

// Request interceptor
api.interceptors.request.use(
  (config) => {
    console.log(`API Request: ${config.method.toUpperCase()} ${config.url}`);
    return config;
  },
  (error) => {
    return Promise.reject(error);
  }
);

// Response interceptor
api.interceptors.response.use(
  (response) => {
    return response;
  },
  (error) => {
    console.error('API Error:', error.response?.data || error.message);
    return Promise.reject(error);
  }
);

export default api;
```

### `src/api/websocket.js`

```javascript
import { io } from 'socket.io-client';

const SOCKET_URL = import.meta.env.DEV ? 'http://localhost:5001' : '';

let socket = null;

export const connectWebSocket = () => {
  if (!socket) {
    socket = io(SOCKET_URL, {
      transports: ['websocket'],
      reconnection: true,
      reconnectionDelay: 1000,
      reconnectionAttempts: 5,
    });

    socket.on('connect', () => {
      console.log('WebSocket connected');
    });

    socket.on('disconnect', () => {
      console.log('WebSocket disconnected');
    });

    socket.on('connect_error', (error) => {
      console.error('WebSocket connection error:', error);
    });
  }

  return socket;
};

export const disconnectWebSocket = () => {
  if (socket) {
    socket.disconnect();
    socket = null;
  }
};

export const getSocket = () => socket;
```

---

## Testing Setup

### `vitest.config.js`

```javascript
import { defineConfig } from 'vitest/config'
import react from '@vitejs/plugin-react'

export default defineConfig({
  plugins: [react()],
  test: {
    globals: true,
    environment: 'jsdom',
    setupFiles: './tests/setup.js',
  },
})
```

### `tests/setup.js`

```javascript
import '@testing-library/jest-dom'
import { expect, afterEach } from 'vitest'
import { cleanup } from '@testing-library/react'

afterEach(() => {
  cleanup()
})
```

### `cypress.config.js`

```javascript
import { defineConfig } from 'cypress'

export default defineConfig({
  e2e: {
    baseUrl: 'http://localhost:5173',
    setupNodeEvents(on, config) {
      // implement node event listeners here
    },
  },
})
```

---

## Build for Production

### 1. Build Frontend

```bash
cd /home/akitha/Desktop/EcoWatt_TeamPowerPort/frontend
npm run build
```

This will build to: `/home/akitha/Desktop/EcoWatt_TeamPowerPort/flask/static/frontend/`

### 2. Update Flask to Serve Frontend

Add to `flask_server_modular.py`:

```python
from flask import send_from_directory
import os

# Serve React frontend
@app.route('/', defaults={'path': ''})
@app.route('/<path:path>')
def serve_frontend(path):
    frontend_dir = os.path.join(app.root_path, 'static', 'frontend')
    if path != "" and os.path.exists(os.path.join(frontend_dir, path)):
        return send_from_directory(frontend_dir, path)
    else:
        return send_from_directory(frontend_dir, 'index.html')
```

### 3. Start Flask

```bash
cd /home/akitha/Desktop/EcoWatt_TeamPowerPort/flask
python3 flask_server_modular.py
```

Access at: `http://localhost:5001`

---

## Useful Commands

```bash
# Development
npm run dev              # Start dev server
npm run build            # Build for production
npm run preview          # Preview production build

# Testing
npm run test             # Run unit tests
npm run test:watch       # Run tests in watch mode
npm run test:ui          # Open Vitest UI
npm run cypress:open     # Open Cypress UI
npm run cypress:run      # Run Cypress headless

# Code Quality
npm run lint             # Run ESLint
npm run format           # Run Prettier
```

---

## Next Steps

1. ✅ Review this guide
2. ⏳ Initialize Vite project
3. ⏳ Install dependencies
4. ⏳ Create folder structure
5. ⏳ Setup Flask CORS and WebSocket
6. ⏳ Create basic layout and routing
7. ⏳ Start Phase 1: Dashboard implementation

Ready to start? I can help you execute these steps!
