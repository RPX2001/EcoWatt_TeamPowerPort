# 🌱 EcoWatt - Smart Energy Monitoring System
**Team PowerPort** | EN4440 Embedded Systems and Design

[![Compression](https://img.shields.io/badge/Compression25-blue?style=flat-square)]()
[![FOTA](https://img.shields.io/badge/FOTA-Secure-success?style=flat-square)]()
---

## 📋 Overview

**EcoWatt** is an IoT system for real-time monitoring and control of solar inverters. The system uses ESP32 hardware with secure cloud connectivity via Flask server, achieving **data compression** and supporting **secure over-the-air firmware updates** (FOTA).


## 📚 Documentation

> **🔍 For detailed system information, see the comprehensive documentation files:**

| Document | Description | Size |
|----------|-------------|------|
| **[FLASK_ARCHITECTURE.md](plans_and_progress/FLASK_ARCHITECTURE.md)** | Complete Flask server architecture - handlers, routes, utilities, security, OTA, compression | 123KB |
| **[FLASK_TESTS.md](plans_and_progress/FLASK_TESTS.md)** | Flask integration tests (M3/M4), test coordination, execution guide | 65KB |
| **[ESP32_ARCHITECTURE.md](plans_and_progress/ESP32_ARCHITECTURE.md)** | ESP32 firmware architecture - Petri net state machine, modules, hardware abstraction | 112KB |
| **[ESP32_TESTS.md](plans_and_progress/ESP32_TESTS.md)** | ESP32 test suites (M3/M4 integration, component tests), security, OTA validation | 68KB |

---

## 🚀 Quick Start

### Prerequisites
- **Python 3.10+** (Flask server)
- **Node.js 20+** (React frontend)
- **PlatformIO** (ESP32 development)
- **Just** command runner (recommended for task automation)

### Automated Setup (Recommended)

The easiest way to get started is using the `just` command runner:

```bash
# Complete first-time setup (installs all dependencies)
just setup

# Check installation status
just status
```

### Manual Setup

If you prefer manual setup:

**Backend (Flask):**
```bash
# Create virtual environment and install dependencies
python3 -m venv .venv
.venv/bin/pip install -r flask/requirements.txt

# Generate cryptographic keys (first time only)
cd flask && python scripts/generate_keys.py
```

**Frontend (React):**
```bash
cd front-end
npm install
```

**ESP32 Firmware:**
```bash
# Install PlatformIO
pip install platformio
```

---

## 🎮 Available Commands

### Quick Commands (Shortcuts)

```bash
just s              # Start Flask backend server
just d              # Start React frontend dev server
just f              # Flash ESP32 firmware
just m              # Monitor ESP32 serial output
just fm             # Flash and monitor ESP32
```

### Development Commands

**Setup & Installation:**
```bash
just setup          # Complete first-time setup
just install-deps   # Install all dependencies
just status         # Check system status
```

**Backend Development:**
```bash
just server         # Start Flask backend (http://localhost:5001)
just s              # Shortcut for 'just server'
```

**Frontend Development:**
```bash
just dev            # Start React frontend (http://localhost:5173)
just d              # Shortcut for 'just dev'
```

**ESP32 Development:**
```bash
just flash          # Build and flash ESP32 firmware
just monitor        # Monitor ESP32 serial output
just flash-monitor  # Flash then immediately monitor
just f              # Shortcut for 'just flash'
just m              # Shortcut for 'just monitor'
just fm             # Shortcut for 'just flash-monitor'
```

**Testing:**
```bash
just test-all       # Run all tests (frontend + ESP32)
just test-frontend  # Frontend tests only
just test-esp32     # ESP32 tests only
```

**Utilities:**
```bash
just clean          # Clean build artifacts
just clean-all      # Remove all dependencies (nuclear option)
just db-init        # Initialize database
just db-backup      # Backup database
just help           # Show detailed help
```

### Manual Commands (Without Just)

**Start Backend:**
```bash
cd flask
../.venv/bin/python3 flask_server_modular.py
```

**Start Frontend:**
```bash
cd front-end
npm run dev
```

**Flash ESP32:**
```bash
cd PIO/ECOWATT
pio run --target upload
```

**Monitor ESP32:**
```bash
cd PIO/ECOWATT
pio device monitor
```

---

## 📂 Project Structure

```
EcoWatt_TeamPowerPort/
├── flask/                          # Flask backend server (Python)
│   ├── flask_server_modular.py     # Main server entry point
│   ├── handlers/                   # Request handlers (command, OTA, security, fault)
│   ├── routes/                     # API routes (10+ modules)
│   ├── utils/                      # Utilities (compression, data, logging)
│   ├── scripts/                    # Admin scripts (key generation, firmware prep)
│   ├── tests/                      # Backend tests (unit + integration)
│   ├── database.py                 # SQLite database interface
│   └── justfile                    # Flask task automation
│
├── front-end/                      # React frontend (JavaScript)
│   ├── src/                        # Source code
│   │   ├── components/             # React components (dashboard, testing, etc.)
│   │   ├── pages/                  # Page components
│   │   ├── api/                    # API client functions
│   │   └── contexts/               # React contexts
│   ├── public/                     # Static assets
│   ├── package.json                # Node.js dependencies
│   └── justfile                    # Frontend task automation
│
├── PIO/ECOWATT/                    # ESP32 firmware (C++)
│   ├── src/main.cpp                # Main firmware (Petri net state machine)
│   ├── include/application/        # Application modules (OTA, compression, security)
│   ├── test/                       # Test suites (M3/M4 integration, component tests)
│   │   ├── test_m3_integration/    # M3 tests (compression, acquisition, upload)
│   │   ├── test_m4_integration/    # M4 tests (security, HMAC, OTA, commands, config)
│   │   ├── test_compression/       # Compression benchmarks
│   │   ├── test_acquisition/       # Modbus sensor tests
│   │   ├── test_security_*/        # Security validation tests
│   │   └── test_fota_*/            # OTA component tests
│   ├── platformio.ini              # PlatformIO configuration
│   └── justfile                    # ESP32 task automation
│
├── plans_and_progress/             # Comprehensive documentation
│   ├── FLASK_ARCHITECTURE.md       # Flask server documentation (123KB)
│   ├── FLASK_TESTS.md              # Flask test documentation (65KB)
│   ├── ESP32_ARCHITECTURE.md       # ESP32 firmware documentation (112KB)
│   ├── ESP32_TESTS.md              # ESP32 test documentation (68KB)
│   └── MILESTONE_5_TODO.md         # M5 progress tracking
│
├── docs/                           # Project requirements & API specs
├── justfile                        # Root task automation (orchestrates all)
└── README.md                       # This file
```

---

## 🏗️ System Architecture

### High-Level Overview

```
┌─────────────────┐         ┌─────────────────┐         ┌─────────────────┐         ┌─────────────────┐
│  Solar Inverter │◄────────┤   ESP32 Device  │────────►│  Flask Server   │────────►│  React Frontend │
│  (Modbus RTU)   │  Modbus │   (EcoWatt)     │  HTTPS  │  (Port 5001)    │   API   │  (Port 5173)    │
└─────────────────┘         └─────────────────┘         └─────────────────┘         └─────────────────┘
                                     │                            │                           │
                              Compression          MQTT Publishing            Web Dashboard
                              AES Encryption                Command Queue              Real-time Monitoring
                              Ring Buffer                   Firmware Hosting          Fault Injection
```

### Data Flow

**Data Acquisition & Upload (ESP32 → Backend):**
```
Modbus RTU Polling (2s interval)
    ↓
Ring Buffer (7 samples)
    ↓
Compression (96.4% ratio: 140 bytes → 5 bytes)
    ↓
AES-128 Encryption + HMAC-SHA256
    ↓
HTTPS Upload (15s interval)
    ↓
Flask Server (Validation & Storage)
    ↓
SQLite Database + MQTT Publish
```

**Remote Control (Frontend → ESP32):**
```
React Dashboard (User Action)
    ↓
REST API Request (POST /command)
    ↓
Flask Server (Command Queue)
    ↓
ESP32 Polling (GET /command/pending)
    ↓
Command Execution (Modbus, Power Control, etc.)
    ↓
Status Report (POST /command/status)
    ↓
Frontend Update (Real-time Display)
```

**Firmware OTA (Backend → ESP32):**
```
Firmware Upload (Flask Server)
    ↓
RSA-2048 Signature + AES-CBC Encryption
    ↓
Manifest Generation (version, hash, signature)
    ↓
ESP32 Check Update (GET /ota/manifest)
    ↓
Chunk Download (GET /ota/firmware/<chunk>)
    ↓
Hash Verification + RSA Signature Validation
    ↓
AES Decryption + Flash Write
    ↓
Boot to New Firmware
```

---

## 🛠️ Technology Stack

**ESP32 Firmware:**
- **Language**: C++ (Arduino Framework)
- **Platform**: PlatformIO
- **Security**: mbedTLS (AES-128, RSA-2048, SHA-256)
- **Protocols**: Modbus RTU, HTTP/HTTPS, MQTT
- **Test Framework**: Unity

**Flask Backend:**
- **Language**: Python 3.10+
- **Framework**: Flask + Flask-CORS
- **Security**: Cryptography library (RSA, AES, HMAC)
- **Database**: SQLite
- **MQTT**: Paho-MQTT (HiveMQ)
- **Test Framework**: Pytest

**React Frontend:**
- **Language**: JavaScript (ES6+)
- **Framework**: React 18 + Vite
- **UI Library**: Material-UI (MUI)
- **State Management**: React Query (TanStack Query)
- **HTTP Client**: Axios
- **Build Tool**: Vite

**DevOps & Tools:**
- **Task Automation**: Just (justfile)
- **Version Control**: Git + GitHub
- **API Testing**: Postman, curl

---

## 🧪 Testing

The system includes comprehensive test coverage:

- **Flask M3 Tests** (8 tests): Compression, sensor acquisition, server upload
- **Flask M4 Tests** (9 tests): Security, HMAC, commands, config, OTA
- **ESP32 M3 Tests** (8 tests): Compression algorithms, Modbus, data upload
- **ESP32 M4 Tests** (9 tests): Security validation, anti-replay, FOTA, commands
- **Component Tests**: Compression benchmarks, acquisition, security primitives, FOTA components

See [ESP32_TESTS.md](plans_and_progress/ESP32_TESTS.md) and [FLASK_TESTS.md](plans_and_progress/FLASK_TESTS.md) for details.

---

## 🔧 Configuration

### ESP32 WiFi & Server Configuration

Edit `PIO/ECOWATT/include/application/credentials.h`:
```cpp
#define WIFI_SSID "YourWiFi"
#define WIFI_PASSWORD "YourPassword"
#define SERVER_IP "192.168.1.100"
#define SERVER_PORT 5000
```

### Flask Server Configuration

Edit `flask/flask_server_modular.py`:
```python
app.config['SECRET_KEY'] = 'your-secret-key'
app.config['MQTT_BROKER'] = 'your-mqtt-broker.cloud'
```

---

## 📝 Contributing

This is an academic project for EN4440 - Embedded Systems and Design at University of Moratuwa. For questions or contributions, please contact the team.

---

## 📄 License

Academic project - University of Moratuwa, EN4440 Module

---

## 👥 Team PowerPort

**Course**: EN4440 - Embedded Systems and Design  
**University**: University of Moratuwa  
**Department**: Electronic and Telecommunication Engineering

---

**Last Updated**: November 5, 2025  
**Current Version**: v1.2.0  