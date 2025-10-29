/**
 * @file diagnostics.cpp
 * @brief Implementation of Diagnostics and Event Logging Module
 */

#include "application/diagnostics.h"
#include <ArduinoJson.h>
#include <time.h>
// Include print.h after ArduinoJson to avoid macro conflicts
#include "peripheral/print.h"

// Helper function to get current Unix timestamp in seconds
static unsigned long getCurrentTimestamp() {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        time_t now = mktime(&timeinfo);
        return (unsigned long)now;
    }
    return millis() / 1000;  // Fallback to seconds since boot
}

// Initialize static members
DiagnosticRingBuffer<50> Diagnostics::eventLog;
Preferences Diagnostics::prefs;

uint32_t Diagnostics::readErrors = 0;
uint32_t Diagnostics::writeErrors = 0;
uint32_t Diagnostics::timeouts = 0;
uint32_t Diagnostics::crcErrors = 0;
uint32_t Diagnostics::malformedFrames = 0;
uint32_t Diagnostics::compressionFailures = 0;
uint32_t Diagnostics::uploadFailures = 0;
uint32_t Diagnostics::securityViolations = 0;
uint32_t Diagnostics::startTime = 0;

void Diagnostics::init() {
    print("Diagnostics: Initializing...\n");
    
    // Load persistent counters from NVS
    loadCounters();
    
    // Record start time
    startTime = millis();
    
    // Log initialization event
    logEvent(EVENT_INFO, "Diagnostics system initialized", 0);
    
    print("Diagnostics: Initialized. Uptime: %u seconds\n", getUptime());
}

void Diagnostics::logEvent(EventType type, const char* message, uint16_t errorCode) {
    DiagnosticEvent event;
    event.timestamp = getCurrentTimestamp();
    event.type = type;
    event.errorCode = errorCode;
    strncpy(event.message, message, sizeof(event.message) - 1);
    event.message[sizeof(event.message) - 1] = '\0';
    
    eventLog.push(event);
    
    // Print event to serial for debugging
    const char* typeStr[] = {"INFO", "WARNING", "ERROR", "FAULT"};
    print("[%s] %s", typeStr[type], message);
    if (errorCode != 0) {
        print(" (code: %u)", errorCode);
    }
    print("\n");
}

void Diagnostics::incrementReadErrors() {
    readErrors++;
    saveCounters();
    logEvent(EVENT_ERROR, "Read error occurred", readErrors);
}

void Diagnostics::incrementWriteErrors() {
    writeErrors++;
    saveCounters();
    logEvent(EVENT_ERROR, "Write error occurred", writeErrors);
}

void Diagnostics::incrementTimeouts() {
    timeouts++;
    saveCounters();
    logEvent(EVENT_WARNING, "Timeout occurred", timeouts);
}

void Diagnostics::incrementCRCErrors() {
    crcErrors++;
    saveCounters();
    logEvent(EVENT_ERROR, "CRC validation failed", crcErrors);
}

void Diagnostics::incrementMalformedFrames() {
    malformedFrames++;
    saveCounters();
    logEvent(EVENT_ERROR, "Malformed frame detected", malformedFrames);
}

void Diagnostics::incrementCompressionFailures() {
    compressionFailures++;
    saveCounters();
    logEvent(EVENT_ERROR, "Compression failed", compressionFailures);
}

void Diagnostics::incrementUploadFailures() {
    uploadFailures++;
    saveCounters();
    logEvent(EVENT_ERROR, "Upload failed", uploadFailures);
}

void Diagnostics::incrementSecurityViolations() {
    securityViolations++;
    saveCounters();
    logEvent(EVENT_FAULT, "Security violation detected", securityViolations);
}

float Diagnostics::getReadSuccessRate() {
    uint32_t total = readErrors + 100; // Assume some successful reads
    if (total == 0) return 100.0f;
    return (1.0f - (float)readErrors / total) * 100.0f;
}

float Diagnostics::getWriteSuccessRate() {
    uint32_t total = writeErrors + 10; // Assume some successful writes
    if (total == 0) return 100.0f;
    return (1.0f - (float)writeErrors / total) * 100.0f;
}

float Diagnostics::getUploadSuccessRate() {
    uint32_t total = uploadFailures + 50; // Assume some successful uploads
    if (total == 0) return 100.0f;
    return (1.0f - (float)uploadFailures / total) * 100.0f;
}

String Diagnostics::generateDiagnosticsJSON() {
    StaticJsonDocument<2048> doc;
    
    doc["device_id"] = "ESP32_EcoWatt_Smart";
    doc["timestamp"] = getCurrentTimestamp();
    doc["uptime_seconds"] = getUptime();
    
    // Error counters
    JsonObject counters = doc.createNestedObject("error_counters");
    counters["read_errors"] = readErrors;
    counters["write_errors"] = writeErrors;
    counters["timeouts"] = timeouts;
    counters["crc_errors"] = crcErrors;
    counters["malformed_frames"] = malformedFrames;
    counters["compression_failures"] = compressionFailures;
    counters["upload_failures"] = uploadFailures;
    counters["security_violations"] = securityViolations;
    
    // Success rates
    JsonObject successRates = doc.createNestedObject("success_rates");
    successRates["read_success_pct"] = getReadSuccessRate();
    successRates["write_success_pct"] = getWriteSuccessRate();
    successRates["upload_success_pct"] = getUploadSuccessRate();
    
    // Recent events (last 10)
    JsonArray events = doc.createNestedArray("recent_events");
    size_t eventCount = getEventCount();
    size_t start = (eventCount > 10) ? (eventCount - 10) : 0;
    
    for (size_t i = start; i < eventCount; i++) {
        DiagnosticEvent evt = getEvent(i);
        JsonObject event = events.createNestedObject();
        event["timestamp"] = evt.timestamp;
        
        const char* typeStr[] = {"INFO", "WARNING", "ERROR", "FAULT"};
        event["type"] = typeStr[evt.type];
        event["message"] = evt.message;
        
        if (evt.errorCode != 0) {
            event["error_code"] = evt.errorCode;
        }
    }
    
    // System health
    JsonObject health = doc.createNestedObject("system_health");
    health["status"] = (securityViolations == 0 && 
                        readErrors < 10 && 
                        uploadFailures < 5) ? "HEALTHY" : "DEGRADED";
    health["free_heap"] = ESP.getFreeHeap();
    // Note: getHeapFragmentation() not available in all ESP32 Arduino versions
    // health["heap_fragmentation"] = ESP.getHeapFragmentation();
    
    String output;
    serializeJson(doc, output);
    return output;
}

void Diagnostics::resetCounters() {
    readErrors = 0;
    writeErrors = 0;
    timeouts = 0;
    crcErrors = 0;
    malformedFrames = 0;
    compressionFailures = 0;
    uploadFailures = 0;
    securityViolations = 0;
    
    saveCounters();
    logEvent(EVENT_INFO, "Counters reset", 0);
}

void Diagnostics::saveCounters() {
    prefs.begin("diagnostics", false);
    prefs.putUInt("read_err", readErrors);
    prefs.putUInt("write_err", writeErrors);
    prefs.putUInt("timeouts", timeouts);
    prefs.putUInt("crc_err", crcErrors);
    prefs.putUInt("malformed", malformedFrames);
    prefs.putUInt("comp_fail", compressionFailures);
    prefs.putUInt("upload_fail", uploadFailures);
    prefs.putUInt("sec_viol", securityViolations);
    prefs.end();
}

void Diagnostics::loadCounters() {
    prefs.begin("diagnostics", true);
    readErrors = prefs.getUInt("read_err", 0);
    writeErrors = prefs.getUInt("write_err", 0);
    timeouts = prefs.getUInt("timeouts", 0);
    crcErrors = prefs.getUInt("crc_err", 0);
    malformedFrames = prefs.getUInt("malformed", 0);
    compressionFailures = prefs.getUInt("comp_fail", 0);
    uploadFailures = prefs.getUInt("upload_fail", 0);
    securityViolations = prefs.getUInt("sec_viol", 0);
    prefs.end();
    
    print("Diagnostics: Loaded counters - Errors: R=%u W=%u T=%u CRC=%u\n",
          readErrors, writeErrors, timeouts, crcErrors);
}
