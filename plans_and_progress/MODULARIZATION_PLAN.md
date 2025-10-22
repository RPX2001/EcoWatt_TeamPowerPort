# Flask Server Modularization Plan

## Overview
This document outlines the plan to refactor `flask_server_hivemq.py` (2057 lines) into a modular structure for better maintainability, testability, and scalability.

## Current State
- **Monolithic file**: All routes, handlers, utilities in one file
- **29 endpoints**: Diagnostics, Security, OTA, Commands, Fault Injection
- **Mixed concerns**: Network, business logic, data processing all intertwined

## Target Structure
```
flask/
├── flask_server_hivemq.py          # Main app (imports and registers blueprints)
├── config.py                        # Configuration and constants
├── routes/                          # Route definitions (Flask blueprints)
│   ├── __init__.py
│   ├── diagnostics_routes.py       # /diagnostics endpoints
│   ├── security_routes.py          # /security/stats endpoints
│   ├── aggregation_routes.py       # /aggregation/stats endpoints
│   ├── ota_routes.py                # /ota/* endpoints
│   ├── command_routes.py            # /command/* endpoints
│   ├── fault_routes.py              # /fault/* endpoints
│   └── general_routes.py            # /status, /health, /process
├── handlers/                        # Business logic
│   ├── __init__.py
│   ├── diagnostics_handler.py      # Store/retrieve diagnostics
│   ├── compression_handler.py      # Decompression logic
│   ├── security_handler.py         # Nonce, signature validation
│   ├── ota_handler.py               # OTA update logic
│   ├── command_handler.py           # Command execution logic
│   └── fault_handler.py             # Fault injection logic
├── utils/                           # Utilities
│   ├── __init__.py
│   ├── compression_utils.py        # ✅ CREATED - Decompression functions
│   ├── mqtt_utils.py                # MQTT initialization and callbacks
│   ├── logger_utils.py              # Logging configuration
│   └── data_utils.py                # Data serialization/deserialization
└── justfile                         # ✅ EXISTS - Build automation

```

## Phase 1: Utilities (✅ COMPLETED)

### compression_utils.py (✅ CREATED - 400 lines)
**Location**: `flask/utils/compression_utils.py`

**Functions**:
- `decompress_dictionary_bitmask(binary_data)` - Dictionary + Bitmask (0xD0)
- `decompress_temporal_delta(binary_data)` - Temporal Delta (0xDE)
- `decompress_semantic_rle(binary_data)` - Semantic RLE (0xAD)
- `decompress_bit_packed(binary_data)` - Bit Packing (0xBF)
- `unpack_bits_from_buffer(buffer, bit_offset, num_bits)` - Bit extraction
- `decompress_smart_binary_data(base64_data)` - Auto-detect and decompress

**Extracted from**: Lines 316-557 of flask_server_hivemq.py

**Status**: ✅ Complete, ready for import

---

## Phase 2: Remaining Utilities (⏳ TO DO)

### mqtt_utils.py (⏳ PENDING)
**Location**: `flask/utils/mqtt_utils.py`

**Functions**:
- `init_mqtt()` - Initialize MQTT client
- `on_connect(client, userdata, flags, rc)` - Connection callback
- `on_publish(client, userdata, mid)` - Publish callback
- `on_disconnect(client, userdata, rc)` - Disconnect callback
- `on_message(client, userdata, msg)` - Message callback

**Extracted from**: Lines 63-150 of flask_server_hivemq.py

**Dependencies**: 
- paho.mqtt.client
- config.py (MQTT_BROKER, MQTT_PORT, MQTT_TOPIC)

---

### logger_utils.py (⏳ PENDING)
**Location**: `flask/utils/logger_utils.py`

**Functions**:
- `init_logging()` - Configure logging for application
- `get_logger(name)` - Get logger instance

**Extracted from**: Lines 27-29 of flask_server_hivemq.py

**Dependencies**: logging module

---

### data_utils.py (⏳ PENDING)
**Location**: `flask/utils/data_utils.py`

**Functions**:
- `deserialize_aggregated_sample(binary_data)` - Parse aggregated sample struct
- `expand_aggregated_to_samples(aggregated_data)` - Expand to individual samples
- `process_register_data(registers, decompressed_values)` - Map values to registers

**Extracted from**: Lines 188-620 of flask_server_hivemq.py

**Dependencies**: struct, datetime

---

## Phase 3: Handlers (⏳ TO DO)

### diagnostics_handler.py (⏳ PENDING)
**Location**: `flask/handlers/diagnostics_handler.py`

**Functions**:
- `store_diagnostics(device_id, diagnostics_data)` - Store diagnostics to storage
- `get_diagnostics_by_device(device_id, limit)` - Retrieve device diagnostics
- `get_all_diagnostics(limit)` - Retrieve all diagnostics

**Extracted from**: Lines 1671-1897 (diagnostics routes logic)

**Dependencies**: 
- utils/data_utils.py
- utils/compression_utils.py
- server_security_layer.py

**Global State**:
- `diagnostics_store` - In-memory storage dict (could move to Redis/DB later)
- `diagnostics_lock` - Thread lock for concurrent access

---

### compression_handler.py (⏳ PENDING)
**Location**: `flask/handlers/compression_handler.py`

**Functions**:
- `handle_compressed_data(base64_data)` - Main decompression entry point
- `validate_compression_crc(data, crc)` - Validate CRC32 checksum
- `get_compression_statistics()` - Return compression performance stats

**Extracted from**: Lines 621-861 (process_compressed_data route logic)

**Dependencies**:
- utils/compression_utils.py
- utils/data_utils.py

---

### security_handler.py (⏳ PENDING)
**Location**: `flask/handlers/security_handler.py`

**Functions**:
- `validate_secured_payload(payload)` - Verify signature and nonce
- `get_security_stats(device_id=None)` - Retrieve security statistics
- `persist_nonce(device_id, nonce)` - Store used nonce

**Extracted from**: Lines 1964-2014 (security routes logic)

**Dependencies**:
- server_security_layer.py

**Global State**:
- `security_stats` - Authentication statistics
- `nonce_store` - Used nonces (prevent replay attacks)

---

### ota_handler.py (⏳ PENDING)
**Location**: `flask/handlers/ota_handler.py`

**Functions**:
- `check_for_update(device_id, current_version)` - Check if update available
- `get_firmware_chunk(device_id, offset, chunk_size)` - Get encrypted chunk
- `verify_firmware_signature(device_id, signature)` - Verify RSA signature
- `complete_ota(device_id)` - Finalize OTA process
- `get_ota_status(device_id)` - Get current OTA progress

**Extracted from**: Lines 949-1174 (OTA routes logic)

**Dependencies**:
- firmware_manager.py (FirmwareManager)

**Global State**:
- `ota_sessions` - Active OTA sessions by device_id
- `ota_lock` - Thread lock

---

### command_handler.py (⏳ PENDING)
**Location**: `flask/handlers/command_handler.py`

**Functions**:
- `queue_command(device_id, command_type, params)` - Queue remote command
- `poll_commands(device_id)` - Get pending commands for device
- `submit_command_result(device_id, command_id, result)` - Store execution result
- `get_command_status(command_id)` - Get command status
- `get_command_history(device_id, limit)` - Get command history
- `get_pending_commands(device_id)` - Get pending commands
- `get_command_statistics(device_id)` - Get command stats
- `get_command_logs(device_id, limit)` - Get command logs

**Extracted from**: Lines 1175-1557 (command routes logic)

**Dependencies**:
- command_manager.py (CommandManager)

**Global State**:
- `command_manager` - CommandManager instance

---

### fault_handler.py (⏳ PENDING)
**Location**: `flask/handlers/fault_handler.py`

**Functions**:
- `enable_fault_injection(fault_type, params)` - Enable fault injection
- `disable_fault_injection()` - Disable fault injection
- `get_fault_status()` - Get fault injection status
- `reset_fault_statistics()` - Reset fault counters

**Extracted from**: Lines 1558-1670 (fault routes logic)

**Dependencies**:
- fault_injector.py (enable_fault_injection, etc.)

**Global State**:
- None (uses fault_injector module)

---

## Phase 4: Routes (⏳ TO DO)

### diagnostics_routes.py (⏳ PENDING)
**Location**: `flask/routes/diagnostics_routes.py`

**Blueprint**: `diagnostics_bp`

**Endpoints**:
- `POST /diagnostics/<device_id>` - Store diagnostics
- `GET /diagnostics/<device_id>` - Get device diagnostics
- `GET /diagnostics` - Get all diagnostics

**Dependencies**:
- handlers/diagnostics_handler.py

---

### security_routes.py (⏳ PENDING)
**Location**: `flask/routes/security_routes.py`

**Blueprint**: `security_bp`

**Endpoints**:
- `GET /security/stats` - Get overall security stats
- `GET /security/stats/<device_id>` - Get device security stats

**Dependencies**:
- handlers/security_handler.py

---

### aggregation_routes.py (⏳ PENDING)
**Location**: `flask/routes/aggregation_routes.py`

**Blueprint**: `aggregation_bp`

**Endpoints**:
- `GET /aggregation/stats` - Get overall aggregation stats
- `GET /aggregation/stats/<device_id>` - Get device aggregation stats

**Dependencies**:
- handlers/diagnostics_handler.py (uses same storage)

---

### ota_routes.py (⏳ PENDING)
**Location**: `flask/routes/ota_routes.py`

**Blueprint**: `ota_bp`

**Endpoints**:
- `POST /ota/check` - Check for updates
- `POST /ota/chunk` - Get firmware chunk
- `POST /ota/verify` - Verify firmware signature
- `POST /ota/complete` - Complete OTA update
- `GET /ota/status` - Get OTA status

**Dependencies**:
- handlers/ota_handler.py

---

### command_routes.py (⏳ PENDING)
**Location**: `flask/routes/command_routes.py`

**Blueprint**: `command_bp`

**Endpoints**:
- `POST /command/queue` - Queue command
- `POST /command/poll` - Poll for commands
- `POST /command/result` - Submit command result
- `GET /command/status/<command_id>` - Get command status
- `GET /command/history` - Get command history
- `GET /command/pending` - Get pending commands
- `GET /command/statistics` - Get command statistics
- `GET /command/logs` - Get command logs

**Dependencies**:
- handlers/command_handler.py

---

### fault_routes.py (⏳ PENDING)
**Location**: `flask/routes/fault_routes.py`

**Blueprint**: `fault_bp`

**Endpoints**:
- `POST /fault/enable` - Enable fault injection
- `POST /fault/disable` - Disable fault injection
- `GET /fault/status` - Get fault status
- `POST /fault/reset` - Reset fault statistics

**Dependencies**:
- handlers/fault_handler.py

---

### general_routes.py (⏳ PENDING)
**Location**: `flask/routes/general_routes.py`

**Blueprint**: `general_bp`

**Endpoints**:
- `POST /process` - Process compressed data
- `GET /status` - Get server status
- `GET /health` - Health check
- `POST /device_settings` - Update device settings
- `POST /changes` - Notify changes

**Dependencies**:
- handlers/compression_handler.py
- utils/mqtt_utils.py

---

## Phase 5: Main Application Refactor (⏳ TO DO)

### flask_server_hivemq.py (⏳ PENDING - Major refactor)
**Location**: `flask/flask_server_hivemq.py`

**New Structure**:
```python
from flask import Flask
from flask_cors import CORS
import logging

# Import configuration
from config import *

# Import utilities
from utils.mqtt_utils import init_mqtt
from utils.logger_utils import init_logging

# Import route blueprints
from routes.diagnostics_routes import diagnostics_bp
from routes.security_routes import security_bp
from routes.aggregation_routes import aggregation_bp
from routes.ota_routes import ota_bp
from routes.command_routes import command_bp
from routes.fault_routes import fault_bp
from routes.general_routes import general_bp

# Initialize Flask app
app = Flask(__name__)
CORS(app)

# Initialize logging
init_logging()
logger = logging.getLogger(__name__)

# Register blueprints
app.register_blueprint(diagnostics_bp)
app.register_blueprint(security_bp)
app.register_blueprint(aggregation_bp)
app.register_blueprint(ota_bp)
app.register_blueprint(command_bp)
app.register_blueprint(fault_bp)
app.register_blueprint(general_bp)

# Initialize MQTT
mqtt_client = init_mqtt()

if __name__ == '__main__':
    logger.info("Starting EcoWatt Flask Server (Modular)")
    app.run(host='0.0.0.0', port=5000, debug=False)
```

**Reduced from**: 2057 lines → ~50 lines

**Changes**:
- All routes moved to blueprints
- All handlers moved to separate modules
- All utilities moved to utils/
- Configuration moved to config.py
- Main file becomes orchestrator only

---

## Phase 6: Configuration Extraction (⏳ TO DO)

### config.py (⏳ PENDING - Enhance existing)
**Location**: `flask/config.py`

**Current State**: Exists (48 lines) with some basic config

**Enhancement Needed**:
```python
# MQTT Configuration
MQTT_BROKER = "broker.hivemq.com"
MQTT_PORT = 1883
MQTT_TOPIC = "esp32/ecowatt_data"

# OTA Configuration
LATEST_FIRMWARE_VERSION = "1.0.4"
FIRMWARE_CHUNK_SIZE = 4096

# Server Configuration
HOST = '0.0.0.0'
PORT = 5000
DEBUG = False

# Logging Configuration
LOG_LEVEL = logging.INFO
LOG_FORMAT = '%(asctime)s - %(name)s - %(levelname)s - %(message)s'

# Storage Configuration (future: Redis/PostgreSQL)
USE_REDIS = False
REDIS_HOST = 'localhost'
REDIS_PORT = 6379

# Security Configuration
MAX_NONCE_AGE_SECONDS = 300
ENABLE_SIGNATURE_VERIFICATION = True

# Command Configuration
COMMAND_TIMEOUT_SECONDS = 60
MAX_PENDING_COMMANDS = 100

# Fault Injection Configuration
FAULT_INJECTION_ENABLED = False
```

---

## Implementation Steps

### Step 1: Create __init__.py files (⏳ TO DO)
```bash
touch flask/routes/__init__.py
touch flask/handlers/__init__.py
touch flask/utils/__init__.py
```

### Step 2: Extract utilities (1/4 ✅ DONE)
- ✅ compression_utils.py - CREATED
- ⏳ mqtt_utils.py - PENDING
- ⏳ logger_utils.py - PENDING
- ⏳ data_utils.py - PENDING

### Step 3: Extract handlers (0/6 ⏳ TO DO)
1. Create diagnostics_handler.py
2. Create compression_handler.py
3. Create security_handler.py
4. Create ota_handler.py
5. Create command_handler.py
6. Create fault_handler.py

### Step 4: Create route blueprints (0/7 ⏳ TO DO)
1. Create diagnostics_routes.py
2. Create security_routes.py
3. Create aggregation_routes.py
4. Create ota_routes.py
5. Create command_routes.py
6. Create fault_routes.py
7. Create general_routes.py

### Step 5: Refactor main application (⏳ TO DO)
1. Update flask_server_hivemq.py to import and register blueprints
2. Remove all inline route definitions
3. Remove all inline handler functions
4. Keep only app initialization and blueprint registration

### Step 6: Update configuration (⏳ TO DO)
1. Enhance config.py with all constants
2. Update all modules to import from config
3. Remove hardcoded values

### Step 7: Testing (⏳ TO DO)
1. Unit tests for each handler
2. Integration tests for each route blueprint
3. End-to-end tests for critical workflows
4. Update justfile with modular test commands

---

## Benefits of Modularization

### Maintainability
- **Small files**: Each file < 300 lines (vs 2057 lines monolith)
- **Single responsibility**: Each module has one clear purpose
- **Easy navigation**: Find code by feature (diagnostics, ota, etc.)

### Testability
- **Unit tests**: Test handlers independently
- **Mocking**: Easy to mock dependencies
- **Coverage**: Better test coverage visibility

### Scalability
- **Team collaboration**: Multiple developers work on different routes
- **Feature flags**: Easy to enable/disable features
- **Microservices**: Can split into separate services later

### Reusability
- **Shared utilities**: Compression, logging, MQTT reusable
- **Consistent patterns**: All routes follow same structure
- **API versioning**: Easy to add /v2/ routes

---

## Migration Strategy

### Approach: Incremental Migration
1. **Create new structure** (routes/, handlers/, utils/)
2. **Extract one module at a time** (compression_utils.py ✅)
3. **Update imports gradually** (keep old code working)
4. **Test after each extraction** (ensure no regressions)
5. **Remove old code** (after all imports updated)

### Backward Compatibility
- Keep flask_server_hivemq.py working throughout migration
- All endpoints remain at same URLs
- All functionality preserved
- No breaking changes to ESP32 client

### Rollback Plan
- Git tags at each migration step
- Can revert individual modules
- Original flask_server_hivemq.py backed up

---

## File Size Estimates (After Modularization)

```
flask_server_hivemq.py:     2057 lines → ~50 lines   (-2007 lines)
config.py:                    48 lines → ~80 lines   (+32 lines)
utils/compression_utils.py:   0 lines → 400 lines   (+400 lines)
utils/mqtt_utils.py:          0 lines → 150 lines   (+150 lines)
utils/logger_utils.py:        0 lines → 50 lines    (+50 lines)
utils/data_utils.py:          0 lines → 400 lines   (+400 lines)
handlers/*:                   0 lines → ~800 lines  (+800 lines)
routes/*:                     0 lines → ~700 lines  (+700 lines)

Total:                     2105 lines → 2630 lines  (+525 lines overhead)
```

**Overhead**: ~25% increase in total lines (due to imports, docstrings, structure)
**Benefit**: 2057-line monolith → 7 manageable files (largest ~400 lines)

---

## Next Actions (Priority Order)

1. ✅ **COMPLETED**: Create compression_utils.py
2. ⏳ **TODO**: Create remaining utility modules (mqtt, logger, data)
3. ⏳ **TODO**: Create handler modules (6 handlers)
4. ⏳ **TODO**: Create route blueprint modules (7 blueprints)
5. ⏳ **TODO**: Refactor main flask_server_hivemq.py
6. ⏳ **TODO**: Update justfile with modular test commands
7. ⏳ **TODO**: Write unit tests for each module
8. ⏳ **TODO**: Integration testing
9. ⏳ **TODO**: Documentation updates

---

## Success Criteria

✅ **Phase Complete When**:
- All routes work exactly as before
- All tests pass
- No endpoints broken
- ESP32 client works without changes
- Code coverage ≥ 80%
- All files < 500 lines
- Documentation updated
- Team trained on new structure

---

## Timeline Estimate

- **Utilities**: 2-3 hours (1/4 done)
- **Handlers**: 4-5 hours
- **Routes**: 3-4 hours
- **Main refactor**: 1-2 hours
- **Testing**: 3-4 hours
- **Documentation**: 1-2 hours

**Total**: 14-20 hours of development work

**Status**: ~15% complete (compression_utils.py done, Wokwi setup done)

---

## References

- Original file: `flask/flask_server_hivemq.py` (2057 lines)
- Flask Blueprints: https://flask.palletsprojects.com/en/2.3.x/blueprints/
- Python Project Structure: https://docs.python-guide.org/writing/structure/
