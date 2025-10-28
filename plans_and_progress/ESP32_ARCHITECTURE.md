# ESP32 Architecture Documentation

**EcoWatt Smart Energy Monitoring System**  
**Team PowerPort**  
**Version:** 2.0.0  
**Date:** October 2025  
**Firmware Version:** 1.0.4

---

## Table of Contents

1. [Overview](#1-overview)
2. [Architecture Design](#2-architecture-design)
3. [Main Application Flow](#3-main-application-flow)
4. [Application Layer Modules](#4-application-layer-modules)
5. [Peripheral Layer](#5-peripheral-layer)
6. [Driver Layer](#6-driver-layer)
7. [Hardware Abstraction](#7-hardware-abstraction)
8. [State Machine & Petri Nets](#8-state-machine--petri-nets)
9. [Data Flow](#9-data-flow)
10. [Configuration & Storage](#10-configuration--storage)
11. [Security Implementation](#11-security-implementation)
12. [Power Management](#12-power-management)
13. [Troubleshooting](#13-troubleshooting)

---

## 1. Overview

### 1.1 Purpose

The ESP32 firmware is the embedded component of the EcoWatt system, responsible for:
- Real-time sensor data acquisition from energy monitoring hardware
- Multi-algorithm data compression to reduce bandwidth
- Secure data transmission to Flask backend
- Remote configuration management
- Over-The-Air (OTA) firmware updates with encryption
- Local diagnostics and fault detection

### 1.2 Milestone Coverage

This documentation covers milestones **M2, M3, and M4**:

- **M2 - Data Acquisition & Basic Compression**: Sensor polling, initial compression algorithms
- **M3 - Advanced Compression**: Dictionary, temporal, semantic, RLE, and adaptive compression
- **M4 - Security & OTA**: HMAC authentication, AES encryption, secure FOTA with rollback

**Note:** M5 (Power Management) is documented as a placeholder for future implementation.

### 1.3 System Specifications

- **Platform**: ESP32 (Xtensa dual-core LX6 microprocessor)
- **Framework**: Arduino/ESP-IDF hybrid
- **Build System**: PlatformIO
- **Memory**: 520KB SRAM, 4MB Flash
- **Connectivity**: WiFi 802.11 b/g/n
- **Security**: HMAC-SHA256, AES-256-CBC, RSA-2048
- **Storage**: NVS (Non-Volatile Storage) for persistent configuration

---

## 2. Architecture Design

### 2.1 Layered Architecture

The firmware follows a clean, modular three-layer architecture:

```
┌─────────────────────────────────────────────────────────┐
│                    APPLICATION LAYER                     │
│  (Business Logic, Data Processing, Network Communication)│
├─────────────────────────────────────────────────────────┤
│                    PERIPHERAL LAYER                      │
│        (Hardware Abstraction, Sensor Interfaces)         │
├─────────────────────────────────────────────────────────┤
│                      DRIVER LAYER                        │
│           (Low-level Hardware Access, Protocols)         │
└─────────────────────────────────────────────────────────┘
```

**Design Principles:**
- **Separation of Concerns**: Each layer has distinct responsibilities
- **Loose Coupling**: Layers communicate through well-defined interfaces
- **High Cohesion**: Related functionality grouped within modules
- **Testability**: Modular design enables unit testing
- **Maintainability**: Clear structure simplifies debugging and updates

### 2.2 Directory Structure

```
PIO/ECOWATT/
├── src/
│   ├── main.cpp                      # Application entry point
│   ├── application/                  # Application layer modules
│   │   ├── OTAManager.cpp            # OTA update management
│   │   ├── compression.cpp           # Data compression algorithms
│   │   ├── security.cpp              # Security layer (HMAC/AES)
│   │   ├── task_coordinator.cpp      # Timer-based task scheduling
│   │   ├── data_pipeline.cpp         # Data acquisition & processing
│   │   ├── data_uploader.cpp         # Network upload manager
│   │   ├── command_executor.cpp      # Remote command execution
│   │   ├── config_manager.cpp        # Configuration synchronization
│   │   ├── statistics_manager.cpp    # Performance metrics
│   │   ├── diagnostics.cpp           # System diagnostics
│   │   ├── nvs.cpp                   # NVS storage wrapper
│   │   ├── power_management.cpp      # Power management (M5 placeholder)
│   │   └── keys.cpp                  # Cryptographic keys
│   ├── peripheral/                   # Peripheral layer
│   │   ├── acquisition.cpp           # Sensor data acquisition
│   │   ├── arduino_wifi.cpp          # WiFi management
│   │   └── print.cpp                 # Debug logging
│   └── driver/                       # Driver layer
│       ├── debug.cpp                 # UART debug driver
│       ├── delay.cpp                 # Timing primitives
│       └── protocol_adapter.cpp      # Protocol conversion
├── include/
│   ├── application/                  # Application headers
│   ├── peripheral/                   # Peripheral headers
│   ├── driver/                       # Driver headers
│   ├── config/                       # Configuration files
│   └── hal/                          # Hardware abstraction
├── test/
│   └── test_m4_integration/          # Integration tests
└── platformio.ini                    # Build configuration
```

### 2.3 Module Dependencies

```
main.cpp
    ├── system_initializer (WiFi, NVS, Security)
    ├── task_coordinator (Hardware timers)
    ├── data_pipeline (Acquisition + Compression)
    ├── data_uploader (HTTP + Security)
    ├── command_executor (Remote commands)
    ├── config_manager (Configuration sync)
    ├── statistics_manager (Metrics)
    └── OTAManager (Firmware updates)
        ├── security (AES decryption)
        └── nvs (Progress persistence)
```

---

## 3. Main Application Flow

### 3.1 main.cpp Overview

The `main.cpp` file is the firmware entry point, implementing a Petri net-inspired state machine with token-based task coordination.

**File:** `src/main.cpp`  
**Version:** 2.0.0 (Modular Architecture)  
**Firmware:** 1.0.4

### 3.2 Setup Phase

The `setup()` function initializes all system components in a specific order:

```cpp
void setup() {
    // 1. Serial & Debug
    Serial.begin(115200);
    print_init();
    
    // 2. System Initialization (WiFi, NVS, Security)
    SystemInitializer::initializeAll();
    
    // 3. OTA Manager
    otaManager = new OTAManager(
        FLASK_SERVER_URL ":5001",
        "ESP32_EcoWatt_Smart",
        FIRMWARE_VERSION
    );
    otaManager->handleRollback();
    
    // 4. Load Configuration from NVS
    uint64_t pollFreq = nvs::getPollFreq();        // Default: 1s
    uint64_t uploadFreq = nvs::getUploadFreq();    // Default: 10s
    uint64_t configCheckFreq = 5000000ULL;         // 5s
    uint64_t otaCheckFreq = 60000000ULL;           // 1min
    
    size_t registerCount = nvs::getReadRegCount();
    const RegID* selection = nvs::getReadRegs();
    
    // 5. Task Coordinator (Hardware Timers)
    TaskCoordinator::init(pollFreq, uploadFreq, configCheckFreq, otaCheckFreq);
    
    // 6. Data Pipeline
    DataPipeline::init(selection, registerCount);
    
    // 7. Data Uploader
    DataUploader::init(FLASK_SERVER_URL "/process", "ESP32_EcoWatt_Smart");
    
    // 8. Command Executor
    CommandExecutor::init(
        FLASK_SERVER_URL "/command/poll",
        FLASK_SERVER_URL "/command/result",
        "ESP32_EcoWatt_Smart"
    );
    
    // 9. Config Manager
    ConfigManager::init(FLASK_SERVER_URL "/changes", "ESP32_EcoWatt_Smart");
    
    // 10. Statistics Manager
    StatisticsManager::init();
    
    // 11. Compression Dictionary Enhancement
    enhanceDictionaryForOptimalCompression();
}
```

**Initialization Sequence:**
1. **Serial Communication**: 115200 baud for debugging
2. **System Components**: WiFi connection, NVS storage, security keys
3. **OTA Manager**: Check for rollback scenarios from previous update failures
4. **Configuration Load**: Retrieve persistent settings from NVS
5. **Timer Setup**: Initialize 4 hardware timers for task scheduling
6. **Data Pipeline**: Configure sensor acquisition and compression
7. **Network Uploaders**: Setup HTTP clients for data transmission
8. **Remote Management**: Initialize command and config sync endpoints
9. **Metrics**: Start performance tracking
10. **Compression Optimization**: Preload compression dictionary patterns

### 3.3 Main Loop (Petri Net State Machine)

The `loop()` function implements a token-based task scheduling system inspired by Petri nets:

```cpp
void loop() {
    // STATE 1: POLLING (Sensor Data Acquisition)
    if (TaskCoordinator::isPollReady()) {
        TaskCoordinator::resetPollToken();
        DataPipeline::pollAndProcess();
    }
    
    // STATE 2: UPLOADING (Data Transmission)
    if (TaskCoordinator::isUploadReady()) {
        TaskCoordinator::resetUploadToken();
        DataUploader::uploadPendingData();
        
        // Apply pending configuration changes after upload
        if (!pollFreq_uptodate) {
            uint64_t newFreq = nvs::getPollFreq();
            TaskCoordinator::updatePollFrequency(newFreq);
            pollFreq_uptodate = true;
        }
        
        if (!uploadFreq_uptodate) {
            uint64_t newFreq = nvs::getUploadFreq();
            TaskCoordinator::updateUploadFrequency(newFreq);
            uploadFreq_uptodate = true;
        }
        
        if (!registers_uptodate) {
            size_t newCount = nvs::getReadRegCount();
            const RegID* newSelection = nvs::getReadRegs();
            DataPipeline::updateRegisterSelection(newSelection, newCount);
            registers_uptodate = true;
        }
    }
    
    // STATE 3: CONFIG CHECK (Remote Configuration Sync)
    if (TaskCoordinator::isChangesReady()) {
        TaskCoordinator::resetChangesToken();
        ConfigManager::checkForChanges(&registers_uptodate, &pollFreq_uptodate, &uploadFreq_uptodate);
        
        // Check for commands every 2nd config check (every 10s)
        command_poll_counter++;
        if (command_poll_counter >= 2) {
            command_poll_counter = 0;
            CommandExecutor::checkAndExecuteCommands();
        }
    }
    
    // STATE 4: OTA CHECK (Firmware Update Check)
    if (TaskCoordinator::isOTAReady()) {
        TaskCoordinator::resetOTAToken();
        performOTAUpdate();
    }
    
    yield();  // Yield to background tasks (WiFi, etc.)
}
```

**Token-Based Scheduling:**
- **Tokens**: Set by hardware timer ISRs, consumed by main loop
- **Non-blocking**: Each task checks its token and executes if ready
- **Priority**: Implicit priority based on check order (Poll → Upload → Config → OTA)
- **Yield**: Ensures background tasks (WiFi stack) get CPU time

### 3.4 Task Timing Diagram

```
Time (seconds):  0     1     2     3     4     5     6     7     8     9    10
                 │     │     │     │     │     │     │     │     │     │     │
Poll Task:       ●─────●─────●─────●─────●─────●─────●─────●─────●─────●─────●  (1s interval)
Upload Task:     ●───────────────────────●───────────────────────●            (10s interval)
Config Check:    ●─────────────●─────────────●─────────────●─────────────●    (5s interval)
Command Check:   ●───────────────────────●───────────────────────●            (10s interval)
OTA Check:       ●───────────────────────────────────────────────────(60s)    (1min interval)

Legend:
● = Task execution point
─ = Waiting period
```

### 3.5 State Transitions

The firmware operates in a cyclical state machine:

```
┌─────────────┐
│    IDLE     │  (Waiting for tokens)
└──────┬──────┘
       │ pollToken = true
       ▼
┌─────────────┐
│   POLLING   │  Read sensors, add to batch
└──────┬──────┘
       │ uploadToken = true
       ▼
┌─────────────┐
│  UPLOADING  │  Compress batch, send to server, apply config
└──────┬──────┘
       │ changesToken = true
       ▼
┌─────────────┐
│CONFIG CHECK │  Sync configuration, check commands
└──────┬──────┘
       │ otaToken = true
       ▼
┌─────────────┐
│  OTA CHECK  │  Check for firmware updates
└──────┬──────┘
       │
       ▼
   (Return to IDLE)
```

**State Descriptions:**

1. **IDLE**: Main loop waiting for any timer to fire
2. **POLLING**: Acquire sensor data, buffer samples, compress if batch full
3. **UPLOADING**: Transmit compressed data, apply pending configuration changes
4. **CONFIG CHECK**: Query server for configuration updates, poll for remote commands
5. **OTA CHECK**: Query server for firmware updates, download and apply if available

### 3.6 OTA Update Flow

When an OTA update is detected, the system pauses normal operations:

```cpp
void performOTAUpdate() {
    if (otaManager->checkForUpdate()) {
        // Pause all periodic tasks
        TaskCoordinator::pauseAllTasks();
        
        // Download and apply firmware
        if (otaManager->downloadAndApplyFirmware()) {
            // Verify and reboot to new firmware
            otaManager->verifyAndReboot();
        } else {
            // Resume normal operations on failure
            TaskCoordinator::resumeAllTasks();
        }
    }
}
```

**OTA Update Steps:**
1. Check server for new firmware version
2. Pause all tasks to prevent data corruption
3. Download encrypted firmware chunks (987 chunks × 1024 bytes)
4. Decrypt chunks using AES-256-CBC
5. Verify SHA-256 hash and RSA signature
6. Write to OTA partition
7. Mark partition as bootable
8. Reboot to new firmware
9. On boot, verify firmware or rollback

---

## 4. Application Layer Modules

### 4.1 Task Coordinator

**File:** `src/application/task_coordinator.cpp`  
**Header:** `include/application/task_coordinator.h`  
**Purpose:** Hardware timer management and token-based task scheduling

#### 4.1.1 Overview

The Task Coordinator manages four ESP32 hardware timers to orchestrate periodic tasks. It uses a token-based approach where ISRs set volatile flags that the main loop checks.

#### 4.1.2 Hardware Timers

```cpp
// Timer allocation
Timer 0: Poll Timer      (Default: 1,000,000 μs = 1 second)
Timer 1: Upload Timer    (Default: 10,000,000 μs = 10 seconds)
Timer 2: Config Timer    (Default: 5,000,000 μs = 5 seconds)
Timer 3: OTA Timer       (Default: 60,000,000 μs = 60 seconds)
```

**Timer Configuration:**
- **Prescaler**: 80 (1 μs per tick at 80 MHz)
- **Auto-reload**: Enabled (periodic mode)
- **Edge-triggered**: Rising edge
- **ISR Priority**: High (IRAM_ATTR for fast execution)

#### 4.1.3 Key Functions

**Initialization:**
```cpp
bool TaskCoordinator::init(uint64_t pollFreqUs, uint64_t uploadFreqUs, 
                          uint64_t changesFreqUs, uint64_t otaFreqUs);
```
- Allocates and configures all 4 hardware timers
- Attaches ISR handlers to each timer
- Enables timer alarms
- Returns `false` if any timer fails to initialize

**Token Management:**
```cpp
bool isPollReady()      // Check if poll token is set
bool isUploadReady()    // Check if upload token is set
bool isChangesReady()   // Check if config token is set
bool isOTAReady()       // Check if OTA token is set

void resetPollToken()   // Clear poll token (consume)
void resetUploadToken() // Clear upload token
void resetChangesToken()// Clear config token
void resetOTAToken()    // Clear OTA token
```

**Frequency Updates:**
```cpp
void updatePollFrequency(uint64_t newFreqUs);
void updateUploadFrequency(uint64_t newFreqUs);
void updateChangesFrequency(uint64_t newFreqUs);
void updateOTAFrequency(uint64_t newFreqUs);
```
- Dynamically adjust timer frequencies without reinitialization
- Used when remote configuration changes polling/upload intervals

**Task Control:**
```cpp
void pauseAllTasks();     // Disable all timer alarms
void resumeAllTasks();    // Enable all timer alarms
void pauseTask(TaskType); // Pause specific task
void resumeTask(TaskType);// Resume specific task
```
- Critical for OTA updates (pause tasks during firmware download)
- Prevents race conditions during system reconfiguration

#### 4.1.4 ISR Handlers

```cpp
void IRAM_ATTR TaskCoordinator::onPollTimer() {
    pollToken = true;  // Set token, main loop will consume
}

void IRAM_ATTR TaskCoordinator::onUploadTimer() {
    uploadToken = true;
}

void IRAM_ATTR TaskCoordinator::onChangesTimer() {
    changesToken = true;
}

void IRAM_ATTR TaskCoordinator::onOTATimer() {
    otaToken = true;
}
```

**Design Notes:**
- ISRs are minimal (single assignment) for fast execution
- `IRAM_ATTR` places ISR in IRAM for reliability during flash operations
- Volatile flags ensure visibility across ISR/main loop contexts

---

### 4.2 OTA Manager

**File:** `src/application/OTAManager.cpp`  
**Header:** `include/application/OTAManager.h`  
**Purpose:** Secure over-the-air firmware updates with encryption and rollback

#### 4.2.1 Overview

The OTA Manager handles the complete firmware update lifecycle:
- Check for updates from Flask server
- Download encrypted firmware chunks
- Decrypt with AES-256-CBC
- Verify with SHA-256 hash and RSA signature
- Write to OTA partition
- Reboot and verify, or rollback on failure

#### 4.2.2 OTA States

```cpp
enum OTAState {
    OTA_IDLE,           // No update in progress
    OTA_CHECKING,       // Checking for updates
    OTA_DOWNLOADING,    // Downloading firmware chunks
    OTA_DECRYPTING,     // Decrypting downloaded chunks
    OTA_VERIFYING,      // Verifying signature and hash
    OTA_UPDATING,       // Writing to OTA partition
    OTA_COMPLETE,       // Update complete, ready to reboot
    OTA_FAILED,         // Update failed
    OTA_ROLLBACK        // Rolling back to previous firmware
};
```

#### 4.2.3 Key Functions

**Update Check:**
```cpp
bool checkForUpdate();
```
- Queries `/ota/check/<device_id>?version=<current_version>`
- Parses manifest from server response
- Returns `true` if new firmware available

**Firmware Download:**
```cpp
bool downloadAndApplyFirmware();
```
- Downloads firmware in chunks (default: 1024 bytes)
- Total chunks: 987 for ~1MB firmware
- Supports resume from partial download (NVS-backed progress)
- Decrypts each chunk with AES-256-CBC
- Writes decrypted data to OTA partition

**Verification and Reboot:**
```cpp
bool verifyAndReboot();
```
- Verifies SHA-256 hash of downloaded firmware
- Validates RSA-2048 signature (server authentication)
- Marks OTA partition as bootable
- Reboots ESP32 to new firmware

**Rollback Handling:**
```cpp
void handleRollback();
```
- Called during `setup()` after every boot
- Checks if previous firmware is marked for rollback
- If rollback flag set, marks previous partition as bootable
- Clears rollback markers on successful boot

#### 4.2.4 Encryption Details

**AES-256-CBC Decryption:**
```cpp
// Key: 32 bytes (from keys.cpp, matches Flask aes_firmware_key.bin)
const uint8_t AES_FIRMWARE_KEY[32] = {
    0xA2, 0x7B, 0xB3, 0xDA, 0x29, 0x42, 0x44, 0xCB,
    0x28, 0x1D, 0x67, 0x8E, 0x40, 0x1F, 0x9C, 0xD5,
    0xB2, 0x35, 0xE8, 0x91, 0x3A, 0x4C, 0x7D, 0x29,
    0x5F, 0x88, 0x2C, 0x14, 0x6B, 0xA9, 0xF3, 0x57
};

// IV: 16 bytes (from manifest, generated per firmware)
uint8_t aes_iv[16];  // Loaded from server manifest
```

**Decryption Process:**
1. Initialize mbedtls AES context
2. Set decryption key (32 bytes)
3. Load IV from manifest (unique per firmware version)
4. Decrypt each 1024-byte chunk
5. Handle PKCS#7 padding removal on last chunk
6. Write decrypted data to OTA partition

#### 4.2.5 Progress Persistence

OTA progress is saved to NVS to support resume after unexpected power loss:

```cpp
struct OTAProgress {
    OTAState state;
    uint32_t chunks_received;
    uint32_t total_chunks;
    uint32_t bytes_downloaded;
    uint8_t percentage;
    String error_message;
};

void saveProgress();    // Save to NVS
void loadProgress();    // Load from NVS
void clearProgress();   // Clear NVS state
```

**NVS Keys:**
- `ota_state`: Current OTA state
- `ota_chunks`: Chunks downloaded
- `ota_bytes`: Bytes downloaded
- `ota_version`: Target firmware version

#### 4.2.6 Fault Injection (Testing)

The OTA Manager includes a test mode for fault injection:

```cpp
enum OTAFaultType {
    OTA_FAULT_NONE,
    OTA_FAULT_DOWNLOAD_FAIL,
    OTA_FAULT_DECRYPT_FAIL,
    OTA_FAULT_VERIFY_FAIL
};

void enableTestMode(OTAFaultType fault);
```

**Test Scenarios:**
- Simulate download failures at specific chunks
- Inject decryption errors
- Force verification failures
- Test rollback mechanisms

#### 4.2.7 Error Handling

```cpp
void setError(const String& message);
String getLastError();
```

**Common Errors:**
- "WiFi not connected" - No network connectivity
- "Invalid JSON response" - Server communication error
- "Memory allocation failed" - Insufficient heap
- "Decryption failed" - Wrong AES key or corrupted data
- "Signature verification failed" - Firmware tampered or wrong key
- "OTA partition not found" - Flash partition table error

---

### 4.3 Compression Module

**File:** `src/application/compression.cpp`  
**Header:** `include/application/compression.h`  
**Purpose:** Multi-algorithm data compression for bandwidth optimization

#### 4.3.1 Overview

The Compression Module implements 5 compression algorithms with adaptive selection:

1. **Dictionary Compression** - Pattern matching against learned sensor sequences
2. **Temporal Delta** - Encode differences between consecutive samples
3. **Semantic RLE** - Run-length encoding for repeated values
4. **Adaptive Bitpacking** - Variable bit-width encoding
5. **Smart Selection** - Automatically choose best algorithm per batch

#### 4.3.2 Compression Algorithms

**1. Dictionary Compression (Header: 0xD0)**

```cpp
std::vector<uint8_t> compressWithDictionary(uint16_t* data, const RegID* selection, size_t count);
```

- Maintains dictionary of common sensor patterns (16 patterns max)
- Each pattern: 6 uint16_t values (one complete sensor reading)
- Encodes data as: `[0xD0][pattern_index][delta_bytes]`
- Best for repetitive sensor readings

**Pattern Learning:**
```cpp
void updateDictionary(uint16_t* data, const RegID* selection, size_t count);
```
- Adds new patterns when unique readings detected
- Learning rate: Configurable (default: 0.1)
- Dictionary size: 16 patterns (fits in 192 bytes)

**2. Temporal Delta (Header: 0x70/0x71)**

```cpp
std::vector<uint8_t> compressWithTemporalDelta(uint16_t* data, const RegID* selection, size_t count);
```

- Encodes first sample as baseline
- Subsequent samples encoded as delta from previous
- Two modes:
  - 0x70: Small deltas (1 byte per value)
  - 0x71: Large deltas (2 bytes per value)
- Best for slowly changing sensor values

**Temporal Window:**
- Maintains window of last 8 samples
- Uses window to predict next value
- Encodes prediction error instead of raw delta

**3. Semantic RLE (Header: 0x50)**

```cpp
std::vector<uint8_t> compressWithSemanticRLE(uint16_t* data, const RegID* selection, size_t count);
```

- Run-length encoding for repeated values
- Format: `[0x50][value][count][value][count]...`
- Best for sensor readings that stay constant

**4. Adaptive Bitpacking (Header: 0x01)**

```cpp
std::vector<uint8_t> compressBinary(uint16_t* data, size_t count);
```

- Analyzes data range to determine bit width
- Packs values into minimal bits
- Supports 8, 12, 14, 16-bit packing
- Best for data with limited range

**5. Smart Selection (Adaptive)**

```cpp
std::vector<uint8_t> compressWithSmartSelection(uint16_t* data, const RegID* selection, size_t count);
```

- Tests all 4 algorithms on data
- Selects algorithm with best compression ratio
- Tracks performance metrics per algorithm
- Adapts over time based on historical performance

**Selection Process:**
1. Test dictionary compression → calculate ratio
2. Test temporal delta → calculate ratio
3. Test semantic RLE → calculate ratio
4. Test bitpacking → calculate ratio
5. Choose algorithm with lowest ratio (best compression)
6. Update performance statistics
7. Return compressed data with algorithm header

#### 4.3.3 Compression Metrics

```cpp
struct CompressionResult {
    std::vector<uint8_t> data;  // Compressed data
    String method;               // Algorithm used
    float academicRatio;         // compressed / original
    float traditionalRatio;      // original / compressed
    unsigned long timeUs;        // Compression time (μs)
};
```

**Performance Tracking:**
```cpp
struct MethodPerformance {
    const char* name;
    uint32_t useCount;           // Times selected
    float avgRatio;              // Average compression ratio
    uint32_t totalCompressions;  // Total attempts
    float avgTimeUs;             // Average time
    float successRate;           // Success rate
};
```

#### 4.3.4 Configuration

```cpp
// Compression parameters (configurable)
static size_t maxMemoryUsage;              // Max buffer size
static float compressionPreference;        // Speed vs ratio preference
static uint16_t largeDeltaThreshold;       // Delta size threshold
static uint8_t bitPackingThreshold;        // Bitpacking trigger
static float dictionaryLearningRate;       // Dictionary update rate
static uint8_t temporalWindowSize;         // Temporal window size
```

#### 4.3.5 Decompression

Each algorithm has a corresponding decompression function:

```cpp
std::vector<uint16_t> decompress(const uint8_t* compressed, size_t compressedSize);
std::vector<uint16_t> decompressDictionary(const uint8_t* data, size_t size);
std::vector<uint16_t> decompressTemporalDelta(const uint8_t* data, size_t size);
std::vector<uint16_t> decompressSemanticRLE(const uint8_t* data, size_t size);
std::vector<uint16_t> decompressBinary(const uint8_t* data, size_t size);
```

**Decompression Process:**
1. Read header byte to identify algorithm
2. Call appropriate decompression function
3. Reconstruct original uint16_t array
4. Return decompressed data

#### 4.3.6 Error Handling

```cpp
enum ErrorType {
    ERROR_NONE,
    ERROR_INVALID_INPUT,
    ERROR_MEMORY_ALLOCATION,
    ERROR_COMPRESSION_FAILED,
    ERROR_DECOMPRESSION_FAILED,
    ERROR_INVALID_FORMAT
};

static void setError(const String& message, ErrorType type);
static String getLastError();
static ErrorType getLastErrorType();
```

---

### 4.4 Security Layer

**File:** `src/application/security.cpp`  
**Header:** `include/application/security.h`  
**Purpose:** HMAC authentication and payload encryption for secure communication

#### 4.4.1 Overview

The Security Layer provides cryptographic protection for all data sent to the Flask server:
- **HMAC-SHA256** for message authentication
- **Nonce-based** replay protection
- **Base64 encoding** for binary data transport
- **AES-256-CBC** encryption (planned, currently mock mode)

#### 4.4.2 Pre-Shared Keys

All keys are synchronized with Flask server:

```cpp
// HMAC Key (32 bytes) - MUST match server PSK_HMAC
const uint8_t PSK_HMAC[32] = {
    0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
    0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c,
    0x76, 0x2e, 0x71, 0x60, 0xf3, 0x8b, 0x4d, 0xa5,
    0x6a, 0x78, 0x4d, 0x90, 0x45, 0x19, 0x0c, 0xfe
};

// AES Key (16 bytes) - For future payload encryption
const uint8_t PSK_AES[16] = {
    0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
    0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c
};

// AES IV (16 bytes)
const uint8_t AES_IV[16] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f
};
```

#### 4.4.3 Nonce Management

Nonces prevent replay attacks:

```cpp
static uint32_t currentNonce;  // Persistent, increments with each request

void init();                          // Load nonce from NVS
uint32_t incrementAndSaveNonce();     // Increment and persist
uint32_t getCurrentNonce();           // Get current value
void setNonce(uint32_t nonce);        // Manually set (for testing)
```

**Nonce Lifecycle:**
1. Initialize from NVS (default: 10000)
2. Increment before each secure transmission
3. Save to NVS for persistence across reboots
4. Server validates nonce is greater than last seen

#### 4.4.4 Payload Securing

```cpp
bool securePayload(const char* payload, char* securedPayload, size_t securedPayloadSize);
```

**Securing Process:**

1. **Increment Nonce**
   ```cpp
   uint32_t nonce = incrementAndSaveNonce();
   ```

2. **Encode Payload** (Base64 mock or AES encryption)
   ```cpp
   String encodedPayload = base64::encode((const uint8_t*)payload, payloadLen);
   ```

3. **Calculate HMAC** over (nonce + original payload)
   ```cpp
   // Construct: [nonce (4 bytes big-endian)][payload]
   uint8_t dataToSign[4 + payloadLen];
   dataToSign[0] = (nonce >> 24) & 0xFF;
   dataToSign[1] = (nonce >> 16) & 0xFF;
   dataToSign[2] = (nonce >> 8) & 0xFF;
   dataToSign[3] = nonce & 0xFF;
   memcpy(dataToSign + 4, payload, payloadLen);
   
   // HMAC-SHA256
   calculateHMAC(dataToSign, dataLen, hmac);  // 32-byte output
   ```

4. **Build Secured JSON**
   ```json
   {
       "nonce": 10001,
       "payload": "base64_encoded_data...",
       "mac": "hmac_hex_string...",
       "encrypted": false
   }
   ```

5. **Return Secured Payload**
   - Serialized JSON ready for HTTP transmission

#### 4.4.5 HMAC Calculation

```cpp
void calculateHMAC(const uint8_t* data, size_t dataLen, uint8_t* hmac);
```

**Implementation:**
- Uses mbedtls HMAC-SHA256
- Key: PSK_HMAC (32 bytes)
- Output: 32-byte MAC
- Constant-time comparison for security

**HMAC Process:**
```cpp
mbedtls_md_context_t ctx;
mbedtls_md_init(&ctx);
mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
mbedtls_md_hmac_starts(&ctx, PSK_HMAC, 32);
mbedtls_md_hmac_update(&ctx, data, dataLen);
mbedtls_md_hmac_finish(&ctx, hmac);
mbedtls_md_free(&ctx);
```

#### 4.4.6 Server Validation

Flask server validates secured payloads:

1. Extract nonce from JSON
2. Check nonce > last seen nonce (replay protection)
3. Decode Base64 payload
4. Recalculate HMAC over (nonce + decoded payload)
5. Compare received MAC with calculated MAC
6. Accept if MAC matches, reject otherwise

#### 4.4.7 Security Configuration

```cpp
#define ENABLE_ENCRYPTION false  // Mock mode (Base64 only)
// Set to true for AES-256-CBC encryption
```

**Mock Mode (Current):**
- Payload encoded with Base64 (not encrypted)
- HMAC provides authentication only
- Suitable for development/testing

**Full Security Mode (Planned):**
- Payload encrypted with AES-256-CBC
- HMAC provides authentication
- Base64 encode encrypted payload

---

### 4.5 Data Pipeline

**File:** `src/application/data_pipeline.cpp`  
**Header:** `include/application/data_pipeline.h`  
**Purpose:** Sensor data acquisition, batching, and compression

#### 4.5.1 Overview

The Data Pipeline orchestrates the flow from sensor reading to compressed data ready for upload:
1. Read sensors based on register selection
2. Add samples to batch buffer
3. When batch full (10 samples), compress
4. Queue compressed data for upload

#### 4.5.2 Key Functions

**Initialization:**
```cpp
static void init(const RegID* selection, size_t registerCount);
```
- Sets active register selection (e.g., [V_RMS, I_RMS, P_ACTIVE])
- Allocates sensor buffer (6 registers × uint16_t)
- Initializes batch structure (10 samples capacity)

**Poll and Process:**
```cpp
static void pollAndProcess();
```
- Called every poll interval (default: 1 second)
- Reads all selected registers from sensors
- Adds reading to batch
- If batch full (10 samples), compress and queue

**Register Update:**
```cpp
static void updateRegisterSelection(const RegID* newSelection, size_t newCount);
```
- Updates register selection from remote configuration
- Clears current batch (data format changed)
- Allocates new sensor buffer

**Force Compression:**
```cpp
static bool forceCompressBatch();
```
- Manually compress current batch (even if not full)
- Useful for testing or graceful shutdown

#### 4.5.3 Sample Batch Structure

```cpp
struct SampleBatch {
    uint16_t** samples;       // Array of sample arrays
    size_t sampleCount;       // Current number of samples
    size_t maxSamples;        // Maximum capacity (10)
    size_t registersPerSample;// Number of registers per sample (6)
    unsigned long timestamps[10]; // Timestamp for each sample
    
    bool addSample(const uint16_t* data);
    bool isFull();
    void clear();
    void toLinearArray(uint16_t* output);
};
```

**Example Batch:**
```
Sample 0: [2429, 177, 73, 4331, 70, 605]  // V_RMS, I_RMS, P_ACTIVE, etc.
Sample 1: [2430, 178, 74, 4345, 71, 606]
Sample 2: [2428, 177, 73, 4328, 70, 604]
...
Sample 9: [2431, 179, 75, 4350, 72, 607]
```

**Batch to Linear Array:**
```cpp
void toLinearArray(uint16_t* output);
// Converts: samples[10][6] → output[60]
// Used for compression algorithms
```

#### 4.5.4 Sensor Reading

```cpp
static bool readSensors(uint16_t* buffer);
```

**Reading Process:**
1. Call `readRequest()` from acquisition module
2. Parse `DecodedValues` structure
3. Copy values to buffer
4. Validate read count matches expected
5. Return success/failure

**Register IDs (RegID enum):**
```cpp
enum RegID {
    V_RMS,       // RMS Voltage
    I_RMS,       // RMS Current
    P_ACTIVE,    // Active Power
    P_REACTIVE,  // Reactive Power
    P_APPARENT,  // Apparent Power
    FREQUENCY    // Line Frequency
};
```

#### 4.5.5 Compression and Queueing

```cpp
static bool compressAndQueue();
```

**Process:**
1. Convert batch to linear array (60 uint16_t values)
2. Create register selection array for batch
3. Call `DataCompression::compressWithSmartSelection()`
4. Smart selection tests all algorithms, chooses best
5. Add compressed data to upload queue
6. Clear batch for next collection
7. Print compression statistics

**Example Output:**
```
Batch full (10 samples), compressing...
COMPRESSION RESULT: TEMPORAL method
Original: 120 bytes -> Compressed: 32 bytes (73.3% savings)
Academic Ratio: 0.267 | Time: 1850 μs
Data queued for upload (32 bytes)
```

#### 4.5.6 Data Flow Diagram

```
┌──────────────┐
│   Sensors    │  (PZEM-004T via Modbus)
└──────┬───────┘
       │ readRequest()
       ▼
┌──────────────┐
│ Acquisition  │  Parse sensor values
└──────┬───────┘
       │ uint16_t[6]
       ▼
┌──────────────┐
│    Batch     │  Accumulate 10 samples
└──────┬───────┘
       │ uint16_t[60]
       ▼
┌──────────────┐
│ Compression  │  Smart algorithm selection
└──────┬───────┘
       │ std::vector<uint8_t>
       ▼
┌──────────────┐
│ Upload Queue │  Ready for transmission
└──────────────┘
```

---

### 4.6 Data Uploader

**File:** `src/application/data_uploader.cpp`  
**Header:** `include/application/data_uploader.h`  
**Purpose:** HTTP transmission of compressed data to Flask server

#### 4.6.1 Overview

The Data Uploader manages the network transmission of queued compressed data:
- Maintains upload queue (FIFO)
- Applies security layer (HMAC)
- Sends HTTP POST to Flask `/aggregated/<device_id>` endpoint
- Retries on failure with exponential backoff

#### 4.6.2 Key Functions

**Initialization:**
```cpp
static void init(const String& serverURL, const String& deviceID);
```
- Sets Flask server URL (e.g., "http://192.168.1.100:5001/process")
- Sets device identifier ("ESP32_EcoWatt_Smart")
- Initializes upload queue

**Upload Pending Data:**
```cpp
static void uploadPendingData();
```
- Called every upload interval (default: 10 seconds)
- Dequeues oldest compressed batch
- Wraps in JSON with metadata
- Applies security layer (HMAC + nonce)
- Sends HTTP POST
- Retries up to 3 times on failure

**Queue Management:**
```cpp
static void enqueueData(const std::vector<uint8_t>& compressedData);
static size_t getQueueSize();
static void clearQueue();
```

#### 4.6.3 Payload Format

**Before Security Layer:**
```json
{
    "device_id": "ESP32_EcoWatt_Smart",
    "timestamp": 1698527400,
    "data": [0xD0, 0x03, 0x01, 0x02, ...],
    "compression_method": "TEMPORAL",
    "original_size": 120,
    "compressed_size": 32,
    "compression_ratio": 0.267
}
```

**After Security Layer:**
```json
{
    "nonce": 10523,
    "payload": "eyJkZXZpY2VfaWQiOiJFU1AzMl9FY29XYXR0X1NtYXJ0Iiw...",
    "mac": "a7f3d8e9c2b1f4a6d8e9c2b1f4a6d8e9c2b1f4a6d8e9c2b1",
    "encrypted": false
}
```

#### 4.6.4 HTTP Transmission

```cpp
bool sendToServer(const char* endpoint, const char* securedPayload);
```

**Transmission Process:**
1. Create HTTPClient instance
2. Set endpoint URL
3. Add headers (Content-Type: application/json)
4. POST secured payload
5. Check response code (200 = success)
6. Parse response for server acknowledgment
7. Return success/failure

**Error Handling:**
- Network errors: Retry with backoff
- Server errors (5xx): Retry
- Client errors (4xx): Log and discard (bad request)
- Timeout: 10 seconds per request

#### 4.6.5 Retry Strategy

```cpp
// Exponential backoff
Retry 1: Wait 1 second
Retry 2: Wait 2 seconds
Retry 3: Wait 4 seconds
// After 3 failures, discard data
```

**Retry Logic:**
- Transient errors (network, server busy): Retry
- Permanent errors (bad MAC, invalid nonce): Discard
- Queue overflow: Discard oldest to prevent memory exhaustion

---

### 4.7 Command Executor

**File:** `src/application/command_executor.cpp`  
**Header:** `include/application/command_executor.h`  
**Purpose:** Remote command polling and execution

#### 4.7.1 Overview

The Command Executor enables Flask server to remotely control the ESP32:
- Poll for pending commands (every 10 seconds)
- Execute commands locally
- Report results back to server

#### 4.7.2 Supported Commands

```cpp
enum CommandType {
    CMD_REBOOT,           // Reboot ESP32
    CMD_CLEAR_NVS,        // Clear NVS storage
    CMD_GET_DIAGNOSTICS,  // Collect diagnostics
    CMD_FORCE_OTA,        // Trigger OTA check
    CMD_SET_LOG_LEVEL,    // Change logging verbosity
    CMD_BENCHMARK_COMPRESSION // Run compression benchmark
};
```

#### 4.7.3 Key Functions

**Initialization:**
```cpp
static void init(const String& pollURL, const String& resultURL, const String& deviceID);
```
- Sets poll endpoint: `/command/poll`
- Sets result endpoint: `/command/result`
- Sets device identifier

**Check and Execute:**
```cpp
static void checkAndExecuteCommands();
```
- Called every 10 seconds (2× config check interval)
- Polls server for pending commands
- Executes commands sequentially
- Reports results back to server

**Command Execution:**
```cpp
static bool executeCommand(const String& commandType, const JsonObject& params);
```
- Parses command type and parameters
- Executes command locally
- Returns success/failure
- Captures output/errors

#### 4.7.4 Command Flow

```
ESP32                                    Flask Server
  │                                           │
  │─────────── GET /command/poll ───────────>│
  │<────── {"command": "reboot"} ────────────│
  │                                           │
  │ Execute reboot command                    │
  │                                           │
  │─── POST /command/result {"success"} ────>│
  │<────── {"status": "acknowledged"} ───────│
  │                                           │
  │ ESP32.restart()                           │
  └─ (reboot)                                 │
```

#### 4.7.5 Command Examples

**Reboot:**
```json
{
    "command_id": "cmd_12345",
    "command": "reboot",
    "params": {}
}
```

**Set Log Level:**
```json
{
    "command_id": "cmd_12346",
    "command": "set_log_level",
    "params": {
        "level": "DEBUG"
    }
}
```

**Benchmark Compression:**
```json
{
    "command_id": "cmd_12347",
    "command": "benchmark_compression",
    "params": {
        "iterations": 100
    }
}
```

#### 4.7.6 Result Reporting

```cpp
static bool reportResult(const String& commandID, bool success, const String& output);
```

**Result Payload:**
```json
{
    "device_id": "ESP32_EcoWatt_Smart",
    "command_id": "cmd_12345",
    "success": true,
    "output": "Rebooting in 3 seconds...",
    "timestamp": 1698527450
}
```

---

### 4.8 Config Manager

**File:** `src/application/config_manager.cpp`  
**Header:** `include/application/config_manager.h`  
**Purpose:** Remote configuration synchronization

#### 4.8.1 Overview

The Config Manager synchronizes ESP32 configuration with Flask server:
- Polls for configuration changes (every 5 seconds)
- Downloads new configuration
- Applies changes to local NVS
- Triggers reconfiguration of affected modules

#### 4.8.2 Configurable Parameters

```cpp
struct DeviceConfig {
    uint64_t pollFreq;       // Sensor polling frequency (μs)
    uint64_t uploadFreq;     // Data upload frequency (μs)
    RegID* registers;        // Register selection array
    size_t registerCount;    // Number of registers
    uint8_t logLevel;        // Logging verbosity
    bool compressionEnabled; // Enable/disable compression
};
```

#### 4.8.3 Key Functions

**Initialization:**
```cpp
static void init(const String& changesURL, const String& deviceID);
```
- Sets changes endpoint: `/changes`
- Sets device identifier

**Check for Changes:**
```cpp
static void checkForChanges(bool* registersFlag, bool* pollFreqFlag, bool* uploadFreqFlag);
```
- Polls server for configuration updates
- Compares server config with local NVS
- Sets flags for changed parameters
- Main loop applies changes after upload

**Apply Configuration:**
```cpp
static bool applyConfiguration(const DeviceConfig& config);
```
- Writes new config to NVS
- Sets update flags for main loop
- Configuration applied after next upload (safe point)

#### 4.8.4 Configuration Flow

```
┌──────────────────────────────────────────────────────┐
│                    Flask Server                       │
│  Admin updates config via REST API                    │
└──────────────────┬───────────────────────────────────┘
                   │
                   ▼
┌──────────────────────────────────────────────────────┐
│              ESP32 Config Manager                     │
│  GET /changes → Compare with local NVS                │
└──────────────────┬───────────────────────────────────┘
                   │ Detected changes
                   ▼
┌──────────────────────────────────────────────────────┐
│                 Set Update Flags                      │
│  registers_uptodate = false                           │
│  pollFreq_uptodate = false                            │
└──────────────────┬───────────────────────────────────┘
                   │
                   ▼
┌──────────────────────────────────────────────────────┐
│              Main Loop (Upload State)                 │
│  Apply changes after upload completes                 │
│  - Update TaskCoordinator frequencies                 │
│  - Update DataPipeline register selection             │
└───────────────────────────────────────────────────────┘
```

#### 4.8.5 Change Detection

```cpp
bool hasConfigChanged(const DeviceConfig& serverConfig, const DeviceConfig& localConfig);
```

**Comparison:**
- Poll frequency different?
- Upload frequency different?
- Register selection different (count or values)?
- Log level different?
- Compression settings different?

#### 4.8.6 Safe Reconfiguration

Configuration changes are applied **after upload** to prevent data loss:

1. Config check detects changes
2. Sets flags (`registers_uptodate = false`)
3. Main loop completes current operations
4. Upload state reached
5. Apply changes from NVS
6. Update TaskCoordinator and DataPipeline
7. Resume with new configuration

---

### 4.9 NVS Module

**File:** `src/application/nvs.cpp`  
**Header:** `include/application/nvs.h`  
**Purpose:** Non-volatile storage wrapper for persistent configuration

#### 4.9.1 Overview

The NVS (Non-Volatile Storage) module provides a clean interface for reading/writing persistent data to ESP32 flash:
- Configuration parameters (polling/upload frequencies)
- Register selections
- Security nonces
- OTA progress

#### 4.9.2 Stored Parameters

```cpp
namespace nvs {
    // Frequencies
    uint64_t getPollFreq();
    void setPollFreq(uint64_t freq);
    
    uint64_t getUploadFreq();
    void setUploadFreq(uint64_t freq);
    
    // Register Selection
    size_t getReadRegCount();
    const RegID* getReadRegs();
    void setReadRegs(const RegID* regs, size_t count);
    
    // Default Values
    const uint64_t DEFAULT_POLL_FREQ = 1000000ULL;    // 1 second
    const uint64_t DEFAULT_UPLOAD_FREQ = 10000000ULL; // 10 seconds
}
```

#### 4.9.3 NVS Namespaces

```cpp
Namespace: "config"
  - poll_freq (uint64_t)
  - upload_freq (uint64_t)
  - reg_count (size_t)
  - registers (blob)

Namespace: "security"
  - nonce (uint32_t)

Namespace: "ota"
  - ota_state (uint8_t)
  - ota_chunks (uint32_t)
  - ota_bytes (uint32_t)
  - ota_version (string)
```

#### 4.9.4 Preferences Wrapper

Uses Arduino `Preferences` library for NVS access:

```cpp
Preferences preferences;
preferences.begin("config", false);  // read-write
uint64_t freq = preferences.getULong64("poll_freq", DEFAULT_POLL_FREQ);
preferences.end();
```

#### 4.9.5 Default Configuration

If NVS is empty (first boot), defaults are used:
- Poll frequency: 1 second
- Upload frequency: 10 seconds
- Register selection: [V_RMS, I_RMS, P_ACTIVE, P_REACTIVE, P_APPARENT, FREQUENCY]
- Nonce: 10000

---

### 4.10 Statistics Manager

**File:** `src/application/statistics_manager.cpp`  
**Header:** `include/application/statistics_manager.h`  
**Purpose:** Performance metrics and diagnostics collection

#### 4.10.1 Overview

The Statistics Manager tracks system performance metrics:
- Uptime
- Memory usage (heap, stack)
- Network statistics (packets sent, failures, retries)
- Compression metrics (ratios, times, algorithm usage)
- Task execution times

#### 4.10.2 Tracked Metrics

```cpp
struct SystemStatistics {
    // System Info
    unsigned long uptimeSeconds;
    uint32_t freeHeap;
    uint32_t minFreeHeap;
    uint32_t heapFragmentation;
    
    // Network Stats
    uint32_t packetsSent;
    uint32_t packetsFailed;
    uint32_t packetsRetried;
    uint32_t bytesUploaded;
    
    // Compression Stats
    uint32_t compressions;
    float avgCompressionRatio;
    unsigned long avgCompressionTime;
    uint32_t algorithmUsage[4];  // DICT, TEMPORAL, SEMANTIC, BITPACK
    
    // Task Timing
    unsigned long avgPollTime;
    unsigned long avgUploadTime;
    unsigned long maxPollTime;
    unsigned long maxUploadTime;
};
```

#### 4.10.3 Key Functions

```cpp
static void init();
static void update();                              // Update all metrics
static SystemStatistics getStatistics();           // Get snapshot
static void printStatistics();                     // Debug output
static void resetStatistics();                     // Clear all metrics
```

#### 4.10.4 Usage Example

```cpp
// In main loop or diagnostics command
StatisticsManager::update();
SystemStatistics stats = StatisticsManager::getStatistics();

print("Uptime: %lu seconds\n", stats.uptimeSeconds);
print("Free Heap: %u bytes\n", stats.freeHeap);
print("Packets Sent: %u\n", stats.packetsSent);
print("Avg Compression Ratio: %.3f\n", stats.avgCompressionRatio);
```

---

### 4.11 Diagnostics Module

**File:** `src/application/diagnostics.cpp`  
**Header:** `include/application/diagnostics.h`  
**Purpose:** System health monitoring and fault detection

#### 4.11.1 Overview

The Diagnostics module provides system health checks and reporting:
- WiFi connection status
- Memory health
- Sensor communication
- Network latency
- Configuration integrity

#### 4.11.2 Diagnostic Tests

```cpp
enum DiagnosticTest {
    DIAG_WIFI,          // WiFi connectivity
    DIAG_MEMORY,        // Heap and stack health
    DIAG_SENSORS,       // Sensor communication
    DIAG_NETWORK,       // Server reachability
    DIAG_NVS,           // NVS integrity
    DIAG_SECURITY,      // Cryptographic functions
    DIAG_COMPRESSION    // Compression algorithms
};
```

#### 4.11.3 Key Functions

```cpp
static bool runDiagnostics();                      // Run all tests
static bool runTest(DiagnosticTest test);          // Run specific test
static String getDiagnosticsReport();              // JSON report
static void sendDiagnosticsToServer();             // Upload to Flask
```

#### 4.11.4 Diagnostics Report Format

```json
{
    "device_id": "ESP32_EcoWatt_Smart",
    "timestamp": 1698527500,
    "tests": {
        "wifi": {"status": "pass", "rssi": -65, "ip": "192.168.1.150"},
        "memory": {"status": "pass", "free_heap": 180000, "fragmentation": 12},
        "sensors": {"status": "pass", "read_time_ms": 45},
        "network": {"status": "pass", "latency_ms": 23},
        "nvs": {"status": "pass", "keys_valid": true},
        "security": {"status": "pass", "hmac_test": "ok"},
        "compression": {"status": "pass", "all_algorithms": "working"}
    },
    "overall": "healthy"
}
```

#### 4.11.5 Fault Detection

```cpp
struct FaultCondition {
    const char* name;
    bool (*detector)();     // Function to detect fault
    const char* remedy;     // Suggested fix
};
```

**Common Faults:**
- WiFi disconnected → Reconnect
- Heap critically low → Clear upload queue, reduce batch size
- Sensor read timeout → Reset Modbus interface
- Server unreachable → Buffer data locally, retry later
- NVS corruption → Reset to defaults

---

### 4.12 Power Management (M5 Placeholder)

**File:** `src/application/power_management.cpp`  
**Header:** `include/application/power_management.h`  
**Purpose:** Power optimization for battery-operated scenarios (M5 future work)

#### 4.12.1 Overview

The Power Management module is a **placeholder for Milestone 5**. Planned features include:
- Deep sleep modes
- WiFi power saving
- Dynamic frequency scaling
- Wake-on-timer
- Battery level monitoring

#### 4.12.2 Planned Functions (Not Implemented)

```cpp
static void init();
static void enterDeepSleep(uint64_t sleepTimeUs);
static void enterLightSleep(uint64_t sleepTimeUs);
static void setWiFiPowerSave(bool enable);
static void setCPUFrequency(uint32_t freqMHz);
static uint8_t getBatteryLevel();
static void setLowPowerMode(bool enable);
```

#### 4.12.3 Deep Sleep Strategy (Planned)

```
Normal Operation (10s upload interval):
  Poll → Buffer → Upload → Repeat
  Power: ~100mA average

Low Power Mode (60s upload interval + sleep):
  Poll → Buffer → Upload → Deep Sleep 55s → Wake → Repeat
  Power: ~10mA average (90% reduction)
```

#### 4.12.4 M5 Goals

- Reduce average power consumption by 80%
- Extend battery life from days to months
- Maintain data integrity during sleep/wake cycles
- Support solar charging scenarios

---

## 5. Peripheral Layer

### 5.1 Overview

The Peripheral Layer provides hardware abstraction for sensors and communication interfaces.

### 5.2 Acquisition Module

**File:** `src/peripheral/acquisition.cpp`  
**Header:** `include/peripheral/acquisition.h`  
**Purpose:** Sensor data acquisition via Modbus/UART

#### 5.2.1 Sensor Interface

```cpp
struct DecodedValues {
    uint16_t values[MAX_REGISTERS];  // Sensor readings
    size_t count;                     // Number of values
    bool success;                     // Read success flag
};

DecodedValues readRequest(const RegID* selection, size_t count);
```

#### 5.2.2 Supported Sensors

- **PZEM-004T**: Energy monitoring module
  - Voltage: 0-300V AC
  - Current: 0-100A
  - Power: 0-30kW
  - Frequency: 45-65Hz
  - Communication: Modbus RTU over UART

#### 5.2.3 Register Mapping

```cpp
enum RegID {
    V_RMS = 0x00,       // RMS Voltage (0.1V)
    I_RMS = 0x01,       // RMS Current (0.001A)
    P_ACTIVE = 0x03,    // Active Power (0.1W)
    P_REACTIVE = 0x04,  // Reactive Power (VAR)
    P_APPARENT = 0x05,  // Apparent Power (VA)
    FREQUENCY = 0x06    // Line Frequency (0.1Hz)
};
```

#### 5.2.4 Read Process

1. Build Modbus request frame
2. Send via UART (9600 baud, 8N1)
3. Wait for response (timeout: 1000ms)
4. Validate CRC
5. Parse register values
6. Scale values (e.g., 0.1V → actual V)
7. Return `DecodedValues` structure

---

### 5.3 Arduino WiFi Module

**File:** `src/peripheral/arduino_wifi.cpp`  
**Header:** `include/peripheral/arduino_wifi.h`  
**Purpose:** WiFi connection management

#### 5.3.1 WiFi Functions

```cpp
class Arduino_Wifi {
public:
    bool connect(const char* ssid, const char* password);
    bool disconnect();
    bool isConnected();
    int32_t getRSSI();
    String getIP();
    String getMAC();
    void startSmartConfig();  // ESP-Touch provisioning
};
```

#### 5.3.2 Connection Flow

```cpp
bool Arduino_Wifi::connect(const char* ssid, const char* password) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    
    int timeout = 20;  // 20 seconds
    while (WiFi.status() != WL_CONNECTED && timeout > 0) {
        delay(1000);
        timeout--;
    }
    
    return WiFi.status() == WL_CONNECTED;
}
```

#### 5.3.3 Credentials

WiFi credentials stored in `include/application/credentials.h`:

```cpp
#define WIFI_SSID "your_network_name"
#define WIFI_PASSWORD "your_password"
#define FLASK_SERVER_URL "http://192.168.1.100:5001"
```

---

### 5.4 Print Module

**File:** `src/peripheral/print.cpp`  
**Header:** `include/peripheral/print.h`  
**Purpose:** Debug logging with formatting

#### 5.4.1 Overview

Provides printf-style logging over serial:

```cpp
void print_init();                          // Initialize serial
void print(const char* format, ...);        // Formatted print
void println(const char* message);          // Print with newline
void printHex(const uint8_t* data, size_t len); // Print hex dump
```

#### 5.4.2 Usage Example

```cpp
print("Sensor reading: V=%d.%dV, I=%d.%03dA\n", 
      v_rms/10, v_rms%10, i_rms/1000, i_rms%1000);
// Output: Sensor reading: V=242.9V, I=0.177A
```

---

## 6. Driver Layer

### 6.1 Overview

The Driver Layer provides low-level hardware access.

### 6.2 Debug Driver

**File:** `src/driver/debug.cpp`  
**Header:** `include/driver/debug.h`  
**Purpose:** UART debug output (low-level)

#### 6.2.1 Functions

```cpp
void debug_init(uint32_t baudRate);
void debug_write(const char* data, size_t len);
void debug_writeByte(uint8_t byte);
```

---

### 6.3 Delay Driver

**File:** `src/driver/delay.cpp`  
**Header:** `include/driver/delay.h`  
**Purpose:** Timing primitives

#### 6.3.1 Functions

```cpp
void delay_ms(uint32_t milliseconds);
void delay_us(uint32_t microseconds);
uint32_t millis();  // Time since boot
uint32_t micros();  // Time since boot (μs)
```

---

### 6.4 Protocol Adapter

**File:** `src/driver/protocol_adapter.cpp`  
**Header:** `include/driver/protocol_adapter.h`  
**Purpose:** Modbus protocol implementation

#### 6.4.1 Modbus Frame

```cpp
struct ModbusFrame {
    uint8_t slaveAddress;
    uint8_t functionCode;
    uint16_t startAddress;
    uint16_t registerCount;
    uint16_t crc;
};
```

#### 6.4.2 Functions

```cpp
bool buildReadRequest(ModbusFrame& frame, uint8_t slave, uint16_t addr, uint16_t count);
bool parseReadResponse(const uint8_t* response, size_t len, uint16_t* values);
uint16_t calculateCRC(const uint8_t* data, size_t len);
bool verifyCRC(const uint8_t* frame, size_t len);
```

---

## 7. Hardware Abstraction

### 7.1 HAL Overview

The Hardware Abstraction Layer (HAL) provides a consistent interface across different ESP32 variants.

**Location:** `include/hal/`

### 7.2 Supported Platforms

- ESP32 (original)
- ESP32-S2
- ESP32-S3
- ESP32-C3

### 7.3 HAL Modules

```cpp
// GPIO abstraction
void hal_gpio_init(uint8_t pin, uint8_t mode);
void hal_gpio_write(uint8_t pin, uint8_t value);
uint8_t hal_gpio_read(uint8_t pin);

// Timer abstraction
hw_timer_t* hal_timer_begin(uint8_t num, uint16_t prescaler, bool countUp);
void hal_timer_attach(hw_timer_t* timer, void (*fn)(), bool edge);

// UART abstraction
void hal_uart_init(uint8_t num, uint32_t baud);
size_t hal_uart_write(uint8_t num, const uint8_t* data, size_t len);
size_t hal_uart_read(uint8_t num, uint8_t* buffer, size_t maxLen);
```

---

## 8. State Machine & Petri Nets

### 8.1 Petri Net Model

The firmware implements a **timed Petri net** for task coordination:

```
Places (States):          Transitions (Events):
┌──────────┐              T1: Poll Timer Fires
│   IDLE   │◄────────┐    T2: Upload Timer Fires
└────┬─────┘         │    T3: Config Timer Fires
     │T1             │    T4: OTA Timer Fires
     ▼               │
┌──────────┐         │
│ POLLING  │         │
└────┬─────┘         │
     │T2             │
     ▼               │
┌──────────┐         │
│UPLOADING │         │
└────┬─────┘         │
     │T3             │
     ▼               │
┌──────────┐         │
│ CONFIG   │         │
└────┬─────┘         │
     │T4             │
     ▼               │
┌──────────┐         │
│   OTA    │─────────┘
└──────────┘
```

### 8.2 Token Flow

**Tokens** represent execution permissions:
- Each timer ISR places a token (sets flag)
- Main loop consumes tokens (checks and clears flags)
- Only one token consumed per loop iteration
- Priority: Poll > Upload > Config > OTA

### 8.3 State Descriptions

| State | Token | Action | Duration | Next State |
|-------|-------|--------|----------|------------|
| IDLE | None | Wait for timer | Variable | Any |
| POLLING | pollToken | Read sensors, add to batch | ~50ms | IDLE |
| UPLOADING | uploadToken | Compress, transmit data | ~200ms | IDLE |
| CONFIG | changesToken | Check config, poll commands | ~100ms | IDLE |
| OTA | otaToken | Check for firmware updates | ~2s | IDLE |

### 8.4 Concurrent Execution

While the state machine appears sequential, multiple tokens can accumulate:

```
Time:     0ms   500ms  1000ms 1500ms 2000ms
Poll:      ●                    ●
Upload:    ●                           (waiting)
Config:          ●       (waiting)

Loop execution order:
  Poll token consumed → Read sensors (50ms)
  Config token consumed → Check config (100ms)  
  Upload token consumed → Transmit data (200ms)
```

### 8.5 State Machine Implementation

```cpp
void loop() {
    // Check each token in priority order
    if (TaskCoordinator::isPollReady()) {
        TaskCoordinator::resetPollToken();
        // Execute polling state
        DataPipeline::pollAndProcess();
        return;  // One state per iteration
    }
    
    if (TaskCoordinator::isUploadReady()) {
        TaskCoordinator::resetUploadToken();
        // Execute uploading state
        DataUploader::uploadPendingData();
        // Apply config changes (safe point)
        applyPendingConfigChanges();
        return;
    }
    
    if (TaskCoordinator::isChangesReady()) {
        TaskCoordinator::resetChangesToken();
        // Execute config check state
        ConfigManager::checkForChanges(...);
        CommandExecutor::checkAndExecuteCommands();
        return;
    }
    
    if (TaskCoordinator::isOTAReady()) {
        TaskCoordinator::resetOTAToken();
        // Execute OTA check state
        performOTAUpdate();
        return;
    }
    
    // No tokens, yield to background
    yield();
}
```

### 8.6 Critical Sections

During OTA updates, all tasks are paused:

```
┌──────────────────────────────────────┐
│        Normal Operation              │
│  Poll → Upload → Config → OTA Check  │
└──────────────┬───────────────────────┘
               │ Update detected
               ▼
┌──────────────────────────────────────┐
│      PAUSE ALL TASKS                 │
│  TaskCoordinator::pauseAllTasks()    │
└──────────────┬───────────────────────┘
               │
               ▼
┌──────────────────────────────────────┐
│     Download Firmware                │
│  (Exclusive CPU access)              │
└──────────────┬───────────────────────┘
               │
               ▼
┌──────────────────────────────────────┐
│     Verify & Reboot                  │
│  New firmware or rollback            │
└──────────────────────────────────────┘
```

---

## 9. Data Flow

### 9.1 End-to-End Data Flow

```
┌─────────────┐
│   Sensors   │  PZEM-004T Energy Monitoring
│  (Modbus)   │
└──────┬──────┘
       │ UART 9600 baud
       ▼
┌─────────────┐
│ Acquisition │  Read registers, validate, scale
│   Module    │
└──────┬──────┘
       │ uint16_t[6] per sample
       ▼
┌─────────────┐
│   Batch     │  Accumulate 10 samples
│   Buffer    │
└──────┬──────┘
       │ uint16_t[60] (10 samples × 6 registers)
       ▼
┌─────────────┐
│ Compression │  Smart algorithm selection
│   Engine    │  (Dictionary/Temporal/RLE/Bitpack)
└──────┬──────┘
       │ std::vector<uint8_t> (~25-35 bytes)
       ▼
┌─────────────┐
│  Security   │  HMAC-SHA256 + Nonce
│    Layer    │
└──────┬──────┘
       │ Secured JSON payload
       ▼
┌─────────────┐
│    WiFi     │  HTTP POST
│   Upload    │
└──────┬──────┘
       │
       ▼
┌─────────────┐
│   Flask     │  Validation, storage, analysis
│   Server    │
└─────────────┘
```

### 9.2 Data Transformation Example

**Original Sensor Reading (1 sample):**
```
V_RMS:      2429 (242.9V)
I_RMS:      177  (0.177A)
P_ACTIVE:   73   (7.3W)
POWER:      4331 (433.1W)
ENERGY:     70   (0.070kWh)
FREQUENCY:  605  (60.5Hz)
Total: 12 bytes (6 × uint16_t)
```

**Batch (10 samples):**
```
Sample 0: [2429, 177, 73, 4331, 70, 605]
Sample 1: [2430, 178, 74, 4345, 71, 605]
...
Sample 9: [2431, 179, 75, 4350, 72, 606]
Total: 120 bytes (10 × 6 × 2 bytes)
```

**After Compression (Temporal Delta):**
```
Header:     0x70 (Temporal Delta mode)
Baseline:   [2429, 177, 73, 4331, 70, 605] (12 bytes)
Delta 1:    [+1, +1, +1, +14, +1, 0] (6 bytes)
Delta 2:    [0, 0, +1, +4, +1, 0] (6 bytes)
...
Delta 9:    [+1, +1, +1, +5, +1, +1] (6 bytes)
Total: ~31 bytes (74% reduction)
```

**After Security Layer:**
```json
{
    "nonce": 10523,
    "payload": "cHB3MDEyOTI3Nzc...",  // Base64 encoded
    "mac": "a7f3d8e9c2b1f4a6...",      // HMAC-SHA256 (64 hex chars)
    "encrypted": false
}
Total: ~180 bytes (JSON overhead)
```

### 9.3 Bandwidth Analysis

**Without Compression:**
- 10 samples: 120 bytes
- 1 hour (360 uploads): 43,200 bytes = 42.2 KB/hour
- 24 hours: 1.01 MB/day

**With Compression (avg 70% reduction):**
- 10 samples: ~36 bytes
- 1 hour: 12,960 bytes = 12.7 KB/hour
- 24 hours: 0.30 MB/day

**With Security Overhead:**
- Per upload: ~180 bytes
- 1 hour: 64,800 bytes = 63.3 KB/hour
- 24 hours: 1.52 MB/day

**Effective Bandwidth Savings:**
- Raw vs Compressed: 70% reduction
- Raw vs Secured+Compressed: Still positive (better than raw without security)

---

## 10. Configuration & Storage

### 10.1 NVS Partition Layout

```
Flash Partition Table:
┌────────────────────────────────────┐
│  Bootloader (0x1000)               │  64KB
├────────────────────────────────────┤
│  Partition Table (0x8000)          │  4KB
├────────────────────────────────────┤
│  NVS (0x9000)                      │  20KB  ← Configuration storage
├────────────────────────────────────┤
│  OTA Data (0xE000)                 │  8KB   ← OTA state
├────────────────────────────────────┤
│  App0 (0x10000)                    │  1.5MB ← Current firmware
├────────────────────────────────────┤
│  App1 (0x190000)                   │  1.5MB ← OTA update partition
├────────────────────────────────────┤
│  SPIFFS (Optional)                 │  Remaining
└────────────────────────────────────┘
```

### 10.2 Configuration Parameters

| Parameter | Type | Default | Range | Description |
|-----------|------|---------|-------|-------------|
| `poll_freq` | uint64_t | 1000000 | 100000-60000000 | Polling interval (μs) |
| `upload_freq` | uint64_t | 10000000 | 1000000-300000000 | Upload interval (μs) |
| `reg_count` | size_t | 6 | 1-16 | Number of registers |
| `registers` | blob | [default] | RegID array | Register selection |
| `nonce` | uint32_t | 10000 | 10000-4294967295 | Security nonce |
| `log_level` | uint8_t | 2 | 0-4 | Log verbosity |

### 10.3 Configuration API

**Read Configuration:**
```cpp
uint64_t pollFreq = nvs::getPollFreq();
size_t regCount = nvs::getReadRegCount();
const RegID* regs = nvs::getReadRegs();
```

**Write Configuration:**
```cpp
nvs::setPollFreq(2000000ULL);  // 2 seconds
RegID newRegs[] = {V_RMS, I_RMS, P_ACTIVE};
nvs::setReadRegs(newRegs, 3);
```

**Reset to Defaults:**
```cpp
nvs::resetToDefaults();
```

### 10.4 Remote Configuration

Flask server can remotely update ESP32 configuration:

1. Admin updates config via REST API
2. ESP32 polls `/changes` endpoint every 5s
3. Detects differences between server and local NVS
4. Downloads new configuration
5. Writes to NVS
6. Applies after next upload (safe point)
7. Confirms new configuration to server

---

## 11. Security Implementation

### 11.1 Security Architecture

```
┌──────────────────────────────────────────┐
│            ESP32 Security                 │
├──────────────────────────────────────────┤
│  • Pre-Shared Keys (HMAC, AES, RSA)      │
│  • Nonce Generation & Management         │
│  • HMAC-SHA256 Calculation               │
│  • AES-256-CBC Decryption (OTA)          │
│  • RSA-2048 Signature Verification       │
│  • Anti-Replay Protection                │
└──────────────────────────────────────────┘
```

### 11.2 Key Management

**Key Storage:**
```cpp
// File: src/application/keys.cpp
const uint8_t PSK_HMAC[32] = { ... };          // Data authentication
const uint8_t PSK_AES[16] = { ... };           // Payload encryption (future)
const uint8_t AES_FIRMWARE_KEY[32] = { ... };  // OTA decryption
const uint8_t RSA_PUBLIC_KEY[] = { ... };      // Signature verification
```

**Key Synchronization:**
- All keys must match Flask server
- Keys embedded at compile time
- No key exchange protocol (pre-shared)
- Future: Secure key provisioning during manufacturing

### 11.3 Data Authentication

**HMAC-SHA256 Process:**

1. **Prepare Data:**
   ```cpp
   uint8_t data[4 + payloadLen];
   data[0..3] = nonce (big-endian);
   data[4..] = payload;
   ```

2. **Calculate HMAC:**
   ```cpp
   hmac = HMAC-SHA256(PSK_HMAC, data);
   // Output: 32 bytes
   ```

3. **Send to Server:**
   ```json
   {
       "nonce": 10523,
       "payload": "base64_data",
       "mac": "hex_hmac"
   }
   ```

4. **Server Validates:**
   - Extract nonce and payload
   - Recalculate HMAC
   - Compare (constant-time)
   - Accept if match

### 11.4 OTA Security

**Firmware Encryption:**

1. **Server Side (Flask):**
   - Generate random IV (16 bytes)
   - Encrypt firmware with AES-256-CBC
   - Calculate SHA-256 hash of encrypted firmware
   - Sign hash with RSA-2048 private key
   - Send manifest with IV, hash, signature

2. **ESP32 Side:**
   - Download encrypted chunks
   - Decrypt each chunk with AES-256-CBC (same IV)
   - Write decrypted data to OTA partition
   - Calculate SHA-256 hash of decrypted firmware
   - Verify hash matches manifest
   - Verify RSA signature of hash
   - Mark partition bootable only if all checks pass

**Security Properties:**
- **Confidentiality**: Firmware encrypted in transit
- **Integrity**: SHA-256 hash detects tampering
- **Authenticity**: RSA signature proves server identity
- **Rollback Protection**: Version checking prevents downgrades

### 11.5 Anti-Replay Protection

**Nonce Strategy:**
- Monotonically increasing counter
- Persisted to NVS after each use
- Server tracks last seen nonce per device
- Rejects requests with nonce ≤ last seen

**Attack Mitigation:**
- Replay attacks: Prevented by nonce validation
- Man-in-the-middle: Detected by HMAC failure
- Firmware injection: Prevented by RSA signature
- Downgrade attacks: Prevented by version checking

---

## 12. Power Management

### 12.1 Current Power Profile (M2-M4)

**Active Mode Power Consumption:**
```
WiFi Active (Tx/Rx):     ~160mA @ 3.3V = 528mW
WiFi Idle (Connected):   ~80mA  @ 3.3V = 264mW
CPU Active (Polling):    ~100mA @ 3.3V = 330mW
CPU Idle (yield):        ~40mA  @ 3.3V = 132mW

Average (10s cycle):
  Polling (1s):   100mA × 1s  = 100mA·s
  Idle (8s):      40mA × 8s   = 320mA·s
  Upload (1s):    160mA × 1s  = 160mA·s
  Total: 580mA·s / 10s = 58mA average
  
Daily Energy: 58mA × 24h = 1.39Ah @ 3.3V = 4.6Wh
```

**Battery Life Estimate:**
- 2000mAh battery: 2000/58 = 34 hours ≈ 1.4 days
- 5000mAh battery: 5000/58 = 86 hours ≈ 3.6 days

### 12.2 M5 Power Optimization Goals

**Target:** 90% power reduction → 5.8mA average

**Strategies (Planned for M5):**

1. **Deep Sleep Between Uploads:**
   ```cpp
   poll() → buffer() → upload() → deepSleep(55s) → wake() → repeat
   Average: ~10mA (includes wake/sleep overhead)
   Battery life: 200 hours ≈ 8 days (2000mAh)
   ```

2. **WiFi Power Save Mode:**
   ```cpp
   WiFi.setSleep(WIFI_PS_MIN_MODEM);  // Modem sleep
   // Reduces idle WiFi from 80mA to 20mA
   ```

3. **Dynamic Frequency Scaling:**
   ```cpp
   setCpuFrequencyMhz(80);  // Reduce from 240MHz
   // Reduces CPU power by ~40%
   ```

4. **Peripheral Power Gating:**
   ```cpp
   // Power off unused peripherals
   esp_pm_config_esp32_t pm_config = {
       .max_freq_mhz = 80,
       .min_freq_mhz = 10,
       .light_sleep_enable = true
   };
   ```

### 12.3 M5 Wake Sources (Planned)

```cpp
// Wake on timer
esp_sleep_enable_timer_wakeup(60 * 1000000ULL);  // 60s

// Wake on external interrupt (button, alarm)
esp_sleep_enable_ext0_wakeup(GPIO_NUM_33, 0);

// Wake on touchpad
esp_sleep_enable_touchpad_wakeup();
```

### 12.4 Sleep Mode Comparison

| Mode | Power | Wake Time | State Preservation | Use Case |
|------|-------|-----------|-------------------|----------|
| Active | 100mA | N/A | Full | Normal operation |
| Modem Sleep | 40mA | <1ms | Full | WiFi idle periods |
| Light Sleep | 0.8mA | 10ms | CPU+RAM | Short sleeps (<60s) |
| Deep Sleep | 10μA | 200ms | RTC only | Long sleeps (>60s) |

### 12.5 Data Preservation During Sleep

**Deep Sleep Challenges:**
- RAM contents lost (except RTC memory)
- Need to persist: batch buffer, upload queue, nonce

**Solutions (M5):**
```cpp
// Before sleep: Save state to NVS
nvs::saveBatchBuffer(currentBatch);
nvs::saveUploadQueue(queuedData);

// After wake: Restore state
currentBatch = nvs::loadBatchBuffer();
queuedData = nvs::loadUploadQueue();
```

**RTC Memory Usage:**
```cpp
RTC_DATA_ATTR uint32_t bootCount = 0;
RTC_DATA_ATTR uint32_t lastNonce = 0;
// Preserved across deep sleep (8KB available)
```

---

## 13. Troubleshooting

### 13.1 Common Issues

#### Issue 1: WiFi Connection Failures

**Symptoms:**
- "WiFi not connected" errors
- Unable to upload data
- OTA checks fail

**Diagnosis:**
```cpp
print("WiFi Status: %d\n", WiFi.status());
print("RSSI: %d dBm\n", WiFi.RSSI());
print("IP: %s\n", WiFi.localIP().toString().c_str());
```

**Solutions:**
- Check SSID/password in `credentials.h`
- Verify router is in range (RSSI > -70 dBm)
- Check DHCP settings
- Try `WiFi.reconnect()`
- Reset WiFi: `WiFi.disconnect()` → `WiFi.begin()`

#### Issue 2: OTA Update Failures

**Symptoms:**
- "Wrong Magic Byte" errors
- "Decryption failed"
- Rollback after update

**Diagnosis:**
```cpp
print("OTA Error: %s\n", otaManager->getLastError().c_str());
print("Magic Byte: 0x%02X (expected 0xE9)\n", firstByte);
```

**Solutions:**
- **Wrong Magic Byte (0xE9 expected):**
  - AES key mismatch between ESP32 and Flask
  - Verify `keys.cpp` AES_FIRMWARE_KEY matches `flask/keys/aes_firmware_key.bin`
  - Check IV in manifest matches decryption IV

- **Signature Verification Failed:**
  - RSA public key mismatch
  - Verify `keys.cpp` RSA_PUBLIC_KEY matches `flask/keys/server_public_key.pem`

- **Hash Mismatch:**
  - Corrupted download
  - Network issue during chunk transfer
  - Try OTA update again

#### Issue 3: HMAC Validation Failures

**Symptoms:**
- Server rejects uploads with "Invalid MAC"
- Security errors in Flask logs

**Diagnosis:**
```cpp
print("Nonce: %u\n", SecurityLayer::getCurrentNonce());
print("HMAC (first 8 bytes): ");
printHex(hmac, 8);
```

**Solutions:**
- **PSK Mismatch:**
  - Verify `keys.cpp` PSK_HMAC matches Flask `PSK_HMAC`
  - Check all 32 bytes match exactly

- **Nonce Issues:**
  - Server may have stale nonce
  - Reset nonce: `SecurityLayer::setNonce(10000)`
  - Restart Flask server to clear nonce cache

- **Byte Order:**
  - Ensure nonce is big-endian in HMAC calculation
  - Check payload encoding (Base64 vs raw)

#### Issue 4: Sensor Read Failures

**Symptoms:**
- "Sensor read timeout"
- All values read as zero
- Intermittent failures

**Diagnosis:**
```cpp
DecodedValues result = readRequest(regs, count);
print("Read success: %d\n", result.success);
print("Values received: %zu\n", result.count);
```

**Solutions:**
- Check UART connections (TX/RX swapped?)
- Verify baud rate (9600 for PZEM-004T)
- Check Modbus address (default: 0x01)
- Increase read timeout
- Add pull-up resistors on RS485 lines

#### Issue 5: Memory Exhaustion

**Symptoms:**
- "Memory allocation failed"
- Random crashes
- Heap fragmentation

**Diagnosis:**
```cpp
print("Free Heap: %u bytes\n", ESP.getFreeHeap());
print("Min Free Heap: %u bytes\n", ESP.getMinFreeHeap());
print("Heap Fragmentation: %u%%\n", ESP.getHeapFragmentation());
```

**Solutions:**
- Reduce batch size (10 → 5 samples)
- Clear upload queue more frequently
- Reduce JSON document size (DynamicJsonDocument allocation)
- Disable debug logging to save RAM
- Monitor for memory leaks in compression module

#### Issue 6: Task Timing Issues

**Symptoms:**
- Missed uploads
- Irregular polling
- Tasks not executing

**Diagnosis:**
```cpp
print("Poll Token: %d\n", TaskCoordinator::isPollReady());
print("Upload Token: %d\n", TaskCoordinator::isUploadReady());
print("Timer Freqs: %llu, %llu, %llu, %llu\n", 
      pollFreq, uploadFreq, configFreq, otaFreq);
```

**Solutions:**
- Verify timer initialization succeeded
- Check timer alarm enable state
- Ensure `yield()` is called in main loop
- Verify ISRs are not blocked
- Check for long-running tasks blocking loop

### 13.2 Debug Commands

**Enable Verbose Logging:**
```cpp
#define DEBUG_MODE 1
DataCompression::setDebugMode(true);
```

**Force Diagnostics:**
```cpp
Diagnostics::runDiagnostics();
String report = Diagnostics::getDiagnosticsReport();
print("%s\n", report.c_str());
```

**Manual OTA Trigger:**
```cpp
otaManager->checkForUpdate();
if (otaManager->downloadAndApplyFirmware()) {
    otaManager->verifyAndReboot();
}
```

**Reset Configuration:**
```cpp
nvs::resetToDefaults();
SecurityLayer::setNonce(10000);
ESP.restart();
```

### 13.3 Log Analysis

**Normal Boot Sequence:**
```
=== EcoWatt ESP32 System Starting (Modular v2.0) ===
[WiFi] Connecting to network...
[WiFi] Connected! IP: 192.168.1.150
[NVS] Configuration loaded
[TaskCoordinator] All timers initialized successfully
[DataPipeline] Initialized with 6 registers
[OTAManager] Checking for rollback... none
[Security] Initialized with nonce = 10523
=== System Initialization Complete ===
```

**OTA Update Sequence:**
```
=== OTA UPDATE CHECK INITIATED ===
Firmware update available!
New version: 1.0.5
Total chunks: 987
[OTA] Downloading chunk 1/987...
[OTA] Downloading chunk 100/987...
...
[OTA] Downloading chunk 987/987...
[OTA] Download complete, verifying...
[OTA] SHA-256 hash: OK
[OTA] RSA signature: OK
[OTA] Marking partition bootable...
[OTA] Rebooting to new firmware...
```

### 13.4 Support Resources

- **Project Documentation**: `plans_and_progress/`
- **Flask Server Docs**: `FLASK_ARCHITECTURE.md`
- **Test Documentation**: `ESP32_TESTS.md`
- **PlatformIO Issues**: Check `platformio.ini` for build flags
- **ESP32 Reference**: https://docs.espressif.com/

---

## Appendix A: Module Reference Quick Guide

| Module | Purpose | Key Functions | Dependencies |
|--------|---------|---------------|--------------|
| `task_coordinator` | Timer management | `init()`, `isPollReady()`, `pauseAllTasks()` | ESP32 HW timers |
| `OTAManager` | Firmware updates | `checkForUpdate()`, `downloadAndApplyFirmware()` | security, nvs, WiFi |
| `compression` | Data compression | `compressWithSmartSelection()`, `decompress()` | None |
| `security` | Authentication | `securePayload()`, `calculateHMAC()` | mbedtls, keys |
| `data_pipeline` | Data acquisition | `pollAndProcess()`, `updateRegisterSelection()` | acquisition, compression |
| `data_uploader` | Network transmission | `uploadPendingData()`, `enqueueData()` | security, WiFi |
| `command_executor` | Remote commands | `checkAndExecuteCommands()` | WiFi, nvs |
| `config_manager` | Config sync | `checkForChanges()`, `applyConfiguration()` | WiFi, nvs |
| `nvs` | Persistent storage | `getPollFreq()`, `setReadRegs()` | ESP32 NVS |
| `statistics_manager` | Metrics | `getStatistics()`, `printStatistics()` | None |
| `diagnostics` | Health checks | `runDiagnostics()`, `getDiagnosticsReport()` | All modules |
| `acquisition` | Sensor interface | `readRequest()` | protocol_adapter |
| `arduino_wifi` | WiFi management | `connect()`, `isConnected()` | ESP32 WiFi |

---

## Appendix B: Build Configuration

**File:** `platformio.ini`

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200

build_flags = 
    -DCORE_DEBUG_LEVEL=3
    -DARDUINO_USB_CDC_ON_BOOT=0
    -DFIRMWARE_VERSION=\"1.0.4\"
    
lib_deps = 
    bblanchon/ArduinoJson@^6.21.3
    https://github.com/espressif/arduino-esp32.git#2.0.14
    mbedtls
    
upload_speed = 921600
board_build.partitions = partitions_ota.csv
```

**Partition Table:** `partitions_ota.csv`
```csv
# Name,   Type, SubType, Offset,  Size
nvs,      data, nvs,     0x9000,  0x5000
otadata,  data, ota,     0xe000,  0x2000
app0,     app,  ota_0,   0x10000, 0x180000
app1,     app,  ota_1,   0x190000,0x180000
spiffs,   data, spiffs,  0x310000,0xF0000
```

---

## Appendix C: Firmware Versions

| Version | Date | Milestone | Changes |
|---------|------|-----------|---------|
| 1.0.0 | Jan 2025 | M2 | Initial release, basic acquisition |
| 1.0.1 | Feb 2025 | M2 | Added dictionary compression |
| 1.0.2 | Mar 2025 | M3 | Multi-algorithm compression |
| 1.0.3 | Apr 2025 | M3 | Smart selection system |
| 1.0.4 | Oct 2025 | M4 | Security layer, OTA, AES key sync |
| 1.0.5 | TBD | M4 | (Next planned release) |
| 2.0.0 | TBD | M5 | Power management, deep sleep |

---

**End of ESP32 Architecture Documentation**

For related documentation, see:
- **Flask Architecture**: `FLASK_ARCHITECTURE.md`
- **Flask Tests**: `FLASK_TESTS.md`
- **ESP32 Tests**: `ESP32_TESTS.md`
- **Main README**: `../README.md`
