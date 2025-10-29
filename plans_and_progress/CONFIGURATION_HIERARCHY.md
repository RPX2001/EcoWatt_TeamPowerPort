# Configuration Hierarchy - EcoWatt ESP32

**Date**: 2025-10-29  
**Status**: Centralized and consolidated  

---

## Configuration Files Overview

### 1. **`application/system_config.h`** - System Constants
**Purpose**: ALL timing, buffer sizes, thresholds, and system limits  
**Scope**: Production and test code  
**Contains**:
- Timing configuration (poll, upload, config, OTA frequencies)
- Task deadlines
- Watchdog thresholds
- Mutex timeouts
- Queue sizes
- Buffer sizes
- Network timeouts
- Register limits
- Compression thresholds
- Logging levels
- Validation macros

**Usage**: `#include "application/system_config.h"`

---

### 2. **`application/credentials.h`** - Production Credentials
**Purpose**: WiFi, server, and device identity for PRODUCTION deployment  
**Scope**: Production code only  
**Contains**:
- WiFi SSID and password
- Flask server URL
- Device ID and name

**Usage**: `#include "application/credentials.h"`

**⚠️ Warning**: Contains sensitive credentials - do not commit to public repo!

---

### 3. **`config/test_config.h`** - Test-Specific Configuration
**Purpose**: Test device IDs, simulator API, test-specific overrides  
**Scope**: Test code only  
**Imports**: `credentials.h` + `system_config.h`  
**Contains**:
- Test device identifiers (M3, M4 test IDs)
- Inverter simulator API configuration
- Modbus test configuration
- URL builder macros
- Test-specific timeouts

**Usage**: `#include "config/test_config.h"` (in test files only)

---

## Configuration Hierarchy

```
┌─────────────────────────────────────────┐
│     application/system_config.h         │
│  (Timing, buffers, thresholds, limits)  │
│         SYSTEM CONSTANTS                │
└──────────────────┬──────────────────────┘
                   │
                   │ imported by
                   │
      ┌────────────┴─────────────┐
      │                          │
      ▼                          ▼
┌─────────────────┐    ┌──────────────────────┐
│ credentials.h   │    │  test_config.h       │
│  (Production)   │    │  (Test-specific)     │
│                 │    │  imports both:       │
│ - WiFi          │    │  - credentials.h     │
│ - Server URL    │    │  - system_config.h   │
│ - Device ID     │    │                      │
└────────┬────────┘    └──────────┬───────────┘
         │                        │
         │ used by                │ used by
         │                        │
         ▼                        ▼
┌─────────────────┐    ┌──────────────────────┐
│  Production     │    │   Test Code          │
│  Source Files   │    │   (test/*.cpp)       │
│  (main.cpp,     │    │                      │
│   modules)      │    │                      │
└─────────────────┘    └──────────────────────┘
```

---

## Single Source of Truth Principle

### Timing Configuration
✅ **ONLY in `system_config.h`**
```cpp
#define DEFAULT_POLL_FREQUENCY_US       5000000ULL      // 5 seconds
#define DEFAULT_UPLOAD_FREQUENCY_US     15000000ULL     // 15 seconds
#define DEFAULT_CONFIG_FREQUENCY_US     5000000ULL      // 5 seconds
#define DEFAULT_OTA_FREQUENCY_US        60000000ULL     // 1 minute
```

❌ **NOT hardcoded in:**
- ~~main.cpp~~ (removed `uploadFreq = 15000000ULL;`)
- ~~task_manager.cpp~~ (now uses `DEFAULT_*_FREQUENCY_US / 1000`)
- ~~test files~~ (import from `system_config.h`)

### WiFi and Server
✅ **ONLY in `credentials.h`** (production)
```cpp
#define WIFI_SSID "Galaxy A32B46A"
#define WIFI_PASSWORD "aubz5724"
#define FLASK_SERVER_URL "http://192.168.194.249:5001"
```

❌ **NOT duplicated in:**
- ~~test_config.h~~ (removed duplicates, now imports `credentials.h`)

### Device Identity
✅ **Production**: `credentials.h`
```cpp
#define DEVICE_ID "ESP32_001"
#define DEVICE_NAME "EcoWatt Smart Monitor"
```

✅ **Tests**: `test_config.h`
```cpp
#define TEST_DEVICE_ID_M3 "TEST_ESP32_M3_INTEGRATION"
#define TEST_DEVICE_ID_M4_INTEGRATION "ESP32_EcoWatt_Smart"
// ... other test device IDs
```

---

## How to Change Configuration

### Change Polling Frequency
**File**: `application/system_config.h`
```cpp
#define DEFAULT_POLL_FREQUENCY_US       10000000ULL     // Change to 10 seconds
```
**Effect**: Automatically applies to all code (main.cpp, task_manager.cpp, tests)

### Change WiFi Credentials
**File**: `application/credentials.h`
```cpp
#define WIFI_SSID "NewNetwork"
#define WIFI_PASSWORD "NewPassword"
```
**Effect**: Used by production code

### Change Test Device ID
**File**: `config/test_config.h`
```cpp
#define TEST_DEVICE_ID_M3 "NEW_TEST_DEVICE"
```
**Effect**: Used by M3 integration tests

### Change Watchdog Threshold
**File**: `application/system_config.h`
```cpp
#define MAX_TASK_IDLE_TIME_MS           180000          // Change to 3 minutes
```
**Effect**: Automatically applies to watchdog task

---

## Dynamic vs Static Configuration

### Dynamic (Runtime Configurable via NVS/Flask)
- Poll frequency (via `/config/<device_id>`)
- Upload frequency (via `/config/<device_id>`)
- Register selection (via `/config/<device_id>`)
- **Batch size** (calculated dynamically: `uploadFreq / pollFreq`)

### Static (Compile-Time Only)
- Config check frequency (5 seconds)
- OTA check frequency (60 seconds)
- Watchdog thresholds
- Queue sizes
- Buffer sizes
- Mutex timeouts

---

## Removed Redundancies

### Before Consolidation
```
❌ system_config.h: DEFAULT_POLL_FREQUENCY_US = 5000000
❌ test_config.h: DEFAULT_POLL_FREQUENCY_US = 5000000  (DUPLICATE!)
❌ task_manager.cpp: pollFrequency = 5000              (DUPLICATE!)
❌ main.cpp: uploadFreq = 15000000ULL                  (HARDCODED!)
```

### After Consolidation
```
✅ system_config.h: DEFAULT_POLL_FREQUENCY_US = 5000000
✅ test_config.h: #include "application/system_config.h"  (imports)
✅ task_manager.cpp: pollFrequency = DEFAULT_POLL_FREQUENCY_US / 1000
✅ main.cpp: uint64_t pollFreq = nvs::getPollFreq()     (from NVS)
```

---

## Configuration Validation

### Compile-Time Validation (Macros)
```cpp
#define IS_VALID_POLL_FREQ(f)    ((f) >= MIN_POLL_FREQUENCY_US && (f) <= MAX_POLL_FREQUENCY_US)
#define IS_VALID_UPLOAD_FREQ(f)  ((f) >= MIN_UPLOAD_FREQUENCY_US && (f) <= MAX_UPLOAD_FREQUENCY_US)
#define IS_VALID_REGISTER_COUNT(c) ((c) >= MIN_REGISTER_COUNT && (c) <= MAX_REGISTER_COUNT)
```

### Runtime Validation (Flask Server)
- Poll frequency: 1 second to 1 hour
- Upload frequency: 10 seconds to 1 hour
- Register count: 3 to 10 registers

---

## Migration from Legacy

### main_legacy.cpp.bak
- ✅ PowerManagement::init() → Moved to SystemInitializer
- ✅ PeripheralPower::init() → Moved to SystemInitializer
- ✅ SecurityLayer::init() → Moved to SystemInitializer
- ✅ OTA diagnostics → Present in current main.cpp
- ✅ Device registration → Present in current main.cpp

### main_timer_legacy.cpp.bak
- ✅ Hardware timers → Replaced by FreeRTOS tasks
- ✅ Token-based scheduling → Replaced by task scheduler
- ✅ Batch size update → Now dynamic calculation
- ✅ Configuration changes → Handled by ConfigManager

---

## Benefits of Centralized Configuration

1. **Single Source of Truth**: Change once, apply everywhere
2. **No Duplicates**: Eliminated redundant definitions
3. **Type Safety**: Macros with validation
4. **Easy Testing**: Override test values without touching production
5. **Maintainability**: All constants in one place
6. **Documentation**: Clear hierarchy and purpose
7. **Dynamic Calculation**: Batch size adapts to timing changes

---

## Future Improvements

1. **Add configuration versioning** to detect NVS/server mismatches
2. **Add range checking** at runtime for all configurable parameters
3. **Add configuration dump command** to print all active settings
4. **Add configuration backup/restore** to NVS for failsafe
5. **Add telemetry** to track configuration change history

---

## Quick Reference

| Parameter | File | Default | Range | Dynamic? |
|-----------|------|---------|-------|----------|
| Poll Frequency | system_config.h | 5s | 1s - 1h | ✅ Yes |
| Upload Frequency | system_config.h | 15s | 10s - 1h | ✅ Yes |
| Config Check Freq | system_config.h | 5s | 1s - 5min | ❌ No |
| OTA Check Freq | system_config.h | 60s | 30s - 24h | ❌ No |
| Batch Size | (calculated) | 3 | 1 - 20 | ✅ Yes |
| Register Count | NVS | 10 | 3 - 10 | ✅ Yes |
| Watchdog Timeout | system_config.h | 600s | N/A | ❌ No |
| Max Idle Time | system_config.h | 120s | N/A | ❌ No |
| WiFi SSID | credentials.h | (set) | N/A | ❌ No |
| Device ID | credentials.h | ESP32_001 | N/A | ❌ No |

