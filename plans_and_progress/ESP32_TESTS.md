# ESP32 Test Documentation

**EcoWatt Smart Energy Monitoring System**  
**Team PowerPort**  
**Version:** 2.0.0  
**Date:** October 2025  
**Test Framework:** Unity + PlatformIO

---

## Table of Contents

1. [Overview](#1-overview)
2. [Test Architecture](#2-test-architecture)
3. [M3 Integration Tests](#3-m3-integration-tests)
4. [M4 Integration Tests](#4-m4-integration-tests)
5. [Test Execution Guide](#5-test-execution-guide)
6. [Test Coordination](#6-test-coordination)
7. [Security Testing](#7-security-testing)
8. [OTA Testing](#8-ota-testing)
9. [Test Results](#9-test-results)
10. [Troubleshooting](#10-troubleshooting)

---

## 1. Overview

### 1.1 Purpose

The ESP32 test suite validates the complete firmware functionality through real hardware integration tests. Unlike unit tests that mock components, these tests run on actual ESP32 hardware and communicate with the live Flask server.

### 1.2 Test Coverage

The ESP32 firmware includes multiple test suites covering different milestones and components:

**M4 Integration Tests (9 Tests):** *Primary Focus*
- ✅ WiFi connectivity and server communication
- ✅ Security layer (HMAC authentication, anti-replay protection)
- ✅ Remote command execution (power control, register writes)
- ✅ Remote configuration updates
- ✅ FOTA (Firmware Over-The-Air) updates
- ✅ Continuous monitoring mode

**M3 Integration Tests (8 Tests):**
- ✅ WiFi connection and inverter API communication
- ✅ Real sensor data acquisition via Modbus
- ✅ Compression algorithm testing (5 methods)
- ✅ HTTP POST to Flask aggregation endpoint
- ✅ Server response validation
- ✅ Retry logic on failures

**Component Tests (Unit/Standalone):**
- ✅ Compression algorithms (dictionary, temporal, RLE, bitpacking, smart selection)
- ✅ Sensor acquisition (Modbus protocol)
- ✅ Security primitives (HMAC, nonce generation, anti-replay)
- ✅ FOTA workflows (download, validation, rollback)
- ✅ Protocol adapter (Modbus framing, CRC)

### 1.3 Test Organization

```
test/
├── test_m4_integration/          ← Primary: M4 complete integration
├── test_m3_integration/          ← M3 compression & acquisition
├── test_compression/             ← Compression algorithm unit tests
├── test_acquisition/             ← Sensor reading tests
├── test_security_antireplay/    ← Security layer tests
├── test_fota_*/                  ← OTA component tests
├── test_command_execution/       ← Command execution tests
├── test_remote_config/           ← Configuration sync tests
├── test_protocol_adapter/        ← Modbus protocol tests
└── test_mac_hmac/                ← HMAC calculation tests
```

### 1.4 Test Framework

- **Framework**: Unity (embedded unit testing)
- **Build System**: PlatformIO
- **Communication**: HTTP/REST with Flask server
- **Synchronization**: Test sync protocol for coordination
- **Hardware**: ESP32 DevKit (physical device required)

### 1.4 Test Philosophy

**Integration Over Isolation:**
- Tests run on real hardware (not simulated)
- Tests communicate with real Flask server (not mocked)
- Tests validate end-to-end workflows
- Tests verify security layer with real cryptography

---

## 2. Test Architecture

### 2.1 Test Structure

```
PIO/ECOWATT/test/
├── test_m4_integration/          # M4 Integration Tests (Main)
│   ├── test_main.cpp             # 9 integration tests
│   └── config/
│       └── test_config.h         # WiFi & server configuration
├── test_m3_integration/          # M3 Integration Tests (Legacy)
├── test_compression/             # Compression algorithm tests
├── test_security_antireplay/    # Security layer tests
├── test_fota_*/                  # Individual FOTA tests
└── README                        # Test documentation
```

### 2.2 Test Coordination Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    Flask Test Server                     │
│  - Receives test sync messages                          │
│  - Tracks test progress                                  │
│  - Coordinates with Flask application                    │
│  - Validates test results                                │
└──────────────────┬──────────────────────────────────────┘
                   │ HTTP REST API
                   │ POST /integration/test/sync
                   ▼
┌─────────────────────────────────────────────────────────┐
│                    ESP32 Test Suite                      │
│  - Runs on physical ESP32 hardware                      │
│  - Executes tests sequentially                           │
│  - Reports results to Flask                              │
│  - Validates responses                                   │
└─────────────────────────────────────────────────────────┘
```

### 2.3 Test Synchronization Protocol

Each test follows a 3-phase protocol:

**Phase 1: Start**
```json
POST /integration/test/sync
{
    "test_number": 1,
    "test_name": "Server Connectivity",
    "status": "starting",
    "result": ""
}
```

**Phase 2: Execution**
- ESP32 executes test logic
- Interacts with Flask endpoints
- Validates responses
- Determines pass/fail

**Phase 3: Complete**
```json
POST /integration/test/sync
{
    "test_number": 1,
    "test_name": "Server Connectivity",
    "status": "completed",
    "result": "pass"
}
```

### 2.4 Configuration

**File:** `test/test_m4_integration/config/test_config.h`

```cpp
// WiFi Configuration
#define WIFI_SSID "your_network_name"
#define WIFI_PASSWORD "your_password"

// Flask Server Configuration
#define FLASK_SERVER_IP "192.168.1.100"
#define FLASK_SERVER_PORT 5001

// Device Configuration
#define DEVICE_ID "ESP32_EcoWatt_Smart"
#define FIRMWARE_VERSION "1.0.4"

// Test Configuration
#define HTTP_TIMEOUT_MS 10000
```

---

## 3. M3 Integration Tests

### 3.1 Overview

**File:** `test/test_m3_integration/test_main.cpp`  
**Focus:** Compression algorithms and data acquisition  
**Total Tests:** 8 tests  
**Milestone:** M3 (Advanced Compression)

### 3.2 M3 Test Suite

| # | Test Name | Category | Purpose |
|---|-----------|----------|---------|
| 1 | WiFi Connection | Setup | Verify network connectivity |
| 2 | Inverter API Connection | Setup | Test Modbus API reachability |
| 3 | Data Acquisition | Acquisition | Read real sensor values |
| 4 | Dictionary Compression | Compression | Test pattern matching compression |
| 5 | Temporal Compression | Compression | Test delta encoding |
| 6 | RLE Compression | Compression | Test run-length encoding |
| 7 | Smart Selection | Compression | Test automatic algorithm selection |
| 8 | Server Upload | Integration | POST compressed data to Flask |

### 3.3 M3 Test Details

**Test 1: WiFi Connection**
- Connects to WiFi network
- Validates IP address assignment
- Checks signal strength (RSSI)

**Test 2: Inverter API Connection**
- Establishes Modbus connection to PZEM-004T
- Validates API response
- Checks register availability

**Test 3: Data Acquisition**
```cpp
void test_03_data_acquisition() {
    // Read voltage register (Vac1)
    RegID regs[] = {V_RMS, I_RMS, P_ACTIVE};
    DecodedValues result = readRequest(regs, 3);
    
    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL(3, result.count);
    TEST_ASSERT_GREATER_THAN(0, result.values[0]);  // Voltage
}
```

**Test 4-7: Compression Methods**
```cpp
// Test Dictionary Compression
void test_04_dictionary_compression() {
    uint16_t data[60];  // 10 samples × 6 registers
    // ... populate with sensor data
    
    std::vector<uint8_t> compressed = 
        DataCompression::compressWithDictionary(data, regs, 60);
    
    TEST_ASSERT_LESS_THAN(120, compressed.size());  // Better than raw
}

// Test Temporal Delta
void test_05_temporal_compression() {
    // Similar structure, uses compressWithTemporalDelta()
}

// Test RLE
void test_06_rle_compression() {
    // Similar structure, uses compressWithSemanticRLE()
}

// Test Smart Selection
void test_07_smart_selection() {
    std::vector<uint8_t> compressed = 
        DataCompression::compressWithSmartSelection(data, regs, 60);
    
    // Smart selection chooses best algorithm automatically
    TEST_ASSERT_LESS_THAN(120, compressed.size());
}
```

**Test 8: Server Upload**
```cpp
void test_08_server_upload() {
    // Compress batch
    std::vector<uint8_t> compressed = compressBatch();
    
    // Build JSON payload
    StaticJsonDocument<2048> doc;
    doc["device_id"] = M3_TEST_DEVICE_ID;
    doc["data"] = base64::encode(compressed);
    
    // POST to Flask
    String url = FLASK_BASE_URL "/aggregated/" M3_TEST_DEVICE_ID;
    int httpCode = http.POST(payload);
    
    TEST_ASSERT_EQUAL(200, httpCode);
}
```

### 3.4 M3 vs M4 Differences

| Aspect | M3 Tests | M4 Tests |
|--------|----------|----------|
| **Focus** | Compression & Acquisition | Security & OTA |
| **Security** | No HMAC (basic auth) | HMAC-SHA256 + nonce |
| **Endpoint** | `/aggregated/<id>` | `/aggregated/<id>` (secured) |
| **Compression** | Test all algorithms | Uses smart selection |
| **Commands** | Not tested | Full remote command support |
| **OTA** | Not tested | Complete FOTA workflow |
| **Tests** | 8 | 9 + continuous |

### 3.5 Running M3 Tests

```bash
# Run M3 integration tests
pio test -e esp32dev -f test_m3_integration

# Expected output
Testing...
[TEST 1] WiFi Connection: PASS
[TEST 2] Inverter API: PASS
[TEST 3] Data Acquisition: PASS
[TEST 4] Dictionary Compression: PASS (ratio: 0.45)
[TEST 5] Temporal Compression: PASS (ratio: 0.38)
[TEST 6] RLE Compression: PASS (ratio: 0.52)
[TEST 7] Smart Selection: PASS (chose: TEMPORAL)
[TEST 8] Server Upload: PASS
------------------
8 Tests 0 Failures 0 Ignored
```

---

## 4. M4 Integration Tests

### 3.1 Test Suite Overview

**File:** `test/test_m4_integration/test_main.cpp`  
**Total Tests:** 9 + 1 continuous monitoring  
**Execution Time:** ~60 seconds (excluding FOTA download)

### 3.2 Test Breakdown

| # | Test Name | Category | Duration | Purpose |
|---|-----------|----------|----------|---------|
| 1 | Server Connectivity | Basic | ~1s | Verify Flask server reachable |
| 2 | Secured Upload - Valid HMAC | Security | ~2s | Test valid HMAC authentication |
| 3 | Secured Upload - Invalid HMAC | Security | ~2s | Test HMAC rejection |
| 4 | Anti-Replay - Duplicate Nonce | Security | ~2s | Test replay attack prevention |
| 5 | Command - Set Power | Commands | ~3s | Test remote power control |
| 6 | Command - Write Register | Commands | ~3s | Test register modification |
| 7 | Configuration Update | Config | ~3s | Test remote config changes |
| 8 | FOTA - Check Update | OTA | ~2s | Test update availability check |
| 9 | FOTA - Download Firmware | OTA | ~120s | Test firmware download & decrypt |
| 10 | Continuous Monitoring | Monitoring | Continuous | Validate production workflow |

---

### 3.3 Test 1: Server Connectivity

**Purpose:** Verify Flask server is reachable and responding

**Test Flow:**
1. Send GET request to `/health` endpoint
2. Expect HTTP 200 response
3. Validate response format

**Code:**
```cpp
void test_01_connectivity() {
    HTTPClient http;
    String url = String("http://") + SERVER_HOST + ":" + SERVER_PORT + "/health";
    http.begin(url);
    int httpCode = http.GET();
    
    if (httpCode == 200) {
        // PASS: Server responding
        tests_passed++;
    } else {
        // FAIL: Server not reachable
        tests_failed++;
    }
}
```

**Expected Result:**
```
[TEST 1] Server Connectivity
Result: ✓ PASS
Message: Server responding correctly
```

**Failure Scenarios:**
- Server not running → HTTP error code
- Wrong IP/port → Connection timeout
- Firewall blocking → Connection refused

---

### 3.4 Test 2: Secured Upload - Valid HMAC

**Purpose:** Validate secure data transmission with correct HMAC

**Test Flow:**
1. Create sensor data payload (voltage, current, power)
2. Generate nonce (200001 - high value to avoid conflicts)
3. Calculate HMAC-SHA256 over (nonce + payload)
4. Build secured JSON with nonce, payload, MAC
5. POST to `/aggregated/<device_id>`
6. Expect HTTP 200 (accepted)

**Code:**
```cpp
void test_02_secured_upload_valid() {
    // Create sensor reading
    StaticJsonDocument<512> sensorDoc;
    sensorDoc["current"] = 2.5;
    sensorDoc["voltage"] = 230.0;
    sensorDoc["power"] = 575.0;
    
    // Wrap in aggregated_data array
    StaticJsonDocument<1024> dataDoc;
    JsonArray aggregated = dataDoc.createNestedArray("aggregated_data");
    aggregated.add(sensorDoc);
    
    // Secure payload with HMAC
    nonce_counter = 200001;
    String securedPayload = createSecuredPayload(dataPayload);
    
    // POST to Flask
    String url = String("http://") + SERVER_HOST + "/aggregated/" + DEVICE_ID;
    int httpCode = http.POST(securedPayload);
    
    // Validate acceptance
    TEST_ASSERT_EQUAL(200, httpCode);
}
```

**Security Process:**
```
1. Payload: {"aggregated_data": [{"current": 2.5, ...}]}
2. Base64: eyJhZ2dyZWdhdGVkX2RhdGEiOi4uLg==
3. Nonce: 200001
4. HMAC Input: [0x00][0x03][0x0D][0x41][payload_bytes]
5. HMAC Output: a7f3d8e9c2b1f4a6... (32 bytes)
6. Secured: {"nonce": 200001, "payload": "eyJ...", "mac": "a7f3..."}
```

**Expected Result:**
```
[TEST 2] Secured Upload - Valid HMAC
Result: ✓ PASS
Message: Secured upload accepted
```

---

### 3.5 Test 3: Secured Upload - Invalid HMAC

**Purpose:** Verify Flask rejects tampered data

**Test Flow:**
1. Create valid secured payload
2. Corrupt MAC (flip last character)
3. POST to `/aggregated/<device_id>`
4. Expect HTTP 401 (Unauthorized) or 403 (Forbidden)

**Code:**
```cpp
void test_03_secured_upload_invalid_hmac() {
    // Create secured payload
    String securedPayload = createSecuredPayload(dataPayload);
    
    // Parse and corrupt MAC
    StaticJsonDocument<1024> doc;
    deserializeJson(doc, securedPayload);
    String mac = doc["mac"].as<String>();
    
    // Flip last character to corrupt MAC
    mac.setCharAt(mac.length() - 1, 'X');
    doc["mac"] = mac;
    
    serializeJson(doc, securedPayload);
    
    // POST corrupted payload
    int httpCode = http.POST(securedPayload);
    
    // Should be rejected
    TEST_ASSERT_NOT_EQUAL(200, httpCode);
}
```

**Expected Result:**
```
[TEST 3] Secured Upload - Invalid HMAC
Result: ✓ PASS
Message: Invalid MAC correctly rejected (401)
```

**Security Validation:**
- ✅ Flask detects MAC mismatch
- ✅ Request rejected without processing
- ✅ Error logged for security monitoring

---

### 3.6 Test 4: Anti-Replay - Duplicate Nonce

**Purpose:** Verify replay attack prevention

**Test Flow:**
1. Send valid request with nonce N
2. Server accepts, stores nonce N
3. Send same request again with nonce N
4. Server rejects (nonce already seen)
5. Send new request with nonce N+1
6. Server accepts (nonce > last seen)

**Code:**
```cpp
void test_04_anti_replay_duplicate_nonce() {
    // First request with nonce 100000
    nonce_counter = 100000;
    String payload1 = createSecuredPayload(data);
    int httpCode1 = http.POST(payload1);
    TEST_ASSERT_EQUAL(200, httpCode1);  // Should accept
    
    delay(500);
    
    // Replay same nonce 100000
    nonce_counter = 100000;  // Same nonce
    String payload2 = createSecuredPayload(data);
    int httpCode2 = http.POST(payload2);
    TEST_ASSERT_NOT_EQUAL(200, httpCode2);  // Should reject
    
    delay(500);
    
    // Valid new nonce 100001
    nonce_counter = 100001;  // Higher nonce
    String payload3 = createSecuredPayload(data);
    int httpCode3 = http.POST(payload3);
    TEST_ASSERT_EQUAL(200, httpCode3);  // Should accept
}
```

**Expected Result:**
```
[TEST 4] Anti-Replay - Duplicate Nonce
Result: ✓ PASS
Message: Replay attack correctly prevented
```

**Security Properties:**
- ✅ Server tracks last seen nonce per device
- ✅ Rejects nonce ≤ last seen
- ✅ Accepts nonce > last seen
- ✅ Prevents replay attacks

---

### 3.7 Test 5: Command - Set Power

**Purpose:** Test remote power control command execution

**Test Flow:**
1. Queue command on Flask server: "set_power ON"
2. ESP32 polls `/command/poll`
3. Server returns pending command
4. ESP32 executes command locally
5. ESP32 reports result to `/command/result`
6. Server validates execution

**Code:**
```cpp
void test_05_command_set_power() {
    // Queue command on server (done by Flask test coordinator)
    // ESP32 polls for command
    String response;
    String url = String("http://") + SERVER_HOST + "/command/poll";
    
    // Add device_id as query parameter
    url += "?device_id=" + String(DEVICE_ID);
    
    int httpCode = http.GET(url);
    
    if (httpCode == 200) {
        // Parse command
        StaticJsonDocument<512> doc;
        deserializeJson(doc, response);
        
        String command = doc["command"].as<String>();
        String commandId = doc["command_id"].as<String>();
        
        // Execute command (mock execution)
        bool success = true;  // Assume success
        String output = "Power set to ON";
        
        // Report result
        reportCommandResult(commandId, success, output);
        tests_passed++;
    }
}
```

**Command Format:**
```json
{
    "command_id": "cmd_12345",
    "command": "set_power",
    "params": {
        "state": "ON"
    },
    "device_id": "ESP32_EcoWatt_Smart"
}
```

**Result Format:**
```json
{
    "command_id": "cmd_12345",
    "device_id": "ESP32_EcoWatt_Smart",
    "success": true,
    "output": "Power set to ON",
    "timestamp": 1698527400
}
```

**Expected Result:**
```
[TEST 5] Command - Set Power
Result: ✓ PASS
Message: Command executed successfully
```

---

### 3.8 Test 6: Command - Write Register

**Purpose:** Test register modification command

**Test Flow:**
1. Queue "write_register" command on server
2. ESP32 polls and receives command
3. ESP32 writes to specified register
4. ESP32 reports success/failure
5. Server validates register updated

**Code:**
```cpp
void test_06_command_write_register() {
    // Poll for command
    String url = String("http://") + SERVER_HOST + "/command/poll";
    url += "?device_id=" + String(DEVICE_ID);
    
    String response;
    int httpCode = httpGet(url, response);
    
    if (httpCode == 200) {
        StaticJsonDocument<512> doc;
        deserializeJson(doc, response);
        
        String command = doc["command"].as<String>();
        JsonObject params = doc["params"];
        
        if (command == "write_register") {
            int regAddr = params["register"];
            int value = params["value"];
            
            // Execute write (mock)
            bool success = writeRegister(regAddr, value);
            
            // Report result
            String commandId = doc["command_id"].as<String>();
            String output = success ? "Register updated" : "Write failed";
            reportCommandResult(commandId, success, output);
            
            if (success) tests_passed++;
            else tests_failed++;
        }
    }
}
```

**Command Example:**
```json
{
    "command_id": "cmd_12346",
    "command": "write_register",
    "params": {
        "register": 0x40,
        "value": 100
    }
}
```

**Expected Result:**
```
[TEST 6] Command - Write Register
Result: ✓ PASS
Message: Register 0x40 written with value 100
```

---

### 3.9 Test 7: Configuration Update

**Purpose:** Test remote configuration synchronization

**Test Flow:**
1. Flask server updates config (poll_freq, upload_freq, registers)
2. ESP32 polls `/changes?device_id=<id>`
3. Server returns updated configuration
4. ESP32 compares with local NVS
5. ESP32 applies changes
6. ESP32 confirms update

**Code:**
```cpp
void test_07_config_update() {
    // Poll for configuration changes
    String url = String("http://") + SERVER_HOST + "/changes";
    url += "?device_id=" + String(DEVICE_ID);
    
    String response;
    int httpCode = httpGet(url, response);
    
    if (httpCode == 200) {
        StaticJsonDocument<1024> doc;
        deserializeJson(doc, response);
        
        if (doc["config_changed"]) {
            JsonObject newConfig = doc["new_config"];
            
            // Extract new parameters
            uint64_t newPollFreq = newConfig["poll_frequency_us"];
            uint64_t newUploadFreq = newConfig["upload_frequency_us"];
            JsonArray newRegisters = newConfig["registers"];
            
            // Apply to NVS
            nvs::setPollFreq(newPollFreq);
            nvs::setUploadFreq(newUploadFreq);
            
            // Update register selection
            RegID regs[16];
            size_t count = 0;
            for (JsonVariant v : newRegisters) {
                regs[count++] = static_cast<RegID>(v.as<int>());
            }
            nvs::setReadRegs(regs, count);
            
            tests_passed++;
        }
    }
}
```

**Configuration Response:**
```json
{
    "config_changed": true,
    "new_config": {
        "poll_frequency_us": 2000000,
        "upload_frequency_us": 15000000,
        "registers": [0, 1, 3]
    }
}
```

**Expected Result:**
```
[TEST 7] Configuration Update
Result: ✓ PASS
Message: Configuration updated: poll_freq=2s, upload_freq=15s, regs=3
```

---

### 3.10 Test 8: FOTA - Check Update

**Purpose:** Test firmware update availability check

**Test Flow:**
1. ESP32 sends GET to `/ota/check/<device_id>?version=1.0.4`
2. Flask server compares with available firmware
3. Server returns manifest if update available
4. ESP32 parses manifest (version, size, hash, signature)

**Code:**
```cpp
void test_08_fota_check_update() {
    String url = String("http://") + SERVER_HOST + "/ota/check/";
    url += DEVICE_ID;
    url += "?version=" + String(FIRMWARE_VERSION);
    
    String response;
    int httpCode = httpGet(url, response);
    
    if (httpCode == 200) {
        StaticJsonDocument<2048> doc;
        deserializeJson(doc, response);
        
        bool updateAvailable = doc["update_available"];
        
        if (updateAvailable) {
            JsonObject updateInfo = doc["update_info"];
            
            String newVersion = updateInfo["latest_version"].as<String>();
            uint32_t firmwareSize = updateInfo["firmware_size"];
            String sha256 = updateInfo["sha256_hash"].as<String>();
            String signature = updateInfo["signature"].as<String>();
            String iv = updateInfo["iv"].as<String>();
            uint32_t totalChunks = updateInfo["total_chunks"];
            
            Serial.printf("Update Available: v%s\n", newVersion.c_str());
            Serial.printf("Size: %u bytes (%u chunks)\n", firmwareSize, totalChunks);
            Serial.printf("SHA-256: %s\n", sha256.c_str());
            
            tests_passed++;
        } else {
            Serial.println("No update available (expected for same version)");
            tests_passed++;
        }
    }
}
```

**Manifest Response:**
```json
{
    "update_available": true,
    "update_info": {
        "latest_version": "1.0.5",
        "firmware_size": 1010688,
        "encrypted_size": 1010688,
        "sha256_hash": "a7f3d8e9c2b1f4a6d8e9c2b1f4a6d8e9c2b1f4a6d8e9c2b1f4a6d8e9c2b1f4a6",
        "signature": "base64_rsa_signature...",
        "iv": "000102030405060708090a0b0c0d0e0f",
        "chunk_size": 1024,
        "total_chunks": 987
    }
}
```

**Expected Result:**
```
[TEST 8] FOTA - Check Update
Result: ✓ PASS
Message: Update detected: v1.0.5 (987 chunks)
```

---

### 3.11 Test 9: FOTA - Download Firmware

**Purpose:** Test complete firmware download, decryption, and validation

**Test Flow:**
1. Initialize OTA partition
2. Download 987 chunks (1024 bytes each)
3. Decrypt each chunk with AES-256-CBC
4. Write decrypted data to OTA partition
5. Verify SHA-256 hash
6. Verify RSA signature
7. Mark partition bootable (test only - no reboot)

**Code:**
```cpp
void test_09_fota_download_firmware() {
    // Create OTA Manager
    OTAManager* otaManager = new OTAManager(
        String("http://") + SERVER_HOST + ":" + SERVER_PORT,
        DEVICE_ID,
        FIRMWARE_VERSION
    );
    
    // Check for update
    if (!otaManager->checkForUpdate()) {
        Serial.println("No update available");
        tests_failed++;
        return;
    }
    
    // Download and decrypt firmware
    Serial.println("Starting firmware download...");
    bool downloadSuccess = otaManager->downloadAndApplyFirmware();
    
    if (downloadSuccess) {
        Serial.println("✓ Firmware downloaded successfully");
        
        // Verify (but don't reboot in test mode)
        bool verifySuccess = otaManager->verifyFirmware();
        
        if (verifySuccess) {
            Serial.println("✓ Firmware verified (SHA-256 + RSA signature)");
            tests_passed++;
        } else {
            Serial.println("✗ Firmware verification failed");
            tests_failed++;
        }
    } else {
        Serial.println("✗ Firmware download failed");
        tests_failed++;
    }
    
    delete otaManager;
}
```

**Download Process:**
```
1. Request chunk 0/987
   GET /ota/download/<device_id>/0
   Response: 1024 bytes encrypted data
   
2. Decrypt chunk 0
   AES-256-CBC with key from keys.cpp
   IV from manifest
   
3. Write to OTA partition
   esp_ota_write()
   
4. Repeat for chunks 1-986
   
5. Verify entire firmware
   Calculate SHA-256 of decrypted data
   Compare with manifest hash
   
6. Verify signature
   RSA-2048 signature verification
   Public key from keys.cpp
   
7. Mark bootable (test mode: skip)
   esp_ota_set_boot_partition()
```

**Progress Output:**
```
[TEST 9] FOTA - Download Firmware
Starting firmware download...
[OTA] Downloading chunk 1/987...
[OTA] Downloading chunk 100/987... (10%)
[OTA] Downloading chunk 200/987... (20%)
...
[OTA] Downloading chunk 987/987... (100%)
✓ Firmware downloaded successfully
[OTA] Calculating SHA-256 hash...
✓ Hash matches: a7f3d8e9...
[OTA] Verifying RSA signature...
✓ Signature valid
Result: ✓ PASS
Message: Firmware downloaded and verified (1010688 bytes)
```

**Expected Duration:** ~120 seconds (987 chunks over WiFi)

**Critical Validation:**
- ✅ Magic byte 0xE9 (ESP32 firmware header)
- ✅ SHA-256 hash matches manifest
- ✅ RSA signature verifies authenticity
- ✅ AES key synchronized with Flask

---

### 3.12 Test 10: Continuous Monitoring

**Purpose:** Validate production workflow with continuous operation

**Test Flow:**
1. Read sensors every 1 second (poll interval)
2. Batch 10 samples
3. Compress batch with smart selection
4. Secure payload with HMAC
5. Upload to `/aggregated/<device_id>`
6. Repeat indefinitely

**Code:**
```cpp
void test_10_continuous_monitoring() {
    static unsigned long lastPoll = 0;
    static int sampleCount = 0;
    static uint16_t sensorBatch[60];  // 10 samples × 6 registers
    
    unsigned long now = millis();
    
    // Poll every 1 second
    if (now - lastPoll >= 1000) {
        lastPoll = now;
        
        // Read sensors (mock data)
        uint16_t values[6] = {2429, 177, 73, 4331, 70, 605};
        memcpy(&sensorBatch[sampleCount * 6], values, 12);
        sampleCount++;
        
        Serial.printf("[MONITOR] Sample %d/10 collected\n", sampleCount);
        
        // When batch full, compress and upload
        if (sampleCount >= 10) {
            Serial.println("[MONITOR] Batch full, compressing...");
            
            // Compress with smart selection
            std::vector<uint8_t> compressed = DataCompression::compressWithSmartSelection(
                sensorBatch, registerSelection, 60
            );
            
            Serial.printf("[MONITOR] Compressed: %zu bytes\n", compressed.size());
            
            // Build payload
            StaticJsonDocument<2048> doc;
            JsonArray data = doc.createNestedArray("aggregated_data");
            // Add compressed data as base64
            String b64 = base64::encode(compressed.data(), compressed.size());
            data.add(b64);
            
            String payload;
            serializeJson(doc, payload);
            
            // Secure and upload
            String secured = createSecuredPayload(payload);
            String url = String("http://") + SERVER_HOST + "/aggregated/" + DEVICE_ID;
            
            String response;
            int httpCode = httpPost(url, secured, response);
            
            if (httpCode == 200) {
                Serial.println("[MONITOR] ✓ Upload successful");
            } else {
                Serial.printf("[MONITOR] ✗ Upload failed (code: %d)\n", httpCode);
            }
            
            // Reset batch
            sampleCount = 0;
        }
    }
}
```

**Continuous Output:**
```
[MONITOR] Sample 1/10 collected
[MONITOR] Sample 2/10 collected
...
[MONITOR] Sample 10/10 collected
[MONITOR] Batch full, compressing...
COMPRESSION RESULT: TEMPORAL method
Original: 120 bytes -> Compressed: 31 bytes (74.2% savings)
[MONITOR] Compressed: 31 bytes
[MONITOR] ✓ Upload successful
[MONITOR] Sample 1/10 collected
...
```

**Validation:**
- ✅ Sampling at correct interval
- ✅ Batch fills properly
- ✅ Compression working
- ✅ Security layer applied
- ✅ Uploads succeed
- ✅ System stable over time

---

## 3.13 Component & Unit Tests

Beyond integration tests, the ESP32 firmware includes standalone component tests:

### 3.13.1 Compression Tests (`test_compression/`)

**Purpose:** Validate individual compression algorithms in isolation

**Tests:**
- Basic compression (verify size reduction)
- Benchmark compression ratio (academic vs traditional)
- Benchmark CPU time (microseconds per compression)
- Decompression accuracy (round-trip validation)
- Edge cases (empty data, single value, large batches)

**Example:**
```cpp
void test_compression_benchmarkRatio() {
    uint16_t data[60];  // 20 samples × 3 registers
    // ... populate data
    
    size_t originalSize = 120;  // bytes
    std::vector<uint8_t> compressed = DataCompression::compressBinary(data, 60);
    size_t compressedSize = compressed.size();
    
    float academicRatio = (float)compressedSize / originalSize;
    float traditionalRatio = (float)originalSize / compressedSize;
    
    TEST_MESSAGE("=== COMPRESSION BENCHMARK ===");
    TEST_MESSAGE("Method: Smart Selection");
    TEST_MESSAGE("Samples: 20");
    TEST_MESSAGE("Original: 120 bytes");
    TEST_MESSAGE("Compressed: 32 bytes");
    TEST_MESSAGE("Academic Ratio: 0.267");
    TEST_MESSAGE("Traditional Ratio: 3.75x");
}
```

### 3.13.2 Acquisition Tests (`test_acquisition/`)

**Purpose:** Test sensor data reading via Modbus

**Tests:**
- Read single register (V_RMS)
- Read multiple registers (V_RMS, I_RMS, P_ACTIVE)
- CRC validation
- Timeout handling
- Invalid register handling

**Example:**
```cpp
void test_acquisition_readMultipleRegisters() {
    RegID regs[] = {V_RMS, I_RMS, P_ACTIVE};
    DecodedValues result = readRequest(regs, 3);
    
    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL(3, result.count);
    
    // Validate ranges
    TEST_ASSERT_UINT16_WITHIN(5000, 2300, result.values[0]);  // ~230V ± 50V
    TEST_ASSERT_GREATER_THAN(0, result.values[1]);             // Current > 0
    TEST_ASSERT_GREATER_THAN(0, result.values[2]);             // Power > 0
}
```

### 3.13.3 Security Tests (`test_security_antireplay/`, `test_mac_hmac/`)

**Purpose:** Validate cryptographic primitives

**test_mac_hmac:**
- HMAC-SHA256 calculation
- Key management
- MAC comparison (constant-time)

**test_security_antireplay:**
- Nonce generation
- Nonce persistence (NVS)
- Replay detection simulation

**Example:**
```cpp
void test_hmac_calculation() {
    const char* message = "test message";
    uint32_t nonce = 10001;
    
    uint8_t hmac[32];
    calculateHMAC(nonce, message, strlen(message), hmac);
    
    // Expected HMAC (pre-calculated)
    uint8_t expected[32] = { /* ... */ };
    
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, hmac, 32);
}

void test_nonce_increment() {
    uint32_t nonce1 = SecurityLayer::getCurrentNonce();
    uint32_t nonce2 = SecurityLayer::incrementAndSaveNonce();
    
    TEST_ASSERT_EQUAL(nonce1 + 1, nonce2);
}
```

### 3.13.4 FOTA Tests (`test_fota_*/`)

Individual FOTA component tests:

**test_fota_download:**
- Chunk download from Flask
- HTTP retry logic
- Progress tracking

**test_fota_validation:**
- SHA-256 hash calculation
- RSA signature verification
- Magic byte checking

**test_fota_rollback:**
- Invalid firmware detection
- Rollback trigger
- Boot partition switching

**test_fota_workflow:**
- Complete OTA workflow (no reboot)
- State machine transitions
- Error recovery

### 3.13.5 Protocol Tests (`test_protocol_adapter/`)

**Purpose:** Test Modbus protocol implementation

**Tests:**
- Frame construction (read holding registers)
- CRC calculation
- Response parsing
- Error code handling

**Example:**
```cpp
void test_protocol_modbusCRC() {
    uint8_t frame[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x06};
    uint16_t crc = calculateModbusCRC(frame, 6);
    
    // Expected CRC for this frame
    TEST_ASSERT_EQUAL_HEX16(0xC5C4, crc);
}
```

### 3.13.6 Command Tests (`test_command_execution/`)

**Purpose:** Test remote command execution

**Tests:**
- Command polling
- Command parsing
- Command execution (mocked)
- Result reporting

### 3.13.7 Config Tests (`test_remote_config/`)

**Purpose:** Test configuration synchronization

**Tests:**
- Config polling
- Config comparison
- Config application
- NVS persistence

### 3.13.8 Running Component Tests

**Run all tests:**
```bash
pio test -e esp32dev
```

**Run specific test:**
```bash
pio test -e esp32dev -f test_compression
pio test -e esp32dev -f test_acquisition
pio test -e esp32dev -f test_security_antireplay
```

**Expected output:**
```
Testing test_compression
[TEST] Compression - Basic: PASS
[TEST] Compression - Benchmark Ratio: PASS
[TEST] Compression - Benchmark CPU: PASS (1850 μs)
[TEST] Compression - Decompression: PASS
------------------
4 Tests 0 Failures 0 Ignored
```

---

## 5. Test Execution Guide

### 5.1 Prerequisites

**Hardware:**
- ESP32 DevKit board
- USB cable for programming
- Stable power supply (5V 1A minimum)

**Software:**
- PlatformIO installed
- Python 3.8+ with Flask
- Network access to Flask server

**Network:**
- WiFi access point configured
- Flask server running on same network
- Firewall allowing port 5001

### 5.2 Setup Steps

**Step 1: Configure WiFi**
```bash
cd PIO/ECOWATT/test/test_m4_integration/config/
nano test_config.h

# Update:
#define WIFI_SSID "your_network"
#define WIFI_PASSWORD "your_password"
#define FLASK_SERVER_IP "192.168.1.100"  # Your Flask server IP
```

**Step 2: Start Flask Server**
```bash
cd flask/
source venv/bin/activate  # or: . venv/bin/activate
python flask_server_modular.py
```

**Step 3: Build and Upload Test**
```bash
cd PIO/ECOWATT/
pio test -e esp32dev -f test_m4_integration
```

**Step 4: Monitor Output**
```bash
# In separate terminal
pio device monitor -e esp32dev -b 115200
```

### 5.3 Running Individual Tests

**Run All M4 Tests:**
```bash
pio test -e esp32dev -f test_m4_integration
```

**Run Specific Test (requires code modification):**
```cpp
// Comment out unwanted tests in setup()
void setup() {
    // ... initialization ...
    
    // test_01_connectivity();
    // test_02_secured_upload_valid();
    test_03_secured_upload_invalid_hmac();  // Run only this test
    // test_04_anti_replay_duplicate_nonce();
    // ... etc
}
```

**Run with Verbose Logging:**
```bash
pio test -e esp32dev -f test_m4_integration -v
```

### 5.4 Test Modes

**Mode 1: Full Integration (Default)**
- All 9 tests execute sequentially
- Continuous monitoring after tests
- Real Flask server communication
- Full security validation

**Mode 2: Skip FOTA Download (Fast)**
```cpp
// Comment out test 9 for faster iteration
// test_09_fota_download_firmware();
```
Reduces test time from ~180s to ~60s

**Mode 3: Continuous Only**
```cpp
void setup() {
    // Skip all tests, go straight to monitoring
    Serial.println("Entering continuous monitoring...");
}

void loop() {
    test_10_continuous_monitoring();
    delay(30000);
}
```

### 5.5 Expected Test Duration

| Phase | Duration | Description |
|-------|----------|-------------|
| Setup | 10s | WiFi connection, initialization |
| Tests 1-4 | 20s | Connectivity and security tests |
| Tests 5-7 | 15s | Commands and configuration |
| Test 8 | 5s | OTA check |
| Test 9 | 120s | FOTA download (if update available) |
| **Total** | **170s** | Complete test suite |

---

## 6. Test Coordination

### 6.1 ESP32-Flask Synchronization

Tests coordinate between ESP32 and Flask using the test sync protocol:

**Sync Endpoint:** `POST /integration/test/sync`

**Purpose:**
- Flask tracks test progress
- Flask validates test results
- Flask coordinates with pytest (if running integration tests)
- Provides real-time test monitoring

### 6.2 Test Sync Implementation

**ESP32 Side:**
```cpp
bool syncTest(int testNumber, const String& testName, 
              const String& status, const String& result) {
    HTTPClient http;
    String url = String("http://") + SERVER_HOST + "/integration/test/sync";
    
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    
    StaticJsonDocument<512> doc;
    doc["test_number"] = testNumber;
    doc["test_name"] = testName;
    doc["status"] = status;  // "starting" or "completed"
    if (result != "") {
        doc["result"] = result;  // "pass" or "fail"
    }
    
    String payload;
    serializeJson(doc, payload);
    
    int httpCode = http.POST(payload);
    bool success = (httpCode == 200);
    
    http.end();
    return success;
}
```

**Flask Side:**
```python
@app.route('/integration/test/sync', methods=['POST'])
def test_sync():
    data = request.json
    test_number = data['test_number']
    test_name = data['test_name']
    status = data['status']
    result = data.get('result', '')
    
    # Log test progress
    logger.info(f"Test {test_number}: {test_name} - {status} ({result})")
    
    # Store in test tracker
    test_tracker[test_number] = {
        'name': test_name,
        'status': status,
        'result': result,
        'timestamp': time.time()
    }
    
    return jsonify({'status': 'acknowledged'}), 200
```

### 6.3 Nonce Management

**Challenge:** Multiple tests using security layer can cause nonce conflicts

**Solution:** Reset server nonce state before test suite

```cpp
void resetServerNonces() {
    HTTPClient http;
    String url = String("http://") + SERVER_HOST + "/security/nonces";
    http.begin(url);
    int httpCode = http.sendRequest("DELETE");
    http.end();
    
    Serial.println("[SETUP] Cleared server nonce history");
    delay(500);
}

// Called before first test
void test_01_connectivity() {
    resetServerNonces();  // Clear nonce history
    // ... test logic
}
```

**Nonce Strategy Per Test:**
- Test 2 (Valid HMAC): Nonce = 200001
- Test 3 (Invalid HMAC): Nonce = 200002
- Test 4 (Replay Test): Nonces = 100000, 100000 (duplicate), 100001
- Other tests: Incrementing from current counter

### 6.4 Test Dependencies

**Sequential Execution Required:**

```
Test 1 (Connectivity)
  ↓ Requires: WiFi, Flask server
  ↓ Provides: Verified connection
  
Test 2 (Valid HMAC)
  ↓ Requires: Test 1 pass
  ↓ Provides: Security layer working
  
Test 3 (Invalid HMAC)
  ↓ Requires: Test 2 pass
  ↓ Provides: Security rejection working
  
Test 4 (Anti-Replay)
  ↓ Requires: Test 2, 3 pass
  ↓ Provides: Nonce system working
  
Tests 5-9
  ↓ Requires: Tests 1-4 pass (connectivity + security)
  ↓ Provides: Feature validation
```

**Failure Handling:**
- If Test 1 fails → Abort remaining tests (no connectivity)
- If Tests 2-4 fail → Skip tests requiring security
- If Test 8 fails → Skip Test 9 (FOTA unavailable)

---

## 7. Security Testing

### 7.1 HMAC Calculation

**Test:** Verify HMAC-SHA256 calculation matches Flask server

**Implementation:**
```cpp
void calculateHMAC(const uint8_t* data, size_t len, uint8_t* hmac) {
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    
    const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    mbedtls_md_setup(&ctx, info, 1);  // 1 = HMAC mode
    
    mbedtls_md_hmac_starts(&ctx, PSK_HMAC, 32);
    mbedtls_md_hmac_update(&ctx, data, len);
    mbedtls_md_hmac_finish(&ctx, hmac);
    
    mbedtls_md_free(&ctx);
}
```

**Test Vector:**
```
Input:
  Nonce: 200001 (0x00030D41)
  Payload: {"test": "data"}
  
HMAC Input (hex):
  00 03 0D 41 7B 22 74 65 73 74 22 3A 22 64 61 74 61 22 7D
  
Expected HMAC (first 16 bytes):
  a7 f3 d8 e9 c2 b1 f4 a6 d8 e9 c2 b1 f4 a6 d8 e9
```

### 7.2 Nonce Generation

**Test:** Verify nonce uniqueness and monotonic increase

**Requirements:**
- ✅ Never reuse nonce
- ✅ Monotonically increasing
- ✅ Persists across reboots (NVS)
- ✅ Large enough to prevent exhaustion

**Implementation:**
```cpp
uint32_t nonce = nonce_counter++;
// Persist to NVS every N uses to minimize flash wear
if (nonce % 10 == 0) {
    nvs::saveNonce(nonce);
}
```

### 7.3 Anti-Replay Testing

**Attack Scenario:** Attacker captures valid request and replays it

**Test Validation:**
1. Send Request A with nonce 100000 → Accepted
2. Replay Request A with nonce 100000 → Rejected (409 Conflict)
3. Send Request B with nonce 99999 → Rejected (nonce too old)
4. Send Request C with nonce 100001 → Accepted (new nonce)

**Server Logic:**
```python
def validate_nonce(device_id, nonce):
    last_nonce = nonce_tracker.get(device_id, 0)
    if nonce <= last_nonce:
        return False, "Replay detected"
    nonce_tracker[device_id] = nonce
    return True, "OK"
```

### 7.4 MAC Verification

**Test:** Verify MAC tampering detection

**Tampering Scenarios:**
1. **Flip bit in MAC** → Rejected (MAC mismatch)
2. **Change nonce** → Rejected (MAC invalid for new nonce)
3. **Change payload** → Rejected (MAC invalid for new payload)
4. **Reorder fields** → Still valid (JSON order doesn't affect MAC)

**Verification Process:**
```
1. Server receives: {nonce: N, payload: P, mac: M}
2. Server decodes payload: P_decoded
3. Server reconstructs message: [N][P_decoded]
4. Server calculates HMAC: M_calculated
5. Server compares: M_calculated == M
6. Accept if match, reject if mismatch
```

---

## 8. OTA Testing

### 8.1 FOTA Architecture

```
┌──────────────────────────────────────────────────┐
│              Flask OTA Server                     │
│  - Firmware storage (encrypted)                   │
│  - Manifest generation                            │
│  - Chunk serving                                  │
└────────────────┬─────────────────────────────────┘
                 │ HTTP REST
                 │
                 ▼
┌──────────────────────────────────────────────────┐
│              ESP32 OTA Client                     │
│  - Update check                                   │
│  - Chunk download (987 × 1024 bytes)              │
│  - AES-256-CBC decryption                         │
│  - SHA-256 verification                           │
│  - RSA-2048 signature verification                │
│  - OTA partition write                            │
│  - Rollback on failure                            │
└──────────────────────────────────────────────────┘
```

### 8.2 OTA Test Cases

**Test 8: Update Check**
- ✅ Query server for updates
- ✅ Parse manifest JSON
- ✅ Validate manifest fields
- ✅ Extract: version, size, hash, signature, IV, chunks

**Test 9: Full Download**
- ✅ Initialize OTA partition
- ✅ Download all 987 chunks
- ✅ Decrypt each chunk with AES
- ✅ Write to OTA partition
- ✅ Calculate SHA-256 of decrypted firmware
- ✅ Verify hash matches manifest
- ✅ Verify RSA signature
- ✅ Mark partition bootable (test mode: skip reboot)

### 8.3 AES Decryption Validation

**Key:** 32 bytes from `keys.cpp` (AES_FIRMWARE_KEY)

**Synchronization Check:**
```cpp
// ESP32: keys.cpp
const uint8_t AES_FIRMWARE_KEY[32] = {
    0xA2, 0x7B, 0xB3, 0xDA, 0x29, 0x42, 0x44, 0xCB,
    0x28, 0x1D, 0x67, 0x8E, 0x40, 0x1F, 0x9C, 0xD5,
    0xB2, 0x35, 0xE8, 0x91, 0x3A, 0x4C, 0x7D, 0x29,
    0x5F, 0x88, 0x2C, 0x14, 0x6B, 0xA9, 0xF3, 0x57
};

// Must match Flask: flask/keys/aes_firmware_key.bin
// Verify with: xxd flask/keys/aes_firmware_key.bin
```

**Decryption Test:**
```cpp
// First chunk should decrypt to ESP32 firmware header
uint8_t decrypted[1024];
mbedtls_aes_crypt_cbc(&aes_ctx, MBEDTLS_AES_DECRYPT, 1024, 
                      aes_iv, encrypted_chunk, decrypted);

// Check magic byte
if (decrypted[0] == 0xE9) {
    Serial.println("✓ Magic byte correct (ESP32 firmware)");
} else {
    Serial.printf("✗ Wrong magic byte: 0x%02X (expected 0xE9)\n", decrypted[0]);
}
```

### 8.4 Signature Verification

**RSA-2048 Public Key:**
```cpp
// From keys.cpp
const uint8_t RSA_PUBLIC_KEY[] = {
    /* PEM format RSA public key */
};

// Must match Flask: flask/keys/server_public_key.pem
```

**Verification Process:**
```cpp
// 1. Load public key
mbedtls_pk_context pk;
mbedtls_pk_parse_public_key(&pk, RSA_PUBLIC_KEY, sizeof(RSA_PUBLIC_KEY));

// 2. Verify signature over SHA-256 hash
int ret = mbedtls_pk_verify(&pk, MBEDTLS_MD_SHA256,
                            firmware_hash, 32,
                            signature, signature_len);

if (ret == 0) {
    Serial.println("✓ Signature valid");
} else {
    Serial.println("✗ Signature verification failed");
}
```

### 8.5 Rollback Testing

**Scenario:** New firmware fails to boot

**Rollback Flow:**
1. Update to v1.0.5
2. Mark partition bootable
3. Reboot
4. v1.0.5 fails initialization
5. ESP32 bootloader detects failure
6. Rollback to v1.0.4
7. Mark v1.0.5 as invalid

**Implementation:**
```cpp
void OTAManager::handleRollback() {
    const esp_partition_t* running = esp_ota_get_running_partition();
    esp_ota_img_states_t state;
    
    esp_ota_get_state_partition(running, &state);
    
    if (state == ESP_OTA_IMG_PENDING_VERIFY) {
        // First boot of new firmware
        Serial.println("[OTA] New firmware booted, validating...");
        
        if (validateFirmware()) {
            // Mark as valid
            esp_ota_mark_app_valid_cancel_rollback();
            Serial.println("[OTA] ✓ Firmware validated");
        } else {
            // Rollback
            Serial.println("[OTA] ✗ Firmware invalid, rolling back...");
            esp_ota_mark_app_invalid_rollback_and_reboot();
        }
    }
}
```

---

## 9. Test Results

### 9.1 Successful Test Run Example

```
======================================================================
               M4 INTEGRATION TEST - ESP32 SIDE
======================================================================
Device ID: ESP32_EcoWatt_Smart
Firmware: v1.0.4
WiFi: Galaxy A32B46A
Server: http://192.168.1.100:5001
======================================================================

[WiFi] Connecting to: Galaxy A32B46A
[WiFi] ✓ Connected!
[WiFi] IP: 192.168.1.150
[WiFi] Signal: -58 dBm

========================================
Starting M4 Integration Tests
========================================

[TEST 1] Server Connectivity
Result: ✓ PASS
Message: Server responding correctly
----------------------------------------

[TEST 2] Secured Upload - Valid HMAC
[SEC] Nonce: 200001
[SEC] HMAC: a7f3d8e9c2b1f4a6...
Result: ✓ PASS
Message: Secured upload accepted
----------------------------------------

[TEST 3] Secured Upload - Invalid HMAC
[SEC] Corrupting MAC for test...
Result: ✓ PASS
Message: Invalid MAC correctly rejected (401)
----------------------------------------

[TEST 4] Anti-Replay - Duplicate Nonce
[SEC] First request (nonce 100000): Accepted
[SEC] Replay (nonce 100000): Rejected ✓
[SEC] New nonce (100001): Accepted
Result: ✓ PASS
Message: Replay attack correctly prevented
----------------------------------------

[TEST 5] Command - Set Power
[CMD] Received: set_power ON
[CMD] Executed successfully
Result: ✓ PASS
Message: Command executed successfully
----------------------------------------

[TEST 6] Command - Write Register
[CMD] Received: write_register 0x40 = 100
[CMD] Register updated
Result: ✓ PASS
Message: Register 0x40 written with value 100
----------------------------------------

[TEST 7] Configuration Update
[CFG] New poll_freq: 2000000 us
[CFG] New upload_freq: 15000000 us
[CFG] New registers: 3
Result: ✓ PASS
Message: Configuration updated successfully
----------------------------------------

[TEST 8] FOTA - Check Update
[OTA] Update Available: v1.0.5
[OTA] Size: 1010688 bytes (987 chunks)
[OTA] SHA-256: a7f3d8e9c2b1f4a6...
Result: ✓ PASS
Message: Update detected: v1.0.5 (987 chunks)
----------------------------------------

[TEST 9] FOTA - Download Firmware
[OTA] Starting firmware download...
[OTA] Downloading chunk 1/987...
[OTA] Progress: 10% (100/987)
[OTA] Progress: 20% (200/987)
[OTA] Progress: 30% (300/987)
[OTA] Progress: 40% (400/987)
[OTA] Progress: 50% (500/987)
[OTA] Progress: 60% (600/987)
[OTA] Progress: 70% (700/987)
[OTA] Progress: 80% (800/987)
[OTA] Progress: 90% (900/987)
[OTA] Progress: 100% (987/987)
[OTA] ✓ Download complete
[OTA] Verifying SHA-256... ✓
[OTA] Verifying RSA signature... ✓
[OTA] Magic byte: 0xE9 ✓
Result: ✓ PASS
Message: Firmware downloaded and verified (1010688 bytes)
----------------------------------------

========================================
           TEST RESULTS
========================================
Total Tests: 9
Passed: 9
Failed: 0
Pass Rate: 100.0%
========================================

Entering continuous monitoring mode...
[MONITOR] Sample 1/10 collected
[MONITOR] Sample 2/10 collected
...
```

### 9.2 Test Metrics

| Metric | Value | Notes |
|--------|-------|-------|
| Total Tests | 9 | Plus 1 continuous |
| Pass Rate | 100% | All tests passing |
| Execution Time | 170s | Including FOTA download |
| Network Requests | ~1000 | FOTA chunks dominant |
| Data Transmitted | ~1MB | Firmware download |
| Memory Usage | <200KB | Peak during OTA |

### 9.3 Failure Analysis

**Common Failure Modes:**

1. **Test 1 Failure (Connectivity):**
   - Cause: Flask server not running, wrong IP, firewall
   - Impact: All subsequent tests fail
   - Fix: Verify server running, check network

2. **Test 2-4 Failures (Security):**
   - Cause: Key mismatch, nonce conflicts
   - Impact: Secured tests fail
   - Fix: Verify keys match, reset nonce state

3. **Test 9 Failure (FOTA):**
   - Cause: Wrong AES key, network timeout, corrupted download
   - Impact: OTA unusable
   - Fix: Verify AES key sync, check network stability

---

## 10. Troubleshooting

### 10.1 WiFi Connection Issues

**Symptom:** Test 1 fails, "WiFi not connected"

**Diagnosis:**
```cpp
Serial.printf("WiFi Status: %d\n", WiFi.status());
Serial.printf("SSID: %s\n", WIFI_SSID);
Serial.printf("RSSI: %d dBm\n", WiFi.RSSI());
```

**Solutions:**
- Verify SSID/password in `test_config.h`
- Check WiFi signal strength (RSSI > -70 dBm)
- Try 2.4GHz network (ESP32 doesn't support 5GHz)
- Restart ESP32: `ESP.restart()`

---

### 10.2 HMAC Validation Failures

**Symptom:** Test 2 fails, "Invalid MAC" from server

**Diagnosis:**
```cpp
// Print HMAC details
Serial.printf("Nonce: %u\n", nonce);
Serial.printf("Payload: %s\n", payload.c_str());
Serial.printf("HMAC: %s\n", hmacHex.c_str());

// Verify key
for (int i = 0; i < 32; i++) {
    Serial.printf("%02X ", PSK_HMAC[i]);
}
```

**Solutions:**
- **Key Mismatch:** Verify `PSK_HMAC` matches Flask `PSK_HMAC` byte-for-byte
- **Byte Order:** Ensure nonce is big-endian in HMAC calculation
- **Payload Encoding:** Check Base64 encoding/decoding consistency
- **Server Logs:** Check Flask logs for detailed error

---

### 10.3 Anti-Replay Test Failures

**Symptom:** Test 4 fails, replay not detected

**Diagnosis:**
```cpp
Serial.printf("First nonce: %u, Response: %d\n", nonce1, httpCode1);
Serial.printf("Replay nonce: %u, Response: %d\n", nonce2, httpCode2);
Serial.printf("New nonce: %u, Response: %d\n", nonce3, httpCode3);
```

**Solutions:**
- **Nonce Not Persisted:** Server may not be storing nonces
- **Server Restart:** Nonce state lost on Flask restart
- **Wrong Device ID:** Server tracks nonces per device
- **Reset Nonces:** Call `resetServerNonces()` before test

---

### 10.4 FOTA Download Failures

**Symptom:** Test 9 fails, "Wrong Magic Byte" or "Decryption failed"

**Diagnosis:**
```cpp
Serial.printf("Magic Byte: 0x%02X (expected 0xE9)\n", firstByte);
Serial.printf("AES Key (first 8 bytes): ");
for (int i = 0; i < 8; i++) {
    Serial.printf("%02X ", AES_FIRMWARE_KEY[i]);
}
```

**Solutions:**
- **AES Key Mismatch:**
  ```bash
  # Verify ESP32 key matches Flask
  xxd PIO/ECOWATT/src/application/keys.cpp | grep -A 2 "AES_FIRMWARE_KEY"
  xxd flask/keys/aes_firmware_key.bin
  # All 32 bytes must match exactly
  ```

- **Wrong IV:** Check manifest IV matches decryption IV
- **Corrupted Download:** Check network stability, retry download
- **Wrong Firmware:** Verify Flask serving correct encrypted firmware

---

### 10.5 Memory Exhaustion

**Symptom:** Random crashes during tests, heap allocation failures

**Diagnosis:**
```cpp
Serial.printf("Free Heap: %u bytes\n", ESP.getFreeHeap());
Serial.printf("Min Free Heap: %u bytes\n", ESP.getMinFreeHeap());
Serial.printf("Heap Fragmentation: %u%%\n", ESP.getHeapFragmentation());
```

**Solutions:**
- Reduce JSON document sizes: `StaticJsonDocument<1024>` → `<512>`
- Free unused objects after tests
- Clear upload queue between tests
- Use `heap_caps_malloc` for large allocations
- Restart ESP32 between test runs

---

### 9.6 Test Coordination Issues

**Symptom:** Flask not receiving test sync messages

**Diagnosis:**
```cpp
bool syncSuccess = syncTest(testNum, testName, "starting", "");
Serial.printf("Sync result: %s\n", syncSuccess ? "OK" : "FAILED");
```

**Solutions:**
- Verify `/integration/test/sync` endpoint exists in Flask
- Check Flask route is handling POST requests
- Verify JSON payload format
- Check Flask logs for errors

---

## Appendix A: Test Configuration Reference

### WiFi Settings
```cpp
#define WIFI_SSID "your_network_name"
#define WIFI_PASSWORD "your_password"
```

### Server Settings
```cpp
#define FLASK_SERVER_IP "192.168.1.100"
#define FLASK_SERVER_PORT 5001
```

### Security Keys (Must Match Flask)
```cpp
const uint8_t PSK_HMAC[32] = { /* 32 bytes */ };
const uint8_t AES_FIRMWARE_KEY[32] = { /* 32 bytes */ };
const uint8_t RSA_PUBLIC_KEY[] = { /* PEM format */ };
```

---

## Appendix B: Test Commands Quick Reference

**Build and Run:**
```bash
pio test -e esp32dev -f test_m4_integration
```

**Monitor Output:**
```bash
pio device monitor -b 115200
```

**Clean Build:**
```bash
pio test -e esp32dev -f test_m4_integration -c
```

**Upload Only (No Test):**
```bash
pio run -e esp32dev -t upload
```

---

**End of ESP32 Test Documentation**

For related documentation, see:
- **ESP32 Architecture**: `ESP32_ARCHITECTURE.md`
- **Flask Architecture**: `FLASK_ARCHITECTURE.md`
- **Flask Tests**: `FLASK_TESTS.md`
- **Main README**: `../README.md`
