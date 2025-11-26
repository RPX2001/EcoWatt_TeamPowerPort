# EcoWatt ESP32 Firmware - Milestone Compliance Report

**Generated:** November 26, 2025  
**Firmware Version:** 3.0.0 (FreeRTOS Dual-Core)  
**Branch:** FreeRTIO  
**Reviewer:** AI Code Analysis

---

## Executive Summary

This report cross-checks the ESP32 firmware implementation against all milestone requirements from the project documentation:

- **Milestone 2:** Inverter SIM Integration and Basic Acquisition ✅
- **Milestone 3:** Local Buffering, Compression, and Upload Cycle ⚠️ **CRITICAL ISSUE**
- **Milestone 4:** Remote Configuration, Security Layer & FOTA ✅
- **Milestone 5:** Fault Recovery, Power Optimization & Final Integration ✅

### Overall Status: **MOSTLY COMPLIANT** with 1 CRITICAL ISSUE

---

## Milestone 2: Inverter SIM Integration and Basic Acquisition

### Requirements (from Milestone 4 Resources)

#### 1. Protocol Adapter Implementation
**Requirement:** Implement Modbus RTU frame building, CRC calculation, and communication with Inverter SIM API.

**Status:** ✅ **FULLY IMPLEMENTED**

**Evidence:**
- **File:** `PIO/ECOWATT/src/peripheral/acquisition.cpp`
- **Lines 25-51:** CRC16 calculation implementation matching Modbus RTU standard
  ```cpp
  static uint16_t calculateCRC(const uint8_t* data, int length) 
  {
    uint16_t crc = 0xFFFF;
    for (int i = 0; i < length; i++) 
    {
      crc ^= (data[i] & 0xFF);
      for (int j = 0; j < 8; j++) 
      {
        if (crc & 0x0001) 
        {
          crc >>= 1;
          crc ^= 0xA001;
        } 
        else 
        {
          crc >>= 1;
        }
      }
    }
    return crc & 0xFFFF;
  }
  ```

- **Lines 117-145:** `buildReadFrame()` - Builds Modbus read frames (Function 0x03)
- **Lines 160-174:** `buildWriteFrame()` - Builds Modbus write frames (Function 0x06)
- **File:** `PIO/ECOWATT/src/driver/protocol_adapter.cpp`
  - HTTP communication with Inverter SIM API
  - Retry mechanism on failure

#### 2. Exception Code Handling
**Requirement:** Detect and handle Modbus exception codes (01-0B per API documentation).

**Status:** ✅ **FULLY IMPLEMENTED**

**Evidence:**
- **File:** `PIO/ECOWATT/include/peripheral/acquisition.h`
- **Lines 49-63:** Exception code enumeration
  ```cpp
  enum ModbusExceptionCode {
      NO_EXCEPTION = 0x00,
      ILLEGAL_FUNCTION = 0x01,
      ILLEGAL_DATA_ADDRESS = 0x02,
      ILLEGAL_DATA_VALUE = 0x03,
      SLAVE_DEVICE_FAILURE = 0x04,
      ACKNOWLEDGE = 0x05,
      SLAVE_DEVICE_BUSY = 0x06,
      MEMORY_PARITY_ERROR = 0x08,
      GATEWAY_PATH_UNAVAILABLE = 0x0A,
      GATEWAY_TARGET_DEVICE_FAILED = 0x0B
  };
  ```

- **File:** `PIO/ECOWATT/src/peripheral/acquisition.cpp`
- **Lines 395-451:** `parseModbusException()` - Parses exception responses

#### 3. Register Map Implementation
**Requirement:** Support reading all 10 registers (Address 0-9) with correct scaling factors.

**Status:** ✅ **FULLY IMPLEMENTED**

**Evidence:**
- **File:** `PIO/ECOWATT/include/peripheral/acquisition.h`
- **Lines 65-87:** Complete register map with addresses and scaling
  ```cpp
  const RegisterDef REGISTER_MAP[] = {
      { REG_VAC1,           0, 10, "Vac1" },
      { REG_IAC1,           1, 10, "Iac1" },
      { REG_FAC1,           2, 100, "Fac1" },
      { REG_VPV1,           3, 10, "Vpv1" },
      { REG_VPV2,           4, 10, "Vpv2" },
      { REG_IPV1,           5, 10, "Ipv1" },
      { REG_IPV2,           6, 10, "Ipv2" },
      { REG_TEMP_INVERTER,  7, 10, "Temp" },
      { REG_POWER_PERCENT,  8, 1,  "Power%" },
      { REG_PAC_L,          9, 1,  "Pac_L" }
  };
  ```

#### 4. Write Register Implementation
**Requirement:** Support writing to Register 8 (Power Percentage, 0-100%).

**Status:** ✅ **FULLY IMPLEMENTED**

**Evidence:**
- **File:** `PIO/ECOWATT/src/peripheral/acquisition.cpp`
- **Lines 189-223:** `setPower()` function with validation
- **File:** `PIO/ECOWATT/src/application/command_executor.cpp`
- Command execution framework integrated

### ⚠️ Issues Found

1. **API Key Hardcoded**
   - **Location:** `PIO/ECOWATT/src/peripheral/acquisition.cpp:305`
   - **Issue:** API key is hardcoded instead of using `credentials.h`
   ```cpp
   adapter.setApiKey("NjhhZWIwNDU1ZDdmMzg3MzNiMTQ5YTFmOjY4YWViMDQ1NWQ3ZjM4NzMzYjE0OWExNQ==");
   ```
   - **Impact:** Security risk, not configurable per device
   - **Recommendation:** Move to `credentials.h` and load at runtime

---

## Milestone 3: Local Buffering, Compression, and Upload Cycle

### Requirements

#### 1. Local Buffer Implementation
**Requirement:** Buffer sensor samples locally with configurable batch size.

**Status:** ✅ **FULLY IMPLEMENTED**

**Evidence:**
- **File:** `PIO/ECOWATT/include/application/compression.h`
- **Lines 8-53:** `SampleBatch` structure with dynamic sizing
  ```cpp
  struct SampleBatch {
      static const size_t MAX_REGISTERS = 10;
      static const size_t MAX_SAMPLES_LIMIT = 50;
      
      uint16_t samples[MAX_SAMPLES_LIMIT][MAX_REGISTERS];
      size_t sampleCount = 0;
      size_t registerCount = 0;
      size_t dynamicBatchSize = 7;  // Current batch size
      unsigned long timestamps[MAX_SAMPLES_LIMIT];
      
      void setBatchSize(size_t newSize) {
          if (newSize < 1) newSize = 1;
          if (newSize > MAX_SAMPLES_LIMIT) newSize = MAX_SAMPLES_LIMIT;
          dynamicBatchSize = newSize;
      }
  };
  ```

#### 2. Compression Algorithm Implementation
**Requirement:** Implement compression with benchmarking.

**Status:** ✅ **FULLY IMPLEMENTED**

**Evidence:**
- **File:** `PIO/ECOWATT/include/application/compression.h`
- **Lines 63-85:** Smart selection system with multiple algorithms
  ```cpp
  // Main smart selection compression method
  static std::vector<uint8_t> compressWithSmartSelection(uint16_t* data, const RegID* selection, size_t count);
  
  // Individual advanced compression methods
  static std::vector<uint8_t> compressWithDictionary(uint16_t* data, const RegID* selection, size_t count);
  static std::vector<uint8_t> compressWithTemporalDelta(uint16_t* data, const RegID* selection, size_t count);
  static std::vector<uint8_t> compressWithSemanticRLE(uint16_t* data, const RegID* selection, size_t count);
  ```

- **Lines 96-104:** Compression result tracking with ratios and timing
  ```cpp
  struct CompressionResult {
      std::vector<uint8_t> data;
      String method;
      float academicRatio;      // Compressed/Original
      float traditionalRatio;   // Original/Compressed
      unsigned long timeUs;
      float efficiency;
      bool lossless;
  };
  ```

#### 3. Upload Cycle Implementation
**Requirement:** Upload compressed data **once every 15 minutes** through constrained link.

**Status:** ❌ **CRITICAL ERROR - INCORRECTLY IMPLEMENTED**

**Evidence:**
- **File:** `PIO/ECOWATT/include/application/system_config.h`
- **Line 37:** Upload frequency is **15 SECONDS instead of 15 MINUTES**
  ```cpp
  #define DEFAULT_UPLOAD_FREQUENCY_US     15000000ULL     // 15 seconds
  ```

**Project Requirement (from Milestone 3):**
> "Upload once every 15 minutes through a constrained link."

**Project Requirement (from Overview):**
> "EcoWatt Device can read data (voltage, current, and later things like frequency) as often as we like, but it is only allowed to send data to the cloud **once every 15 minutes**."

**Impact:** 
- **CRITICAL:** Device uploads 60x more frequently than specification
- Violates constrained communication requirement
- Defeats purpose of local buffering and compression
- May overwhelm cloud server
- Wastes network bandwidth and power

**Fix Required:**
```cpp
// INCORRECT (current implementation)
#define DEFAULT_UPLOAD_FREQUENCY_US     15000000ULL     // 15 seconds

// CORRECT (should be)
#define DEFAULT_UPLOAD_FREQUENCY_US     900000000ULL    // 15 minutes (900 seconds)
```

**Files to Update:**
1. `PIO/ECOWATT/include/application/system_config.h:37`
2. Verify batch size calculations account for 15-minute window
3. Update all test files referencing upload frequency

#### 4. Batch Size Calculation
**Requirement:** Calculate batch size to fit within 15-minute upload window.

**Status:** ⚠️ **NEEDS VERIFICATION**

**Evidence:**
- **File:** `PIO/ECOWATT/src/application/task_manager.cpp`
- Batch size calculation may not account for correct upload frequency
- With corrected upload frequency (900s vs 15s), batch size needs recalculation:
  - Poll every 5 seconds → 180 samples per 15 minutes
  - Current MAX_SAMPLES_LIMIT = 50 may be insufficient

**Recommendation:**
```cpp
// With 15-minute upload window and 5s polling:
// Samples per upload = (900s / 5s) = 180 samples
// Adjust MAX_SAMPLES_LIMIT accordingly or implement multiple batches
```

---

## Milestone 4: Remote Configuration, Security Layer & FOTA

### Requirements

#### 1. Remote Configuration
**Requirement:** Runtime updates to sampling interval and register list without reboot.

**Status:** ✅ **FULLY IMPLEMENTED**

**Evidence:**
- **File:** `PIO/ECOWATT/src/application/config_manager.cpp`
- **Lines 111-125:** Sampling interval updates
  ```cpp
  if (config.containsKey("sampling_interval")) {
      uint32_t sampling_interval = config["sampling_interval"];  // in seconds
      uint64_t new_poll_freq = sampling_interval * 1000000ULL;  // convert to microseconds
      
      if (new_poll_freq >= MIN_POLL_FREQUENCY_US && new_poll_freq <= MAX_POLL_FREQUENCY_US) {
          nvs::setPollFreq(new_poll_freq);
          
          // Apply immediately
          TaskManager::updatePollFrequency(sampling_interval * 1000);
          
          accepted.add("sampling_interval");
          LOG_SUCCESS(LOG_TAG_CONFIG, "Sampling interval updated: %u seconds (%llu μs)", 
                    sampling_interval, new_poll_freq);
      }
  }
  ```

- **Lines 129-176:** Register list updates with validation
  ```cpp
  if (config.containsKey("registers")) {
      JsonArray regArray = config["registers"].as<JsonArray>();
      // Dynamic register selection and NVS storage
  }
  ```

#### 2. Command Execution Protocol
**Requirement:** Execute write commands from cloud with acknowledgment.

**Status:** ✅ **FULLY IMPLEMENTED**

**Evidence:**
- **File:** `PIO/ECOWATT/src/application/command_executor.cpp`
- Full command execution framework with queuing
- Acknowledgment sent back to server

#### 3. HMAC-SHA256 Security
**Requirement:** Implement HMAC-SHA256 for authentication and integrity.

**Status:** ✅ **FULLY IMPLEMENTED**

**Evidence:**
- **File:** `PIO/ECOWATT/include/application/security.h`
- **Lines 9-28:** Security layer with HMAC-SHA256
  ```cpp
  /**
   * Features:
   * - HMAC-SHA256 authentication and integrity verification
   * - Optional AES-128-CBC encryption (disabled by default for mock mode)
   * - Anti-replay protection with persistent nonce storage
   * - Base64 encoding for JSON transmission
   */
  class SecurityLayer {
  ```

- **File:** `PIO/ECOWATT/src/application/security.cpp`
- Complete HMAC implementation using mbedtls

#### 4. Anti-Replay Protection (Nonce)
**Requirement:** Sequential nonce with persistent storage to prevent replay attacks.

**Status:** ✅ **FULLY IMPLEMENTED**

**Evidence:**
- **File:** `PIO/ECOWATT/include/application/security.h`
- **Lines 65-71:** Nonce management
  ```cpp
  static uint32_t getCurrentNonce();
  static void setNonce(uint32_t nonce);
  static bool syncNonceWithServer(const char* serverURL, const char* deviceID);
  ```

- **File:** `PIO/ECOWATT/src/application/security.cpp`
- Nonce stored in NVS, incremented on each upload
- Server sync function to recover from nonce mismatches

#### 5. FOTA Implementation
**Requirement:** Firmware-over-the-air with chunking, verification, and rollback.

**Status:** ✅ **FULLY IMPLEMENTED**

**Evidence:**
- **File:** `PIO/ECOWATT/include/application/OTAManager.h`
- **Lines 65-75:** Complete FOTA structure
  ```cpp
  struct FirmwareManifest {
      String version;
      String sha256_hash;
      String signature;
      String iv;
      uint32_t original_size;
      uint32_t encrypted_size;
      uint32_t firmware_size;
      uint16_t chunk_size;
      uint16_t total_chunks;
  };
  ```

- **File:** `PIO/ECOWATT/src/application/OTAManager.cpp`
- Chunked download with retry (2KB chunks)
- SHA-256 hash verification
- RSA signature verification
- Rollback mechanism on failure

### ⚠️ Issues Found

1. **AES Encryption in Mock Mode**
   - **Location:** `PIO/ECOWATT/include/application/security.h:80`
   - **Issue:** AES encryption disabled for "mock mode"
   ```cpp
   static const bool ENABLE_ENCRYPTION = false;  // Mock encryption mode (Base64 only)
   ```
   - **Impact:** Data transmitted without encryption (Base64 encoding only)
   - **Spec Compliance:** Milestone 4 states encryption is "optional/simplified"
   - **Status:** ✅ **ACCEPTABLE** - Meets "simplified encryption" requirement

---

## Milestone 5: Fault Recovery, Power Optimization & Final Integration

### Requirements

#### 1. Power Management Techniques
**Requirement:** Implement as many power-saving techniques as possible from the provided list.

| Technique | Status | Evidence |
|-----------|--------|----------|
| **Light CPU Idle** | ✅ PRESENT | `delay()` with CPU idle states |
| **Dynamic Clock Scaling** | ✅ PRESENT | 240/160/80 MHz via `setCPUFrequency()` |
| **Light Sleep Mode** | ✅ PRESENT | `lightSleep()` function with duration control |
| **Peripheral Gating** | ✅ PRESENT | UART power control via `PeripheralPower` class |
| **WiFi Modem Sleep** | ✅ PRESENT | `WIFI_PS_MAX_MODEM` mode |
| **Deep Sleep** | ⚠️ NOT IMPLEMENTED | Marked as optional in spec |

**Evidence:**
- **File:** `PIO/ECOWATT/include/application/power_management.h`
- **Lines 38-45:** Power technique flags
  ```cpp
  enum PowerTechnique {
      POWER_TECH_NONE = 0,
      POWER_TECH_WIFI_MODEM_SLEEP = (1 << 0),
      POWER_TECH_CPU_FREQ_SCALING = (1 << 1),
      POWER_TECH_LIGHT_SLEEP = (1 << 2),
      POWER_TECH_PERIPHERAL_GATING = (1 << 3)
  };
  ```

- **File:** `PIO/ECOWATT/src/application/power_management.cpp`
- **Lines 31-52:** Initialization with technique selection
- Power statistics tracking with energy savings estimation

#### 2. Fault Injection Handling
**Requirement:** Handle malformed CRC, truncated payloads, buffer overflow, random garbage.

**Status:** ✅ **FULLY IMPLEMENTED**

**Evidence:**
- **File:** `PIO/ECOWATT/include/application/fault_recovery.h`
- **Lines 37-46:** Fault type enumeration
  ```cpp
  enum class FaultType {
      NONE = 0,
      CRC_ERROR,          // Malformed CRC frames
      TRUNCATED_PAYLOAD,  // Incomplete response
      GARBAGE_DATA,       // Random byte corruption
      BUFFER_OVERFLOW,    // Response too large
      TIMEOUT,            // No response
      MODBUS_EXCEPTION    // Inverter exception code
  };
  ```

- **Lines 87-123:** Fault detection functions
  ```cpp
  bool validateModbusCRC(const char* frameHex);
  bool validatePayloadLength(const char* frameHex, uint8_t expectedByteCount);
  bool checkForGarbage(const char* frameHex);
  bool checkBufferOverflow(const char* frameHex, size_t bufferSize);
  FaultType detectFault(const char* frameHex, uint8_t expectedByteCount, size_t bufferSize);
  ```

- **File:** `PIO/ECOWATT/src/application/fault_recovery.cpp`
- Complete implementation of all validation functions

#### 3. Fault Recovery with Retry
**Requirement:** Implement retry mechanism with backoff for fault recovery.

**Status:** ✅ **FULLY IMPLEMENTED**

**Evidence:**
- **File:** `PIO/ECOWATT/include/application/fault_recovery.h`
- **Lines 24-27:** Retry constants
  ```cpp
  #define MAX_RECOVERY_RETRIES 3
  #define INITIAL_RETRY_DELAY_MS 1000
  #define MAX_RETRY_DELAY_MS 4000
  ```

- **Lines 127-132:** Execute recovery function
  ```cpp
  bool executeRecovery(FaultType fault, std::function<bool()> retryFunction, uint8_t& retryCount);
  ```

- **File:** `PIO/ECOWATT/src/peripheral/acquisition.cpp`
- **Lines 241-262:** Retry helper with fault detection
  ```cpp
  static bool retryModbusRead(const char* frame, char* responseFrame, size_t responseSize, uint8_t expectedByteCount) {
    static char retryResponse[256];
    
    bool retryOk = adapter.readRegister(frame, retryResponse, sizeof(retryResponse));
    
    if (!retryOk) return false;
    
    // Check if retry response is fault-free
    FaultType retryFault = detectFault(retryResponse, expectedByteCount, sizeof(retryResponse));
    
    if (retryFault == FaultType::NONE) {
        strncpy(responseFrame, retryResponse, responseSize);
        responseFrame[responseSize - 1] = '\0';
        return true;
    }
    
    return false;
  }
  ```

#### 4. FOTA Rollback Simulation
**Requirement:** Support firmware rollback on verification failure.

**Status:** ✅ **FULLY IMPLEMENTED**

**Evidence:**
- **File:** `PIO/ECOWATT/include/application/OTAManager.h`
- **Lines 34-43:** OTA state includes rollback
  ```cpp
  enum OTAState {
      OTA_IDLE,
      OTA_CHECKING,
      OTA_DOWNLOADING,
      OTA_VERIFYING,
      OTA_APPLYING,
      OTA_COMPLETED,
      OTA_ERROR,
      OTA_ROLLBACK  // Rollback state
  };
  ```

- **File:** `PIO/ECOWATT/src/application/OTAManager.cpp`
- `handleRollback()` function with ESP32 partition rollback
- **File:** `PIO/ECOWATT/src/main.cpp`
- **Lines 185-190:** Rollback handling on boot
  ```cpp
  // Handle rollback if new firmware failed
  if (esp_ota_get_boot_partition() != esp_ota_get_running_partition()) {
      LOG_WARN(LOG_TAG_BOOT, "Firmware mismatch detected, attempting rollback");
      otaManager->handleRollback();
  }
  ```

---

## Critical Issues Summary

### 🔴 CRITICAL (Must Fix Immediately)

1. **Upload Frequency - MAJOR SPECIFICATION VIOLATION**
   - **Issue:** Upload every 15 seconds instead of 15 minutes
   - **Impact:** Defeats core project requirement, wastes 60x bandwidth/power
   - **Fix Location:** `PIO/ECOWATT/include/application/system_config.h:37`
   - **Fix Required:**
     ```cpp
     // Change from:
     #define DEFAULT_UPLOAD_FREQUENCY_US     15000000ULL     // 15 seconds
     
     // Change to:
     #define DEFAULT_UPLOAD_FREQUENCY_US     900000000ULL    // 15 minutes (900 seconds)
     ```

### ⚠️ WARNING (Should Fix)

2. **API Key Hardcoded**
   - **Location:** `PIO/ECOWATT/src/peripheral/acquisition.cpp:305`
   - **Fix:** Move to `credentials.h` and use `#define INVERTER_API_KEY`

3. **Batch Size May Be Insufficient**
   - **Issue:** MAX_SAMPLES_LIMIT=50, but 15-minute window with 5s polling = 180 samples
   - **Fix:** Increase MAX_SAMPLES_LIMIT or implement multi-batch upload
   - **Location:** `PIO/ECOWATT/include/application/compression.h:10`

### ℹ️ INFORMATIONAL (Optional)

4. **AES Encryption Disabled**
   - **Status:** Acceptable per "simplified encryption" spec
   - **Note:** Base64 encoding + HMAC-SHA256 provides integrity but not confidentiality

5. **Deep Sleep Not Implemented**
   - **Status:** Acceptable per "optional" spec
   - **Note:** Requires GPIO16-RST jumper hardware modification

---

## Recommendations

### Immediate Actions (Priority 1)

1. **Fix Upload Frequency**
   ```bash
   # Edit system_config.h
   sed -i 's/15000000ULL     \/\/ 15 seconds/900000000ULL    \/\/ 15 minutes (900 seconds)/' \
     PIO/ECOWATT/include/application/system_config.h
   ```

2. **Update Batch Size Calculations**
   - Review compression task logic in `task_manager.cpp`
   - Ensure batch size calculation accounts for 900-second window
   - Consider increasing MAX_SAMPLES_LIMIT to 200+

3. **Move API Key to Credentials**
   ```cpp
   // In credentials.h
   #define INVERTER_API_KEY "NjhhZWIwNDU1ZDdmMzg3MzNiMTQ5YTFmOjY4YWViMDQ1NWQ3ZjM4NzMzYjE0OWExNQ=="
   
   // In acquisition.cpp
   adapter.setApiKey(INVERTER_API_KEY);
   ```

### Short-Term Improvements (Priority 2)

4. **Verify Upload Window Compliance**
   - Test with 15-minute upload frequency
   - Confirm no buffer overflow occurs
   - Validate compression ratios are sufficient

5. **Add Upload Frequency Validation**
   ```cpp
   // In config_manager.cpp, add validation for upload_frequency updates
   if (newUploadFreq < 900000000ULL) {  // Minimum 15 minutes
       LOG_ERROR(LOG_TAG_CONFIG, "Upload frequency must be >= 15 minutes");
       rejected.add("upload_frequency");
   }
   ```

### Long-Term Enhancements (Priority 3)

6. **Consider Enabling AES Encryption**
   - If security requirements change, enable AES-128-CBC
   - PSK_AES and AES_IV already defined in security.cpp

7. **Implement Deep Sleep (Optional)**
   - For ultra-low-power scenarios
   - Requires hardware modification (GPIO16-RST jumper)
   - May not be practical with FreeRTOS tasks

---

## Testing Recommendations

### 1. Upload Frequency Test
```cpp
// Test Case: Verify 15-minute upload window
// Expected: 1 upload per 900 seconds
// Method: Monitor upload task, count uploads over 30 minutes
// Pass: Exactly 2 uploads in 30 minutes
```

### 2. Batch Size Stress Test
```cpp
// Test Case: Fill batch with 180 samples (15min @ 5s polling)
// Expected: No overflow, successful compression & upload
// Method: Poll every 5s for 15 minutes, upload batch
// Pass: All samples retained, compression successful
```

### 3. Fault Injection Test
```cpp
// Test Case: Inject all fault types from Milestone 5 spec
// Expected: Recovery succeeds or gracefully degrades
// Method: Use Error Emulation API endpoint
// Pass: Device recovers without crash, logs events
```

---

## Conclusion

The ESP32 firmware is **mostly compliant** with all milestone requirements, demonstrating:
- ✅ Complete Modbus protocol implementation
- ✅ Advanced compression with smart algorithm selection  
- ✅ Full HMAC-SHA256 + nonce security layer
- ✅ Comprehensive FOTA with rollback
- ✅ Multi-technique power management
- ✅ Robust fault detection and recovery

**However**, there is **ONE CRITICAL SPECIFICATION VIOLATION**:
- ❌ Upload frequency is 15 seconds instead of 15 minutes (60x error)

This must be corrected immediately as it undermines the core project requirement of constrained communication and local data buffering.

### Final Grade: **B+ (85/100)**
- Deducted 15 points for critical upload frequency error
- All other requirements met or exceeded

---

## Files Referenced

### Core Implementation
- `PIO/ECOWATT/src/peripheral/acquisition.cpp` - Modbus protocol
- `PIO/ECOWATT/src/application/compression.cpp` - Compression engine
- `PIO/ECOWATT/src/application/security.cpp` - HMAC + nonce
- `PIO/ECOWATT/src/application/OTAManager.cpp` - FOTA system
- `PIO/ECOWATT/src/application/power_management.cpp` - Power optimization
- `PIO/ECOWATT/src/application/fault_recovery.cpp` - Fault handling

### Configuration
- `PIO/ECOWATT/include/application/system_config.h` - **Critical fix required**
- `PIO/ECOWATT/src/application/config_manager.cpp` - Runtime config
- `PIO/ECOWATT/src/application/nvs.cpp` - Persistent storage

### Documentation
- `docs/In21-EN4440-Project Outline.md` - Project overview
- `docs/In21-EN4440-API Service Documentation.md` - Inverter SIM API
- `docs/Milestone 4 - Resources - In21-EN4440-Project Outline.md` - M4 spec
- `docs/Milestone 5 - Resources - In21-EN4440-Project Outline.md` - M5 spec

---

**Report End**
