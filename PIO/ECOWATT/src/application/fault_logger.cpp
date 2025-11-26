/**
 * @file fault_logger.cpp
 * @brief Implementation of fault event logging system
 * 
 * @version 1.0.0
 * @date 2025-10-19
 */

#include "application/fault_logger.h"
#include "peripheral/formatted_print.h"
#include <time.h>
#include <sys/time.h>

// Static member initialization
std::vector<FaultEvent> FaultLogger::fault_log;

/**
 * @brief Initialize fault logger
 */
void FaultLogger::init() {
    PRINT_SECTION("FAULT LOGGER INITIALIZATION");
    
    fault_log.clear();
    fault_log.reserve(MAX_LOG_ENTRIES);
    
    // TODO: Load previous events from NVS if needed
    // loadFromNVS();
    
    PRINT_INFO("Fault logger initialized");
    Serial.printf("  Max log entries: %u\n", MAX_LOG_ENTRIES);
    PRINT_SUCCESS("Fault logger ready");
}

/**
 * @brief Log a fault event
 */
void FaultLogger::logFault(
    FaultType type,
    const String& description,
    const String& module,
    bool recovered,
    const String& recovery_action,
    uint8_t exception_code,
    uint8_t retry_count
) {
    FaultEvent event;
    event.timestamp = millis();
    event.type = type;
    event.event_description = description;
    event.module = module;
    event.recovered = recovered;
    event.recovery_action = recovery_action;
    event.exception_code = exception_code;
    event.retry_count = retry_count;
    
    // Add to log (maintain circular buffer)
    if (fault_log.size() >= MAX_LOG_ENTRIES) {
        fault_log.erase(fault_log.begin());  // Remove oldest entry
    }
    fault_log.push_back(event);
    
    // Print to serial for immediate visibility
    Serial.printf("  [ERROR] FAULT: %s\n", description.c_str());
    if (exception_code > 0) {
        Serial.printf("  Exception Code: 0x%02X (%s)\n", exception_code, exceptionCodeToString(exception_code));
    }
    Serial.printf("  Module: %s\n", module.c_str());
    Serial.printf("  Recovered: %s\n", recovered ? "YES" : "NO");
    if (!recovery_action.isEmpty()) {
        Serial.printf("  Recovery: %s\n", recovery_action.c_str());
    }
    if (retry_count > 0) {
        Serial.printf("  Retries: %u\n", retry_count);
    }
    
    // TODO: Save to NVS for persistence
    // saveToNVS();
}

/**
 * @brief Convert FaultType to string
 */
const char* FaultLogger::faultTypeToString(FaultType type) {
    switch (type) {
        case FaultType::MODBUS_EXCEPTION:  return "Modbus Exception";
        case FaultType::MODBUS_TIMEOUT:    return "Modbus Timeout";
        case FaultType::CRC_ERROR:         return "CRC Error";
        case FaultType::CORRUPT_FRAME:     return "Corrupt Frame";
        case FaultType::BUFFER_OVERFLOW:   return "Buffer Overflow";
        case FaultType::HTTP_ERROR:        return "HTTP Error";
        default:                           return "Unknown";
    }
}

/**
 * @brief Convert Modbus exception code to description
 */
const char* FaultLogger::exceptionCodeToString(uint8_t code) {
    switch (code) {
        case 0x01: return "Illegal Function";
        case 0x02: return "Illegal Data Address";
        case 0x03: return "Illegal Data Value";
        case 0x04: return "Slave Device Failure";
        case 0x05: return "Acknowledge";
        case 0x06: return "Slave Device Busy";
        case 0x08: return "Memory Parity Error";
        case 0x0A: return "Gateway Path Unavailable";
        case 0x0B: return "Gateway Target Failed to Respond";
        default:   return "Unknown Exception";
    }
}

/**
 * @brief Get current ISO 8601 timestamp
 */
String FaultLogger::getISO8601Timestamp() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    
    struct tm timeinfo;
    gmtime_r(&tv.tv_sec, &timeinfo);
    
    char buffer[30];
    strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", &timeinfo);
    
    // Add milliseconds and Z suffix
    snprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer), ".%03ldZ", tv.tv_usec / 1000);
    
    return String(buffer);
}

/**
 * @brief Convert fault event to JSON object
 */
void FaultLogger::eventToJSON(const FaultEvent& event, JsonObject& obj) {
    obj["timestamp"] = getISO8601Timestamp();
    obj["event"] = event.event_description;
    obj["type"] = faultTypeToString(event.type);
    obj["module"] = event.module;
    obj["recovered"] = event.recovered;
    
    if (!event.recovery_action.isEmpty()) {
        obj["recovery_action"] = event.recovery_action;
    }
    
    if (event.exception_code > 0) {
        obj["exception_code"] = event.exception_code;
        obj["exception_desc"] = exceptionCodeToString(event.exception_code);
    }
    
    if (event.retry_count > 0) {
        obj["retry_count"] = event.retry_count;
    }
    
    obj["timestamp_ms"] = event.timestamp;
}

/**
 * @brief Get all fault events as JSON array
 */
String FaultLogger::getAllEventsJSON() {
    DynamicJsonDocument doc(8192);  // 8KB buffer
    JsonArray events = doc.createNestedArray("events");
    
    for (const auto& event : fault_log) {
        JsonObject obj = events.createNestedObject();
        eventToJSON(event, obj);
    }
    
    doc["total_count"] = fault_log.size();
    doc["recovery_rate"] = getRecoveryRate();
    
    String output;
    serializeJsonPretty(doc, output);
    return output;
}

/**
 * @brief Get recent fault events
 */
String FaultLogger::getRecentEventsJSON(size_t count) {
    DynamicJsonDocument doc(4096);  // 4KB buffer
    JsonArray events = doc.createNestedArray("events");
    
    size_t start = (fault_log.size() > count) ? (fault_log.size() - count) : 0;
    
    for (size_t i = start; i < fault_log.size(); i++) {
        JsonObject obj = events.createNestedObject();
        eventToJSON(fault_log[i], obj);
    }
    
    doc["showing"] = events.size();
    doc["total_count"] = fault_log.size();
    
    String output;
    serializeJsonPretty(doc, output);
    return output;
}

/**
 * @brief Print all fault events to serial
 */
void FaultLogger::printAllEvents() {
    PRINT_SECTION("FAULT EVENT LOG");
    
    if (fault_log.empty()) {
        PRINT_INFO("No fault events logged");
        return;
    }
    
    Serial.printf("  Total Events: %u\n", fault_log.size());
    Serial.printf("  Recovery Rate: %.1f%%\n\n", getRecoveryRate());
    
    for (size_t i = 0; i < fault_log.size(); i++) {
        const auto& event = fault_log[i];
        
        Serial.println("  ┌────────────────────────────────────────");
        Serial.printf("  │ Event #%u\n", i + 1);
        Serial.println("  ├────────────────────────────────────────");
        Serial.printf("  │ Time:     %lu ms\n", event.timestamp);
        Serial.printf("  │ Type:     %s\n", faultTypeToString(event.type));
        Serial.printf("  │ Event:    %s\n", event.event_description.c_str());
        Serial.printf("  │ Module:   %s\n", event.module.c_str());
        Serial.printf("  │ Recovered: %s\n", event.recovered ? "YES" : "NO");
        
        if (!event.recovery_action.isEmpty()) {
            Serial.printf("  │ Recovery: %s\n", event.recovery_action.c_str());
        }
        
        if (event.exception_code > 0) {
            Serial.printf("  │ Exception: 0x%02X (%s)\n", 
                         event.exception_code, 
                         exceptionCodeToString(event.exception_code));
        }
        
        if (event.retry_count > 0) {
            Serial.printf("  │ Retries:  %u\n", event.retry_count);
        }
        
        Serial.println("  └────────────────────────────────────────");
    }
}

/**
 * @brief Print fault statistics
 */
void FaultLogger::printStatistics() {
    PRINT_SECTION("FAULT STATISTICS");
    
    if (fault_log.empty()) {
        PRINT_INFO("No fault events logged");
        return;
    }
    
    // Count by type
    size_t count_exception = 0;
    size_t count_timeout = 0;
    size_t count_crc = 0;
    size_t count_corrupt = 0;
    size_t count_overflow = 0;
    size_t count_http = 0;
    size_t count_recovered = 0;
    
    for (const auto& event : fault_log) {
        switch (event.type) {
            case FaultType::MODBUS_EXCEPTION: count_exception++; break;
            case FaultType::MODBUS_TIMEOUT:   count_timeout++; break;
            case FaultType::CRC_ERROR:        count_crc++; break;
            case FaultType::CORRUPT_FRAME:    count_corrupt++; break;
            case FaultType::BUFFER_OVERFLOW:  count_overflow++; break;
            case FaultType::HTTP_ERROR:       count_http++; break;
            default: break;
        }
        if (event.recovered) count_recovered++;
    }
    
    Serial.printf("  Total Faults:       %u\n", fault_log.size());
    Serial.printf("  Recovered:          %u (%.1f%%)\n", count_recovered, getRecoveryRate());
    Serial.printf("  Failed:             %u\n\n", fault_log.size() - count_recovered);
    
    Serial.println("  Fault Breakdown:");
    if (count_exception > 0) Serial.printf("    Modbus Exception: %u\n", count_exception);
    if (count_timeout > 0)   Serial.printf("    Timeout:          %u\n", count_timeout);
    if (count_crc > 0)       Serial.printf("    CRC Error:        %u\n", count_crc);
    if (count_corrupt > 0)   Serial.printf("    Corrupt Frame:    %u\n", count_corrupt);
    if (count_overflow > 0)  Serial.printf("    Buffer Overflow:  %u\n", count_overflow);
    if (count_http > 0)      Serial.printf("    HTTP Error:       %u\n", count_http);
}

/**
 * @brief Clear all fault events
 */
void FaultLogger::clearAllEvents() {
    fault_log.clear();
    PRINT_SUCCESS("Fault log cleared");
}

/**
 * @brief Get total fault count
 */
size_t FaultLogger::getTotalFaultCount() {
    return fault_log.size();
}

/**
 * @brief Get recovery success rate
 */
float FaultLogger::getRecoveryRate() {
    if (fault_log.empty()) return 100.0f;
    
    size_t recovered = 0;
    for (const auto& event : fault_log) {
        if (event.recovered) recovered++;
    }
    
    return (float)recovered / (float)fault_log.size() * 100.0f;
}

/**
 * @brief Load fault log from NVS (placeholder)
 */
void FaultLogger::loadFromNVS() {
    // TODO: Implement NVS loading
    // For now, start with empty log
}

/**
 * @brief Save fault log to NVS (placeholder)
 */
void FaultLogger::saveToNVS() {
    // TODO: Implement NVS saving
    // For now, keep in RAM only
}
