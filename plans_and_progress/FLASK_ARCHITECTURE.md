# Flask Server Architecture

**EcoWatt Smart Monitoring System - Cloud Backend**  
**Milestones:** M2, M3, M4 Complete | M5 Pending  
**Last Updated:** 2025-10-28

---

## Table of Contents

1. [Overview](#overview)
2. [Architecture Summary](#architecture-summary)
3. [Core Components](#core-components)
4. [Handler Layer](#handler-layer)
5. [Routes Layer](#routes-layer)
6. [Utilities Layer](#utilities-layer)
7. [Security Implementation](#security-implementation)
8. [FOTA System](#fota-system)
9. [Data Flow](#data-flow)
10. [Configuration](#configuration)

---

## Overview

The Flask server acts as the **EcoWatt Cloud Control Service**, managing:
- **Secure Data Ingestion** - HMAC-secured uploads with anti-replay protection
- **Command Execution** - Queue and execute device commands (set_power, write_register)
- **Remote Configuration** - Update device parameters without reflashing
- **Firmware Over-The-Air (FOTA)** - Secure, chunked firmware distribution
- **Diagnostics** - Device health monitoring and fault logging
- **Compression** - Handle compressed/aggregated data from devices

**Architecture Philosophy:**
- **Modular Design** - Separated handlers, routes, and utilities
- **Stateless HTTP** - RESTful API design
- **Thread-Safe** - Locks protect shared state (sessions, stats, nonces)
- **Persistent Storage** - Diagnostics saved to JSONL, firmware/keys on disk

---

## Architecture Summary

```
┌─────────────────────────────────────────────────────────────┐
│                    Flask Application                         │
│                (flask_server_modular.py)                     │
└────────────┬────────────────────────────────────┬───────────┘
             │                                    │
     ┌───────▼────────┐                  ┌────────▼──────────┐
     │  Routes Layer  │                  │  Handler Layer    │
     │  (Blueprints)  │──────calls──────►│  (Business Logic) │
     └────────────────┘                  └───────────────────┘
             │                                    │
             │                           ┌────────▼──────────┐
             │                           │  Utilities Layer  │
             │                           │  (Shared Tools)   │
             │                           └───────────────────┘
             │
     ┌───────▼────────────────────────────────────────────────┐
     │              External Systems                          │
     │  • MQTT Broker (HiveMQ)                               │
     │  • Filesystem (diagnostics/, firmware/, keys/)        │
     │  • ESP32 Devices (HTTP clients)                       │
     └────────────────────────────────────────────────────────┘
```

### Directory Structure

```
flask/
├── flask_server_modular.py      # Main application entry point
├── requirements.txt              # Python dependencies
├── justfile                      # Build automation commands
│
├── handlers/                     # Business logic layer
│   ├── security_handler.py       # HMAC/nonce validation
│   ├── ota_handler.py            # Firmware update management
│   ├── command_handler.py        # Command queue management
│   ├── compression_handler.py    # Decompression & aggregation
│   ├── diagnostics_handler.py    # Device diagnostics storage
│   └── fault_handler.py          # Fault injection (testing)
│
├── routes/                       # HTTP endpoint layer
│   ├── general_routes.py         # Health, config, test sync
│   ├── security_routes.py        # Security stats, nonce mgmt
│   ├── aggregation_routes.py     # Data upload endpoints
│   ├── command_routes.py         # Command queue/poll/result
│   ├── diagnostics_routes.py     # Diagnostics endpoints
│   ├── ota_routes.py             # FOTA check/chunk/session
│   └── fault_routes.py           # Fault injection endpoints
│
├── utils/                        # Shared utilities
│   ├── logger_utils.py           # Centralized logging setup
│   ├── mqtt_utils.py             # MQTT publish helpers
│   ├── compression_utils.py      # Decompression algorithms
│   └── data_utils.py             # Data validation helpers
│
├── scripts/                      # Standalone tools (see UTILITIES.md)
│   ├── generate_keys.py          # Generate RSA/AES/HMAC keys
│   ├── firmware_manager.py       # Encrypt/sign/chunk firmware
│   ├── prepare_firmware.py       # CLI for firmware prep
│   ├── benchmark_compression.py  # Python compression tests
│   ├── fault_injector.py         # Inject faults for testing
│   ├── server_security_layer.py  # Core security functions
│   ├── queue_command_for_esp32.py # CLI command queue tool
│   └── mqtt_subscriber.py        # MQTT message subscriber
│
├── tests/                        # Test suites
│   └── integration_tests/
│       ├── test_m3_integration/  # M3 integration tests
│       └── test_m4_integration/  # M4 integration tests
│
├── firmware/                     # Firmware storage
│   ├── firmware_1.0.4_manifest.json
│   ├── firmware_1.0.4_encrypted.bin
│   ├── firmware_1.0.5_manifest.json
│   └── firmware_1.0.5_encrypted.bin
│
├── keys/                         # Cryptographic keys
│   ├── server_private_key.pem    # RSA-2048 private key
│   ├── server_public_key.pem     # RSA-2048 public key
│   ├── aes_firmware_key.bin      # AES-256 key (32 bytes)
│   └── keys.h                    # C header for ESP32
│
└── diagnostics_data/             # Device diagnostic logs
    └── {device_id}_diagnostics.jsonl
```

---

## Core Components

### Main Application (`flask_server_modular.py`)

**Purpose:** Flask app initialization, blueprint registration, CORS setup

**Key Features:**
- Registers 7 blueprints (general, security, aggregation, command, diagnostics, ota, fault)
- Configures CORS for cross-origin requests
- Initializes logging system
- Sets up MQTT client (placeholder, not active in M4)

**Startup Flow:**
```python
1. Initialize logging (logger_utils.py)
2. Create Flask app
3. Configure CORS
4. Register blueprints:
   - general_bp      → /health, /config, /integration/test/sync
   - security_bp     → /security/*
   - aggregation_bp  → /aggregated/<device_id>
   - command_bp      → /commands/<device_id>/*
   - diagnostics_bp  → /diagnostics/*
   - ota_bp          → /ota/*
   - fault_bp        → /fault/*
5. Log registered routes
6. Run server (0.0.0.0:5001)
```

**Dependencies:**
```
Flask==3.0.0
Flask-CORS==4.0.0
paho-mqtt==1.6.1
cryptography==41.0.7
```

---

## Handler Layer

Handlers contain **business logic** separated from HTTP routing. All handlers are thread-safe using locks where needed.

### 1. Security Handler (`security_handler.py`)

**Purpose:** HMAC validation, anti-replay protection, security statistics

**Key Functions:**

```python
validate_secured_payload(device_id, payload_dict)
  └─► Validates HMAC signature and nonce freshness
  └─► Returns: (success: bool, error_message: str)
  
get_security_stats() → dict
  └─► Returns: success/failure counts, replay blocks
  
clear_nonces(device_id=None)
  └─► Clears nonce tracking (for testing)
```

**Anti-Replay Mechanism:**
- Each device tracks `last_valid_nonce` in memory dict
- New nonce must be > last_valid_nonce
- Nonces stored in `nonce_state.json` (persistent)
- Automatic cleanup of expired nonces (>24h old)

**HMAC Validation:**
```python
# Expected payload structure:
{
  "payload": "<base64_encoded_data>",
  "nonce": 12345,
  "mac": "abcd1234...",  # HMAC-SHA256 hex
  "device_id": "ESP32_EcoWatt_Smart"
}

# HMAC calculation:
data_to_sign = nonce_bytes (4 bytes, big-endian) + payload_utf8_bytes
hmac = HMAC-SHA256(PSK_HMAC, data_to_sign)
```

**Statistics Tracked:**
- Total validations attempted
- Successful validations
- Signature failures
- CRC failures
- Replay attacks blocked
- Success rate (%)

**Thread Safety:** Uses `ssl_lock` from `server_security_layer.py`

---

### 2. OTA Handler (`ota_handler.py`)

**Purpose:** Firmware update management, chunked distribution, session tracking

**Key Functions:**

```python
check_for_update(device_id, current_version)
  └─► Checks if newer firmware available
  └─► Returns: (update_available: bool, update_info: dict)
  
initiate_ota_session(device_id, firmware_version)
  └─► Creates OTA session, prevents duplicate sessions
  └─► Auto-cleans stale sessions (>5min inactive)
  └─► Returns: (success: bool, session_id: str)
  
get_firmware_chunk_for_device(device_id, firmware_version, chunk_index)
  └─► Retrieves base64-encoded firmware chunk
  └─► Updates session progress
  └─► Returns: (success: bool, chunk_data: bytes, error: str)
  
complete_ota_session(device_id, success: bool)
  └─► Marks session as completed/failed
  └─► Updates statistics
```

**Session Management:**
```python
# Session structure:
ota_sessions = {
  "ESP32_EcoWatt_Smart": {
    "session_id": "ESP32_EcoWatt_Smart_1.0.5_1234567890",
    "device_id": "ESP32_EcoWatt_Smart",
    "firmware_version": "1.0.5",
    "status": "in_progress",  # or "completed", "failed"
    "current_chunk": 0,
    "total_chunks": 987,
    "bytes_transferred": 0,
    "started_at": "2025-10-28T05:54:18",
    "last_activity": "2025-10-28T05:54:18"
  }
}
```

**Stale Session Cleanup:**
- If session inactive >5min, auto-replace on new initiate
- Prevents deadlock from device reboot mid-OTA

**Firmware Manifest Structure:**
```json
{
  "version": "1.0.5",
  "original_size": 1009680,
  "encrypted_size": 1009696,
  "sha256_hash": "5c29fe6639d62e14...",
  "signature": "eAsBdqqzquSBf4tdV03...",
  "iv": "pXhtjXdzN6uHCJOiqjqB9A==",
  "chunk_size": 1024,
  "total_chunks": 987,
  "release_notes": ""
}
```

**Statistics:**
- Total updates initiated
- Successful updates
- Failed updates
- Active sessions
- Total bytes transferred

**Thread Safety:** Uses `ota_lock` (threading.Lock)

---

### 3. Command Handler (`command_handler.py`)

**Purpose:** Command queue management, execution tracking, result reporting

**Key Functions:**

```python
queue_command(device_id, command_type, parameters)
  └─► Adds command to device queue
  └─► Returns: command_id (UUID)
  
get_next_command(device_id)
  └─► Retrieves next pending command for device
  └─► Marks command as "sent"
  └─► Returns: command dict or None
  
record_result(device_id, command_id, status, result)
  └─► Stores command execution result
  └─► Marks command as "completed" or "failed"
  
get_command_status(command_id) → dict
  └─► Returns command status and result
```

**Command Structure:**
```python
{
  "command_id": "e1ac4484-7d6b-4c9d-9047-d6b33eabb43b",
  "device_id": "ESP32_EcoWatt_Smart",
  "command_type": "set_power",  # or "write_register"
  "parameters": {"power": "75%"},  # or {"register": 40001, "value": 1234}
  "status": "pending",  # → "sent" → "completed"/"failed"
  "queued_at": "2025-10-28T05:54:15",
  "sent_at": "2025-10-28T05:54:15",
  "completed_at": "2025-10-28T05:54:16",
  "result": "Power set to 75%"
}
```

**Command Types:**
1. **set_power** - Set inverter power output
   - Parameters: `{"power": "75%"}`
   - Execution: ESP32 → Inverter SIM write

2. **write_register** - Write Modbus register
   - Parameters: `{"register": 40001, "value": 1234}`
   - Execution: ESP32 → Modbus write → Result

**Command Flow:**
```
1. User/Admin → POST /commands/{device_id}
2. Flask → queue_command() → Assign UUID → Store in queue
3. ESP32 polls → GET /commands/{device_id}/poll
4. Flask → get_next_command() → Return oldest pending
5. ESP32 executes → POST /commands/{device_id}/result
6. Flask → record_result() → Mark completed/failed
```

**Thread Safety:** Uses `command_lock` (threading.Lock)

---

### 4. Compression Handler (`compression_handler.py`)

**Purpose:** Decompress uploaded data, handle aggregated samples, track statistics

**Key Functions:**

```python
handle_compressed_data(device_id, compressed_data_dict)
  └─► Decompresses binary data using specified method
  └─► Returns: (success: bool, decompressed_data: list, error: str)
  
handle_aggregated_data(device_id, aggregated_data_list)
  └─► Processes pre-aggregated JSON samples
  └─► Returns: (success: bool, sample_count: int, error: str)
  
get_compression_statistics() → dict
  └─► Returns: method usage, ratios, success rates
```

**Supported Compression Methods:**
```python
0xD0 = DICTIONARY  # Dictionary-based compression
0xDE = TEMPORAL    # Temporal delta encoding
0xAD = RLE         # Run-Length Encoding
0xBF = ADAPTIVE    # Adaptive selection
0xFF = SMART       # Smart method selection
```

**Compressed Data Format:**
```python
{
  "compressed_data": "<base64_binary>",
  "original_size": 1024,
  "compressed_size": 128,
  "method": 0xD0,  # Dictionary
  "crc32": "abcd1234",
  "timestamp": 1698765432
}
```

**Aggregated Data Format:**
```python
{
  "aggregated_data": [
    {"current": 2.5, "voltage": 230, "power": 575, "timestamp": 5400},
    {"current": 2.6, "voltage": 231, "power": 580, "timestamp": 5405}
  ]
}
```

**Statistics:**
- Total decompressions attempted
- Successful decompressions
- Failed decompressions
- Methods used (count per method)
- Average compression ratio
- Total bytes saved
- Success rate (%)

**Thread Safety:** Uses `compression_lock` (threading.Lock)

---

### 5. Diagnostics Handler (`diagnostics_handler.py`)

**Purpose:** Store and retrieve device diagnostics, health monitoring

**Key Functions:**

```python
store_diagnostics(device_id, diagnostics_dict)
  └─► Stores diagnostic entry (in-memory + JSONL file)
  └─► Limits to 100 entries per device
  
get_diagnostics_by_device(device_id, limit=50)
  └─► Retrieves recent diagnostics (newest first)
  
get_diagnostics_summary() → dict
  └─► Returns: device count, total entries, file sizes
  
clear_diagnostics(device_id=None)
  └─► Clears diagnostics (specific device or all)
```

**Diagnostic Entry Structure:**
```python
{
  "device_id": "ESP32_EcoWatt_Smart",
  "timestamp": "2025-10-28T05:54:18",
  "firmware_version": "1.0.4",
  "uptime_seconds": 12345,
  "free_heap": 87654,
  "wifi_rssi": -45,
  "compression_stats": {...},
  "buffer_usage": 45,
  "last_upload_status": "success",
  "poll_frequency": 5,
  "upload_frequency": 900
}
```

**Persistent Storage:**
- File: `diagnostics_data/{device_id}_diagnostics.jsonl`
- Format: JSON Lines (one JSON object per line)
- Automatic file creation per device
- Thread-safe append operations

**In-Memory Limits:**
- Max 100 entries per device (FIFO eviction)
- Auto-persistence on every store

**Thread Safety:** Uses `diagnostics_lock` (threading.Lock)

---

### 6. Fault Handler (`fault_handler.py`)

**Purpose:** Inject faults for testing (M5 fault recovery validation)

**Key Functions:**

```python
inject_fault(fault_type, parameters)
  └─► Injects simulated fault into system
  └─► Returns: fault_id, expected_behavior
  
get_fault_history() → list
  └─► Returns: all injected faults with timestamps
  
clear_fault_history()
  └─► Clears fault injection history
```

**Supported Fault Types:**
- `network_timeout` - Simulate network delay/failure
- `corrupted_payload` - Inject malformed data
- `ota_hash_mismatch` - Wrong firmware hash
- `modbus_timeout` - Inverter SIM timeout
- `buffer_overflow` - Excessive data injection

**Use Case:** M5 fault recovery testing (not active in M4)

---

## Routes Layer

Routes handle **HTTP routing** and delegate to handlers. All routes return JSON responses.

### General Routes (`general_routes.py`)

**Blueprint:** `general_bp`

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/health` | GET | Server health check |
| `/config` | GET | Get device configuration |
| `/integration/test/sync` | POST | Test synchronization endpoint |

**Example - Health Check:**
```bash
GET /health
Response: {"status": "healthy", "timestamp": "2025-10-28T05:54:18"}
```

**Example - Config:**
```bash
GET /config?device_id=ESP32_EcoWatt_Smart
Response: {
  "device_id": "ESP32_EcoWatt_Smart",
  "poll_frequency": 30,
  "upload_frequency": 300,
  "registers": [40000, 40001, 40002],
  "compression_enabled": true
}
```

---

### Security Routes (`security_routes.py`)

**Blueprint:** `security_bp`

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/security/stats` | GET | Security statistics |
| `/security/nonces` | DELETE | Clear nonce tracking |

---

### Aggregation Routes (`aggregation_routes.py`)

**Blueprint:** `aggregation_bp`

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/aggregated/<device_id>` | POST | Upload secured data |

**Request Structure:**
```json
{
  "payload": "eyJhZ2dyZWdhdGVkX2RhdGEiOlt7...==",
  "nonce": 200001,
  "mac": "cf42b1ad9dbffcdbf59fad4b9dce48bc...",
  "device_id": "ESP32_EcoWatt_Smart"
}
```

**Processing Flow:**
1. Validate HMAC (security_handler)
2. Decode base64 payload
3. Check if compressed or aggregated
4. Decompress if needed (compression_handler)
5. Publish to MQTT (mqtt_utils)
6. Return success/error

---

### Command Routes (`command_routes.py`)

**Blueprint:** `command_bp`

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/commands/<device_id>` | POST | Queue command |
| `/commands/<device_id>/poll` | GET | Poll for commands |
| `/commands/<device_id>/result` | POST | Submit result |
| `/commands/status/<command_id>` | GET | Get command status |

**Command Queue Example:**
```bash
POST /commands/ESP32_EcoWatt_Smart
{
  "command_type": "set_power",
  "parameters": {"power": "75%"}
}

Response:
{
  "success": true,
  "command_id": "e1ac4484-7d6b-4c9d-9047-d6b33eabb43b"
}
```

---

### Diagnostics Routes (`diagnostics_routes.py`)

**Blueprint:** `diagnostics_bp`

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/diagnostics/<device_id>` | POST | Store diagnostics |
| `/diagnostics/<device_id>` | GET | Get diagnostics |
| `/diagnostics/summary` | GET | Get summary stats |

---

### OTA Routes (`ota_routes.py`)

**Blueprint:** `ota_bp`

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/ota/check/<device_id>` | GET | Check for updates |
| `/ota/initiate/<device_id>` | POST | Start OTA session |
| `/ota/chunk/<device_id>` | GET | Get firmware chunk |
| `/ota/complete/<device_id>` | POST | Complete session |
| `/ota/status/<device_id>` | GET | Get session status |

**FOTA Flow:**
```
1. ESP32 → GET /ota/check/ESP32_EcoWatt_Smart?version=1.0.4
   Response: {"update_available": true, "latest_version": "1.0.5", ...}

2. ESP32 → POST /ota/initiate/ESP32_EcoWatt_Smart
   Body: {"firmware_version": "1.0.5"}
   Response: {"success": true, "session_id": "ESP32_EcoWatt_Smart_1.0.5_1234"}

3. ESP32 → GET /ota/chunk/ESP32_EcoWatt_Smart?version=1.0.5&chunk=0
   Response: {"chunk_data": "<base64>", "chunk_index": 0, "chunk_size": 1024}

4. Repeat step 3 for chunks 1-986

5. ESP32 → POST /ota/complete/ESP32_EcoWatt_Smart
   Body: {"success": true}
   Response: {"success": true}
```

---

## Utilities Layer

### Logger Utils (`logger_utils.py`)

**Purpose:** Centralized logging configuration

**Features:**
- Console and file logging
- Configurable log levels
- Timestamp formatting
- Log rotation (future enhancement)

---

### MQTT Utils (`mqtt_utils.py`)

**Purpose:** MQTT message publishing

**Status:** Placeholder (not active in M4, planned for M5)

**Functions:**
```python
publish_to_mqtt(topic, message)
  └─► Publishes JSON message to MQTT broker
  
initialize_mqtt_client(broker, port)
  └─► Initializes MQTT client connection
```

**Configuration:**
- Broker: broker.hivemq.com
- Port: 1883
- Topics: `ecowatt/data/{device_id}`, `ecowatt/commands/{device_id}`

---

### Compression Utils (`compression_utils.py`)

**Purpose:** Decompression algorithms (Python implementations)

**Supported Methods:**
```python
decompress_dictionary(data) → bytes
decompress_temporal(data) → bytes
decompress_rle(data) → bytes
decompress_adaptive(data) → bytes
decompress_smart(data) → bytes
```

**Compression Method Details:**

1. **Dictionary (0xD0)** - Lookup table compression
   - Best for: Repetitive data with known patterns
   - Ratio: ~70-90% reduction

2. **Temporal (0xDE)** - Delta encoding
   - Best for: Time-series with gradual changes
   - Stores: first_value + deltas

3. **RLE (0xAD)** - Run-Length Encoding
   - Best for: Repeated consecutive values
   - Format: [value, count] pairs

4. **Adaptive (0xBF)** - Selects best method per block
   - Tries all methods, picks smallest
   - Overhead: Method byte per block

5. **Smart (0xFF)** - Intelligent selection
   - Analyzes data characteristics
   - Chooses optimal method dynamically

---

### Data Utils (`data_utils.py`)

**Purpose:** Data validation and transformation

**Functions:**
```python
validate_device_id(device_id) → bool
validate_timestamp(timestamp) → bool
sanitize_json(data) → dict
```

---

## Security Implementation

### Cryptographic Stack

**Asymmetric (RSA-2048):**
- Purpose: Firmware signature verification
- Key Generation: `scripts/generate_keys.py`
- Storage: `keys/server_private_key.pem`, `keys/server_public_key.pem`
- Usage: Sign firmware manifests

**Symmetric (AES-256-CBC):**
- Purpose: Firmware encryption
- Key: `keys/aes_firmware_key.bin` (32 bytes)
- IV: Random per firmware (stored in manifest)
- Usage: Encrypt firmware binaries

**MAC (HMAC-SHA256):**
- Purpose: Data integrity + authentication
- PSK: 32-byte pre-shared key
- Usage: Validate uploaded payloads

### Security Flow

```
┌──────────────────────────────────────────────────────────┐
│  ESP32 Upload                                            │
│                                                          │
│  1. Create JSON payload: {"aggregated_data": [...]}    │
│  2. Base64 encode: eyJhZ2dyZWdhdGVkX2RhdGEiOlt7...      │
│  3. Generate nonce: 200001                              │
│  4. Calculate HMAC: HMAC-SHA256(PSK, nonce_bytes + payload_bytes) │
│  5. Send secured JSON:                                  │
│     {                                                   │
│       "payload": "<base64>",                           │
│       "nonce": 200001,                                 │
│       "mac": "cf42b1ad9dbffcdbf59fad4b9dce48bc...",   │
│       "device_id": "ESP32_EcoWatt_Smart"              │
│     }                                                   │
└──────────────────────────────────────────────────────────┘
                        ↓ HTTP POST
┌──────────────────────────────────────────────────────────┐
│  Flask Server Validation                                 │
│                                                          │
│  1. Extract payload, nonce, mac, device_id              │
│  2. Check nonce > last_valid_nonce (anti-replay)       │
│  3. Decode base64 payload                               │
│  4. Recalculate HMAC: HMAC-SHA256(PSK, nonce_bytes + payload_bytes) │
│  5. Compare HMACs (constant-time comparison)            │
│  6. If valid:                                           │
│     - Update last_valid_nonce                           │
│     - Save to nonce_state.json                          │
│     - Process payload                                   │
│     - Return 200 OK                                     │
│  7. If invalid:                                         │
│     - Log security violation                            │
│     - Return 401 Unauthorized                           │
└──────────────────────────────────────────────────────────┘
```

### Key Management

**Generation:**
```bash
cd flask
python scripts/generate_keys.py
```

**Generated Files:**
- `keys/server_private_key.pem` (keep secret!)
- `keys/server_public_key.pem` (embed in ESP32)
- `keys/aes_firmware_key.bin` (32 bytes, keep secret!)
- `keys/keys.h` (C header for ESP32)

**Synchronization:**
- ESP32 `keys.cpp` must match Flask `keys/`
- Use `generate_keys.py` to create matching keys
- Update both ESP32 and Flask when rotating keys

---

## FOTA System

### Firmware Preparation Pipeline

```
┌──────────────────────────────────────────────────────────┐
│  Firmware Compilation (PlatformIO)                       │
│  .pio/build/esp32dev/firmware.bin  (1,009,680 bytes)   │
└─────────────────┬────────────────────────────────────────┘
                  ↓
┌──────────────────────────────────────────────────────────┐
│  Firmware Manager (scripts/firmware_manager.py)          │
│                                                          │
│  1. Load firmware binary                                │
│  2. Calculate SHA-256 hash                              │
│  3. PKCS7 padding (align to 16 bytes)                   │
│  4. Generate random IV (16 bytes)                       │
│  5. Encrypt with AES-256-CBC:                           │
│     - Key: aes_firmware_key.bin                         │
│     - Mode: CBC                                         │
│     - Result: firmware_1.0.5_encrypted.bin (1,009,696)  │
│  6. Sign hash with RSA-2048:                            │
│     - Hash: SHA-256(original_firmware)                  │
│     - Sign with server_private_key.pem                  │
│     - Padding: PSS                                      │
│  7. Create manifest:                                    │
│     - version, sizes, hash, signature, IV, chunk_info   │
│  8. Save files:                                         │
│     - firmware/firmware_1.0.5_encrypted.bin             │
│     - firmware/firmware_1.0.5_manifest.json             │
└──────────────────────────────────────────────────────────┘
```

### Manifest Structure

```json
{
  "version": "1.0.5",
  "original_size": 1009680,
  "encrypted_size": 1009696,
  "sha256_hash": "5c29fe6639d62e14e8b000827e145151350d4eb19c7c2f2b3033072beb26db03",
  "signature": "eAsBdqqzquSBf4tdV03HDUyhDOvbVKEPk+YhMdTuqNp...",
  "iv": "pXhtjXdzN6uHCJOiqjqB9A==",
  "chunk_size": 1024,
  "total_chunks": 987,
  "release_notes": "",
  "created_at": "2025-10-28T05:30:10"
}
```

### OTA Update Flow

```
┌──────────────────────────────────────────────────────────┐
│  Phase 1: Check for Update                               │
│                                                          │
│  ESP32 → GET /ota/check/ESP32_EcoWatt_Smart?version=1.0.4 │
│                                                          │
│  Flask checks firmware/ directory:                       │
│  - Find latest manifest (by mtime)                      │
│  - Compare versions: "1.0.5" > "1.0.4"                  │
│  - Return update info                                   │
│                                                          │
│  Response: {                                            │
│    "update_available": true,                           │
│    "latest_version": "1.0.5",                          │
│    "firmware_size": 1009680,                           │
│    "encrypted_size": 1009696,                          │
│    "sha256_hash": "5c29fe6639d62e14...",              │
│    "signature": "eAsBdqqzquSBf4tdV03...",             │
│    "iv": "pXhtjXdzN6uHCJOiqjqB9A==",                  │
│    "chunk_size": 1024,                                 │
│    "total_chunks": 987                                 │
│  }                                                      │
└──────────────────────────────────────────────────────────┘
                        ↓
┌──────────────────────────────────────────────────────────┐
│  Phase 2: Initiate Session                               │
│                                                          │
│  ESP32 → POST /ota/initiate/ESP32_EcoWatt_Smart         │
│          Body: {"firmware_version": "1.0.5"}            │
│                                                          │
│  Flask creates session:                                 │
│  - Check for stale sessions (>5min) → auto-replace     │
│  - Generate session_id                                  │
│  - Initialize progress tracking                         │
│  - Mark status: "in_progress"                          │
│                                                          │
│  Response: {                                            │
│    "success": true,                                    │
│    "session_id": "ESP32_EcoWatt_Smart_1.0.5_1761611058" │
│  }                                                      │
└──────────────────────────────────────────────────────────┘
                        ↓
┌──────────────────────────────────────────────────────────┐
│  Phase 3: Download Chunks (0-986)                        │
│                                                          │
│  Loop for chunk_index in range(987):                    │
│    ESP32 → GET /ota/chunk/ESP32_EcoWatt_Smart?version=1.0.5&chunk=N │
│                                                          │
│    Flask:                                               │
│    - Verify session exists                             │
│    - Read 1024 bytes from encrypted firmware           │
│    - Base64 encode chunk                               │
│    - Update session progress                           │
│    - Update last_activity timestamp                    │
│                                                          │
│    Response: {                                          │
│      "chunk_data": "OEdtuPRfSbFKScqSZMfnAfsf89d8...",  │
│      "chunk_index": N,                                 │
│      "chunk_size": 1024                                │
│    }                                                    │
│                                                          │
│  ESP32 processes each chunk:                            │
│  - Base64 decode                                        │
│  - Decrypt with AES-256-CBC (IV chaining)              │
│  - Write to OTA partition                              │
└──────────────────────────────────────────────────────────┘
                        ↓
┌──────────────────────────────────────────────────────────┐
│  Phase 4: Verification & Completion                      │
│                                                          │
│  ESP32:                                                 │
│  - Calculate SHA-256 of decrypted firmware             │
│  - Verify RSA signature (PSS padding)                  │
│  - Verify ESP32 magic byte (0xE9)                      │
│  - Mark OTA partition as bootable                      │
│                                                          │
│  ESP32 → POST /ota/complete/ESP32_EcoWatt_Smart        │
│          Body: {"success": true}                        │
│                                                          │
│  Flask:                                                 │
│  - Update session status: "completed"                  │
│  - Increment success statistics                        │
│  - Clean up session                                    │
│                                                          │
│  ESP32 → ESP.restart() → Boot into new firmware        │
└──────────────────────────────────────────────────────────┘
```

### Security Features

**Encryption:** AES-256-CBC
- Entire firmware encrypted as single stream
- Random IV per firmware version
- IV stored in manifest, transmitted to ESP32

**Signature:** RSA-2048 with PSS padding
- Signs SHA-256 hash of **original** (unencrypted) firmware
- ESP32 verifies after decryption
- Prevents tampered firmware

**Integrity Checks:**
- SHA-256 hash verification
- ESP32 magic byte check (0xE9)
- Partition validity check

**Rollback Protection:**
- Dual-bank OTA partitions
- Mark new partition bootable only after verification
- Automatic rollback if boot fails

---

## Data Flow

### Upload Cycle Flow

```
┌──────────────────────────────────────────────────────────┐
│  ESP32 Device                                            │
│                                                          │
│  1. Poll inverter every 5s (configurable)              │
│  2. Buffer samples in memory                            │
│  3. Every 15min (900s):                                 │
│     a. Aggregate samples (min/avg/max)                 │
│     b. Create JSON: {"aggregated_data": [...]}         │
│     c. Apply security:                                 │
│        - Generate nonce                                │
│        - Base64 encode payload                         │
│        - Calculate HMAC                                │
│     d. POST /aggregated/ESP32_EcoWatt_Smart            │
└──────────────────┬───────────────────────────────────────┘
                   ↓ HTTP POST (secured)
┌──────────────────────────────────────────────────────────┐
│  Flask Server                                            │
│                                                          │
│  1. Route: aggregation_routes.py                        │
│     - Endpoint: /aggregated/<device_id>                │
│                                                          │
│  2. Security Handler:                                   │
│     - Validate HMAC                                     │
│     - Check nonce (anti-replay)                        │
│     - Decode base64 payload                            │
│                                                          │
│  3. Compression Handler:                                │
│     - Check if compressed or aggregated                │
│     - Decompress if needed                             │
│                                                          │
│  4. MQTT Publisher:                                     │
│     - Publish to ecowatt/data/ESP32_EcoWatt_Smart      │
│                                                          │
│  5. Response:                                           │
│     - 200 OK (success)                                 │
│     - 401 Unauthorized (security failure)              │
└──────────────────────────────────────────────────────────┘
```

### Command Execution Flow

```
┌──────────────────────────────────────────────────────────┐
│  User/Admin                                              │
│                                                          │
│  POST /commands/ESP32_EcoWatt_Smart                     │
│  {                                                       │
│    "command_type": "set_power",                        │
│    "parameters": {"power": "75%"}                      │
│  }                                                       │
└──────────────────┬───────────────────────────────────────┘
                   ↓
┌──────────────────────────────────────────────────────────┐
│  Flask Command Handler                                   │
│                                                          │
│  1. Generate UUID: e1ac4484-7d6b-4c9d-9047-d6b33eabb43b │
│  2. Create command entry                                │
│  3. Add to device queue                                 │
│  4. Return command_id                                   │
└──────────────────────────────────────────────────────────┘
                   ↓ (ESP32 polls periodically)
┌──────────────────────────────────────────────────────────┐
│  ESP32 Device                                            │
│                                                          │
│  1. GET /commands/ESP32_EcoWatt_Smart/poll              │
│  2. Receive command                                     │
│  3. Execute command:                                    │
│     - set_power → Inverter SIM                         │
│     - write_register → Modbus write                    │
│  4. Collect result                                      │
│  5. POST /commands/ESP32_EcoWatt_Smart/result           │
│     {                                                   │
│       "command_id": "e1ac4484...",                     │
│       "status": "completed",                           │
│       "result": "Power set to 75%"                     │
│     }                                                   │
└──────────────────────────────────────────────────────────┘
```

---

## Configuration

### Environment Variables

```bash
# Flask Configuration
FLASK_HOST=0.0.0.0
FLASK_PORT=5001
FLASK_DEBUG=False

# MQTT Configuration
MQTT_BROKER=broker.hivemq.com
MQTT_PORT=1883
MQTT_TOPIC_PREFIX=ecowatt

# Security
HMAC_PSK=<32-byte hex key>
AES_KEY_PATH=keys/aes_firmware_key.bin
RSA_PRIVATE_KEY_PATH=keys/server_private_key.pem

# Storage Paths
FIRMWARE_DIR=firmware
DIAGNOSTICS_DIR=diagnostics_data
KEYS_DIR=keys
```

### Device Configuration

```python
# Returned by GET /config endpoint
{
  "device_id": "ESP32_EcoWatt_Smart",
  "poll_frequency": 30,        # Seconds between inverter polls
  "upload_frequency": 300,     # Seconds between uploads (5min)
  "registers": [40000, 40001, 40002],
  "compression_enabled": true,
  "compression_method": "smart",
  "encryption_enabled": true,
  "firmware_version": "1.0.4"
}
```

---

## Performance Metrics

**Request Handling:**
- Health check: <5ms
- Secured upload: 10-50ms (depends on payload size)
- Command queue: <10ms
- OTA chunk: 20-100ms (depends on chunk size)

**Security Validation:**
- HMAC verification: ~2-5ms
- Nonce check: <1ms

**Compression:**
- Decompression: 5-50ms (method-dependent)
- Dictionary: ~5-10ms
- Temporal: ~10-20ms
- RLE: ~5-15ms

**OTA Performance:**
- Chunk encoding: ~10ms per chunk
- Session overhead: ~5ms per request
- Total OTA time: ~30-60 seconds (987 chunks @ 50ms avg)

---

## Troubleshooting

### Common Issues

**1. OTA Session Already In Progress**
```
Error: "OTA session already in progress"
Cause: Previous session not cleaned up (device reboot mid-OTA)
Solution: Wait 5 minutes (auto-cleanup) or restart Flask server
```

**2. HMAC Validation Failed**
```
Error: 401 Unauthorized - "HMAC validation failed"
Cause: Key mismatch between ESP32 and Flask
Solution: 
  1. Regenerate keys: python scripts/generate_keys.py
  2. Update ESP32 keys.cpp
  3. Rebuild firmware
```

**3. Nonce Replay Attack Detected**
```
Error: "Replay attack detected! Nonce X <= last valid nonce Y"
Cause: ESP32 using old nonce (reboot without NVS save)
Solution: DELETE /security/nonces to reset (testing only!)
```

**4. Firmware Decryption Failed**
```
Error: "Invalid ESP32 firmware magic byte: 0xC7 (expected 0xE9)"
Cause: AES key mismatch
Solution: Verify keys match in both systems
```

---

## Future Enhancements (M5)

1. **MQTT Integration** - Enable MQTT publishing (currently placeholder)
2. **Database Backend** - Replace in-memory storage with PostgreSQL/MongoDB
3. **Authentication** - Add JWT-based user authentication
4. **Rate Limiting** - Prevent abuse with per-device rate limits
5. **Metrics Dashboard** - Web UI for monitoring devices
6. **Log Rotation** - Automated log file management
7. **Docker Deployment** - Containerized server deployment
8. **Multi-Tenant** - Support multiple organizations

---

## Summary

The Flask server provides a robust, modular cloud backend for the EcoWatt system with:
- ✅ **Security** - HMAC authentication, anti-replay, encrypted firmware
- ✅ **Modularity** - Clean separation: routes → handlers → utilities
- ✅ **FOTA** - Secure, chunked firmware distribution with rollback
- ✅ **Commands** - Remote command execution with result tracking
- ✅ **Diagnostics** - Device health monitoring and logging
- ✅ **Compression** - Multiple decompression algorithms
- ✅ **Thread-Safe** - Proper locking for concurrent requests

**Testing Status:** M4 integration tests passing (88.9% → 100% after key sync fix)

**Next Steps:** M5 - Fault recovery, power optimization, final integration

---

**Document Version:** 1.0  
**Last Updated:** 2025-10-28  
**Authors:** Team PowerPort  
**Related Docs:** `FLASK_TESTS.md`, `ESP32_ARCHITECTURE.md`, `ESP32_TESTS.md`, `UTILITIES.md`
