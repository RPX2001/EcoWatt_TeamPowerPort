# Logging System Documentation

## Overview

The EcoWatt project now uses a standardized, professional logging system across both ESP32 firmware and Flask backend with:

- **Consistent formatting** with module tags and timestamps
- **Visual indicators**: ✓ for success, ✗ for errors, [!] for warnings
- **Log levels**: DEBUG, INFO, WARN, ERROR with runtime toggling
- **Request tracking** (Flask): Each HTTP request gets a unique ID
- **File + Console output** with different formatters

---

## ESP32 Logging System

### Header File
Location: `PIO/ECOWATT/include/peripheral/logger.h`

### Log Levels
```cpp
enum LogLevel {
    LOG_LEVEL_DEBUG = 0,  // Detailed debug information
    LOG_LEVEL_INFO = 1,   // General informational messages
    LOG_LEVEL_WARN = 2,   // Warning messages
    LOG_LEVEL_ERROR = 3,  // Error messages
    LOG_LEVEL_NONE = 4    // Disable all logging
};
```

### Module Tags
Pre-defined tags for different subsystems:
```cpp
LOG_TAG_BOOT        // System boot/initialization
LOG_TAG_WIFI        // WiFi operations
LOG_TAG_TASK        // FreeRTOS task management
LOG_TAG_SENSOR      // Sensor data acquisition
LOG_TAG_COMPRESS    // Data compression
LOG_TAG_UPLOAD      // Data upload operations
LOG_TAG_COMMAND     // Command execution
LOG_TAG_CONFIG      // Configuration management
LOG_TAG_FOTA        // Firmware OTA updates
LOG_TAG_SECURITY    // Security operations
LOG_TAG_POWER       // Power management
LOG_TAG_NVS         // Non-volatile storage
LOG_TAG_DIAG        // Diagnostics
LOG_TAG_STATS       // Statistics
LOG_TAG_FAULT       // Fault recovery
LOG_TAG_MODBUS      // Modbus communication
LOG_TAG_BUFFER      // Ring buffer operations
LOG_TAG_WATCHDOG    // Watchdog timer
```

### Basic Usage

**Initialization:**
```cpp
#include "peripheral/logger.h"

void setup() {
    initLogger(LOG_LEVEL_INFO);  // Initialize with INFO level
}
```

**Logging Messages:**
```cpp
// Debug (only shown if LOG_LEVEL_DEBUG)
LOG_DEBUG(LOG_TAG_WIFI, "Connecting to SSID: %s", ssid);

// Info
LOG_INFO(LOG_TAG_UPLOAD, "Uploading %d packets", count);

// Warning (shows [!] symbol)
LOG_WARN(LOG_TAG_BUFFER, "Buffer is 90%% full (%zu/20)", size);

// Error (shows ✗ symbol)
LOG_ERROR(LOG_TAG_FOTA, "OTA download failed: %d", error);

// Success (shows ✓ symbol)
LOG_SUCCESS(LOG_TAG_BOOT, "All systems initialized");
```

**Section Headers:**
```cpp
LOG_SECTION("DATA UPLOAD CYCLE");  // Boxed section header
LOG_SUBSECTION("Compression Statistics");  // Simple subsection
LOG_DIVIDER();  // Horizontal line
```

**Runtime Log Level Control:**
```cpp
setLogLevel(LOG_LEVEL_DEBUG);  // Enable all debug logs
setLogLevel(LOG_LEVEL_ERROR);  // Only show errors
setLogLevel(LOG_LEVEL_NONE);   // Disable all logging
```

### Example Output
```
╔════════════════════════════════════════════════════════════╗
║              EcoWatt Logger Initialized                    ║
╚════════════════════════════════════════════════════════════╝
Log Level: 1 (0=DEBUG, 1=INFO, 2=WARN, 3=ERROR, 4=NONE)

[00:00:05.234] [BOOT      ] ✓   WiFi initialized
[00:00:05.456] [BOOT      ] ✓   Power Management initialized
[00:00:05.678] [SECURITY  ] ✓   Security layer initialized

╔════════════════════════════════════════════════════════════╗
║  DATA UPLOAD CYCLE                                         ║
╚════════════════════════════════════════════════════════════╝
[00:01:23.456] [UPLOAD    ]     Preparing 5 compressed batches
[00:01:23.789] [COMPRESS  ]     Compression: 2048 → 512 bytes (75.0% savings)
[00:01:24.012] [SECURITY  ]     Securing payload with HMAC
[00:01:24.234] [SECURITY  ] ✓   Payload secured
[00:01:24.567] [UPLOAD    ] ✓   Upload successful (HTTP 200)
```

---

## Flask Logging System

### Location
`flask/utils/logger_utils.py`

### Features
- **Colored console output** with ANSI codes
- **Plain text file output** for log persistence
- **Request ID tracking** for debugging HTTP requests
- **Success logging** with ✓ symbol
- **Module-based loggers** for fine-grained control

### Initialization
```python
from utils.logger_utils import init_logging

# Initialize logging (console + file)
init_logging(
    level=logging.INFO,
    log_file='logs/ecowatt_server.log',  # Optional
    console_colors=True
)
```

### Basic Usage

**Get a Logger:**
```python
import logging
from utils.logger_utils import log_success

logger = logging.getLogger(__name__)
```

**Logging Messages:**
```python
# Debug
logger.debug(f"Processing request from {device_id}")

# Info
logger.info(f"Received data from {device_id}: {size} bytes")

# Warning
logger.warning(f"High memory usage: {usage}%")

# Error
logger.error(f"✗ Failed to decompress data: {error}")

# Success (with ✓ symbol)
log_success(logger, "Data stored for %s", device_id)
```

**Request ID Tracking:**
```python
from utils.logger_utils import set_request_id, clear_request_id

# In Flask request handler
@app.before_request
def before_request():
    set_request_id()  # Auto-generates UUID

@app.after_request
def after_request(response):
    clear_request_id()
    return response

# Logs will now include [12345678] request ID
```

**Runtime Level Control:**
```python
from utils.logger_utils import enable_debug_logging, disable_debug_logging

# Enable debug mode
enable_debug_logging()

# Disable debug mode (back to INFO)
disable_debug_logging()
```

### Example Output

**Console (with colors):**
```
[10:30:45] [flask          ]     Starting EcoWatt server v2.0.0
[10:30:45] [diagnostics    ] ✓   Diagnostics handler initialized
[10:30:46] [compression    ]     Compression stats: 1234 decompressions
[10:31:15] [aggregation    ] [a1b2c3d4] ✓   Stored diagnostics for ESP32_DEV (12 total)
[10:31:16] [ota            ] [a1b2c3d4]     OTA check for ESP32_DEV
[10:31:17] [security       ] [a1b2c3d4] ✓   Payload validated
```

**Log File (plain text):**
```
[2025-11-05 10:30:45] [INFO    ] [flask          ] [ ] Starting EcoWatt server v2.0.0
[2025-11-05 10:30:45] [INFO    ] [diagnostics    ] [✓] Diagnostics handler initialized
[2025-11-05 10:30:46] [INFO    ] [compression    ] [ ] Compression stats: 1234 decompressions
[2025-11-05 10:31:15] [INFO    ] [aggregation    ] [a1b2c3d4-e5f6-7890] [✓] Stored diagnostics for ESP32_DEV (12 total)
[2025-11-05 10:31:16] [INFO    ] [ota            ] [a1b2c3d4-e5f6-7890] [ ] OTA check for ESP32_DEV
[2025-11-05 10:31:17] [INFO    ] [security       ] [a1b2c3d4-e5f6-7890] [✓] Payload validated
```

---

## Migration Guide

### ESP32 Files

**Before:**
```cpp
#include "peripheral/print.h"
#include "peripheral/formatted_print.h"

print("[DataUploader] Initialized\n");
PRINT_SUCCESS("Upload complete!");
PRINT_ERROR("Upload failed: %d", code);
```

**After:**
```cpp
#include "peripheral/logger.h"

LOG_INFO(LOG_TAG_UPLOAD, "Data uploader initialized");
LOG_SUCCESS(LOG_TAG_UPLOAD, "Upload complete");
LOG_ERROR(LOG_TAG_UPLOAD, "Upload failed: %d", code);
```

### Flask Files

**Before:**
```python
logger.info(f"Stored diagnostics for {device_id}: {count} total entries")
logger.error(f"Error storing diagnostics for {device_id}: {e}")
```

**After:**
```python
from utils.logger_utils import log_success

log_success(logger, "Stored diagnostics for %s (%d total)", device_id, count)
logger.error(f"✗ Failed to store diagnostics for {device_id}: {e}")
```

---

## Log Level Guidelines

| Level | When to Use | Example |
|-------|-------------|---------|
| **DEBUG** | Detailed diagnostic info for development | Variable values, loop iterations, detailed flow |
| **INFO** | General operation progress | "Task started", "Connection established", "Data received" |
| **WARN** | Unusual but handled situations | Buffer nearly full, retry attempts, fallback used |
| **ERROR** | Failure that prevents operation | Upload failed, invalid data, connection lost |
| **SUCCESS** | Successful completion of important operations | "Upload complete", "OTA successful", "System ready" |

---

## Best Practices

### ESP32
1. **Use appropriate tags** for each module
2. **Keep messages concise** (serial bandwidth is limited)
3. **Use DEBUG for verbose output** (can be disabled in production)
4. **Avoid logging in tight loops** (can cause watchdog timeouts)
5. **Use LOG_SUCCESS** for milestones (system init, uploads, etc.)

### Flask
1. **Use log_success()** for successful operations
2. **Include device_id** in messages when applicable
3. **Log at DEBUG level** for internal details
4. **Use ERROR with ✗** for actual failures
5. **Set request IDs** in route handlers for tracing

---

## Configuration

### ESP32 - Compile Time
Edit in `main.cpp` or during `initLogger()`:
```cpp
initLogger(LOG_LEVEL_INFO);  // Default level
```

### ESP32 - Runtime
```cpp
// In command handler or serial interface
if (command == "debug") {
    setLogLevel(LOG_LEVEL_DEBUG);
} else if (command == "quiet") {
    setLogLevel(LOG_LEVEL_ERROR);
}
```

### Flask - Environment Variable
```bash
# Set log level via environment
export ECOWATT_LOG_LEVEL=DEBUG

# In Flask app startup
import os
log_level = os.getenv('ECOWATT_LOG_LEVEL', 'INFO')
init_logging(level=getattr(logging, log_level))
```

---

## Troubleshooting

### ESP32: No logs appearing
1. Check log level: `setLogLevel(LOG_LEVEL_DEBUG)`
2. Verify Serial.begin(115200) is called
3. Ensure logger is included: `#include "peripheral/logger.h"`

### Flask: Colors not showing
1. Check terminal supports ANSI colors
2. Use `console_colors=True` in `init_logging()`
3. For Windows: Install `colorama`

### Request IDs not appearing
1. Ensure `set_request_id()` called before logging
2. Use Flask `before_request` hook
3. Clear with `clear_request_id()` after response

---

## Files Modified

### ESP32
- `include/peripheral/logger.h` (NEW)
- `src/peripheral/logger.cpp` (NEW)
- `src/application/system_initializer.cpp`
- `src/application/data_uploader.cpp`
- Additional files being updated...

### Flask
- `utils/logger_utils.py` (ENHANCED)
- `handlers/diagnostics_handler.py`
- Additional files being updated...

---

## Summary

The new logging system provides:
✓ Clean, professional output
✓ Easy debugging with module tags and timestamps
✓ Visual success/error indicators (✓/✗)
✓ Runtime log level control
✓ Request tracing for Flask
✓ File + console output
✓ Consistent format across ESP32 and Flask
