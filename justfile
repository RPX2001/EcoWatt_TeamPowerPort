# EcoWatt Project - Root Task Automation
# Manages Flask backend, React frontend, and ESP32 firmware
# Usage: just <command>

# Python virtual environment path
venv_dir := ".venv"
python := venv_dir + "/bin/python3"
pip := venv_dir + "/bin/pip"

# Detect OS for different commands
os_type := if os() == "linux" { "linux" } else if os() == "macos" { "macos" } else { "windows" }

# Default recipe - show all available commands
default:
    @just --list

# ============================================================
# Initial Setup
# ============================================================

# Complete project setup (first-time installation)
setup: check-just install-python install-node install-platformio install-deps
    @echo ""
    @echo "════════════════════════════════════════════════════"
    @echo "✅ EcoWatt Project Setup Complete!"
    @echo "════════════════════════════════════════════════════"
    @echo ""
    @echo "Next steps:"
    @echo "  1. Start backend:  just server"
    @echo "  2. Start frontend: just dev"
    @echo "  3. Flash ESP32:    just flash"
    @echo "  4. Monitor ESP32:  just monitor"
    @echo ""

# Check if just is installed (called automatically)
@check-just:
    @echo "Checking for just command runner..."
    @if ! command -v just &> /dev/null; then \
        echo "❌ just is not installed!"; \
        echo ""; \
        echo "Install just:"; \
        echo "  • Ubuntu/Debian: curl --proto '=https' --tlsv1.2 -sSf https://just.systems/install.sh | bash -s -- --to /usr/local/bin"; \
        echo "  • macOS:         brew install just"; \
        echo "  • Arch:          sudo pacman -S just"; \
        echo ""; \
        echo "Or visit: https://github.com/casey/just#installation"; \
        exit 1; \
    else \
        echo "✅ just is installed ($(just --version))"; \
    fi

# Install Python dependencies (Flask server)
install-python:
    @echo ""
    @echo "════════════════════════════════════════════════════"
    @echo "📦 Installing Python Dependencies"
    @echo "════════════════════════════════════════════════════"
    @if ! command -v python3 &> /dev/null; then \
        echo "❌ Python 3 is not installed!"; \
        echo "Install Python 3.8+ and try again."; \
        exit 1; \
    fi
    @echo "✅ Python $(python3 --version) found"
    @echo "Creating virtual environment..."
    @if [ ! -d "{{venv_dir}}" ]; then \
        python3 -m venv {{venv_dir}}; \
        echo "✅ Virtual environment created"; \
    else \
        echo "✅ Virtual environment already exists"; \
    fi
    @echo "Installing Flask dependencies..."
    {{pip}} install --upgrade pip
    {{pip}} install -r flask/requirements.txt
    @echo "✅ Python dependencies installed"

# Install Node.js dependencies (React frontend)
install-node:
    @echo ""
    @echo "════════════════════════════════════════════════════"
    @echo "📦 Installing Node.js Dependencies"
    @echo "════════════════════════════════════════════════════"
    @if ! command -v node &> /dev/null; then \
        echo "❌ Node.js is not installed!"; \
        echo ""; \
        echo "Install Node.js 18+ from:"; \
        echo "  • Ubuntu/Debian: curl -fsSL https://deb.nodesource.com/setup_20.x | sudo -E bash - && sudo apt-get install -y nodejs"; \
        echo "  • macOS:         brew install node"; \
        echo "  • Or visit:      https://nodejs.org/"; \
        exit 1; \
    fi
    @echo "✅ Node.js $(node --version) found"
    @echo "✅ npm $(npm --version) found"
    @echo "Installing frontend dependencies..."
    cd front-end && npm install
    @echo "✅ Node.js dependencies installed"

# Install PlatformIO for ESP32 development
install-platformio:
    @echo ""
    @echo "════════════════════════════════════════════════════"
    @echo "📦 Installing PlatformIO"
    @echo "════════════════════════════════════════════════════"
    @if ! command -v pio &> /dev/null && [ ! -f "$HOME/.platformio/penv/bin/platformio" ]; then \
        echo "Installing PlatformIO Core..."; \
        {{pip}} install platformio; \
        echo "✅ PlatformIO installed"; \
    else \
        echo "✅ PlatformIO already installed"; \
    fi
    @if command -v pio &> /dev/null; then \
        echo "✅ PlatformIO $(pio --version)"; \
    elif [ -f "$HOME/.platformio/penv/bin/platformio" ]; then \
        echo "✅ PlatformIO $($HOME/.platformio/penv/bin/platformio --version)"; \
    fi

# Install all project dependencies
install-deps: install-python install-node install-platformio
    @echo ""
    @echo "✅ All dependencies installed successfully"

# ============================================================
# Development - Backend (Flask)
# ============================================================

# Start Flask backend server (development mode)
server:
    @echo "🚀 Starting Flask backend server..."
    @echo "Server will be available at: http://localhost:5001"
    @echo ""
    cd flask && {{python}} flask_server_modular.py

# Start Flask server in background
server-bg:
    @echo "🚀 Starting Flask server in background..."
    cd flask && nohup {{python}} flask_server_modular.py > server.log 2>&1 &
    @echo "✅ Server started in background (logs: flask/server.log)"
    @echo "   Access at: http://localhost:5001"

# Stop background Flask server
server-stop:
    @echo "🛑 Stopping Flask server..."
    @pkill -f flask_server_modular.py || echo "Server not running"
    @echo "✅ Server stopped"

# View Flask server logs
server-logs:
    @echo "📋 Flask server logs (Ctrl+C to exit):"
    @tail -f flask/server.log

# ============================================================
# Development - Frontend (React)
# ============================================================

# Start React frontend development server
dev:
    @echo "🚀 Starting React frontend development server..."
    @echo "Frontend will be available at: http://localhost:5173"
    @echo ""
    cd front-end && npm run dev

# Build frontend for production
build:
    @echo "🏗️  Building React frontend for production..."
    cd front-end && npm run build
    @echo "✅ Frontend built to: flask/static/frontend"

# Install frontend dependencies only
install-frontend:
    @echo "📦 Installing frontend dependencies..."
    cd front-end && npm install
    @echo "✅ Frontend dependencies installed"

# Run frontend tests
test-frontend:
    @echo "🧪 Running frontend tests..."
    cd front-end && npm run test

# ============================================================
# Development - ESP32 Firmware
# ============================================================

# Flash firmware to ESP32 (build + upload)
flash:
    @echo "⚡ Flashing ESP32 firmware..."
    cd PIO/ECOWATT && just flash

# Build ESP32 firmware only (no upload)
build-esp32:
    @echo "🔨 Building ESP32 firmware..."
    cd PIO/ECOWATT && just build

# Upload pre-built firmware to ESP32
upload-esp32:
    @echo "📤 Uploading firmware to ESP32..."
    cd PIO/ECOWATT && just upload

# Monitor ESP32 serial output
monitor:
    @echo "🖥️  Monitoring ESP32 serial output (Ctrl+C to exit)..."
    @echo ""
    cd PIO/ECOWATT && just monitor

# Flash and immediately start monitoring
flash-monitor: flash monitor

# Clean ESP32 build artifacts
clean-esp32:
    @echo "🧹 Cleaning ESP32 build artifacts..."
    cd PIO/ECOWATT && just clean

# Run ESP32 tests
test-esp32:
    @echo "🧪 Running ESP32 tests..."
    cd PIO/ECOWATT && just test

# ============================================================
# Complete Development Environment
# ============================================================

# Start both backend and frontend in parallel (requires tmux or separate terminals)
start-all:
    @echo "🚀 Starting complete development environment..."
    @echo ""
    @echo "This will start:"
    @echo "  1. Flask backend (http://localhost:5001)"
    @echo "  2. React frontend (http://localhost:5173)"
    @echo ""
    @echo "⚠️  Run these in separate terminals:"
    @echo "  Terminal 1: just server"
    @echo "  Terminal 2: just dev"
    @echo ""

# Stop all background services
stop-all: server-stop
    @echo "✅ All services stopped"

# ============================================================
# Database & Data Management
# ============================================================

# Initialize/reset database
db-init:
    @echo "🗄️  Initializing database..."
    cd flask && {{python}} -c "from database import Database; Database.init_db(); print('✅ Database initialized')"

# Backup database
db-backup:
    @echo "💾 Backing up database..."
    @mkdir -p backups
    @cp flask/ecowatt.db backups/ecowatt_$(date +%Y%m%d_%H%M%S).db
    @echo "✅ Database backed up to: backups/"

# ============================================================
# Code Quality & Testing
# ============================================================

# Run all tests (backend + frontend + ESP32)
test-all: test-frontend test-esp32
    @echo "✅ All tests completed"

# Format Python code
format-python:
    @echo "🎨 Formatting Python code..."
    cd flask && {{python}} -m black . || echo "Install black: pip install black"

# Lint Python code
lint-python:
    @echo "🔍 Linting Python code..."
    cd flask && {{python}} -m pylint *.py routes/ handlers/ utils/ || echo "Install pylint: pip install pylint"

# Format frontend code
format-frontend:
    @echo "🎨 Formatting frontend code..."
    cd front-end && npm run format || echo "No format script defined"

# Lint frontend code
lint-frontend:
    @echo "🔍 Linting frontend code..."
    cd front-end && npm run lint

# ============================================================
# Utilities
# ============================================================

# Check system status and installed tools
status:
    @echo "════════════════════════════════════════════════════"
    @echo "📊 EcoWatt Project Status"
    @echo "════════════════════════════════════════════════════"
    @echo ""
    @echo "Python:"
    @python3 --version 2>/dev/null || echo "  ❌ Not installed"
    @echo ""
    @echo "Node.js:"
    @node --version 2>/dev/null || echo "  ❌ Not installed"
    @npm --version 2>/dev/null | sed 's/^/  npm: /' || true
    @echo ""
    @echo "PlatformIO:"
    @pio --version 2>/dev/null || $HOME/.platformio/penv/bin/platformio --version 2>/dev/null || echo "  ❌ Not installed"
    @echo ""
    @echo "Virtual Environment:"
    @if [ -d "{{venv_dir}}" ]; then echo "  ✅ Created at {{venv_dir}}"; else echo "  ❌ Not created"; fi
    @echo ""
    @echo "Flask Dependencies:"
    @if [ -f "flask/requirements.txt" ]; then echo "  ✅ requirements.txt exists"; else echo "  ❌ requirements.txt missing"; fi
    @echo ""
    @echo "Frontend Dependencies:"
    @if [ -d "front-end/node_modules" ]; then echo "  ✅ node_modules installed"; else echo "  ❌ node_modules missing (run: just install-node)"; fi
    @echo ""
    @echo "════════════════════════════════════════════════════"

# Clean all build artifacts and caches
clean:
    @echo "🧹 Cleaning all build artifacts..."
    rm -rf front-end/dist
    rm -rf front-end/node_modules/.vite
    cd PIO/ECOWATT && just clean
    @echo "✅ Clean complete"

# Clean everything including dependencies (nuclear option)
clean-all: clean
    @echo "🧹 Removing all dependencies..."
    @read -p "⚠️  This will remove venv, node_modules, and PlatformIO builds. Continue? [y/N] " -n 1 -r; \
    echo; \
    if [[ $$REPLY =~ ^[Yy]$$ ]]; then \
        rm -rf {{venv_dir}}; \
        rm -rf front-end/node_modules; \
        rm -rf PIO/ECOWATT/.pio; \
        echo "✅ All dependencies removed. Run 'just setup' to reinstall."; \
    else \
        echo "Cancelled."; \
    fi

# Show project structure
tree:
    @echo "📁 Project structure:"
    @tree -L 2 -I 'node_modules|.venv|.pio|__pycache__|*.pyc' || echo "Install tree: sudo apt install tree"

# Open project documentation
docs:
    @echo "📚 Opening project documentation..."
    @xdg-open docs/In21-EN4440-Project\ Outline.md 2>/dev/null || \
     open docs/In21-EN4440-Project\ Outline.md 2>/dev/null || \
     echo "Open docs/In21-EN4440-Project Outline.md manually"

# ============================================================
# Quick Commands (Shortcuts)
# ============================================================

# Alias: s = server
alias s := server

# Alias: d = dev  
alias d := dev

# Alias: f = flash
alias f := flash

# Alias: m = monitor
alias m := monitor

# Alias: fm = flash-monitor
alias fm := flash-monitor

# ============================================================
# Help & Information
# ============================================================

# Show detailed help
help:
    @echo "════════════════════════════════════════════════════"
    @echo "EcoWatt Project - Task Automation Help"
    @echo "════════════════════════════════════════════════════"
    @echo ""
    @echo "SETUP:"
    @echo "  just setup          Complete first-time setup"
    @echo "  just install-deps   Install all dependencies"
    @echo "  just status         Check system status"
    @echo ""
    @echo "DEVELOPMENT:"
    @echo "  just server         Start Flask backend (Ctrl+C to stop)"
    @echo "  just dev            Start React frontend (Ctrl+C to stop)"
    @echo "  just flash          Flash ESP32 firmware"
    @echo "  just monitor        Monitor ESP32 serial output"
    @echo ""
    @echo "SHORTCUTS:"
    @echo "  just s              Same as 'just server'"
    @echo "  just d              Same as 'just dev'"
    @echo "  just f              Same as 'just flash'"
    @echo "  just m              Same as 'just monitor'"
    @echo "  just fm             Flash and monitor ESP32"
    @echo ""
    @echo "TESTING:"
    @echo "  just test-all       Run all tests"
    @echo "  just test-frontend  Frontend tests only"
    @echo "  just test-esp32     ESP32 tests only"
    @echo ""
    @echo "DATABASE:"
    @echo "  just db-init        Initialize database"
    @echo "  just db-backup      Backup database"
    @echo ""
    @echo "UTILITIES:"
    @echo "  just clean          Clean build artifacts"
    @echo "  just clean-all      Remove all dependencies"
    @echo "  just build          Build frontend for production"
    @echo ""
    @echo "For complete list: just --list"
    @echo "════════════════════════════════════════════════════"
