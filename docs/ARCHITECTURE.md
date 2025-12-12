# 🏛️ EcoWatt System Architecture

This document provides detailed system architecture, design patterns, and data flows.

---

## Table of Contents

- [System Overview](#system-overview)
- [Component Architecture](#component-architecture)
- [ESP32 Firmware Architecture](#esp32-firmware-architecture)
- [Backend Architecture](#backend-architecture)
- [Frontend Architecture](#frontend-architecture)
- [Data Flow Diagrams](#data-flow-diagrams)
- [Communication Protocols](#communication-protocols)

---

## System Overview

### High-Level Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                        EcoWatt System                           │
└─────────────────────────────────────────────────────────────────┘

┌──────────────┐         ┌──────────────┐         ┌─────────────┐
│    Solar     │◄───────►│    ESP32     │◄───────►│   Flask     │
│   Inverter   │ Modbus  │   EcoWatt    │  HTTPS  │   Server    │
│              │  RTU    │   Device     │         │             │
└──────────────┘         └──────────────┘         └──────┬──────┘
                                                          │
                         ┌────────────────────────────────┼─────┐
                         │                                │     │
                    ┌────▼────┐                    ┌──────▼─────▼┐
                    │  SQLite │                    │    MQTT     │
                    │Database │                    │   Broker    │
                    └─────────┘                    └──────┬──────┘
                                                          │
                                                   ┌──────▼─────┐
                                                   │   React    │
                                                   │ Dashboard  │
                                                   └────────────┘
```

### Deployment Models

#### Model 1: Cloud Deployment
```
ESP32 ──HTTPS──► Cloud Server (AWS/Azure) ──WebSocket──► Users
   │                    │
   └─────Modbus────► Inverter
```

#### Model 2: Local Deployment  
```
ESP32 ──HTTPS──► Local Server (Raspberry Pi) ──LAN──► Local Dashboard
   │                    │
   └─────Modbus────► Inverter
```

#### Model 3: Hybrid Deployment
```
ESP32 ──HTTPS──► Edge Gateway ──HTTPS──► Cloud Backend
   │                                          │
   └─────Modbus────► Inverter                └──► Analytics
```

---

## Component Architecture

### ESP32 Device

**Hardware Components**:
```
┌─────────────────────────────────────────┐
│            ESP32-WROOM-32               │
├─────────────────────────────────────────┤
│  CPU: Dual-core Xtensa 240MHz           │
│  RAM: 520 KB SRAM                       │
│  Flash: 4 MB                            │
│  WiFi: 802.11 b/g/n                     │
│  Bluetooth: BLE 4.2                     │
└─────────────────────────────────────────┘
         │              │
    ┌────▼───┐     ┌───▼────┐
    │ RS485  │     │ GPIO   │
    │  UART  │     │ Power  │
    │  TX/RX │     │ Control│
    └────┬───┘     └───┬────┘
         │             │
    ┌────▼─────────────▼────┐
    │   Solar Inverter       │
    │   (Modbus RTU Slave)   │
    └────────────────────────┘
```

**Peripheral Connections**:
- **UART2**: Modbus RS485 communication (TX/RX)
- **GPIO 4**: UART power gating control
- **GPIO 2**: Status LED
- **NVS**: Non-volatile storage for nonce, config

---

## ESP32 Firmware Architecture

### Petri Net State Machine

The ESP32 firmware implements a **Petri Net** design pattern for state management:

```
┌──────────────────────────────────────────────────────────────┐
│                    Petri Net States                          │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│   ┌─────────┐                                               │
│   │  INIT   │───────┐                                       │
│   └────┬────┘       │                                       │
│        │            │                                       │
│        v            v                                       │
│   ┌─────────┐  ┌─────────┐  ┌──────────┐  ┌──────────┐   │
│   │ MODBUS  │→ │COMPRESS │→ │  SECURE  │→ │  UPLOAD  │   │
│   │  POLL   │  │  DATA   │  │   DATA   │  │   DATA   │   │
│   └────┬────┘  └─────────┘  └──────────┘  └────┬─────┘   │
│        │                                         │          │
│        └─────────────────────────────────────────┘          │
│                      2s cycle                               │
│                                                              │
│   ┌──────────┐  ┌──────────┐  ┌──────────┐                │
│   │ COMMAND  │  │   OTA    │  │  ERROR   │                │
│   │  CHECK   │  │  UPDATE  │  │ RECOVERY │                │
│   └──────────┘  └──────────┘  └──────────┘                │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

### Module Organization

```
PIO/ECOWATT/src/application/
├── modbus_manager.cpp      # Modbus communication
├── compression.cpp         # Compression algorithms
├── security.cpp            # HMAC, nonce management
├── OTAManager.cpp          # Firmware updates
├── power_management.cpp    # Power gating control
├── config_manager.cpp      # Device configuration
├── command_handler.cpp     # Remote command execution
└── network_manager.cpp     # WiFi, HTTPS communication
```

### Task Architecture (FreeRTOS)

```cpp
// Main tasks running on ESP32
void modbus_task(void *pvParameters) {
    // Priority: High (3)
    // Stack: 4096 bytes
    // Runs every 2 seconds
}

void upload_task(void *pvParameters) {
    // Priority: Medium (2)
    // Stack: 8192 bytes
    // Runs every 15 seconds
}

void command_task(void *pvParameters) {
    // Priority: Medium (2)
    // Stack: 4096 bytes
    // Polls every 5 seconds
}

void ota_task(void *pvParameters) {
    // Priority: Low (1)
    // Stack: 8192 bytes
    // Runs on-demand
}
```

### Memory Management

```
ESP32 Memory Layout:
┌─────────────────────────────┐ 0x3FF80000
│       ROM (448 KB)          │
├─────────────────────────────┤ 0x3FF00000
│    Internal RAM (520 KB)    │
│  ┌──────────────────────┐   │
│  │  Heap (300 KB)       │   │
│  ├──────────────────────┤   │
│  │  Stack (80 KB)       │   │
│  ├──────────────────────┤   │
│  │  Static Data         │   │
│  └──────────────────────┘   │
├─────────────────────────────┤
│    Flash (4 MB)             │
│  ┌──────────────────────┐   │
│  │  Bootloader (32 KB)  │   │
│  ├──────────────────────┤   │
│  │  Partition 0 (1.5MB) │   │
│  ├──────────────────────┤   │
│  │  Partition 1 (1.5MB) │   │
│  ├──────────────────────┤   │
│  │  NVS (24 KB)         │   │
│  └──────────────────────┘   │
└─────────────────────────────┘
```

---

## Backend Architecture

### Flask Server Structure

```
flask/
├── flask_server_modular.py    # Main application
│
├── handlers/                   # Business logic layer
│   ├── ota_handler.py         # FOTA operations
│   ├── compression_handler.py  # Decompression
│   ├── security_handler.py     # Authentication
│   ├── diagnostics_handler.py  # System diagnostics
│   ├── fault_handler.py        # Error management
│   └── command_handler.py      # Command processing
│
├── routes/                     # API layer
│   ├── device_routes.py       # Device management
│   ├── power_routes.py        # Power data
│   ├── ota_routes.py          # Firmware updates
│   ├── command_routes.py      # Remote commands
│   ├── security_routes.py     # Authentication
│   ├── diagnostics_routes.py  # Diagnostics
│   └── fault_routes.py        # Fault reporting
│
├── utils/                      # Utilities
│   ├── compression_utils.py   # Compression helpers
│   ├── data_utils.py          # Data processing
│   └── logger_utils.py        # Logging
│
└── database.py                # SQLite ORM
```

### Request Processing Pipeline

```
HTTP Request
    │
    ▼
┌─────────────┐
│   CORS      │  ← Flask-CORS middleware
└──────┬──────┘
       │
       ▼
┌─────────────┐
│  Security   │  ← HMAC validation
│  Validation │     Nonce checking
└──────┬──────┘
       │
       ▼
┌─────────────┐
│  Business   │  ← Handler functions
│   Logic     │     Data processing
└──────┬──────┘
       │
       ▼
┌─────────────┐
│  Database   │  ← SQLite operations
│  Operations │
└──────┬──────┘
       │
       ▼
┌─────────────┐
│   MQTT      │  ← Publish updates
│  Publish    │
└──────┬──────┘
       │
       ▼
  JSON Response
```

### Database Schema

```sql
-- Device table
CREATE TABLE devices (
    device_id TEXT PRIMARY KEY,
    last_seen TIMESTAMP,
    firmware_version TEXT,
    ip_address TEXT,
    wifi_rssi INTEGER,
    last_nonce INTEGER
);

-- Power data table
CREATE TABLE power_data (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id TEXT,
    timestamp TIMESTAMP,
    voltage REAL,
    current REAL,
    power REAL,
    frequency REAL,
    temperature REAL,
    FOREIGN KEY (device_id) REFERENCES devices(device_id)
);

-- Command queue table
CREATE TABLE command_queue (
    command_id TEXT PRIMARY KEY,
    device_id TEXT,
    command_type TEXT,
    parameters JSON,
    status TEXT,
    created_at TIMESTAMP,
    executed_at TIMESTAMP,
    FOREIGN KEY (device_id) REFERENCES devices(device_id)
);

-- OTA updates table
CREATE TABLE ota_updates (
    update_id INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id TEXT,
    firmware_version TEXT,
    status TEXT,
    started_at TIMESTAMP,
    completed_at TIMESTAMP,
    FOREIGN KEY (device_id) REFERENCES devices(device_id)
);
```

---

## Frontend Architecture

### React Component Hierarchy

```
App
├── Layout
│   ├── Header
│   │   ├── Logo
│   │   └── Navigation
│   └── Sidebar
│       ├── DeviceSelector
│       └── QuickStats
│
├── Pages
│   ├── Dashboard
│   │   ├── LiveChart
│   │   ├── StatusCards
│   │   └── RecentAlerts
│   │
│   ├── PowerAnalytics
│   │   ├── EnergyChart
│   │   ├── PowerDistribution
│   │   └── HistoricalData
│   │
│   ├── Configuration
│   │   ├── DeviceSettings
│   │   ├── CompressionConfig
│   │   └── SecuritySettings
│   │
│   ├── FOTA
│   │   ├── FirmwareUpload
│   │   ├── UpdateProgress
│   │   └── UpdateHistory
│   │
│   └── Testing
│       ├── FaultInjector
│       ├── CommandTester
│       └── DiagnosticsViewer
│
└── Components (Shared)
    ├── DataChart
    ├── StatusBadge
    ├── CommandButton
    └── ErrorBoundary
```

### State Management

```javascript
// React Query for server state
const { data, isLoading } = useQuery({
  queryKey: ['devices'],
  queryFn: fetchDevices,
  refetchInterval: 5000  // Poll every 5 seconds
});

// Context for global state
const AppContext = createContext({
  selectedDevice: null,
  theme: 'light',
  notifications: []
});

// WebSocket for real-time updates
const ws = new WebSocket('ws://localhost:5001/ws');
ws.onmessage = (event) => {
  const update = JSON.parse(event.data);
  queryClient.invalidateQueries(['liveData']);
};
```

---

## Data Flow Diagrams

### Upload Flow (ESP32 → Server)

```
┌──────────┐
│  ESP32   │
└────┬─────┘
     │ 1. Collect data (2s interval)
     ▼
┌────────────┐
│Ring Buffer │ (7 samples)
└────┬───────┘
     │ 2. Compress (15s interval)
     ▼
┌──────────────┐
│ Compression  │ (Dictionary/Delta/RLE/BitPack)
│   Engine     │
└────┬─────────┘
     │ 3. Secure payload
     ▼
┌──────────────┐
│  Security    │ (Nonce + HMAC-SHA256)
│    Layer     │
└────┬─────────┘
     │ 4. HTTPS POST
     ▼
┌──────────────┐
│ Flask Server │
└────┬─────────┘
     │ 5. Validate & decompress
     ▼
┌──────────────┐
│  Database    │
└────┬─────────┘
     │ 6. Publish update
     ▼
┌──────────────┐
│ MQTT Broker  │
└────┬─────────┘
     │ 7. WebSocket push
     ▼
┌──────────────┐
│   Dashboard  │
└──────────────┘
```

### Command Flow (Dashboard → ESP32)

```
┌──────────┐
│Dashboard │
└────┬─────┘
     │ 1. User action (button click)
     ▼
┌──────────────┐
│  POST /api/  │
│   command    │
└────┬─────────┘
     │ 2. Validate & queue
     ▼
┌──────────────┐
│Command Queue │ (Database)
└────┬─────────┘
     │ 3. ESP32 polls (GET /command/pending)
     ▼
┌──────────────┐
│    ESP32     │
└────┬─────────┘
     │ 4. Execute via Modbus
     ▼
┌──────────────┐
│   Inverter   │
└────┬─────────┘
     │ 5. Report status (POST /command/status)
     ▼
┌──────────────┐
│Flask Server  │
└────┬─────────┘
     │ 6. Update dashboard
     ▼
┌──────────────┐
│  Dashboard   │ (Shows result)
└──────────────┘
```

### FOTA Flow

```
┌──────────┐
│ Dashboard│
└────┬─────┘
     │ 1. Upload firmware.bin
     ▼
┌──────────────┐
│   Server     │
└────┬─────────┘
     │ 2. Encrypt (AES-256-CBC)
     │ 3. Sign (RSA-2048)
     │ 4. Create manifest
     ▼
┌──────────────┐
│ Firmware/    │
└────┬─────────┘
     │ 5. ESP32 checks manifest
     ▼
┌──────────────┐
│   ESP32      │
└────┬─────────┘
     │ 6. Download chunks (2KB each)
     │ 7. Decrypt each chunk
     │ 8. Write to flash partition
     ▼
┌──────────────┐
│  Flash       │
│ Partition 1  │
└────┬─────────┘
     │ 9. Verify hash
     │ 10. Verify signature
     ▼
┌──────────────┐
│   Reboot     │
│ to new FW    │
└──────────────┘
```

---

## Communication Protocols

### Modbus RTU

**Protocol Details**:
- **Physical**: RS485 differential signaling
- **Baud Rate**: 9600 bps
- **Data Format**: 8N1 (8 data bits, no parity, 1 stop bit)
- **Addressing**: Slave ID 0x01
- **Function Codes**: 0x03 (Read Holding Registers)

**Frame Format**:
```
┌────────┬──────────┬──────────┬──────────┬────────────┐
│Slave ID│Function  │Start Addr│Reg Count │   CRC16    │
│ 1 byte │ 1 byte   │ 2 bytes  │ 2 bytes  │  2 bytes   │
└────────┴──────────┴──────────┴──────────┴────────────┘
```

### HTTPS

**Request Format**:
```http
POST /api/upload/data HTTP/1.1
Host: server.example.com:5001
Content-Type: application/json
Content-Length: 245

{
  "device_id": "ESP32_001",
  "nonce": 10001,
  "payload": "base64_encoded_compressed_data",
  "mac": "hmac_sha256_signature",
  "compressed": true,
  "compression_type": "dictionary"
}
```

**Response Format**:
```http
HTTP/1.1 200 OK
Content-Type: application/json

{
  "status": "success",
  "message": "Data uploaded successfully",
  "timestamp": "2024-12-12T10:30:45Z"
}
```

### MQTT

**Topic Structure**:
```
ecowatt/
├── devices/
│   └── {device_id}/
│       ├── power          # Power data updates
│       ├── status         # Device status changes
│       ├── commands       # Command results
│       └── alerts         # Error/warning alerts
└── system/
    ├── health             # System health metrics
    └── updates            # Firmware update notifications
```

**Message Format**:
```json
{
  "topic": "ecowatt/devices/ESP32_001/power",
  "payload": {
    "timestamp": "2024-12-12T10:30:45Z",
    "voltage": 220.5,
    "current": 5.2,
    "power": 1146.6,
    "frequency": 50.0,
    "temperature": 25.3
  },
  "qos": 1,
  "retain": false
}
```

### WebSocket

**Connection**: `ws://server:5001/ws`

**Client Subscribe**:
```json
{
  "action": "subscribe",
  "topics": ["ecowatt/devices/ESP32_001/#"]
}
```

**Server Push**:
```json
{
  "type": "data_update",
  "device_id": "ESP32_001",
  "data": {
    "voltage": 220.5,
    "current": 5.2,
    "power": 1146.6
  }
}
```

---

## Design Patterns

### 1. Petri Net State Machine (ESP32)

**Purpose**: Manage complex state transitions with concurrency

**Benefits**:
- Clear state visualization
- Prevents invalid state transitions
- Supports parallel state execution
- Easy debugging

### 2. Handler Pattern (Flask)

**Purpose**: Separate business logic from routing

**Benefits**:
- Reusable logic across routes
- Easy testing
- Clear separation of concerns

### 3. Repository Pattern (Database)

**Purpose**: Abstract database operations

**Benefits**:
- Easy to swap databases
- Centralized query logic
- Testable without database

### 4. Component Composition (React)

**Purpose**: Build complex UIs from simple components

**Benefits**:
- Reusable components
- Easy to maintain
- Clear hierarchy

---

## Performance Considerations

### ESP32 Optimizations

- **RTOS Tasks**: Parallel execution of independent operations
- **Watchdog Timer**: Prevents infinite loops
- **Memory Pools**: Pre-allocated buffers for compression
- **Hardware Crypto**: Uses ESP32 crypto accelerators

### Backend Optimizations

- **Connection Pooling**: Reuse database connections
- **Async I/O**: Non-blocking MQTT operations
- **Caching**: In-memory cache for frequently accessed data
- **Batch Processing**: Group database writes

### Frontend Optimizations

- **Code Splitting**: Load components on-demand
- **Memoization**: Cache expensive computations
- **Virtual Scrolling**: Efficient rendering of large lists
- **Debouncing**: Limit API call frequency

---

## Scalability

### Horizontal Scaling

```
         Load Balancer
              │
    ┌─────────┼─────────┐
    │         │         │
Flask 1   Flask 2   Flask 3
    │         │         │
    └─────────┼─────────┘
              │
         Database
```

### Vertical Scaling

- Increase server CPU/RAM
- Use faster storage (SSD)
- Optimize database indexes

### Edge Computing

```
Multiple ESP32 → Edge Gateway → Batch upload → Cloud
```

---

[← Back to Main README](../README.md)
