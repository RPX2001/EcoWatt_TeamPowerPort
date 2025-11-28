/**
 * @file diagnostics.h
 * @brief Diagnostics and Event Logging Module for EcoWatt Device
 * 
 * Provides local event logging with ring buffer, persistent error counters,
 * and diagnostics API endpoint for remote monitoring.
 */

#ifndef DIAGNOSTICS_H
#define DIAGNOSTICS_H

#include <Arduino.h>
#include <Preferences.h>

// Event types for diagnostic logging
enum EventType {
    EVENT_INFO = 0,
    EVENT_WARNING = 1,
    EVENT_ERROR = 2,
    EVENT_FAULT = 3
};

// Diagnostic event structure
struct DiagnosticEvent {
    uint32_t timestamp;       // millis() when event occurred
    EventType type;           // Event severity level
    char message[64];         // Event description
    uint16_t errorCode;       // Specific error code (0 if none)
    
    DiagnosticEvent() : timestamp(0), type(EVENT_INFO), errorCode(0) {
        message[0] = '\0';
    }
};

// Ring buffer for diagnostic events
template<size_t N>
class DiagnosticRingBuffer {
private:
    DiagnosticEvent buffer[N];
    size_t head;
    size_t tail;
    bool full;
    size_t count;

public:
    DiagnosticRingBuffer() : head(0), tail(0), full(false), count(0) {}
    
    void push(const DiagnosticEvent& event) {
        buffer[head] = event;
        head = (head + 1) % N;
        if (full) {
            tail = (tail + 1) % N;
        } else {
            count++;
        }
        if (head == tail) {
            full = true;
        }
    }
    
    size_t size() const {
        return full ? N : count;
    }
    
    bool empty() const {
        return (!full && head == tail);
    }
    
    DiagnosticEvent get(size_t index) const {
        if (index >= size()) {
            return DiagnosticEvent();
        }
        size_t actualIndex = (tail + index) % N;
        return buffer[actualIndex];
    }
    
    void clear() {
        head = tail = count = 0;
        full = false;
    }
};

// Main diagnostics class
class Diagnostics {
private:
    static DiagnosticRingBuffer<50> eventLog;
    static Preferences prefs;
    
    // Persistent counters (stored in NVS)
    static uint32_t readErrors;
    static uint32_t writeErrors;
    static uint32_t timeouts;
    static uint32_t crcErrors;
    static uint32_t malformedFrames;
    static uint32_t compressionFailures;
    static uint32_t uploadFailures;
    static uint32_t securityViolations;
    static uint32_t startTime;

public:
    // Initialize diagnostics system
    static void init();
    
    // Log an event
    static void logEvent(EventType type, const char* message, uint16_t errorCode = 0);
    
    // Increment error counters
    static void incrementReadErrors();
    static void incrementWriteErrors();
    static void incrementTimeouts();
    static void incrementCRCErrors();
    static void incrementMalformedFrames();
    static void incrementCompressionFailures();
    static void incrementUploadFailures();
    static void incrementSecurityViolations();
    
    // Get error counts
    static uint32_t getReadErrors() { return readErrors; }
    static uint32_t getWriteErrors() { return writeErrors; }
    static uint32_t getTimeouts() { return timeouts; }
    static uint32_t getCRCErrors() { return crcErrors; }
    static uint32_t getMalformedFrames() { return malformedFrames; }
    static uint32_t getCompressionFailures() { return compressionFailures; }
    static uint32_t getUploadFailures() { return uploadFailures; }
    static uint32_t getSecurityViolations() { return securityViolations; }
    static uint32_t getUptime() { return (millis() - startTime) / 1000; }
    
    // Get recent events
    static size_t getEventCount() { return eventLog.size(); }
    static DiagnosticEvent getEvent(size_t index) { return eventLog.get(index); }
    
    // Get success rates
    static float getReadSuccessRate();
    static float getWriteSuccessRate();
    static float getUploadSuccessRate();
    
    // Generate diagnostics JSON string
    static String generateDiagnosticsJSON();
    
    // Reset counters
    static void resetCounters();
    
    // Save counters to NVS
    static void saveCounters();
    
    // Load counters from NVS
    static void loadCounters();
};

#endif // DIAGNOSTICS_H
