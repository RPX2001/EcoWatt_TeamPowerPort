/**
 * @file fault_recovery.cpp
 * @brief Fault Detection and Recovery Implementation (Milestone 5)
 */

#include "application/fault_recovery.h"
#include "application/data_uploader.h"
#include "application/credentials.h"  // For FLASK_SERVER_URL
#include <HTTPClient.h>
#include <WiFi.h>
#include <cstring>
#include <cctype>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <time.h>

// ============================================
// Helper Functions
// ============================================

// Get current Unix timestamp in seconds (same as data_uploader)
static unsigned long getCurrentTimestamp() {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        // Convert tm struct to Unix timestamp in SECONDS
        time_t now = mktime(&timeinfo);
        return (unsigned long)now;
    }
    // Fallback: Use time(nullptr) which works if NTP is synced
    time_t now = time(nullptr);
    if (now > 1000000000) { // Sanity check (after year 2001)
        return (unsigned long)now;
    }
    // Last resort: millis/1000 (better than nothing)
    return millis() / 1000;
}

// ============================================
// Global State
// ============================================

static bool s_initialized = false;
static char s_deviceId[32] = "ESP32_EcoWatt_Smart";

// ============================================
// Helper to get current device ID
// ============================================

static const char* getCurrentDeviceId() {
    if (s_initialized && strlen(s_deviceId) > 0) {
        return s_deviceId;
    }
    // Fallback to DataUploader if not initialized yet
    const char* id = DataUploader::getDeviceID();
    return id ? id : "ESP32_EcoWatt_Smart";
}

// ============================================
// Initialization
// ============================================

void initFaultRecovery() {
    if (s_initialized) return;
    
    // Get device ID from DataUploader (uses system config)
    const char* configDeviceId = DataUploader::getDeviceID();
    if (configDeviceId && strlen(configDeviceId) > 0) {
        strncpy(s_deviceId, configDeviceId, sizeof(s_deviceId) - 1);
        s_deviceId[sizeof(s_deviceId) - 1] = '\0';
    }
    
    debug.log("[FaultRecovery] Initialized with device_id: %s\n", s_deviceId);
    s_initialized = true;
}

// ============================================
// Fault Detection Functions
// ============================================

/**
 * @brief Calculate Modbus CRC16 checksum
 */
static uint16_t calculateCRC(const uint8_t* data, int length) {
    uint16_t crc = 0xFFFF;
    for (int i = 0; i < length; i++) {
        crc ^= (data[i] & 0xFF);
        for (int j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc >>= 1;
                crc ^= 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc & 0xFFFF;
}

/**
 * @brief Convert hex string to binary
 */
static bool hexToBinary(const char* hexStr, uint8_t* binary, size_t binarySize, size_t& outLen) {
    size_t hexLen = strlen(hexStr);
    if (hexLen % 2 != 0) return false; // Must be even length
    
    outLen = hexLen / 2;
    if (outLen > binarySize) return false;
    
    for (size_t i = 0; i < outLen; i++) {
        char byteStr[3] = {hexStr[i*2], hexStr[i*2+1], '\0'};
        binary[i] = (uint8_t)strtol(byteStr, NULL, 16);
    }
    
    return true;
}

bool validateModbusCRC(const char* frameHex) {
    if (!frameHex || strlen(frameHex) < 8) {
        return false; // Too short to have CRC
    }
    
    uint8_t binary[256];
    size_t binaryLen;
    
    if (!hexToBinary(frameHex, binary, sizeof(binary), binaryLen)) {
        return false; // Invalid hex format
    }
    
    if (binaryLen < 4) {
        return false; // Need at least address + function + CRC
    }
    
    // Extract received CRC (last 2 bytes, little-endian)
    uint16_t receivedCRC = binary[binaryLen - 2] | (binary[binaryLen - 1] << 8);
    
    // Calculate CRC on everything except last 2 bytes
    uint16_t calculatedCRC = calculateCRC(binary, binaryLen - 2);
    
    bool valid = (receivedCRC == calculatedCRC);
    
    if (!valid) {
        debug.log("[FaultRecovery] CRC ERROR! Received: 0x%04X, Calculated: 0x%04X\n", 
                  receivedCRC, calculatedCRC);
    }
    
    return valid;
}

bool validatePayloadLength(const char* frameHex, uint8_t expectedByteCount) {
    if (!frameHex) return false;
    
    size_t hexLen = strlen(frameHex);
    
    // Modbus read response format: [Slave(1)] [Function(1)] [ByteCount(1)] [Data(N)] [CRC(2)]
    // In hex: 2 chars per byte
    // Minimum: 2 + 2 + 2 + (expectedByteCount*2) + 4 = 10 + (expectedByteCount*2)
    size_t expectedHexLen = 10 + (expectedByteCount * 2);
    
    if (hexLen < expectedHexLen) {
        debug.log("[FaultRecovery] TRUNCATED! Expected %zu hex chars, got %zu\n", 
                  expectedHexLen, hexLen);
        return false;
    }
    
    // Also check the byte count field in the response
    if (hexLen >= 6) {
        char byteCountHex[3] = {frameHex[4], frameHex[5], '\0'};
        uint8_t reportedByteCount = (uint8_t)strtol(byteCountHex, NULL, 16);
        
        if (reportedByteCount != expectedByteCount) {
            debug.log("[FaultRecovery] BYTE COUNT MISMATCH! Expected %u, got %u\n", 
                      expectedByteCount, reportedByteCount);
            return false;
        }
    }
    
    return true;
}

bool checkForGarbage(const char* frameHex) {
    if (!frameHex) return false;
    
    size_t len = strlen(frameHex);
    
    // Check if all characters are valid hex (0-9, A-F, a-f)
    for (size_t i = 0; i < len; i++) {
        char c = frameHex[i];
        if (!isxdigit(c)) {
            debug.log("[FaultRecovery] GARBAGE DETECTED! Invalid char '%c' at position %zu\n", 
                      c, i);
            return false; // Garbage found
        }
    }
    
    // Additional check: response should start with slave address (0x11 = "11" in hex)
    if (len >= 2) {
        if (frameHex[0] != '1' || frameHex[1] != '1') {
            debug.log("[FaultRecovery] GARBAGE! Invalid slave address: %c%c\n", 
                      frameHex[0], frameHex[1]);
            return false;
        }
    }
    
    return true; // No garbage detected
}

bool checkBufferOverflow(const char* frameHex, size_t bufferSize) {
    if (!frameHex) return true; // NULL is safe
    
    size_t frameLen = strlen(frameHex);
    
    if (frameLen >= bufferSize) {
        debug.log("[FaultRecovery] BUFFER OVERFLOW RISK! Frame %zu bytes, buffer %zu bytes\n", 
                  frameLen, bufferSize);
        return false;
    }
    
    return true;
}

FaultType detectFault(const char* frameHex, uint8_t expectedByteCount, size_t bufferSize) {
    if (!frameHex) {
        return FaultType::TIMEOUT; // NULL response = timeout
    }
    
    // Check in order of severity
    
    // 1. Buffer overflow (most critical)
    if (!checkBufferOverflow(frameHex, bufferSize)) {
        return FaultType::BUFFER_OVERFLOW;
    }
    
    // 2. Garbage data (corrupt format)
    if (!checkForGarbage(frameHex)) {
        return FaultType::GARBAGE_DATA;
    }
    
    // 3. Truncated payload
    if (!validatePayloadLength(frameHex, expectedByteCount)) {
        return FaultType::TRUNCATED_PAYLOAD;
    }
    
    // 4. CRC error
    if (!validateModbusCRC(frameHex)) {
        return FaultType::CRC_ERROR;
    }
    
    // Check for Modbus exception response
    if (strlen(frameHex) >= 4) {
        char funcHex[3] = {frameHex[2], frameHex[3], '\0'};
        uint8_t func = (uint8_t)strtol(funcHex, NULL, 16);
        if (func >= 0x80) { // Exception bit set
            return FaultType::MODBUS_EXCEPTION;
        }
    }
    
    return FaultType::NONE; // No fault detected
}

// ============================================
// Recovery Execution
// ============================================

bool executeRecovery(FaultType fault, std::function<bool()> retryFunction, uint8_t& retryCount) {
    retryCount = 0;
    
    if (fault == FaultType::NONE) {
        return true; // No recovery needed
    }
    
    debug.log("[FaultRecovery] Executing recovery for fault: %s\n", getFaultTypeName(fault));
    
    // Retry with exponential backoff
    uint32_t delayMs = INITIAL_RETRY_DELAY_MS;
    
    for (uint8_t i = 0; i < MAX_RECOVERY_RETRIES; i++) {
        retryCount = i + 1;
        
        debug.log("[FaultRecovery] Retry attempt %u/%u after %lu ms\n", 
                  retryCount, MAX_RECOVERY_RETRIES, delayMs);
        
        // Use vTaskDelay instead of delay() to be FreeRTOS-aware
        // This allows other tasks to run during the retry backoff
        vTaskDelay(pdMS_TO_TICKS(delayMs));
        
        if (retryFunction()) {
            debug.log("[FaultRecovery] ✅ Recovery successful after %u retries\n", retryCount);
            return true;
        }
        
        // Exponential backoff (double delay, max 4s)
        delayMs = min(delayMs * 2, (uint32_t)MAX_RETRY_DELAY_MS);
    }
    
    debug.log("[FaultRecovery] ❌ Recovery FAILED after %u retries\n", retryCount);
    return false;
}

// ============================================
// Event Reporting
// ============================================

bool sendRecoveryEvent(const FaultRecoveryEvent& event) {
    if (WiFi.status() != WL_CONNECTED) {
        debug.log("[FaultRecovery] WiFi not connected, cannot send event\n");
        return false;
    }
    
    // Build JSON payload
    StaticJsonDocument<512> doc;
    doc["device_id"] = event.device_id;
    doc["timestamp"] = event.timestamp;
    doc["fault_type"] = getFaultTypeName(event.fault_type);
    doc["recovery_action"] = getRecoveryActionName(event.recovery_action);
    doc["success"] = event.success;
    doc["details"] = event.details;
    doc["retry_count"] = event.retry_count;
    
    String jsonStr;
    serializeJson(doc, jsonStr);
    
    // Use Flask server URL from credentials.h
    char url[256];
    snprintf(url, sizeof(url), "%s/fault/recovery", FLASK_SERVER_URL);
    
    HTTPClient http;
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    
    debug.log("[FaultRecovery] Sending recovery event to %s\n", url);
    debug.log("[FaultRecovery] Payload: %s\n", jsonStr.c_str());
    
    int httpCode = http.POST(jsonStr);
    
    if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED) {
        debug.log("[FaultRecovery] ✅ Event sent successfully (HTTP %d)\n", httpCode);
        http.end();
        return true;
    } else {
        debug.log("[FaultRecovery] ❌ Failed to send event (HTTP %d): %s\n", 
                  httpCode, http.getString().c_str());
        http.end();
        return false;
    }
}

// ============================================
// Helper Functions
// ============================================

const char* getFaultTypeName(FaultType fault) {
    switch (fault) {
        case FaultType::NONE: return "none";
        case FaultType::CRC_ERROR: return "crc_error";
        case FaultType::TRUNCATED_PAYLOAD: return "truncated_payload";
        case FaultType::GARBAGE_DATA: return "garbage_data";
        case FaultType::BUFFER_OVERFLOW: return "buffer_overflow";
        case FaultType::TIMEOUT: return "timeout";
        case FaultType::MODBUS_EXCEPTION: return "modbus_exception";
        default: return "unknown";
    }
}

const char* getRecoveryActionName(RecoveryAction action) {
    switch (action) {
        case RecoveryAction::RETRY_READ: return "retry_read";
        case RecoveryAction::RESET_CONNECTION: return "reset_connection";
        case RecoveryAction::SKIP_SAMPLE: return "skip_sample";
        case RecoveryAction::REBOOT_DEVICE: return "reboot_device";
        default: return "unknown";
    }
}
