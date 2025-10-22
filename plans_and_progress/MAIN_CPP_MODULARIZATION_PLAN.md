# Main.cpp Modularization Plan

## Current State Analysis

The `main.cpp` file (1250 lines) contains:
1. **Initialization logic** - WiFi, Power Management, Security, OTA, Timers
2. **Task coordination** - Timer interrupts and token-based task scheduling
3. **Data acquisition** - Polling and saving sensor data
4. **Data compression** - Smart compression with method selection
5. **Cloud upload** - Ring buffer management and HTTP uploads
6. **Configuration management** - Dynamic register/frequency updates
7. **Command handling** - Polling and executing remote commands
8. **OTA management** - Update checks and firmware downloads
9. **Statistics tracking** - Performance metrics and compression analytics
10. **Main loop** - Token-based task dispatcher

## Modularization Strategy

### Principle: Minimal Changes, Maximum Modularity
- Keep `application/`, `peripheral/`, `driver/` folders as-is
- Extract reusable logic from main.cpp into new application modules
- main.cpp becomes a thin orchestrator
- Each module has clear, single responsibility

---

## New Modules to Create

### 1. `application/task_coordinator.h/cpp`
**Responsibility**: Timer management and task scheduling

**Interface**:
```cpp
class TaskCoordinator {
public:
    static void init(uint64_t pollFreq, uint64_t uploadFreq, uint64_t changesFreq, uint64_t otaFreq);
    static void updatePollFrequency(uint64_t newFreq);
    static void updateUploadFrequency(uint64_t newFreq);
    static void pauseAllTasks();
    static void resumeAllTasks();
    static bool isPollReady();
    static bool isUploadReady();
    static bool isChangesReady();
    static bool isOTAReady();
    static void resetPollToken();
    static void resetUploadToken();
    static void resetChangesToken();
    static void resetOTAToken();
};
```

**Extracts from main.cpp**:
- Timer setup (`timerBegin`, `timerAttachInterrupt`, etc.)
- Token flags (`poll_token`, `upload_token`, etc.)
- ISR handlers (`set_poll_token`, `set_upload_token`, etc.)
- Timer frequency updates

---

### 2. `application/statistics_manager.h/cpp`
**Responsibility**: Performance statistics tracking and reporting

**Interface**:
```cpp
struct PerformanceStats {
    uint32_t totalCompressions;
    uint64_t totalCompressionTime;
    float averageAcademicRatio;
    float bestAcademicRatio;
    float worstAcademicRatio;
    char currentOptimalMethod[32];
    uint32_t excellentCompressionCount;
    uint32_t goodCompressionCount;
    uint32_t fairCompressionCount;
    uint32_t poorCompressionCount;
    uint64_t fastestCompressionTime;
    uint64_t slowestCompressionTime;
};

class StatisticsManager {
public:
    static void init();
    static void updateCompressionStats(const char* method, float academicRatio, unsigned long timeUs);
    static void printPerformanceReport();
    static const PerformanceStats& getStats();
    static void reset();
};
```

**Extracts from main.cpp**:
- `SmartPerformanceStats` struct
- `updateSmartPerformanceStatistics()` function
- `printSmartPerformanceStatistics()` function
- All statistics tracking logic

---

### 3. `application/data_uploader.h/cpp`
**Responsibility**: Cloud data upload and ring buffer management

**Interface**:
```cpp
class DataUploader {
public:
    static void init(const char* serverURL, const char* deviceID);
    static bool addToQueue(const SmartCompressedData& data);
    static void uploadPendingData();
    static size_t getQueueSize();
    static bool isQueueFull();
    static void clearQueue();
    
private:
    static bool uploadSinglePayload(const SmartCompressedData& data);
    static RingBuffer<SmartCompressedData, 20> ringBuffer;
};
```

**Extracts from main.cpp**:
- `smartRingBuffer` global
- `uploadSmartCompressedDataToCloud()` function
- HTTP upload logic
- Ring buffer management

---

### 4. `application/command_executor.h/cpp`
**Responsibility**: Remote command polling and execution

**Interface**:
```cpp
class CommandExecutor {
public:
    static void init(const char* pollURL, const char* resultURL);
    static void checkAndExecuteCommands();
    static bool executeCommand(const char* commandId, const char* type, const char* params);
    static void sendCommandResult(const char* commandId, bool success, const char* result);
    
private:
    static bool executePowerCommand(const char* params);
    static bool executeDiagnosticsCommand(const char* params);
    static bool executeConfigCommand(const char* params);
};
```

**Extracts from main.cpp**:
- `checkForCommands()` function
- `executeCommand()` function
- `sendCommandResult()` function
- Command polling logic

---

### 5. `application/config_manager.h/cpp`
**Responsibility**: Configuration updates and change detection

**Interface**:
```cpp
struct SystemConfig {
    const RegID* registers;
    size_t registerCount;
    uint64_t pollFrequency;
    uint64_t uploadFrequency;
};

class ConfigManager {
public:
    static void init();
    static void checkForChanges(bool* registersChanged, bool* pollChanged, bool* uploadChanged);
    static void applyRegisterChanges(const RegID** newSelection, size_t* newCount);
    static void applyPollFrequencyChange(uint64_t* newFreq);
    static void applyUploadFrequencyChange(uint64_t* newFreq);
    static const SystemConfig& getCurrentConfig();
};
```

**Extracts from main.cpp**:
- `checkChanges()` function
- Configuration update logic in loop
- Register/frequency change detection

---

### 6. `application/data_pipeline.h/cpp`
**Responsibility**: Data acquisition, compression, and queuing pipeline

**Interface**:
```cpp
class DataPipeline {
public:
    static void init(const RegID* selection, size_t registerCount);
    static void pollAndProcess();
    static void updateRegisterSelection(const RegID* newSelection, size_t newCount);
    
private:
    static bool acquireData(uint16_t* buffer);
    static SmartCompressedData compressData(uint16_t* data);
    static void queueForUpload(const SmartCompressedData& compressed);
};
```

**Extracts from main.cpp**:
- `poll_and_save()` function
- `compressWithSmartSelection()` function
- `compressBatchWithSmartSelection()` function
- Data acquisition pipeline logic

---

### 7. `application/system_initializer.h/cpp`
**Responsibility**: System initialization and setup orchestration

**Interface**:
```cpp
class SystemInitializer {
public:
    static bool initializeAll();
    static bool initWiFi();
    static bool initPowerManagement();
    static bool initSecurity();
    static bool initOTA(const char* serverURL, const char* deviceID, const char* version);
    static void printBootSequence();
};
```

**Extracts from main.cpp**:
- Setup sequence from `setup()` function
- Initialization order management
- Boot logging

---

## Refactored main.cpp Structure

After modularization, `main.cpp` will look like:

```cpp
#include <Arduino.h>
#include "application/system_initializer.h"
#include "application/task_coordinator.h"
#include "application/data_pipeline.h"
#include "application/data_uploader.h"
#include "application/command_executor.h"
#include "application/config_manager.h"
#include "application/statistics_manager.h"
#include "application/OTAManager.h"

// Global OTA Manager (needs to persist)
OTAManager* otaManager = nullptr;

// Command poll counter
static uint8_t command_poll_counter = 0;

void performOTAUpdate() {
    print("=== OTA UPDATE CHECK INITIATED ===\n");
    
    if (otaManager->checkForUpdate()) {
        TaskCoordinator::pauseAllTasks();
        
        if (otaManager->downloadAndApplyFirmware()) {
            otaManager->verifyAndReboot();
        } else {
            print("OTA download/apply failed\n");
            TaskCoordinator::resumeAllTasks();
        }
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    // Initialize all subsystems
    SystemInitializer::initializeAll();
    
    // Initialize modules
    const SystemConfig& config = ConfigManager::getCurrentConfig();
    TaskCoordinator::init(config.pollFrequency, config.uploadFrequency, 5000000, 60000000);
    DataPipeline::init(config.registers, config.registerCount);
    DataUploader::init(FLASK_SERVER_URL "/process", "ESP32_EcoWatt_Smart");
    CommandExecutor::init(FLASK_SERVER_URL "/command/poll", FLASK_SERVER_URL "/command/result");
    StatisticsManager::init();
}

void loop() {
    // Poll and save sensor data
    if (TaskCoordinator::isPollReady()) {
        TaskCoordinator::resetPollToken();
        DataPipeline::pollAndProcess();
    }
    
    // Upload queued data
    if (TaskCoordinator::isUploadReady()) {
        TaskCoordinator::resetUploadToken();
        DataUploader::uploadPendingData();
        
        // Apply any pending configuration changes
        ConfigManager::applyAllPendingChanges();
    }
    
    // Check for configuration changes and commands
    if (TaskCoordinator::isChangesReady()) {
        TaskCoordinator::resetChangesToken();
        
        bool regChanged = false, pollChanged = false, uploadChanged = false;
        ConfigManager::checkForChanges(&regChanged, &pollChanged, &uploadChanged);
        
        // Check for commands every 2nd iteration (10s)
        command_poll_counter++;
        if (command_poll_counter >= 2) {
            command_poll_counter = 0;
            CommandExecutor::checkAndExecuteCommands();
        }
    }
    
    // Handle OTA updates
    if (TaskCoordinator::isOTAReady()) {
        TaskCoordinator::resetOTAToken();
        performOTAUpdate();
    }
    
    yield();
}
```

**Estimated new size**: ~150 lines (down from 1250 lines)

---

## Implementation Order

1. ✅ Create `task_coordinator.h/cpp` - Timer management
2. ✅ Create `statistics_manager.h/cpp` - Stats tracking  
3. ✅ Create `data_uploader.h/cpp` - Cloud upload
4. ✅ Create `command_executor.h/cpp` - Command handling
5. ✅ Create `config_manager.h/cpp` - Configuration
6. ✅ Create `data_pipeline.h/cpp` - Data acquisition
7. ✅ Create `system_initializer.h/cpp` - Initialization
8. ✅ Refactor `main.cpp` to use new modules
9. ✅ Test each module individually
10. ✅ Integration test with full system

---

## Testing Strategy

For each new module:
1. **Unit test** - Test module in isolation with mocks
2. **Component test** - Test with real dependencies
3. **Integration test** - Test in full system context

Test files to create:
- `test/test_task_coordinator.cpp`
- `test/test_statistics_manager.cpp`
- `test/test_data_uploader.cpp`
- `test/test_command_executor.cpp`
- `test/test_config_manager.cpp`
- `test/test_data_pipeline.cpp`
- `test/test_system_initializer.cpp`

---

## Benefits of This Modularization

1. **Maintainability**: Each module has <300 lines, single responsibility
2. **Testability**: Modules can be tested independently
3. **Reusability**: Modules can be used in other projects
4. **Readability**: main.cpp becomes a clear orchestration layer
5. **Debugging**: Easier to isolate issues to specific modules
6. **Collaboration**: Team members can work on different modules
7. **Documentation**: Each module is self-documenting

---

## Notes

- All existing `application/`, `peripheral/`, `driver/` files remain unchanged
- No breaking changes to existing APIs
- Modules use static classes for simplicity (no instance management needed)
- Error handling maintained in each module
- Logging/printing preserved from original code
- NVS access wrapped in ConfigManager for consistency

