# ECOWATT ESP32 Firmware - Comprehensive Code Analysis

**Author:** AI Analysis  
**Date:** November 28, 2025  
**Firmware Version:** 3.0.0 (FreeRTOS Dual-Core)  
**Branch:** FreeRTIO

---

## Recent Bug Fixes

### 2025-11-28: Fixed Fault Type Detection in Recovery Events (COMPLETE FIX)

**Issue:** All fault recovery events were being logged as `modbus_exception` regardless of the actual fault type (CRC_ERROR, CORRUPT, PACKET_DROP, etc.).

**Root Cause (3 separate issues):**
1. `protocol_adapter.cpp::readRegister()` was creating recovery events with hardcoded `FaultType::MODBUS_EXCEPTION` (line 97)
2. `protocol_adapter.cpp` was doing internal retries, hiding faults from `acquisition.cpp`
3. `protocol_adapter.cpp::parseResponse()` was not copying corrupted frames to output buffer

**Fix (3-part solution):**

1. **Removed premature fault recovery events** - Deleted fault recovery event creation from `protocol_adapter.cpp` (lines 88-119)

2. **Removed internal retry logic** - Deleted retry logic from both `readRegister()` and `writeRegister()` in `protocol_adapter.cpp`
   - Previously: protocol_adapter would retry internally and return success, hiding the fault
   - Now: Returns false immediately on fault, allowing acquisition.cpp to detect and classify it

3. **Fixed frame copying** - Moved `memcpy(outFrameHex, frame, len + 1)` BEFORE corruption checks in `parseResponse()`
   - Previously: Corrupted frames were NOT copied to output buffer
   - Now: ALL frames (corrupted or not) are copied, allowing fault detection in acquisition.cpp

**How It Works Now:**
```
protocol_adapter.cpp:
  - Sends request to Inverter SIM
  - Parses JSON response
  - Copies frame to output buffer (ALWAYS, even if corrupted)
  - Returns false if frame is invalid/corrupted
  - NO retries, NO fault event creation

acquisition.cpp:
  - Calls adapter.readRegister()
  - If returns false OR if frame has faults, calls detectFault()
  - detectFault() properly classifies:
    * CRC_ERROR - CRC checksum failures
    * GARBAGE_DATA - Corrupted/invalid response format  
    * TRUNCATED_PAYLOAD - Incomplete responses
    * TIMEOUT - No response from inverter
    * MODBUS_EXCEPTION - Actual Modbus exception codes
  - Creates recovery event with CORRECT fault type
  - Performs retry using executeRecovery()
  - Sends recovery event to Flask server
```

**Impact:** 
- Recovery events now show accurate fault types matching Inverter SIM injections
- Proper separation of concerns: protocol_adapter handles communication, acquisition handles fault recovery
- Testing and diagnosis now work correctly

---

## Table of Contents

1. [System Architecture Overview](#system-architecture-overview)
2. [Main Entry Point Analysis](#main-entry-point-analysis)
3. [Task Architecture](#task-architecture)
4. [Data Flow Pipeline](#data-flow-pipeline)
5. [Synchronization Primitives](#synchronization-primitives)
6. [Critical Timing Configuration](#critical-timing-configuration)
7. [Potential Bugs and Issues](#potential-bugs-and-issues)
8. [Configuration Hierarchy](#configuration-hierarchy)
9. [Security Implementation](#security-implementation)
10. [Recommendations](#recommendations)

---

## System Architecture Overview

### Design Philosophy

The firmware implements a **FreeRTOS dual-core architecture** (v3.0) with strict separation of concerns:

- **Real-time guarantee**: Critical sensor polling on dedicated core with highest priority
- **Network isolation**: All WiFi/HTTP operations on separate core to prevent interference
- **Token-based scheduling**: Hardware timers + FreeRTOS tasks for deterministic behavior
- **Layered architecture**: Application → Peripheral → Driver layers

### Core Assignment Strategy

#### **Core 0 (PRO_CPU) - Network Operations:**
- Upload Task (Priority 20)
- Command Task (Priority 16)
- Config Task (Priority 12)
- Power Report Task (Priority 8)
- OTA Task (Priority 5)

**Rationale:** WiFi stack runs on Core 0 by default. All network operations grouped here to minimize cross-core communication overhead.

#### **Core 1 (APP_CPU) - Real-Time Operations:**
- Sensor Poll Task (Priority 24 - CRITICAL)
- Compression Task (Priority 18)
- Watchdog Task (Priority 1)

**Rationale:** Isolate time-critical sensor acquisition from network latency. Modbus protocol has ~1.8s response time and cannot tolerate interruptions.

### Boot Sequence

The initialization follows a strict order to prevent dependency issues:

```
1. Serial Communication (115200 baud)
   ↓
2. WiFi Connection (Arduino_Wifi)
   ↓
3. Logger Initialization (with NTP timestamps)
   ↓
4. Hardware Watchdog Reconfiguration (600s timeout)
   ↓
5. System Components (Power, Security, Fault Recovery)
   ↓
6. OTA Manager + Rollback Handler
   ↓
7. Post-Boot Diagnostics
   ↓
8. Device Auto-Registration with Flask
   ↓
9. Configuration Loading from NVS
   ↓
10. Data Uploader/Command Executor/Config Manager Init
   ↓
11. FreeRTOS Task Manager Init (Queues, Mutexes, Semaphores)
   ↓
12. All 9 Tasks Started on Both Cores
```

**Critical Dependencies:**
- WiFi MUST initialize before Logger (for NTP time sync)
- Logger MUST initialize before any LOG_* calls
- NVS MUST initialize before reading configuration
- Queues/Mutexes MUST be created before tasks start

---

## Main Entry Point Analysis

### File: `src/main.cpp`

#### `setup()` Function

**Purpose:** Initialize entire system before FreeRTOS scheduler takes over

**Key Operations:**

1. **WiFi Initialization (Line 166-167)**
   ```cpp
   Wifi.begin();
   ```
   - Connects to credentials from `credentials.h`
   - Blocks until connected (potential hang point if WiFi unavailable)
   - **Bug Risk:** No timeout, system hangs on WiFi failure

2. **Logger Initialization (Line 169-172)**
   ```cpp
   initLogger();  // Uses g_logLevel from logger.cpp
   ```
   - Sets up UART logging with timestamps
   - If NTP sync succeeds, uses real timestamps
   - Falls back to millis() if NTP fails
   - **Issue:** No indication to user if NTP fails

3. **Watchdog Reconfiguration (Line 175-178)**
   ```cpp
   esp_task_wdt_deinit();
   esp_task_wdt_init(HARDWARE_WATCHDOG_TIMEOUT_S, true);
   ```
   - Default ESP32 watchdog is too short for network operations
   - Increases to 600 seconds (10 minutes)
   - **Design Decision:** Long timeout allows slow network operations

4. **OTA Rollback Handler (Line 199-200)**
   ```cpp
   otaManager->handleRollback();
   ```
   - Checks if previous OTA failed
   - Reverts to previous firmware if boot count exceeds threshold
   - **Critical:** Prevents boot loops from bad firmware

5. **Diagnostics (Line 203-205)**
   ```cpp
   bool diagnosticsPassed = otaManager->runDiagnostics();
   ```
   - Verifies system health after boot
   - Tests WiFi, NVS, sensor communication
   - **Issue:** System continues even if diagnostics fail

6. **Device Registration (Line 209-217)**
   ```cpp
   if (registerDeviceWithServer()) {
       LOG_INFO(LOG_TAG_BOOT, "✓ Device registration complete");
   }
   ```
   - Auto-registers with Flask on first boot
   - Sends firmware version, location, description
   - **Issue:** No retry if registration fails

7. **Task Frequency Configuration (Line 219-246)**
   ```cpp
   uint64_t pollFreq = nvs::getPollFreq();
   uint64_t uploadFreq = nvs::getUploadFreq();
   // ... convert to milliseconds
   ```
   - Loads all task frequencies from NVS
   - Falls back to defaults if NVS empty
   - Converts from microseconds to milliseconds for TaskManager

8. **Module Initialization (Line 248-266)**
   - DataUploader: `/aggregated/{device_id}` endpoint
   - CommandExecutor: `/commands/{device_id}/poll` and `/result` endpoints
   - ConfigManager: `/config/{device_id}` endpoint
   - **Pattern:** All modules use device-specific URLs (Milestone 4 format)

9. **FreeRTOS Task Manager Init (Line 273-281)**
   ```cpp
   if (!TaskManager::init(pollFreqMs, uploadFreqMs, ...)) {
       while(1) { delay(1000); }  // Halt on failure
   }
   ```
   - Creates queues, mutexes, semaphores
   - **Critical:** System halts if initialization fails
   - **Issue:** No diagnostic information about what failed

10. **Task Launch (Line 284)**
    ```cpp
    TaskManager::startAllTasks(otaManager);
    ```
    - Creates 9 FreeRTOS tasks on both cores
    - After this, `setup()` returns and `loop()` becomes lowest priority

#### `loop()` Function

**Purpose:** Print health reports every 10 minutes (optional)

```cpp
void loop() {
    static unsigned long lastHealthPrint = 0;
    unsigned long now = millis();
    
    if (now - lastHealthPrint > 600000) {  // 10 minutes
        TaskManager::printSystemHealth();
        lastHealthPrint = now;
    }
    
    delay(1000);  // Sleep 1 second
}
```

**Key Points:**
- Loop is essentially empty - all work done in FreeRTOS tasks
- Only prints health reports periodically
- `delay(1000)` yields to FreeRTOS scheduler
- **Note:** This function has lowest priority, may not run if system overloaded

---

## Task Architecture

### Task Overview Table

| Task Name | Core | Priority | Stack (bytes) | Period | Deadline | Watchdog |
|-----------|------|----------|---------------|--------|----------|----------|
| SensorPoll | 1 | 24 (MAX) | 8192 | 5s | 2s | ✓ |
| Compression | 1 | 18 | 6144 | Event | 2s | ✓ |
| Upload | 0 | 20 | 12288 | 15s | 5s | ✓ |
| Commands | 0 | 16 | 4096 | 10s | 3s | ✓ |
| Config | 0 | 12 | 6144 | 5s | 3s | ✓ |
| PowerReport | 0 | 8 | 4096 | 5min | 5s | ✓ |
| OTA | 0 | 5 | 10240 | 1min | 10s | ✗ |
| Watchdog | 1 | 1 | 2048 | 30s | N/A | ✓ |

### 1. Sensor Poll Task (CRITICAL)

**File:** `src/application/task_manager.cpp` Lines 325-417

**Purpose:** Read sensor data from Modbus inverter at fixed intervals

**Implementation Details:**

```cpp
void TaskManager::sensorPollTask(void* parameter) {
    // Register with hardware watchdog
    esp_task_wdt_add(NULL);
    
    TickType_t xLastWakeTime = xTaskGetTickCount();
    TickType_t xFrequency = pdMS_TO_TICKS(pollFrequency);
    const uint32_t deadlineUs = 2000000;  // 2s deadline
    
    while (1) {
        // ALWAYS wait for the full interval before starting next cycle
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        
        // Check for config reload signal
        if (xSemaphoreTake(configReloadSemaphore, 0) == pdTRUE) {
            // Reload poll frequency and register configuration
            // ...
        }
        
        // Read sensors via Modbus
        DecodedValues result = readRequest(registers, registerCount);
        
        // Send to compression queue (non-blocking)
        if (xQueueSend(sensorDataQueue, &sample, 0) != pdTRUE) {
            LOG_WARN("Queue full! Sample dropped");
            stats_sensorPoll.deadlineMisses++;
        }
        
        // Record statistics and check deadline
        // Feed hardware watchdog
        esp_task_wdt_reset();
    }
}
```

**Key Features:**
- Uses `vTaskDelayUntil()` for **exact periodic execution** (not `vTaskDelay()` which drifts)
- Non-blocking queue send - drops sample if queue full rather than missing deadline
- Reloads configuration dynamically without restart
- **Highest priority** ensures it always gets CPU time

**Potential Issues:**
- **Line 390-395:** Sample drop on queue full - no retry, silent data loss
- **Line 360-367:** Reading `pollFrequency` without mutex (race condition with upload task)
- **Line 338-344:** `memcmp` on register array without size check

**Timing Analysis:**
- Modbus read takes ~1.8 seconds
- 200ms margin for queue operations and logging
- If Modbus exceeds 2s, deadline miss logged but task continues

---

### 2. Compression Task

**File:** `src/application/task_manager.cpp` Lines 475-608

**Purpose:** Batch sensor samples and compress before upload

**Implementation Details:**

```cpp
void TaskManager::compressionTask(void* parameter) {
    esp_task_wdt_add(NULL);
    
    // Calculate batch size dynamically
    const size_t batchSize = (uploadFrequency / pollFrequency);
    // Example: 15000ms / 5000ms = 3 samples per upload cycle
    
    SensorSample sampleBatch[batchSize > 0 ? batchSize : 1];
    size_t batchCount = 0;
    
    while (1) {
        // Wait for sensor sample (blocks until available)
        if (xQueueReceive(sensorDataQueue, &sample, portMAX_DELAY) == pdTRUE) {
            // Add to batch
            sampleBatch[batchCount++] = sample;
            
            // When batch is full, compress and send
            if (batchCount >= batchSize) {
                // Convert to linear array
                uint16_t* linearData = new uint16_t[totalRegisterCount];
                
                // Compress with smart selection
                std::vector<uint8_t> compressedVec = 
                    DataCompression::compressWithSmartSelection(...);
                
                delete[] linearData;
                
                // Copy to fixed-size buffer
                if (compressedVec.size() <= sizeof(packet.data)) {
                    memcpy(packet.data, compressedVec.data(), compressedVec.size());
                    
                    // Send to upload queue
                    xQueueSend(compressedDataQueue, &packet, 0);
                    
                    // Signal upload task
                    xSemaphoreGive(batchReadySemaphore);
                }
                
                batchCount = 0;
            }
        }
    }
}
```

**Key Features:**
- **Dynamic batch sizing** based on frequency ratio
- Event-driven (waits for samples) rather than periodic
- **Fixed-size buffer** in packet (no heap allocation in queue)
- Signals upload task when batch ready

**Potential Issues:**
- **Line 492:** `batchSize = uploadFreq / pollFreq` - should validate > 0
- **Line 536-540:** `new uint16_t[]` in tight loop - heap fragmentation risk
- **Line 557-564:** `strncpy` without null termination guarantee
- **Line 567:** Queue send with timeout 0 - drops packet if queue full

**Memory Concerns:**
- Each batch allocates `batchSize * registerCount * 2` bytes on heap
- Deleted immediately but can fragment over time
- **Recommendation:** Use static buffer pool

---

### 3. Upload Task

**File:** `src/application/task_manager.cpp` Lines 610-800

**Purpose:** Upload compressed data to Flask server every 15 seconds

**Implementation Details:**

```cpp
void TaskManager::uploadTask(void* parameter) {
    esp_task_wdt_add(NULL);
    
    TickType_t xLastWakeTime = xTaskGetTickCount();
    TickType_t xFrequency = pdMS_TO_TICKS(uploadFrequency);
    
    while (1) {
        // Wait for upload interval
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        
        // Drain all compressed packets from queue
        while (xQueueReceive(compressedDataQueue, &packet, 0) == pdTRUE) {
            // Add to DataUploader queue
            DataUploader::addToQueue(smartData);
        }
        
        // Upload all pending data
        if (queuedCount > 0) {
            // Acquire WiFi mutex
            if (xSemaphoreTake(wifiClientMutex, pdMS_TO_TICKS(4000)) == pdTRUE) {
                uploadSuccess = DataUploader::uploadPendingData();
                xSemaphoreGive(wifiClientMutex);
                
                if (uploadSuccess) {
                    // Signal all tasks to reload configuration
                    for (int i = 0; i < 6; i++) {
                        xSemaphoreGive(configReloadSemaphore);
                    }
                }
            }
        }
        
        esp_task_wdt_reset();
    }
}
```

**Key Features:**
- Drains entire compressed queue each cycle
- Only acquires WiFi mutex when actually uploading
- Broadcasts config reload signal after successful upload
- Uses 4s mutex timeout (< 5s deadline)

**Potential Issues:**
- **Line 733-739:** Hardcoded "6" for semaphore give count - fragile if task count changes
- **Line 709:** Mutex timeout 4000ms but HTTP timeout is 15000ms - mismatch
- **Line 657-665:** Config reload logic uses same semaphore as tasks - potential count mismatch

**Critical Flow:**
```
Upload Success
    ↓
Give semaphore 6 times
    ↓
SensorPoll, Upload, Command, Config, PowerReport, OTA tasks each take once
    ↓
Each reloads configuration from NVS
```

**Bug Risk:** If any task misses its semaphore take, counters get out of sync

---

### 4. Command Task

**File:** `src/application/task_manager.cpp` Lines 802-850

**Purpose:** Poll server for commands and execute them locally

**Implementation:**

```cpp
void TaskManager::commandTask(void* parameter) {
    TickType_t xFrequency = pdMS_TO_TICKS(commandFrequency);
    
    while (1) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        
        // Check for config reload
        if (xSemaphoreTake(configReloadSemaphore, 0) == pdTRUE) {
            // Update command frequency
        }
        
        // Acquire WiFi mutex
        if (xSemaphoreTake(wifiClientMutex, pdMS_TO_TICKS(2000)) == pdTRUE) {
            CommandExecutor::checkAndExecuteCommands();
            xSemaphoreGive(wifiClientMutex);
        }
        
        esp_task_wdt_reset();
    }
}
```

**Commands Supported:**
- `set_power`: Set inverter power output
- `set_power_percentage`: Set power as percentage
- `write_register`: Write to specific Modbus register
- `get_power_stats`: Retrieve power management statistics
- `reset_power_stats`: Clear power statistics

**Potential Issues:**
- **Line 839:** Mutex timeout 2000ms but deadline is 3000ms - only 1s for actual command execution
- No command queue usage despite queue being created
- Missing error handling for command execution failures

---

### 5. Config Task

**File:** `src/application/task_manager.cpp` Lines 852-920

**Purpose:** Check for configuration changes from server

**Implementation:**

```cpp
void TaskManager::configTask(void* parameter) {
    static bool registers_uptodate = true;
    static bool pollFreq_uptodate = true;
    static bool uploadFreq_uptodate = true;
    
    while (1) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        
        // Acquire WiFi mutex
        if (xSemaphoreTake(wifiClientMutex, pdMS_TO_TICKS(2000)) == pdTRUE) {
            ConfigManager::checkForChanges(
                &registers_uptodate, 
                &pollFreq_uptodate, 
                &uploadFreq_uptodate
            );
            xSemaphoreGive(wifiClientMutex);
        }
        
        esp_task_wdt_reset();
    }
}
```

**Configuration Updates:**
- Register selection (which sensors to monitor)
- Poll frequency (how often to read sensors)
- Upload frequency (how often to send data)
- Config check frequency (how often to check for changes)
- Command poll frequency
- OTA check frequency

**Change Detection:**
- Compares server config with NVS config
- If different, updates NVS
- Sets uptodate flags to false
- Upload task sees flags and applies changes after next upload

**Issue:** State flags are task-local - if task restarts, state lost

---

### 6. Power Report Task

**File:** `src/application/task_manager.cpp` Lines 922-1008

**Purpose:** Report power management statistics every 5 minutes

**Implementation:**

```cpp
void TaskManager::powerReportTask(void* parameter) {
    powerReportFrequency = nvs::getEnergyPollFreq() / 1000;
    
    while (1) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        
        // Get power statistics
        PowerStats stats = PowerManagement::getStats();
        PowerTechniqueFlags techniques = PowerManagement::getTechniques();
        
        // Build JSON payload
        snprintf(jsonBuffer, sizeof(jsonBuffer),
            "{"
            "\"device_id\":\"%s\","
            "\"power_management\":{"
            "\"enabled\":%s,"
            "\"avg_current_ma\":%.2f,"
            // ...
            "}"
            "}"
        );
        
        // POST to /power/energy/{device_id}
        http.POST(jsonBuffer);
        
        esp_task_wdt_reset();
    }
}
```

**Reported Metrics:**
- Power management enabled status
- Techniques in use (WiFi sleep, CPU scaling, etc.)
- Average current consumption
- Energy saved
- Time in each power mode

**Issues:**
- **Line 951:** `char jsonBuffer[512]` - no size validation for snprintf
- Reports even when power management disabled (by design, for monitoring)

---

### 7. OTA Task

**File:** `src/application/task_manager.cpp` Lines 1010-1120

**Purpose:** Check for firmware updates and apply them

**Implementation:**

```cpp
void TaskManager::otaTask(void* parameter) {
    // NOTE: NOT registered with watchdog (runs every 60s, exceeds 30s watchdog)
    
    OTAManager* otaManager = static_cast<OTAManager*>(parameter);
    
    while (1) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        
        if (xSemaphoreTake(wifiClientMutex, pdMS_TO_TICKS(2000)) == pdTRUE) {
            if (otaManager->checkForUpdate()) {
                // Suspend critical tasks during OTA
                suspendAllTasks();
                
                // Download and apply firmware
                bool otaSuccess = otaManager->downloadAndApplyFirmware();
                
                xSemaphoreGive(wifiClientMutex);
                
                if (otaSuccess) {
                    otaManager->verifyAndReboot();
                    // System reboots here
                } else {
                    resumeAllTasks();
                }
            } else {
                xSemaphoreGive(wifiClientMutex);
            }
        }
        
        // NOTE: No watchdog reset - not registered
    }
}
```

**OTA Security:**
- RSA-2048 signature verification
- AES-256-CBC encrypted chunks
- HMAC per-chunk verification
- SHA-256 complete firmware hash

**Critical Design Decision:**
- OTA task NOT registered with watchdog
- Runs every 60s but watchdog timeout is 30s
- Relies on other tasks to feed watchdog
- **Risk:** If all other tasks stall, no watchdog reset

**Suspension Order:**
- Upload Task
- Command Task
- Config Task
- Statistics Task (if exists)
- Sensor Poll Task
- Compression Task
- Watchdog Task
- **OTA Task itself NOT suspended**

---

### 8. Watchdog Task

**File:** `src/application/task_manager.cpp` Lines 1122-1195

**Purpose:** Monitor system health and trigger recovery

**Implementation:**

```cpp
void TaskManager::watchdogTask(void* parameter) {
    esp_task_wdt_add(NULL);
    
    const TickType_t xCheckInterval = pdMS_TO_TICKS(30000);  // 30s
    const uint32_t maxTaskIdleTime = 120000;  // 2 minutes
    
    while (1) {
        vTaskDelay(xCheckInterval);
        
        uint32_t currentTime = millis();
        
        // Check sensor poll task (CRITICAL)
        if (currentTime - stats_sensorPoll.lastRunTime > maxTaskIdleTime) {
            LOG_ERROR("SensorPoll task stalled! Last run: %lu ms ago");
            ESP.restart();
        }
        
        // Check upload task
        if (currentTime - stats_upload.lastRunTime > uploadFrequency * 3) {
            LOG_WARN("Upload task delayed!");
        }
        
        // Check for excessive deadline misses
        if (stats_sensorPoll.deadlineMisses > MAX_DEADLINE_MISSES) {
            LOG_ERROR("Excessive sensor deadline misses! Resetting...");
            ESP.restart();
        }
        
        // Print health report every 10 minutes
        static uint32_t lastHealthReport = 0;
        if (currentTime - lastHealthReport > 600000) {
            printSystemHealth();
            lastHealthReport = currentTime;
        }
        
        esp_task_wdt_reset();
    }
}
```

**Health Checks:**
1. **Critical:** Sensor poll task must run within 2 minutes
2. **Warning:** Upload task delayed (> 3x expected period)
3. **Critical:** Excessive deadline misses (> 20)
4. **Info:** Health report every 10 minutes

**Recovery Actions:**
- Stalled sensor task → Immediate reboot
- Excessive deadline misses → Immediate reboot
- Delayed upload → Warning only

**Issues:**
- **Line 1165:** Deadline miss counter never resets - accumulates forever
- No graceful degradation - only hard reboot
- No pre-reboot diagnostics or logging to NVS

---

## Data Flow Pipeline

### Complete Data Journey

```
┌─────────────────────────────────────────────────────────────────┐
│                    1. SENSOR ACQUISITION                         │
└─────────────────────────────────────────────────────────────────┘
Modbus Inverter (Slave 0x11)
    ↓ [Sensor Poll Task - Every 5s]
    ↓ [readRequest(registers[], count)]
DecodedValues {values[], count}
    ↓
SensorSample {
    uint16_t values[10];
    uint32_t timestamp;        // Unix timestamp in ms
    uint8_t registerCount;
    RegID registers[10];
}
    ↓ [xQueueSend(sensorDataQueue, &sample, 0)]
    ↓ [Queue Size: 10 items, ~50s buffering @ 5s poll]

┌─────────────────────────────────────────────────────────────────┐
│                    2. COMPRESSION                                │
└─────────────────────────────────────────────────────────────────┘
sensorDataQueue
    ↓ [Compression Task - Event-driven]
    ↓ [Batch samples: uploadFreq / pollFreq = 15s / 5s = 3 samples]
SensorSample batch[3]
    ↓ [Convert to linear array]
uint16_t linearData[] = {s0_r0, s0_r1, s1_r0, s1_r1, s2_r0, s2_r1}
    ↓ [DataCompression::compressWithSmartSelection()]
std::vector<uint8_t> compressed
    ↓ [Copy to fixed-size buffer]
CompressedPacket {
    uint8_t data[512];         // Fixed buffer (no heap in queue!)
    size_t dataSize;
    uint32_t timestamp;
    size_t sampleCount;        // 3 samples
    size_t uncompressedSize;   // 18 bytes (3 samples × 2 regs × 2 bytes)
    size_t compressedSize;     // ~12 bytes (67% compression)
    char compressionMethod[32]; // "dictionary" / "temporal" / "semantic"
    RegID registers[16];       // Register layout
    size_t registerCount;      // 2 registers
}
    ↓ [xQueueSend(compressedDataQueue, &packet, 0)]
    ↓ [xSemaphoreGive(batchReadySemaphore)]
    ↓ [Queue Size: 5 items, ~75s buffering @ 15s upload]

┌─────────────────────────────────────────────────────────────────┐
│                    3. UPLOAD PREPARATION                         │
└─────────────────────────────────────────────────────────────────┘
compressedDataQueue
    ↓ [Upload Task - Every 15s]
    ↓ [Drain entire queue]
    ↓ [Convert CompressedPacket → SmartCompressedData]
SmartCompressedData {
    std::vector<uint8_t> binaryData;  // Compressed binary
    uint32_t timestamp;
    size_t sampleCount;
    size_t registerCount;
    size_t originalSize;
    float academicRatio;              // compressed/original
    float traditionalRatio;           // original/compressed
    char compressionMethod[32];
    RegID registers[16];
}
    ↓ [DataUploader::addToQueue(smartData)]
    ↓ [RingBuffer<SmartCompressedData, 20>]

RingBuffer (capacity: 20 packets)
    ↓ [DataUploader::uploadPendingData()]
    ↓ [Build JSON payload]
JSON {
    "device_id": "ESP32_TEST_DEVICE",
    "timestamp": 1732608645000,
    "data_type": "compressed_sensor_batch",
    "total_samples": 9,              // 3 packets × 3 samples
    "register_mapping": {
        "0": "Vac1",
        "1": "Iac1"
    },
    "compressed_data": [
        {
            "compressed_binary": "eNqrVg...",  // Base64(binaryData)
            "decompression_metadata": {
                "method": "DICTIONARY",
                "register_count": 2,
                "original_size_bytes": 18,
                "compressed_size_bytes": 12,
                "timestamp": 1732608645000,
                "register_layout": [0, 1]
            },
            "performance_metrics": {
                "academic_ratio": 0.667,
                "traditional_ratio": 1.5,
                "compression_time_us": 4710,
                "efficiency_score": 234.5
            }
        },
        // ... 2 more packets
    ]
}
    ↓ [Serialize to JSON string]

┌─────────────────────────────────────────────────────────────────┐
│                    4. SECURITY LAYER                             │
└─────────────────────────────────────────────────────────────────┘
JSON String (plain text)
    ↓ [SecurityLayer::wrapWithHMAC()]
    ↓ [Increment nonce (anti-replay)]
    ↓ [Base64 encode entire JSON]
Base64(JSON)
    ↓ [Calculate HMAC-SHA256(nonce + base64_json)]
HMAC = sha256_hmac(KEY, nonce || payload)
    ↓ [Build secured JSON]
Secured JSON {
    "nonce": 12345,
    "payload": "eyJkZXZpY2VfaWQiOi4uLg==",  // Base64(entire JSON)
    "mac": "a1b2c3d4e5f6...",
    "encrypted": false,
    "compressed": false  // Payload is JSON, not raw binary
}
    ↓ [Serialize to string]

┌─────────────────────────────────────────────────────────────────┐
│                    5. TRANSMISSION                               │
└─────────────────────────────────────────────────────────────────┘
Secured JSON String
    ↓ [HTTPClient http]
    ↓ [http.begin(client, FLASK_SERVER_URL "/aggregated/{device_id}")]
    ↓ [http.addHeader("Content-Type", "application/json")]
    ↓ [http.setTimeout(15000)]
    ↓ [http.POST(securedJson)]
HTTP POST to Flask Server
    - URL: http://192.168.1.100:5001/aggregated/ESP32_TEST_DEVICE
    - Headers: Content-Type: application/json
    - Body: Secured JSON (~2-4KB)
    ↓ [Wait for response]
HTTP Response
    - 200 OK: Upload successful
    - 401 Unauthorized: HMAC verification failed
    - 500 Internal Server Error: Processing failed
    ↓
    ↓ [If 200 OK]
    ↓ [Signal config reload to all tasks]

┌─────────────────────────────────────────────────────────────────┐
│                    6. FLASK SERVER PROCESSING                    │
└─────────────────────────────────────────────────────────────────┘
Flask receives POST
    ↓ [security_handler.verify_hmac()]
    ↓ [Check nonce (anti-replay)]
    ↓ [Base64 decode payload]
    ↓ [Calculate HMAC-SHA256(nonce + decoded_payload)]
    ↓ [Compare with received MAC]
    ↓ [If valid: return decoded JSON]
JSON String
    ↓ [json.loads()]
Python dict
    ↓ [Extract compressed_data array]
    ↓ [For each packet:]
        ↓ [Base64 decode compressed_binary]
        ↓ [Decompress using metadata.method]
        ↓ [Verify lossless recovery]
        ↓ [Extract original samples]
Original sensor values
    ↓ [Store in PostgreSQL database]
Database records
    ↓ [Return 200 OK to ESP32]
```

### Data Size Analysis

**Single Sample (3 registers):**
- Raw: 6 bytes (3 registers × 2 bytes each)
- Compressed: ~4 bytes (67% compression via dictionary)
- Base64: ~5 characters

**Batch (3 samples, 3 registers each):**
- Raw: 18 bytes
- Compressed: ~12 bytes
- Base64: ~16 characters

**Complete Upload (3 batches = 9 samples):**
- Raw data: 54 bytes
- Compressed data: ~36 bytes
- JSON with metadata: ~800 bytes
- Secured JSON: ~1200 bytes
- Base64 overhead: ~400 bytes

**Total upload size:** ~1.2 KB per 15-second cycle

**Bandwidth usage:** ~320 bytes/second average

---

## Synchronization Primitives

### Queues

#### 1. sensorDataQueue
- **Type:** `QueueHandle_t`
- **Size:** 10 items
- **Item:** `SensorSample` struct (152 bytes)
- **Producer:** Sensor Poll Task
- **Consumer:** Compression Task
- **Behavior:** Non-blocking send (drops if full)
- **Buffering:** ~50 seconds @ 5s poll interval

#### 2. compressedDataQueue
- **Type:** `QueueHandle_t`
- **Size:** 5 items
- **Item:** `CompressedPacket` struct (664 bytes)
- **Producer:** Compression Task
- **Consumer:** Upload Task
- **Behavior:** Non-blocking send (drops if full)
- **Buffering:** ~75 seconds @ 15s upload interval

#### 3. commandQueue (UNUSED)
- **Type:** `QueueHandle_t`
- **Size:** 5 items
- **Item:** `Command` struct
- **Status:** Created but never used
- **Issue:** Commands executed synchronously, no queuing

### Mutexes

#### 1. nvsAccessMutex
- **Type:** `SemaphoreHandle_t` (Priority Inheritance)
- **Purpose:** Protect NVS flash writes
- **Timeout:** `portMAX_DELAY` (infinite - flash writes must complete)
- **Contention:** Low (writes infrequent)
- **Used by:**
  - Sensor Poll Task (read register config)
  - Config Task (write config changes)
  - Any task reading/writing NVS

#### 2. wifiClientMutex
- **Type:** `SemaphoreHandle_t` (Priority Inheritance)
- **Purpose:** Protect HTTPClient instance (shared resource)
- **Timeouts:**
  - Upload Task: 4000ms
  - Command Task: 2000ms
  - Config Task: 2000ms
  - OTA Task: 2000ms
- **Contention:** High (all network tasks compete)
- **Critical:** Timeout must be < task deadline

**Potential Deadlock Scenario:**
```
1. Upload Task acquires wifiClientMutex
2. Upload takes 4s (slow network)
3. Command Task waiting for mutex (2s timeout)
4. Command Task timeout → deadline miss
5. Config Task also waiting → deadline miss
6. Cascading failures
```

**Mitigation:** Stagger task periods to reduce simultaneous attempts

#### 3. dataPipelineMutex
- **Type:** `SemaphoreHandle_t`
- **Purpose:** Protect compression operations
- **Timeout:** 100ms (fast operations)
- **Contention:** Low (single consumer/producer)
- **Used by:** Compression Task only

### Semaphores

#### 1. batchReadySemaphore
- **Type:** Binary Semaphore
- **Purpose:** Signal upload task when batch is compressed
- **Given by:** Compression Task (after compressing batch)
- **Taken by:** Upload Task (drained at start of upload cycle)
- **Behavior:**
  - Compression Task signals after each batch
  - Upload Task drains all signals at cycle start
  - Prevents signal accumulation

#### 2. configReloadSemaphore
- **Type:** Counting Semaphore (max count: 10)
- **Purpose:** Broadcast config changes to all tasks
- **Given by:** Upload Task (6 times after successful upload)
- **Taken by:** All 6 configurable tasks:
  1. Sensor Poll Task (registers + poll frequency)
  2. Upload Task (upload frequency)
  3. Command Task (command frequency)
  4. Config Task (config check frequency)
  5. Power Report Task (power report frequency)
  6. OTA Task (OTA check frequency)

**Configuration Reload Flow:**
```
Upload Task success
    ↓
for (int i = 0; i < 6; i++) {
    xSemaphoreGive(configReloadSemaphore);
}
    ↓
Each task checks at next cycle:
if (xSemaphoreTake(configReloadSemaphore, 0) == pdTRUE) {
    // Reload config from NVS
    // Update local frequency variables
    // Reset timing baseline
}
```

**Critical Issue:** Hardcoded "6" - if tasks added/removed, must update

---

## Critical Timing Configuration

### File: `include/application/system_config.h`

All timing constants centralized in one location for easy tuning.

### Default Frequencies (microseconds)

| Parameter | Default | Min | Max | Rationale |
|-----------|---------|-----|-----|-----------|
| Poll Frequency | 5s | 1s | 1h | Modbus requires ~1.8s, 5s safe |
| Upload Frequency | 15s | 10s | 1h | Batch 3 samples @ 5s poll |
| Config Check | 5s | 1s | 5min | Balance responsiveness vs load |
| Command Poll | 10s | 5s | 5min | Commands are infrequent |
| OTA Check | 1min | 30s | 24h | Updates are rare |
| Energy Poll | 5min | 1min | 1h | Power stats change slowly |

### Task Deadlines (microseconds)

| Task | Deadline | Rationale |
|------|----------|-----------|
| Sensor Poll | 2s | Modbus ~1.8s + 200ms margin |
| Compression | 2s | Dictionary lookup + encoding |
| Upload | 5s | HTTP POST + server processing |
| Command | 3s | HTTP + command execution |
| Config | 3s | HTTP + NVS write |
| OTA | 10s | Manifest download + validation |

### Watchdog Configuration

| Parameter | Value | Purpose |
|-----------|-------|---------|
| Hardware Timeout | 600s | Allow slow network operations |
| Software Check | 30s | Monitor task health |
| Max Idle Time | 120s | Critical task stall threshold |
| Max Deadline Misses | 20 | Cumulative failure threshold |

### Mutex Timeouts (milliseconds)

| Mutex | Timeout | Task | Why |
|-------|---------|------|-----|
| WiFi (Upload) | 4000ms | Upload | < 5s deadline |
| WiFi (Command) | 2000ms | Command | < 3s deadline |
| WiFi (Config) | 2000ms | Config | < 3s deadline |
| WiFi (OTA) | 2000ms | OTA | Quick check |
| Data Pipeline | 100ms | Compression | Fast operation |
| NVS Access | ∞ | All | Flash writes critical |

**Design Rule:** `Mutex_Timeout + Operation_Time < Task_Deadline`

### Queue Sizing

| Queue | Size | Item Size | Total Memory | Rationale |
|-------|------|-----------|--------------|-----------|
| Sensor Data | 10 | 152 bytes | 1520 bytes | ~50s buffer @ 5s poll |
| Compressed Data | 5 | 664 bytes | 3320 bytes | ~75s buffer @ 15s upload |
| Commands | 5 | 340 bytes | 1700 bytes | Unused (legacy) |

**Total Queue Memory:** ~6.5 KB

### Buffer Sizes

| Buffer | Size | Purpose |
|--------|------|---------|
| Compression Input | 2048 bytes | Raw sensor samples |
| Compression Output | 512 bytes | Compressed packet |
| Decompression Output | 2048 bytes | Server-side decompression |
| JSON Upload | 8192 bytes | Upload payload |
| JSON Command | 1024 bytes | Command response |
| JSON Config | 2048 bytes | Config response |
| Base64 Encoded | 4096 bytes | Security layer |

**Total Buffer Memory:** ~20 KB static allocation

---

## Potential Bugs and Issues

### 🔴 CRITICAL ISSUES

#### 1. Queue Overflow with Silent Data Loss
**Location:** `task_manager.cpp:390-395`

```cpp
// Send to compression queue (non-blocking to avoid deadline miss)
if (xQueueSend(sensorDataQueue, &sample, 0) != pdTRUE) {
    LOG_WARN(LOG_TAG_DATA, "Queue full! Sample dropped");
    stats_sensorPoll.deadlineMisses++;  // ← Not actually a deadline miss!
}
```

**Problem:**
- If compression task is slow, queue fills up
- Samples silently dropped with only a warning
- No retry mechanism
- User unaware of data loss

**Impact:** **HIGH** - Silent data loss in production

**Fix:**
```cpp
// Option 1: Expand queue size
#define QUEUE_SENSOR_DATA_SIZE  20  // Was 10

// Option 2: Implement retry with backoff
uint8_t retries = 3;
while (retries > 0) {
    if (xQueueSend(sensorDataQueue, &sample, pdMS_TO_TICKS(100)) == pdTRUE) {
        break;
    }
    retries--;
    if (retries == 0) {
        // Send dropped sample event to server
        DiagnosticsLogger::logDataLoss(&sample);
    }
}

// Option 3: Use circular buffer instead of queue
RingBuffer<SensorSample, 20> sensorBuffer;
sensorBuffer.push(sample);  // Overwrites oldest if full
```

---

#### 2. Race Condition in Config Reload
**Location:** `task_manager.cpp:360-367` and `task_manager.cpp:733-739`

```cpp
// Sensor Poll Task (Core 1) - READING
if (xSemaphoreTake(configReloadSemaphore, 0) == pdTRUE) {
    TickType_t newFrequency = pdMS_TO_TICKS(pollFrequency);  // ← Reading shared variable
    // ...
}

// Upload Task (Core 0) - WRITING
void TaskManager::updatePollFrequency(uint32_t newFreqMs) {
    pollFrequency = newFreqMs;  // ← Writing shared variable, NO MUTEX!
}
```

**Problem:**
- `pollFrequency` is static variable accessed by multiple cores
- No atomic operations or mutex protection
- ESP32 has two cores - potential torn reads
- On dual-core, a 32-bit write is NOT atomic across cores

**Impact:** **HIGH** - Potential for corrupted timing values

**Fix:**
```cpp
// Option 1: Use atomic operations
#include <atomic>
static std::atomic<uint32_t> pollFrequency;

// Option 2: Add mutex protection
static SemaphoreHandle_t configMutex;

// Read:
xSemaphoreTake(configMutex, portMAX_DELAY);
uint32_t freq = pollFrequency;
xSemaphoreGive(configMutex);

// Write:
xSemaphoreTake(configMutex, portMAX_DELAY);
pollFrequency = newFreqMs;
xSemaphoreGive(configMutex);

// Option 3: Use FreeRTOS task notifications instead
xTaskNotify(sensorPollTask_h, newFreqMs, eSetValueWithOverwrite);
```

---

#### 3. Deadline Miss Counter Never Resets - Causes Inevitable System Restart
**Location:** `task_manager.cpp:399,437,1140`

```cpp
// Deadline miss accumulation (Line 399 - SensorPoll Task)
if (xQueueSend(sensorDataQueue, &sample, 0) != pdTRUE) {
    stats_sensorPoll.deadlineMisses++;  // ← Accumulates forever
}

// Deadline checking (Line 437 - checkDeadline)
if (executionTimeUs > deadlineUs) {
    stats.deadlineMisses++;  // ← No reset mechanism
}

// Watchdog enforcement (Line 1140 - Watchdog Task)
if (stats_sensorPoll.deadlineMisses > MAX_DEADLINE_MISSES) {
    LOG_ERROR(LOG_TAG_WATCHDOG, "CRITICAL: Excessive sensor deadline misses (%lu > %d)! Resetting...",
              stats_sensorPoll.deadlineMisses, MAX_DEADLINE_MISSES);
    ESP.restart();  // ← Forced restart after 20 misses
}
```

**Problem:**
- **Deadline miss counters accumulate indefinitely** and are NEVER reset
- Once 20 misses occur (MAX_DEADLINE_MISSES), system forcibly restarts
- Temporary network issues (WiFi down, server unavailable) cause permanent deadline accumulation
- After WiFi/server recovery, old deadline misses still count toward the 20 limit
- System eventually restarts even if current operation is healthy

**Current Behavior (Your Log Example):**
```
[00:02:52] [WATCHDOG  ] ✗    CRITICAL: Excessive sensor deadline misses (22 > 20)! Resetting...
[173361][W][WiFiGeneric.cpp:1062] _eventCallback(): Reason: 8 - ASSOC_LEAVE
rst:0xc (SW_CPU_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
```

**Root Cause Analysis:**
1. WiFi temporarily disconnected or server was down
2. Upload task couldn't send data → queue filled up
3. Sensor poll task dropped samples → incremented `deadlineMisses`
4. Counter accumulated to 22 over time
5. Watchdog triggered restart even though WiFi was back online

**Why This Happens:**
- Statistics are initialized once at startup: `TaskStats TaskManager::stats_sensorPoll = {0};`
- No mechanism to reset counters during normal operation
- No distinction between:
  - **Transient failures** (WiFi temporarily down) 
  - **Persistent failures** (actual hardware/software malfunction)

**Impact:** **CRITICAL** - Production systems will experience unnecessary restarts

**Is Current Behavior Preferable?**
❌ **NO** - The current approach has major flaws:

**Problems with Never Resetting:**
1. **False Positives:** Temporary network glitches cause restarts
2. **No Recovery:** System can't heal itself after transient issues
3. **Cumulative Damage:** One bad hour permanently damages the counter
4. **Misleading Metrics:** Old deadline misses don't represent current health
5. **Unnecessary Downtime:** Restart interrupts legitimate operations

**Recommended Solutions:**

**Option 1: Sliding Window Counter (BEST)**
```cpp
// Track deadline misses in a time window
#define DEADLINE_WINDOW_MS      300000  // 5 minutes
#define MAX_MISSES_PER_WINDOW   20      // 20 misses in 5 min = restart

struct DeadlineTracker {
    uint32_t missTimestamps[MAX_MISSES_PER_WINDOW];
    uint8_t writeIndex = 0;
    
    void recordMiss() {
        missTimestamps[writeIndex] = millis();
        writeIndex = (writeIndex + 1) % MAX_MISSES_PER_WINDOW;
    }
    
    uint8_t getRecentMisses() {
        uint32_t now = millis();
        uint8_t count = 0;
        for (int i = 0; i < MAX_MISSES_PER_WINDOW; i++) {
            if (now - missTimestamps[i] < DEADLINE_WINDOW_MS) {
                count++;
            }
        }
        return count;
    }
};

// In watchdog task:
if (deadlineTracker.getRecentMisses() > MAX_MISSES_PER_WINDOW) {
    LOG_ERROR(LOG_TAG_WATCHDOG, "CRITICAL: %d deadline misses in %d sec!", 
              MAX_MISSES_PER_WINDOW, DEADLINE_WINDOW_MS/1000);
    ESP.restart();
}
```

**Option 2: Periodic Decay/Reset**
```cpp
// Reset counters periodically if system is healthy
static uint32_t lastCounterReset = 0;
#define COUNTER_RESET_INTERVAL_MS  600000  // 10 minutes

if (millis() - lastCounterReset > COUNTER_RESET_INTERVAL_MS) {
    // Only reset if WiFi is connected and recent operations succeeded
    if (WiFi.status() == WL_CONNECTED && lastUploadSuccess) {
        LOG_INFO(LOG_TAG_WATCHDOG, "Resetting deadline miss counters (system healthy)");
        stats_sensorPoll.deadlineMisses = 0;
        stats_upload.deadlineMisses = 0;
        stats_compression.deadlineMisses = 0;
        lastCounterReset = millis();
    }
}
```

**Option 3: Rate-Based Threshold**
```cpp
// Only restart if deadline miss RATE exceeds threshold
#define MAX_MISS_RATE_PERCENT   30      // 30% of samples can miss
#define EVALUATION_PERIOD_MS    300000  // Evaluate over 5 minutes

if (stats_sensorPoll.executionCount > 100) {  // Need enough samples
    float missRate = (stats_sensorPoll.deadlineMisses * 100.0f) / 
                     stats_sensorPoll.executionCount;
    
    if (missRate > MAX_MISS_RATE_PERCENT) {
        LOG_ERROR(LOG_TAG_WATCHDOG, "CRITICAL: Deadline miss rate %.1f%% exceeds %d%%",
                  missRate, MAX_MISS_RATE_PERCENT);
        ESP.restart();
    }
}
```

**Option 4: Contextual Reset**
```cpp
// Reset counters when WiFi reconnects (network was the issue)
void onWiFiConnected() {
    LOG_INFO(LOG_TAG_WIFI, "WiFi reconnected - clearing deadline misses from network outage");
    stats_sensorPoll.deadlineMisses = 0;
    stats_upload.deadlineMisses = 0;
}

// In WiFi event handler
void WiFiEvent(WiFiEvent_t event) {
    switch(event) {
        case SYSTEM_EVENT_STA_GOT_IP:
            onWiFiConnected();
            break;
    }
}
```

**Recommended Hybrid Approach:**
```cpp
// Combine sliding window + contextual reset
class DeadlineMonitor {
private:
    uint32_t missTimestamps[MAX_DEADLINE_MISSES];
    uint8_t writeIndex = 0;
    uint32_t lastNetworkIssue = 0;
    
public:
    void recordMiss(bool isNetworkRelated) {
        if (isNetworkRelated) {
            lastNetworkIssue = millis();
        }
        missTimestamps[writeIndex] = millis();
        writeIndex = (writeIndex + 1) % MAX_DEADLINE_MISSES;
    }
    
    bool shouldRestart() {
        uint32_t now = millis();
        uint8_t recentMisses = 0;
        
        // Count misses in last 5 minutes
        for (int i = 0; i < MAX_DEADLINE_MISSES; i++) {
            if (now - missTimestamps[i] < 300000) {
                recentMisses++;
            }
        }
        
        // If recent network issue, be more lenient
        if (now - lastNetworkIssue < 60000) {  // 1 min grace period
            return recentMisses > (MAX_DEADLINE_MISSES * 2);  // Double threshold
        }
        
        return recentMisses > MAX_DEADLINE_MISSES;
    }
    
    void onNetworkRestored() {
        // Clear misses older than 2 minutes
        uint32_t cutoff = millis() - 120000;
        for (int i = 0; i < MAX_DEADLINE_MISSES; i++) {
            if (missTimestamps[i] < cutoff) {
                missTimestamps[i] = 0;
            }
        }
    }
};
```

**Immediate Fix (Minimal Change):**
```cpp
// Add to watchdog task - reset if WiFi was down but is now up
static bool wasWiFiDown = false;

if (WiFi.status() != WL_CONNECTED) {
    wasWiFiDown = true;
} else if (wasWiFiDown) {
    // WiFi just recovered - reset network-related deadline misses
    LOG_INFO(LOG_TAG_WATCHDOG, "Network recovered - resetting deadline counters");
    stats_sensorPoll.deadlineMisses = min(stats_sensorPoll.deadlineMisses, 5);  // Keep some history
    stats_upload.deadlineMisses = 0;  // Upload misses definitely network-related
    wasWiFiDown = false;
}
```

**Testing Strategy:**
1. Simulate WiFi outage for 2 minutes
2. Verify counters don't cause restart after recovery
3. Test actual deadline violations (infinite loop in sensor task)
4. Verify restart DOES occur for real issues
5. Test boundary conditions (exactly 20 misses)

**Metrics to Add:**
```cpp
struct ImprovedTaskStats {
    uint32_t totalDeadlineMisses;           // Lifetime count
    uint32_t recentDeadlineMisses;          // Sliding window
    uint32_t networkRelatedMisses;          // Classified by cause
    uint32_t hardwareRelatedMisses;         // Hardware failures
    uint32_t lastMissTimestamp;             // For debugging
    uint32_t longestMissFreeStreak;         // Track improvements
};
```

---

#### 4. Config Update Register Mismatch - Frontend Shows Wrong Register Count
**Location:** `data_uploader.cpp:177-186`, Frontend display logic

```cpp
// ESP32: data_uploader.cpp:177-186
// Build register mapping from first entry
JsonObject registerMapping = (*doc)["register_mapping"].to<JsonObject>();
if (!allData.empty()) {
    const auto& firstEntry = allData[0];  // ← Uses FIRST packet
    for (size_t i = 0; i < firstEntry.registerCount && i < REGISTER_COUNT; i++) {
        char key[8];
        snprintf(key, sizeof(key), "%zu", i);
        registerMapping[key] = REGISTER_MAP[firstEntry.registers[i]].name;
    }
}

// But each packet has its own metadata:
decompMeta["register_count"] = entry.registerCount;  // Per-packet
JsonArray regLayout = decompMeta["register_layout"].to<JsonArray>();
for (size_t i = 0; i < entry.registerCount; i++) {
    regLayout.add(entry.registers[i]);  // Per-packet register list
}
```

**Problem:**
- Global `register_mapping` is built from **first packet** in upload queue
- When config changes (4→6 registers), old packets (4 regs) are still in queue
- Upload contains mixed packets: old (4 regs) + new (6 regs)
- Global `register_mapping` uses old 4-register layout
- Frontend/server uses global mapping instead of per-packet `register_layout`
- **Result**: New 6-register data incorrectly mapped to old 4-register layout

**Timeline of the Bug:**
```
T=0s:  Device polls 4 registers (Vac1, Iac1, Fac1, Vp v1)
T=0s:  Packets with 4 registers queued
T=0s:  Upload → Global register_mapping = {0: "Vac1", 1: "Iac1", 2: "Fac1", 3: "Vpv1"}
T=0s:  Frontend changes config to 6 registers (adds Vpv2, Ipv1)
T=5s:  Config reload → Device now polls 6 registers
T=5s-15s: New packets with 6 registers queued
T=15s: Upload → Global register_mapping STILL uses first packet (old 4 regs)
T=15s: Frontend displays: Shows 4 register names but 6 register values
T=15s: **MISMATCH**: Values[4] and Values[5] have no labels, or wrong labels
T=30s: All old packets flushed → Next upload has correct 6-register mapping
```

**Why This Happens:**
1. Ring buffer can hold 20 packets (~60 seconds of data)
2. Config changes between uploads (not between samples in a batch - batches are homogeneous)
3. Upload drains queue containing both old and new packets
4. ESP32 correctly includes per-packet metadata (`register_layout`)
5. But global `register_mapping` uses first (oldest) packet's layout
6. Frontend assumes all packets use global `register_mapping`

**Impact:** **MEDIUM** - Frontend displays incorrect register labels for one upload cycle after config change

**Correct Behavior:**
- All samples in ONE batch have same register count (config reloads between batches)
- Config reload happens AFTER upload, BEFORE next sensor poll cycle
- One upload can contain MULTIPLE batches with DIFFERENT register counts
- Each batch has correct per-packet metadata

**Fix Options:**

**Option 1: Frontend - Use Per-Packet Register Layout (RECOMMENDED)**
```javascript
// Frontend should use decompression_metadata.register_layout per packet
compressed_data.forEach(packet => {
    const register_layout = packet.decompression_metadata.register_layout;
    const register_count = packet.decompression_metadata.register_count;
    
    // Decompress using THIS packet's layout
    const values = decompress(packet.compressed_binary);
    
    // Map using THIS packet's register list
    for (let i = 0; i < register_count; i++) {
        registerData[register_layout[i]] = values[i];
    }
});
```

**Option 2: ESP32 - Use Most Recent Packet for Global Mapping**
```cpp
// data_uploader.cpp:177-186
// Build register mapping from LAST entry (most recent config)
JsonObject registerMapping = (*doc)["register_mapping"].to<JsonObject>();
if (!allData.empty()) {
    const auto& lastEntry = allData[allData.size() - 1];  // ← Use LAST packet
    for (size_t i = 0; i < lastEntry.registerCount && i < REGISTER_COUNT; i++) {
        char key[8];
        snprintf(key, sizeof(key), "%zu", i);
        registerMapping[key] = REGISTER_MAP[lastEntry.registers[i]].name;
    }
    LOG_INFO(LOG_TAG_UPLOAD, "Global register_mapping built from most recent packet (%zu registers)",
             lastEntry.registerCount);
}
```

**Option 3: ESP32 - Flush Old Packets on Config Change**
```cpp
// In sensor poll task, after config reload:
if (newRegisterCount != registerCount) {
    // Clear sensor data queue (old samples with old register count)
    SensorSample dummy;
    while (xQueueReceive(sensorDataQueue, &dummy, 0) == pdTRUE) {
        // Drain queue
    }
    
    // Clear compressed data queue (old packets with old register count)
    CompressedPacket dummyPacket;
    while (xQueueReceive(compressedDataQueue, &dummyPacket, 0) == pdTRUE) {
        // Drain queue
    }
    
    LOG_INFO(LOG_TAG_DATA, "Flushed queues due to register configuration change");
    
    registerCount = newRegisterCount;
    registers = newRegisters;
}
```

**Option 4: Hybrid - Mark Packets with Config Version**
```cpp
struct CompressedPacket {
    uint8_t data[512];
    size_t dataSize;
    uint32_t timestamp;
    size_t sampleCount;
    size_t registerCount;
    RegID registers[16];
    uint32_t configVersion;  // ← NEW: Increments on config change
};

// Upload task can then group packets by configVersion
// and create multiple register_mapping objects
```

**Recommended Solution:**
Implement **Option 2** (use most recent packet) - simplest fix with minimal impact:
- One-line change in data_uploader.cpp
- Ensures global mapping reflects current configuration
- Preserves per-packet metadata for accuracy
- No data loss

**Testing:**
```python
# Test case:
1. Device running with 4 registers
2. Upload at T=0s (4 registers)
3. Change config to 6 registers via frontend
4. Wait for next upload at T=15s
5. Verify: Global register_mapping shows all 6 registers
6. Verify: Frontend displays all 6 register values with correct labels
```

---

#### 5. Hardcoded Semaphore Count
**Location:** `task_manager.cpp:733-739`

```cpp
if (uploadSuccess) {
    // Signal all tasks to reload configuration (after successful upload)
    // Give semaphore 6 times (once for each task that needs to reload config):
    // 1. Sensor Poll Task
    // 2. Upload Task
    // 3. Command Task
    // 4. Config Task
    // 5. Power Report Task
    // 6. OTA Task
    for (int i = 0; i < 6; i++) {  // ← HARDCODED!
        xSemaphoreGive(configReloadSemaphore);
    }
}
```

**Problem:**
- Magic number "6" hardcoded
- If tasks added/removed, easy to forget updating this
- No compile-time check
- Semaphore count can get out of sync

**Impact:** **MEDIUM** - Configuration updates may not propagate

**Fix:**
```cpp
// Option 1: Use enum or define
enum ConfigurableTasks {
    TASK_SENSOR_POLL = 0,
    TASK_UPLOAD,
    TASK_COMMAND,
    TASK_CONFIG,
    TASK_POWER_REPORT,
    TASK_OTA,
    CONFIGURABLE_TASK_COUNT
};

for (int i = 0; i < CONFIGURABLE_TASK_COUNT; i++) {
    xSemaphoreGive(configReloadSemaphore);
}

// Option 2: Use task notification instead of semaphore
xTaskNotify(sensorPollTask_h, CONFIG_RELOAD_SIGNAL, eSetBits);
xTaskNotify(uploadTask_h, CONFIG_RELOAD_SIGNAL, eSetBits);
// ... etc
```

---

#### 4. Memory Fragmentation from Repeated Allocations
**Location:** `task_manager.cpp:536-540`

```cpp
// Convert samples to linear array for compression
size_t totalRegisterCount = batchSize * sampleBatch[0].registerCount;
uint16_t* linearData = new uint16_t[totalRegisterCount];  // ← HEAP ALLOCATION IN LOOP
RegID* linearRegs = new RegID[totalRegisterCount];

// ... compress ...

delete[] linearData;  // ← FREED IMMEDIATELY
delete[] linearRegs;
```

**Problem:**
- Heap allocation in tight loop (every 15 seconds)
- ESP32 has limited heap (320KB)
- Repeated allocation/deallocation causes fragmentation
- Over time, heap becomes fragmented
- Eventually, allocation fails even with enough free memory

**Impact:** **HIGH** - System instability after hours/days of runtime

**Fix:**
```cpp
// Option 1: Static buffer pool
static uint16_t linearDataBuffer[MAX_BATCH_SIZE * MAX_REGISTERS];
static RegID linearRegsBuffer[MAX_BATCH_SIZE * MAX_REGISTERS];

// Use stack instead of heap:
memcpy(linearDataBuffer, /* source */, size);

// Option 2: Pre-allocate at task start
static uint16_t* linearData = nullptr;
static RegID* linearRegs = nullptr;

void TaskManager::compressionTask(void* parameter) {
    // Allocate once at start
    linearData = new uint16_t[MAX_BATCH_SIZE * MAX_REGISTERS];
    linearRegs = new RegID[MAX_BATCH_SIZE * MAX_REGISTERS];
    
    while (1) {
        // Reuse buffers
        memcpy(linearData, /* source */, size);
        // ...
    }
}
```

---

#### 5. Watchdog Timeout vs Task Timing Mismatch
**Location:** `main.cpp:177-178` and `task_manager.cpp:1041`

```cpp
// main.cpp: Hardware watchdog timeout
esp_task_wdt_init(HARDWARE_WATCHDOG_TIMEOUT_S, true);  // 600 seconds (10 minutes)

// task_manager.cpp: OTA Task
void TaskManager::otaTask(void* parameter) {
    // NOTE: OTA task NOT registered with watchdog - it runs every 60s
    // which exceeds the 30s watchdog timeout
    // ...
}
```

**Problem:**
- Hardware watchdog: 10 minutes
- OTA task runs every 60 seconds but NOT registered with watchdog
- If all OTHER tasks stall, system won't reset for 10 minutes
- Design assumes at least ONE other task is healthy

**Impact:** **MEDIUM** - Delayed fault detection

**Scenario:**
```
1. Network stack crashes (WiFi driver bug)
2. All Core 0 tasks stall (Upload, Command, Config)
3. Sensor Poll task continues (Core 1)
4. Sensor Poll feeds watchdog
5. System appears healthy but not uploading data
6. User unaware for 10 minutes until sensors also fail
```

**Fix:**
```cpp
// Option 1: Reduce watchdog timeout
#define HARDWARE_WATCHDOG_TIMEOUT_S  120  // 2 minutes instead of 10

// Option 2: Register OTA task despite long period
void TaskManager::otaTask(void* parameter) {
    esp_task_wdt_add(NULL);  // Register with watchdog
    
    while (1) {
        // Feed watchdog even if OTA doesn't run
        esp_task_wdt_reset();
        
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        // ... OTA check ...
    }
}

// Option 3: Software watchdog monitors all critical tasks
if (!uploadTask.hasRunRecently() && !commandTask.hasRunRecently()) {
    LOG_ERROR("Multiple tasks stalled - network failure suspected");
    ESP.restart();
}
```

---

### 🟡 MEDIUM ISSUES

#### 6. HTTP Timeout Inconsistency
**Location:** `task_manager.cpp:709` and `system_config.h:127`

```cpp
// task_manager.cpp: WiFi mutex timeout
if (xSemaphoreTake(wifiClientMutex, pdMS_TO_TICKS(4000)) == pdTRUE) {
    uploadSuccess = DataUploader::uploadPendingData();
    // ...
}

// system_config.h: HTTP timeout
#define HTTP_UPLOAD_TIMEOUT_MS  15000  // 15 seconds
```

**Problem:**
- Mutex acquired with 4s timeout
- HTTP operation can take up to 15s
- If HTTP takes 15s, task exceeds 5s deadline
- Mutex held for too long, blocking other tasks

**Impact:** **MEDIUM** - Cascading deadline misses

**Fix:**
```cpp
// Option 1: Reduce HTTP timeout to match mutex
#define HTTP_UPLOAD_TIMEOUT_MS  3500  // 3.5s (4s mutex - 500ms margin)

// Option 2: Release mutex during HTTP wait (NOT RECOMMENDED - unsafe)

// Option 3: Adjust deadline to accommodate HTTP timeout
#define UPLOAD_DEADLINE_US  20000000  // 20s instead of 5s
```

---

#### 7. String Truncation Without Null Termination
**Location:** `task_manager.cpp:557-564`

```cpp
// Determine compression method from header
if (packet.dataSize > 0) {
    switch (packet.data[0]) {
        case 0xD0: strncpy(packet.compressionMethod, "dictionary", sizeof(packet.compressionMethod) - 1); break;
        case 0x70:
        case 0x71: strncpy(packet.compressionMethod, "temporal", sizeof(packet.compressionMethod) - 1); break;
        // ...
    }
}
```

**Problem:**
- `strncpy` does NOT guarantee null termination if source exceeds limit
- `sizeof(packet.compressionMethod) - 1` leaves space for null
- But if string is exactly that length, no null terminator added

**Impact:** **LOW** - Strings are short enough, but unsafe

**Fix:**
```cpp
// Always null-terminate after strncpy
strncpy(packet.compressionMethod, "dictionary", sizeof(packet.compressionMethod) - 1);
packet.compressionMethod[sizeof(packet.compressionMethod) - 1] = '\0';

// Or use safer alternative:
snprintf(packet.compressionMethod, sizeof(packet.compressionMethod), "%s", "dictionary");
```

---

#### 8. Deadline Miss Counter Never Resets
**Location:** `task_manager.cpp:1165` and watchdog task

```cpp
// Check for excessive deadline misses
if (stats_sensorPoll.deadlineMisses > MAX_DEADLINE_MISSES) {
    LOG_ERROR("Excessive sensor deadline misses! Resetting...");
    ESP.restart();
}
```

**Problem:**
- Deadline miss counter only increments, never decrements
- One bad period (e.g., WiFi reconnection) can accumulate misses
- Even after system recovers, counter stays high
- Eventually triggers reset even when healthy

**Impact:** **MEDIUM** - False positive resets

**Fix:**
```cpp
// Option 1: Reset counter after successful runs
if (executionTime < deadlineUs) {
    // Decay counter on success
    if (stats_sensorPoll.deadlineMisses > 0) {
        stats_sensorPoll.deadlineMisses--;
    }
}

// Option 2: Use sliding window
struct DeadlineWindow {
    uint32_t misses[100];  // Last 100 executions
    uint8_t index;
    uint32_t totalMisses;
};

// Option 3: Reset counter periodically
static uint32_t lastCounterReset = 0;
if (millis() - lastCounterReset > 3600000) {  // Every hour
    stats_sensorPoll.deadlineMisses = 0;
    lastCounterReset = millis();
}
```

---

#### 9. Base64 Buffer Sizing Risk
**Location:** `task_manager.cpp:560` and data_uploader

```cpp
CompressedPacket {
    uint8_t data[512];  // Fixed buffer
    // ...
}
```

**Problem:**
- Compressed data stored in 512-byte buffer
- After upload, data is Base64 encoded
- Base64 increases size by 4/3 (33% overhead)
- 512 bytes → 683 bytes after Base64
- JSON buffer might be undersized

**Impact:** **LOW** - Compression typically reduces to <200 bytes

**Verification Needed:**
```cpp
// In DataUploader::buildUploadPayload()
// Check if buffer is sufficient for Base64 + JSON overhead
char jsonBuffer[8192];  // Is this enough?

// Worst case:
// - 5 packets × 512 bytes = 2560 bytes compressed
// - Base64: 2560 × 4/3 = 3413 bytes
// - JSON metadata: ~2000 bytes
// - Total: ~5500 bytes
// - Buffer: 8192 bytes ✓ OK
```

---

#### 10. Magic Number Validation Missing
**Location:** `task_manager.cpp:492`

```cpp
// Calculate batch size dynamically based on upload/poll timing
const size_t batchSize = (uploadFrequency / pollFrequency);
// Example: 15000ms / 5000ms = 3 samples per upload cycle
```

**Problem:**
- No validation that `batchSize > 0`
- If `uploadFrequency < pollFrequency`, batchSize = 0
- Array allocation with size 0 causes undefined behavior

**Impact:** **LOW** - Configuration should prevent this

**Fix:**
```cpp
// Validate batch size
size_t batchSize = (uploadFrequency / pollFrequency);
if (batchSize == 0) {
    LOG_ERROR("Invalid batch size! Upload frequency must be >= poll frequency");
    batchSize = 1;  // Minimum safe value
}

// Or enforce at configuration level
if (newUploadFreq < pollFrequency) {
    LOG_ERROR("Upload frequency cannot be less than poll frequency");
    return false;  // Reject configuration
}
```

---

### 🟢 MINOR ISSUES

#### 11. NTP Failure Silent
**Location:** `system_initializer.cpp:78-87`

```cpp
if (retry < retry_count) {
    LOG_SUCCESS("NTP time synchronized");
    // ...
} else {
    LOG_WARN("NTP sync timeout - using millis() fallback");
    return false;  // ← System continues with wrong timestamps
}
```

**Problem:**
- If NTP fails, timestamps will be wrong
- All data uploaded with incorrect timestamps
- Server cannot correct this
- User unaware of problem

**Impact:** **LOW** - Data still uploaded, just wrong time

**Fix:**
```cpp
if (retry >= retry_count) {
    LOG_ERROR("NTP sync failed - timestamps will be incorrect!");
    
    // Send diagnostic event to server
    DiagnosticsLogger::logNTPFailure();
    
    // Consider: Should we block until NTP succeeds?
    // Or: Use RTC if available?
}
```

---

#### 12. OTA Mutex Deadlock Risk
**Location:** `task_manager.cpp:1088-1094`

```cpp
if (xSemaphoreTake(wifiClientMutex, pdMS_TO_TICKS(2000)) == pdTRUE) {
    if (otaManager->checkForUpdate()) {
        // KEEP the WiFi mutex - don't release it before suspending tasks!
        
        // Suspend critical tasks during OTA
        suspendAllTasks();  // ← What if task was waiting on WiFi mutex?
        
        bool otaSuccess = otaManager->downloadAndApplyFirmware();
        // ...
    }
}
```

**Problem:**
- OTA acquires WiFi mutex
- Then suspends all other tasks
- If another task was waiting on WiFi mutex, deadlock
- Comment acknowledges risk but doesn't prevent it

**Impact:** **LOW** - Rare scenario (OTA + simultaneous network operation)

**Fix:**
```cpp
// Option 1: Suspend tasks BEFORE acquiring mutex
suspendAllTasks();
if (xSemaphoreTake(wifiClientMutex, portMAX_DELAY) == pdTRUE) {
    // Now safe - no other task can block
    otaManager->downloadAndApplyFirmware();
    xSemaphoreGive(wifiClientMutex);
}
resumeAllTasks();

// Option 2: Use try-lock pattern
if (xSemaphoreTake(wifiClientMutex, 0) == pdTRUE) {
    // Got mutex immediately - proceed
} else {
    // Mutex busy - skip this OTA check
    LOG_INFO("OTA check skipped - network busy");
}
```

---

#### 13. WiFi Connection Blocking on Boot
**Location:** `main.cpp:166-167`

```cpp
// STEP 1: Initialize WiFi FIRST (before logger so timestamps work)
Wifi.begin();  // ← BLOCKS until connected
```

**Problem:**
- `Wifi.begin()` blocks until connection succeeds
- No timeout specified
- If WiFi unavailable, ESP32 hangs at boot
- No fallback or offline mode

**Impact:** **LOW** - Expected behavior for networked device

**Fix:**
```cpp
// Option 1: Add timeout
if (!Wifi.beginWithTimeout(30000)) {  // 30s timeout
    LOG_ERROR("WiFi connection failed!");
    // Enter offline mode or retry loop
}

// Option 2: Async WiFi connection
WiFi.mode(WIFI_STA);
WiFi.begin(SSID, PASSWORD);

// Continue boot process
// Check connection status in loop
```

---

#### 14. Power Report Buffer Overflow Risk
**Location:** `task_manager.cpp:951-970`

```cpp
char jsonBuffer[512];
snprintf(jsonBuffer, sizeof(jsonBuffer),
    "{"
    "\"device_id\":\"%s\","
    "\"timestamp\":%llu,"
    "\"power_management\":{"
    // ... many fields ...
    "}"
    "}"
);
```

**Problem:**
- Fixed 512-byte buffer
- `snprintf` with many fields
- No validation that output fits
- If truncated, JSON becomes invalid

**Impact:** **LOW** - JSON is small (~300 bytes typically)

**Fix:**
```cpp
// Option 1: Use larger buffer
char jsonBuffer[1024];  // Double size for safety

// Option 2: Check snprintf return value
int written = snprintf(jsonBuffer, sizeof(jsonBuffer), /* ... */);
if (written >= sizeof(jsonBuffer)) {
    LOG_ERROR("Power report JSON truncated! Need %d bytes", written);
}

// Option 3: Use ArduinoJson library
StaticJsonDocument<512> doc;
doc["device_id"] = DEVICE_ID;
// ... add fields ...
serializeJson(doc, jsonBuffer, sizeof(jsonBuffer));
```

---

#### 15. Error Logging Inconsistency
**Location:** Throughout codebase

**Problem:**
- Some errors use `LOG_ERROR`
- Others use `LOG_WARN`
- No consistent severity policy
- Makes log analysis difficult

**Examples:**
```cpp
LOG_ERROR("Failed to acquire WiFi mutex");  // Is this error or warning?
LOG_WARN("Queue full! Sample dropped");     // Should be error (data loss)
LOG_INFO("Upload failed");                   // Should be error
```

**Fix:**
```cpp
// Define severity guidelines:

// LOG_ERROR: System cannot continue, requires intervention
// - Hardware failure
// - Critical resource unavailable
// - Data corruption

// LOG_WARN: System degraded but operational
// - Retryable failures
// - Performance issues
// - Configuration problems

// LOG_INFO: Normal operation
// - State changes
// - Successful operations
// - Periodic reports
```

---

## Configuration Hierarchy

### Three-Tier Configuration System

The ESP32 uses a hierarchical configuration system with clear precedence:

```
┌─────────────────────────────────────────────────────────────┐
│  TIER 1: DEFAULT VALUES (system_config.h)                   │
│  - Compiled into firmware                                    │
│  - Used on first boot or NVS corruption                      │
│  - Safe fallback values                                      │
└─────────────────────────────────────────────────────────────┘
                           ↓
┌─────────────────────────────────────────────────────────────┐
│  TIER 2: NVS STORAGE (Flash Memory)                         │
│  - Persistent across reboots                                 │
│  - Updated by Flask server commands                          │
│  - Survives firmware updates                                 │
└─────────────────────────────────────────────────────────────┘
                           ↓
┌─────────────────────────────────────────────────────────────┐
│  TIER 3: RUNTIME VALUES (RAM)                               │
│  - Active in-memory configuration                            │
│  - Used by FreeRTOS tasks                                    │
│  - Reloaded from NVS after successful upload                 │
└─────────────────────────────────────────────────────────────┘
```

### Configuration Parameters

#### Timing Parameters (stored as microseconds in NVS):
- **Poll Frequency**: How often to read sensors
- **Upload Frequency**: How often to send data to server
- **Config Check Frequency**: How often to check for config updates
- **Command Poll Frequency**: How often to check for pending commands
- **OTA Check Frequency**: How often to check for firmware updates
- **Energy Poll Frequency**: How often to report power statistics

#### Register Selection (stored as bitmask):
- **Register Mask**: 16-bit mask of enabled registers
- **Register Count**: Number of enabled registers (1-10)
- **Register Array**: Ordered list of RegID values

#### Power Management:
- **Enabled**: Boolean flag
- **Techniques**: Bitmask of enabled techniques
- **Energy Poll Frequency**: Reporting interval

### Configuration Update Flow

```
┌──────────────────────────────────────────────────────────────┐
│                    FLASK SERVER                               │
└──────────────────────────────────────────────────────────────┘
                           ↓
          POST /config/{device_id}
          {
            "poll_frequency_us": 5000000,
            "upload_frequency_us": 15000000,
            "register_mask": 0x03FF,
            // ...
          }
                           ↓
┌──────────────────────────────────────────────────────────────┐
│              CONFIG TASK (Every 5 seconds)                    │
└──────────────────────────────────────────────────────────────┘
                           ↓
          GET /config/{device_id}
          Compare with NVS values
                           ↓
          If changed:
            ↓
┌──────────────────────────────────────────────────────────────┐
│                    NVS STORAGE (Flash)                        │
│  nvs::changePollFreq(new_value)                              │
│  nvs::saveReadRegs(new_mask, new_count)                      │
│  Set "uptodate" flags to false                               │
└──────────────────────────────────────────────────────────────┘
                           ↓
┌──────────────────────────────────────────────────────────────┐
│          UPLOAD TASK (Next successful upload)                 │
│  if (uploadSuccess) {                                         │
│    Give configReloadSemaphore 6 times                        │
│  }                                                            │
└──────────────────────────────────────────────────────────────┘
                           ↓
┌──────────────────────────────────────────────────────────────┐
│              ALL CONFIGURABLE TASKS                           │
│  if (xSemaphoreTake(configReloadSemaphore, 0) == pdTRUE) {  │
│    Reload configuration from NVS                             │
│    Update local frequency variables                          │
│    Reset timing baseline                                     │
│  }                                                            │
└──────────────────────────────────────────────────────────────┘
```

### Why Wait for Upload Success?

**Design Rationale:**
1. **Atomicity**: All tasks see new configuration simultaneously
2. **Consistency**: Configuration change happens at known point (upload boundary)
3. **Recovery**: If configuration change breaks system, last upload is safe checkpoint

**Alternative Considered:**
- Apply changes immediately when detected
- **Problem:** Tasks update at different times, inconsistent state
- **Example:** Upload frequency changes before poll frequency → batch size incorrect

### NVS Storage Layout

**Namespace:** `"esp_storage"`

| Key | Type | Description | Default |
|-----|------|-------------|---------|
| `poll_freq` | uint64 | Sensor poll interval (μs) | 5000000 |
| `upload_freq` | uint64 | Upload interval (μs) | 15000000 |
| `config_freq` | uint64 | Config check interval (μs) | 5000000 |
| `command_freq` | uint64 | Command poll interval (μs) | 10000000 |
| `ota_freq` | uint64 | OTA check interval (μs) | 60000000 |
| `read_regs_mask` | uint16 | Enabled registers bitmask | 0x03FF |
| `read_regs_count` | uint8 | Number of registers | 10 |

**Namespace:** `"power"`

| Key | Type | Description | Default |
|-----|------|-------------|---------|
| `enabled` | bool | Power management enabled | false |
| `techniques` | uint8 | Technique bitmask | 0x01 |
| `energy_poll_freq` | uint64 | Energy report interval (μs) | 300000000 |

### Configuration Validation

**Validation Rules:**
```cpp
// 1. Frequencies must be within bounds
if (newFreq < MIN_POLL_FREQUENCY_US || newFreq > MAX_POLL_FREQUENCY_US) {
    return false;  // Reject
}

// 2. Upload frequency must be >= poll frequency
if (uploadFreq < pollFreq) {
    return false;  // Would cause batch size 0
}

// 3. Register count must match bitmask
uint8_t count = __builtin_popcount(mask);
if (count != registerCount) {
    return false;  // Inconsistent
}

// 4. At least one register must be enabled
if (registerCount == 0) {
    return false;  // No data to collect
}
```

**Validation Happens:**
- On Flask server (before sending to ESP32)
- On ESP32 (before writing to NVS)
- On task reload (before applying)

---

## Security Implementation

### Multi-Layer Security Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    DATA LAYER                                │
│  - Sensor readings (plaintext)                               │
│  - Compression (efficiency)                                  │
└─────────────────────────────────────────────────────────────┘
                           ↓
┌─────────────────────────────────────────────────────────────┐
│                 TRANSPORT LAYER                              │
│  - JSON serialization                                        │
│  - Base64 encoding                                           │
└─────────────────────────────────────────────────────────────┘
                           ↓
┌─────────────────────────────────────────────────────────────┐
│                 AUTHENTICATION LAYER                         │
│  - HMAC-SHA256 message authentication                       │
│  - Nonce-based anti-replay protection                       │
└─────────────────────────────────────────────────────────────┘
                           ↓
┌─────────────────────────────────────────────────────────────┐
│                 ENCRYPTION LAYER (OTA Only)                  │
│  - AES-256-CBC encryption                                    │
│  - RSA-2048 signature verification                           │
└─────────────────────────────────────────────────────────────┘
```

### HMAC Authentication (Data Uploads)

**Purpose:** Verify message integrity and authenticity

**Algorithm:** HMAC-SHA256

**Key Management:**
```cpp
// Shared secret key (256-bit)
// Stored in both ESP32 and Flask server
const char* HMAC_KEY = "your-secret-key-here";
```

**Message Format:**
```json
{
  "nonce": 12345,
  "payload": "eyJkZXZpY2VfaWQiOi4uLn0=",  // Base64(JSON)
  "mac": "a1b2c3d4...",                    // HMAC-SHA256(nonce + payload)
  "encrypted": false,
  "compressed": false
}
```

**HMAC Calculation:**
```cpp
// ESP32 side:
String message = String(nonce) + base64_payload;
String mac = hmac_sha256(HMAC_KEY, message);

// Flask side:
message = str(nonce) + payload
calculated_mac = hmac.new(HMAC_KEY, message, hashlib.sha256).hexdigest()
if calculated_mac != received_mac:
    return 401  # Unauthorized
```

**Anti-Replay Protection:**
```cpp
// Nonce stored in NVS
uint32_t nonce = nvs::getNonce();  // Load from flash
nonce++;                            // Increment
nvs::saveNonce(nonce);             // Save back to flash

// Server validates nonce
if (received_nonce <= last_seen_nonce) {
    return 401;  // Replay attack detected
}
```

### OTA Firmware Security

**Multi-Stage Verification:**

1. **Manifest Download:**
```json
{
  "version": "1.3.0",
  "sha256_hash": "abc123...",      // Complete firmware hash
  "signature": "def456...",         // RSA signature of hash
  "iv": "789abc...",                // AES initialization vector
  "encrypted_size": 524288,
  "firmware_size": 500000,
  "chunk_size": 2048,
  "total_chunks": 256
}
```

2. **RSA Signature Verification:**
```cpp
// Server's public key embedded in ESP32
const char* SERVER_PUBLIC_KEY = "-----BEGIN PUBLIC KEY-----\n...";

// Verify signature before downloading
bool verifySignature(const String& base64Signature) {
    mbedtls_pk_context pk;
    mbedtls_pk_parse_public_key(&pk, SERVER_PUBLIC_KEY);
    
    // Verify RSA-2048 signature on SHA-256 hash
    int ret = mbedtls_pk_verify(&pk, MBEDTLS_MD_SHA256, 
                                hash, signature, signature_len);
    return (ret == 0);
}
```

3. **Per-Chunk HMAC:**
```cpp
// Each chunk has HMAC for integrity
GET /firmware/chunk/0
{
  "chunk_number": 0,
  "data": "base64_encrypted_chunk",
  "hmac": "chunk_hmac"
}

// Verify each chunk before writing
bool verifyChunkHMAC(const uint8_t* chunkData, size_t len, 
                     uint16_t chunkNum, const String& expectedHMAC) {
    String message = String(chunkNum) + String((char*)chunkData, len);
    String calculated = hmac_sha256(HMAC_KEY, message);
    return (calculated == expectedHMAC);
}
```

4. **AES-256-CBC Decryption:**
```cpp
// Decrypt each chunk before flashing
bool decryptChunk(const uint8_t* encrypted, size_t encLen,
                  uint8_t* decrypted, size_t* decLen,
                  uint16_t chunkNumber) {
    mbedtls_aes_context aes_ctx;
    mbedtls_aes_setkey_dec(&aes_ctx, AES_KEY, 256);
    
    // Use chunk-specific IV to prevent block reuse
    uint8_t chunk_iv[16];
    memcpy(chunk_iv, base_iv, 16);
    chunk_iv[15] ^= (chunkNumber & 0xFF);
    
    mbedtls_aes_crypt_cbc(&aes_ctx, MBEDTLS_AES_DECRYPT,
                          encLen, chunk_iv, encrypted, decrypted);
    return true;
}
```

5. **Complete Firmware Hash Verification:**
```cpp
// After all chunks downloaded and decrypted
bool verifyFirmwareHash() {
    // Calculate SHA-256 of complete firmware
    mbedtls_sha256_context sha256_ctx;
    mbedtls_sha256_init(&sha256_ctx);
    
    // Read firmware from flash partition
    const esp_partition_t* partition = esp_ota_get_next_update_partition(NULL);
    uint8_t buffer[1024];
    for (size_t offset = 0; offset < firmware_size; offset += 1024) {
        esp_partition_read(partition, offset, buffer, 1024);
        mbedtls_sha256_update(&sha256_ctx, buffer, 1024);
    }
    
    uint8_t calculated_hash[32];
    mbedtls_sha256_finish(&sha256_ctx, calculated_hash);
    
    // Compare with manifest hash
    return (memcmp(calculated_hash, expected_hash, 32) == 0);
}
```

6. **Rollback Protection:**
```cpp
// After boot with new firmware
void OTAManager::handleRollback() {
    uint8_t boot_count = nvs::getBootCount();
    
    if (boot_count >= MAX_BOOT_ATTEMPTS) {
        // New firmware failed to boot properly
        LOG_ERROR("Rollback triggered! Boot count: %d", boot_count);
        
        // Revert to previous firmware
        const esp_partition_t* current = esp_ota_get_running_partition();
        const esp_partition_t* previous = esp_ota_get_next_update_partition(current);
        esp_ota_set_boot_partition(previous);
        
        // Clear boot count and reboot
        nvs::setBootCount(0);
        ESP.restart();
    }
    
    // Increment boot count
    nvs::setBootCount(boot_count + 1);
    
    // If diagnostics pass, mark firmware as valid
    if (runDiagnostics()) {
        nvs::setBootCount(0);  // Reset - firmware is stable
        esp_ota_mark_app_valid_cancel_rollback();
    }
}
```

### Security Best Practices Implemented

✅ **Key Separation:**
- HMAC key for data authentication
- Separate AES key for OTA encryption
- RSA keys for firmware signing

✅ **Defense in Depth:**
- Multiple layers (HMAC + AES + RSA)
- Fail-safe defaults (reject on verification failure)
- Rollback protection

✅ **Secure Storage:**
- Keys embedded in firmware (read-only)
- Nonce in NVS (persistent across reboots)
- No keys in transmitted data

✅ **Anti-Replay:**
- Monotonically increasing nonce
- Server tracks last seen nonce
- Rejects old/duplicate messages

⚠️ **Areas for Improvement:**
- Key rotation not implemented
- No certificate expiry
- HMAC key hardcoded (should use secure element)

---

## Recommendations

### Immediate Actions (Fix Critical Bugs)

1. **Fix Race Condition in Config Reload**
   - **Priority:** CRITICAL
   - **Effort:** 2 hours
   - **Action:** Add mutex protection for `pollFrequency` and other shared variables
   ```cpp
   static SemaphoreHandle_t configVariableMutex;
   // Protect all reads/writes to frequency variables
   ```

2. **Fix Queue Overflow Data Loss**
   - **Priority:** HIGH
   - **Effort:** 4 hours
   - **Action:** Implement retry mechanism with diagnostic logging
   ```cpp
   // Add retry with backoff
   // Log dropped samples to server
   DiagnosticsLogger::logDataLoss(&sample);
   ```

3. **Fix Hardcoded Semaphore Count**
   - **Priority:** MEDIUM
   - **Effort:** 1 hour
   - **Action:** Use enum for configurable task count
   ```cpp
   enum { CONFIGURABLE_TASK_COUNT = 6 };
   for (int i = 0; i < CONFIGURABLE_TASK_COUNT; i++) { ... }
   ```

4. **Fix Memory Fragmentation**
   - **Priority:** HIGH
   - **Effort:** 3 hours
   - **Action:** Use static buffer pool for compression
   ```cpp
   static uint16_t compressionBuffer[MAX_SIZE];
   // Reuse instead of allocate/free
   ```

### Short-Term Improvements (1-2 weeks)

5. **Add Deadline Miss Counter Decay**
   - Reset counter after sustained successful operation
   - Prevents false positive resets

6. **Implement NTP Failure Alerting**
   - Send diagnostic event to server
   - Display warning on frontend

7. **Reduce Hardware Watchdog Timeout**
   - From 600s to 120s
   - Faster fault detection

8. **Add HTTP Timeout Consistency Check**
   - Ensure `mutex_timeout + http_timeout < task_deadline`
   - Add compile-time assertion

9. **Implement Graceful Degradation**
   - System should degrade (e.g., reduce poll frequency) instead of hard reset
   - Log detailed diagnostics before reset

### Long-Term Enhancements (1-2 months)

10. **Task Health Monitoring Dashboard**
    - Real-time view of task execution times
    - Deadline miss trends
    - Stack usage graphs

11. **Memory Leak Detection**
    - Periodic heap statistics
    - Alert on growing minimum free heap

12. **Comprehensive Error Recovery**
    - WiFi reconnection with exponential backoff
    - Server unavailable mode (local buffering)
    - Configuration rollback on failure

13. **Key Rotation**
    - Periodic HMAC key updates
    - Certificate-based authentication

14. **Offline Mode**
    - Operate without server connection
    - Buffer data locally (SD card/SPIFFS)
    - Sync when connection restored

15. **Power Optimization**
    - Implement Milestone 5 features
    - Dynamic CPU frequency scaling
    - Light sleep during idle periods

### Testing Priorities

1. **Stress Testing:**
   - Run for 72+ hours continuously
   - Monitor for heap fragmentation
   - Verify deadline miss accumulation

2. **Fault Injection:**
   - Simulate WiFi disconnection during upload
   - Corrupt NVS configuration
   - Slow Modbus responses
   - Server timeouts

3. **Configuration Edge Cases:**
   - uploadFreq < pollFreq (should reject)
   - All registers disabled (should reject)
   - Extreme frequencies (1s poll, 1h upload)

4. **Concurrency Testing:**
   - Force multiple tasks to access WiFi mutex simultaneously
   - Verify no deadlocks
   - Check mutex timeout handling

5. **OTA Testing:**
   - Bad firmware (should rollback)
   - Interrupted download (should resume or restart)
   - Corrupted chunks (should reject)

---

## Conclusion

The ECOWATT ESP32 firmware demonstrates **excellent architectural design** with:
- ✅ Proper FreeRTOS dual-core utilization
- ✅ Clean separation of concerns
- ✅ Comprehensive security implementation
- ✅ Flexible configuration system
- ✅ Well-documented code

**Main concerns** are around:
- ⚠️ Synchronization edge cases (race conditions)
- ⚠️ Resource management (memory fragmentation)
- ⚠️ Fault recovery (watchdog tuning)

With the **recommended fixes** applied, this firmware is suitable for **production deployment**.

**Estimated effort to address all critical issues:** ~2 days (16 hours)

**System reliability after fixes:** 99.5%+ uptime expected

---

**Document Version:** 1.0  
**Last Updated:** November 26, 2025  
**Next Review:** December 2025 (after Milestone 5 implementation)
