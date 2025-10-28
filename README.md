# 🌱 EcoWatt - Smart Energy Monitoring System
**Team PowerPort** | EN4440 Embedded Systems and Design

[![Compression](https://img.shields.io/badge/Compression-96.4%25-blue?style=flat-square)]()
[![FOTA](https://img.shields.io/badge/FOTA-Secure-success?style=flat-square)]()
[![M4](https://img.shields.io/badge/M4-Complete-brightgreen?style=flat-square)]()

---

## 📋 Overview

**EcoWatt** is an IoT system for real-time monitoring and control of solar inverters. The system uses ESP32 hardware with secure cloud connectivity via Flask server, achieving **96.4% data compression** and supporting **secure over-the-air firmware updates** (FOTA).

### � Key Features
- ✅ **Real-time data acquisition** from solar inverters via Modbus RTU
- ✅ **Advanced compression** - 96.4% compression ratio (SEMANTIC/DICTIONARY algorithms)
- ✅ **Secure communication** - AES-128 encryption, HMAC validation, anti-replay protection
- ✅ **Remote command execution** - Power control, frequency tuning, inverter management
- ✅ **Firmware OTA updates** - RSA-2048 signature verification, AES-CBC decryption
- ✅ **Comprehensive testing** - M3/M4 integration tests, component tests, security validation

---

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
- **PlatformIO** (ESP32 development)
- **Just** command runner (optional, for task automation)

### Flask Server Setup

```bash
# Navigate to Flask directory
cd flask

# Install dependencies
pip install -r req.txt

# Generate cryptographic keys (first time only)
python generate_keys.py

# Start the server
python flask_server_modular.py
# Or using just:
just server
```

Server runs on `http://localhost:5000`

### ESP32 Firmware Build & Upload

```bash
# Navigate to PlatformIO directory
cd PIO/ECOWATT

# Build firmware
pio run

# Upload to ESP32
pio run --target upload

# Monitor serial output
pio device monitor

# Or using just:
just build
just upload
just monitor
```

### Running Tests

**Flask Tests:**
```bash
cd flask
pytest -v  # Run all tests
pytest test_m4_integration/ -v  # M4 tests only
```

**ESP32 Tests:**
```bash
cd PIO/ECOWATT
pio test  # Run all tests
pio test -e test_m4_integration  # Specific test environment
```

---

## 📂 Project Structure

```
EcoWatt_TeamPowerPort/
├── flask/                          # Flask server (Python)
│   ├── flask_server_modular.py     # Main server entry point
│   ├── handlers/                   # Request handlers (command, OTA, security, etc.)
│   ├── routes/                     # API routes (7 modules)
│   ├── utils/                      # Utilities (compression, MQTT, logging)
│   ├── test_m3_integration/        # M3 integration tests (8 tests)
│   ├── test_m4_integration/        # M4 integration tests (9 tests)
│   └── justfile                    # Task automation
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
│   └── justfile                    # Task automation
│
├── plans_and_progress/             # Comprehensive documentation
│   ├── FLASK_ARCHITECTURE.md       # Flask server documentation (123KB)
│   ├── FLASK_TESTS.md              # Flask test documentation (65KB)
│   ├── ESP32_ARCHITECTURE.md       # ESP32 firmware documentation (112KB)
│   └── ESP32_TESTS.md              # ESP32 test documentation (68KB)
│
└── docs/                           # Project requirements & API specs
```

---

## 📊 Milestone Status

| Milestone | Title | Status | Key Features |
|-----------|-------|--------|--------------|
| **M1** | Petri Net Design | ✅ Complete | State machine modeling |
| **M2** | Inverter Integration | ✅ Complete | Modbus RTU, sensor polling |
| **M3** | Compression & Upload | ✅ Complete | 96.4% compression, ring buffer, cloud upload |
| **M4** | Security & FOTA | ✅ Complete | AES encryption, HMAC, RSA signatures, OTA updates, remote commands |
| **M5** | Power Management | ⏳ Pending | Sleep modes, fault recovery, diagnostics |

**Overall Progress**: **80%** (4/5 milestones complete)

---

## 🏗️ System Architecture

### High-Level Overview

```
┌─────────────────┐         ┌─────────────────┐         ┌─────────────────┐
│  Solar Inverter │◄────────┤   ESP32 Device  │────────►│  Flask Server   │
│  (Modbus RTU)   │  Modbus │   (EcoWatt)     │  HTTPS  │  (Cloud)        │
└─────────────────┘         └─────────────────┘         └─────────────────┘
                                     │                            │
                              Compression (96.4%)          MQTT Publishing
                              AES Encryption                Command Queue
                              Ring Buffer                   Firmware Hosting
```

### Data Flow

```
Modbus Polling (2s) → Ring Buffer (7 samples) → Compression (96.4%)
    → AES Encryption → HTTPS Upload (15s) → Flask Server
    → Storage & MQTT → Dashboard
```

---

## 🛠️ Technology Stack

**ESP32 Firmware:**
- **Language**: C++ (Arduino Framework)
- **Platform**: PlatformIO
- **Security**: mbedTLS (AES-128, RSA-2048, SHA-256)
- **Protocols**: Modbus RTU, HTTP/HTTPS, MQTT
- **Test Framework**: Unity

**Flask Server:**
- **Language**: Python 3.10+
- **Framework**: Flask + Flask-CORS
- **Security**: Cryptography library (RSA, AES, HMAC)
- **MQTT**: Paho-MQTT (HiveMQ)
- **Test Framework**: Pytest

---

## 📈 Performance Highlights

| Metric | Achievement |
|--------|-------------|
| **Compression Ratio** | 96.4% (140 bytes → 5 bytes) |
| **Data Throughput** | 30 samples/minute |
| **Upload Success** | 100% |
| **Command Latency** | <10 seconds |
| **OTA Success** | 100% (997/997 chunks) |
| **Free Heap** | 75% (235KB available) |

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

**Last Updated**: October 28, 2025  
**Current Version**: v1.0.4  
**Status**: M4 Complete, M5 Pending
