#include "application/fault_recovery.h"
#include "driver/debug.h"
#include <ArduinoJson.h>
#include <Preferences.h>

// Static member initialization
std::vector<FaultEvent> FaultRecovery::faultLog;
uint32_t FaultRecovery::totalFaults = 0;
uint32_t FaultRecovery::recoveredFaults = 0;

void FaultRecovery::init() {
    faultLog.clear();
    faultLog.reserve(MAX_LOG_SIZE);
    
    debug.log("Fault Recovery System initialized\n");
    debug.log("Max log size: %d events\n", MAX_LOG_SIZE);
    debug.log("Retry limits: Timeout=%d, CRC=%d, Exception=%d\n", 
          MAX_TIMEOUT_RETRIES, MAX_CRC_RETRIES, MAX_EXCEPTION_RETRIES);
}

void FaultRecovery::logFault(FaultType type, const char* description, 
                             uint8_t exceptionCode, uint8_t slaveAddr,
                             uint8_t funcCode, uint16_t regAddr) {
    FaultEvent event;
    event.timestamp = millis();
    event.type = type;
    event.exceptionCode = exceptionCode;
    strncpy(event.description, description, sizeof(event.description) - 1);
    event.description[sizeof(event.description) - 1] = '\0';
    event.slaveAddress = slaveAddr;
    event.functionCode = funcCode;
    event.registerAddress = regAddr;
    event.recovered = false;
    event.retryCount = 0;
    
    addToLog(event);
    totalFaults++;
    
    // Print fault notification
    debug.log("\n⚠️ FAULT DETECTED ⚠️\n");
    debug.log("Type: %s\n", getFaultTypeDescription(type));
    debug.log("Time: %lu ms\n", event.timestamp);
    debug.log("Description: %s\n", description);
    
    if (exceptionCode != 0) {
        debug.log("Exception Code: 0x%02X (%s)\n", exceptionCode, getExceptionDescription(exceptionCode));
    }
    
    if (slaveAddr != 0) {
        debug.log("Slave: 0x%02X, Function: 0x%02X, Register: 0x%04X\n", 
              slaveAddr, funcCode, regAddr);
    }
    debug.log("\n");
}

void FaultRecovery::markRecovered() {
    if (!faultLog.empty()) {
        faultLog.back().recovered = true;
        recoveredFaults++;
        debug.log("✓ Last fault marked as RECOVERED\n");
    }
}

void FaultRecovery::addToLog(const FaultEvent& event) {
    // Circular buffer: remove oldest if at capacity
    if (faultLog.size() >= MAX_LOG_SIZE) {
        faultLog.erase(faultLog.begin());
    }
    
    faultLog.push_back(event);
    
    // Persist critical faults
    if (event.type == FAULT_MODBUS_EXCEPTION || 
        event.type == FAULT_BUFFER_OVERFLOW) {
        persistCriticalFault(event);
    }
}

std::vector<FaultEvent> FaultRecovery::getFaultLog() {
    return faultLog;
}

bool FaultRecovery::getFaultLogJSON(char* output, size_t maxSize) {
    DynamicJsonDocument doc(8192);
    
    doc["device_id"] = "ESP32_EcoWatt_Smart";
    doc["timestamp"] = millis();
    doc["total_faults"] = totalFaults;
    doc["recovered_faults"] = recoveredFaults;
    doc["unresolved_faults"] = totalFaults - recoveredFaults;
    doc["log_size"] = faultLog.size();
    
    JsonArray events = doc["fault_events"].to<JsonArray>();
    
    for (const auto& event : faultLog) {
        JsonObject ev = events.createNestedObject();
        ev["timestamp"] = event.timestamp;
        ev["type"] = getFaultTypeDescription(event.type);
        ev["type_code"] = (int)event.type;
        ev["description"] = event.description;
        ev["recovered"] = event.recovered;
        ev["retry_count"] = event.retryCount;
        
        if (event.exceptionCode != 0) {
            ev["exception_code"] = event.exceptionCode;
            ev["exception_desc"] = getExceptionDescription(event.exceptionCode);
        }
        
        if (event.slaveAddress != 0) {
            ev["slave_address"] = event.slaveAddress;
            ev["function_code"] = event.functionCode;
            ev["register_address"] = event.registerAddress;
        }
    }
    
    size_t len = serializeJson(doc, output, maxSize);
    return (len > 0 && len < maxSize);
}

void FaultRecovery::printFaultLog() {
    debug.log("\n╔═══════════════════════════════════════════════════════╗\n");
    debug.log("║              FAULT RECOVERY LOG                       ║\n");
    debug.log("╠═══════════════════════════════════════════════════════╣\n");
    debug.log("║ Total Faults: %4u                                    ║\n", totalFaults);
    debug.log("║ Recovered:    %4u                                    ║\n", recoveredFaults);
    debug.log("║ Unresolved:   %4u                                    ║\n", totalFaults - recoveredFaults);
    debug.log("║ Log Entries:  %4u / %3u                              ║\n", faultLog.size(), MAX_LOG_SIZE);
    debug.log("╠═══════════════════════════════════════════════════════╣\n");
    
    if (faultLog.empty()) {
        debug.log("║ No fault events recorded                              ║\n");
        debug.log("╚═══════════════════════════════════════════════════════╝\n\n");
        return;
    }
    
    for (size_t i = 0; i < faultLog.size(); i++) {
        const auto& event = faultLog[i];
        debug.log("║ [%3d] %-50s ║\n", i + 1, "");
        debug.log("║   Time:   %10lu ms                              ║\n", event.timestamp);
        debug.log("║   Type:   %-40s ║\n", getFaultTypeDescription(event.type));
        debug.log("║   Desc:   %-40s ║\n", event.description);
        debug.log("║   Status: %-40s ║\n", event.recovered ? "✓ RECOVERED" : "✗ UNRESOLVED");
        
        if (event.exceptionCode != 0) {
            debug.log("║   Exception: 0x%02X (%s)%*s ║\n", 
                  event.exceptionCode, getExceptionDescription(event.exceptionCode), 
                  28 - strlen(getExceptionDescription(event.exceptionCode)), "");
        }
        
        if (event.retryCount > 0) {
            debug.log("║   Retries: %d%*s ║\n", event.retryCount, 45, "");
        }
        
        if (i < faultLog.size() - 1) {
            debug.log("╟───────────────────────────────────────────────────────╢\n");
        }
    }
    
    debug.log("╚═══════════════════════════════════════════════════════╝\n\n");
}

void FaultRecovery::clearFaultLog() {
    faultLog.clear();
    totalFaults = 0;
    recoveredFaults = 0;
    debug.log("Fault log cleared\n");
}

void FaultRecovery::getFaultStatistics(uint32_t* totalFaults_, uint32_t* recoveredFaults_,
                                      uint32_t* unresolvedFaults) {
    if (totalFaults_) *totalFaults_ = totalFaults;
    if (recoveredFaults_) *recoveredFaults_ = recoveredFaults;
    if (unresolvedFaults) *unresolvedFaults = totalFaults - recoveredFaults;
}

bool FaultRecovery::isModbusException(const uint8_t* response, size_t length, uint8_t* exceptionCode) {
    // Minimum Modbus exception frame: Slave(1) + FuncCode+0x80(1) + ExceptionCode(1) + CRC(2) = 5 bytes
    if (length < 5) {
        return false;
    }
    
    // Check if function code has error bit set (0x80)
    if ((response[1] & 0x80) != 0x80) {
        return false;
    }
    
    // Validate CRC
    if (!validateCRC(response, length)) {
        return false;
    }
    
    // Extract exception code
    if (exceptionCode) {
        *exceptionCode = response[2];
    }
    
    return true;
}

bool FaultRecovery::validateCRC(const uint8_t* frame, size_t length) {
    if (length < 4) {  // Minimum: Slave + Function + CRC(2)
        return false;
    }
    
    // Calculate CRC on all bytes except the last 2 (which contain the CRC)
    uint16_t calculatedCRC = calculateCRC16(frame, length - 2);
    
    // Extract received CRC (little-endian)
    uint16_t receivedCRC = frame[length - 2] | (frame[length - 1] << 8);
    
    return (calculatedCRC == receivedCRC);
}

uint16_t FaultRecovery::calculateCRC16(const uint8_t* data, size_t length) {
    uint16_t crc = 0xFFFF;
    
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc >>= 1;
                crc ^= 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    
    return crc;
}

bool FaultRecovery::isMalformedFrame(const uint8_t* frame, size_t length) {
    // Check minimum length
    if (length < 4) {
        return true;
    }
    
    // Check slave address (should be 1-247, 0 is broadcast)
    if (frame[0] > 247) {
        return true;
    }
    
    // Check function code (valid range: 1-127, >128 is exception)
    uint8_t funcCode = frame[1] & 0x7F;  // Remove error bit
    if (funcCode == 0 || funcCode > 127) {
        return true;
    }
    
    return false;
}

const char* FaultRecovery::getExceptionDescription(uint8_t code) {
    switch (code) {
        case MODBUS_EXCEPTION_ILLEGAL_FUNCTION:
            return "Illegal Function";
        case MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS:
            return "Illegal Data Address";
        case MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE:
            return "Illegal Data Value";
        case MODBUS_EXCEPTION_SLAVE_DEVICE_FAILURE:
            return "Slave Device Failure";
        case MODBUS_EXCEPTION_ACKNOWLEDGE:
            return "Acknowledge";
        case MODBUS_EXCEPTION_SLAVE_DEVICE_BUSY:
            return "Slave Device Busy";
        case MODBUS_EXCEPTION_MEMORY_PARITY_ERROR:
            return "Memory Parity Error";
        case MODBUS_EXCEPTION_GATEWAY_PATH_UNAVAILABLE:
            return "Gateway Path Unavailable";
        case MODBUS_EXCEPTION_GATEWAY_TARGET_FAILED:
            return "Gateway Target Failed";
        default:
            return "Unknown Exception";
    }
}

const char* FaultRecovery::getFaultTypeDescription(FaultType type) {
    switch (type) {
        case FAULT_NONE:
            return "No Fault";
        case FAULT_TIMEOUT:
            return "Timeout";
        case FAULT_CRC_ERROR:
            return "CRC Error";
        case FAULT_CORRUPT_RESPONSE:
            return "Corrupt Response";
        case FAULT_PACKET_DROP:
            return "Packet Drop";
        case FAULT_DELAY:
            return "Excessive Delay";
        case FAULT_MODBUS_EXCEPTION:
            return "Modbus Exception";
        case FAULT_MALFORMED_FRAME:
            return "Malformed Frame";
        case FAULT_BUFFER_OVERFLOW:
            return "Buffer Overflow";
        default:
            return "Unknown Fault";
    }
}

bool FaultRecovery::handleTimeout(uint16_t regAddr, uint8_t retryCount) {
    char desc[128];
    snprintf(desc, sizeof(desc), "Read timeout for register 0x%04X (retry %d/%d)", 
             regAddr, retryCount, MAX_TIMEOUT_RETRIES);
    
    logFault(FAULT_TIMEOUT, desc, 0, 0x01, 0x03, regAddr);
    
    if (!faultLog.empty()) {
        faultLog.back().retryCount = retryCount;
    }
    
    if (retryCount >= MAX_TIMEOUT_RETRIES) {
        debug.log("❌ Timeout retry limit exceeded for register 0x%04X\n", regAddr);
        return false;
    }
    
    // Apply exponential backoff delay
    unsigned long delayMs = getRetryDelay(retryCount);
    debug.log("⏳ Retry in %lu ms...\n", delayMs);
    delay(delayMs);
    
    return true;  // Should retry
}

bool FaultRecovery::handleCRCError(const uint8_t* frame, size_t length, uint8_t retryCount) {
    char desc[128];
    snprintf(desc, sizeof(desc), "CRC validation failed (frame length: %d, retry %d/%d)", 
             length, retryCount, MAX_CRC_RETRIES);
    
    logFault(FAULT_CRC_ERROR, desc);
    
    if (!faultLog.empty()) {
        faultLog.back().retryCount = retryCount;
    }
    
    // Print frame data for debugging
    if (length > 0 && length < 32) {
        debug.log("Frame data: ");
        for (size_t i = 0; i < length; i++) {
            debug.log("%02X ", frame[i]);
        }
        debug.log("\n");
    }
    
    if (retryCount >= MAX_CRC_RETRIES) {
        debug.log("❌ CRC error retry limit exceeded\n");
        return false;
    }
    
    unsigned long delayMs = getRetryDelay(retryCount);
    debug.log("⏳ Retry in %lu ms...\n", delayMs);
    delay(delayMs);
    
    return true;  // Should retry
}

void FaultRecovery::handlePacketDrop(size_t expectedBytes, size_t receivedBytes) {
    char desc[128];
    snprintf(desc, sizeof(desc), "Packet drop: expected %d bytes, received %d bytes", 
             expectedBytes, receivedBytes);
    
    logFault(FAULT_PACKET_DROP, desc);
}

void FaultRecovery::handleDelay(unsigned long expectedTime, unsigned long actualTime) {
    char desc[128];
    snprintf(desc, sizeof(desc), "Excessive delay: expected %lu ms, actual %lu ms (%.1fx slower)", 
             expectedTime, actualTime, (float)actualTime / expectedTime);
    
    logFault(FAULT_DELAY, desc);
}

bool FaultRecovery::handleModbusException(uint8_t exceptionCode, uint8_t slaveAddr,
                                         uint8_t funcCode, uint16_t regAddr) {
    char desc[128];
    snprintf(desc, sizeof(desc), "Modbus exception from slave 0x%02X", slaveAddr);
    
    logFault(FAULT_MODBUS_EXCEPTION, desc, exceptionCode, slaveAddr, funcCode, regAddr);
    
    // Determine if exception is recoverable
    switch (exceptionCode) {
        case MODBUS_EXCEPTION_ILLEGAL_FUNCTION:
        case MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS:
        case MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE:
            // These are configuration errors - not recoverable by retry
            debug.log("❌ Non-recoverable exception: %s\n", getExceptionDescription(exceptionCode));
            return false;
            
        case MODBUS_EXCEPTION_SLAVE_DEVICE_BUSY:
        case MODBUS_EXCEPTION_ACKNOWLEDGE:
            // Device is temporarily busy - can retry
            debug.log("⚠️ Recoverable exception: %s - will retry\n", getExceptionDescription(exceptionCode));
            delay(500);  // Wait for device to become ready
            return true;
            
        case MODBUS_EXCEPTION_SLAVE_DEVICE_FAILURE:
        case MODBUS_EXCEPTION_MEMORY_PARITY_ERROR:
        case MODBUS_EXCEPTION_GATEWAY_PATH_UNAVAILABLE:
        case MODBUS_EXCEPTION_GATEWAY_TARGET_FAILED:
            // Hardware/communication issues - limited retries
            debug.log("⚠️ Hardware exception: %s - limited retries\n", getExceptionDescription(exceptionCode));
            return true;
            
        default:
            debug.log("❓ Unknown exception code: 0x%02X\n", exceptionCode);
            return false;
    }
}

uint8_t FaultRecovery::getMaxRetries(FaultType type) {
    switch (type) {
        case FAULT_TIMEOUT:
            return MAX_TIMEOUT_RETRIES;
        case FAULT_CRC_ERROR:
            return MAX_CRC_RETRIES;
        case FAULT_MODBUS_EXCEPTION:
            return MAX_EXCEPTION_RETRIES;
        default:
            return 1;
    }
}

unsigned long FaultRecovery::getRetryDelay(uint8_t retryCount) {
    // Exponential backoff: 100ms, 200ms, 400ms, 800ms, etc.
    unsigned long delay = BASE_RETRY_DELAY * (1 << retryCount);
    
    // Cap at 2 seconds
    if (delay > 2000) {
        delay = 2000;
    }
    
    return delay;
}

void FaultRecovery::persistCriticalFault(const FaultEvent& event) {
    // Store critical fault info in NVS for post-reboot analysis
    Preferences prefs;
    prefs.begin("faults", false);
    
    // Store last critical fault
    prefs.putUInt("last_fault_time", event.timestamp);
    prefs.putUChar("last_fault_type", (uint8_t)event.type);
    prefs.putUChar("last_exc_code", event.exceptionCode);
    
    prefs.end();
    
    debug.log("💾 Critical fault persisted to NVS\n");
}
