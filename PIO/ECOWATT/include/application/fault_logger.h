/**
 * @file fault_logger.h
 * @brief Fault event logging system for ESP32 EcoWatt device
 * 
 * Logs fault events (Modbus errors, timeouts, CRC errors, etc.) to NVS storage
 * with JSON formatting for easy analysis and remote querying.
 * 
 * @version 1.0.0
 * @date 2025-10-19
 */

#ifndef FAULT_LOGGER_H
#define FAULT_LOGGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>

/**
 * @brief Fault event types
 */
enum class FaultType {
    MODBUS_EXCEPTION,      // Modbus exception response
    MODBUS_TIMEOUT,        // No response within timeout
    CRC_ERROR,             // CRC checksum mismatch
    CORRUPT_FRAME,         // Malformed frame structure
    BUFFER_OVERFLOW,       // Data buffer overflow
    HTTP_ERROR,            // HTTP communication error
    UNKNOWN                // Unknown error type
};

/**
 * @brief Fault event structure
 */
struct FaultEvent {
    unsigned long timestamp;        // Unix timestamp
    FaultType type;                // Type of fault
    String event_description;      // Human-readable description
    String module;                 // Module where fault occurred
    bool recovered;                // Whether recovery was successful
    String recovery_action;        // Action taken to recover
    uint8_t exception_code;        // Modbus exception code (if applicable)
    uint8_t retry_count;           // Number of retry attempts
};

/**
 * @brief Fault Logger class
 * 
 * Manages fault event logging with NVS persistence and JSON formatting
 */
class FaultLogger {
public:
    /**
     * @brief Initialize fault logger
     */
    static void init();
    
    /**
     * @brief Log a fault event
     * 
     * @param type Fault type
     * @param description Event description
     * @param module Module name where fault occurred
     * @param recovered Whether fault was recovered
     * @param recovery_action Recovery action taken
     * @param exception_code Modbus exception code (0 if not applicable)
     * @param retry_count Number of retry attempts
     */
    static void logFault(
        FaultType type,
        const String& description,
        const String& module = "unknown",
        bool recovered = false,
        const String& recovery_action = "",
        uint8_t exception_code = 0,
        uint8_t retry_count = 0
    );
    
    /**
     * @brief Get all fault events as JSON array
     * 
     * @return JSON string containing all logged events
     */
    static String getAllEventsJSON();
    
    /**
     * @brief Get recent fault events (last N events)
     * 
     * @param count Number of recent events to retrieve
     * @return JSON string containing recent events
     */
    static String getRecentEventsJSON(size_t count = 10);
    
    /**
     * @brief Print all fault events to serial
     */
    static void printAllEvents();
    
    /**
     * @brief Print fault statistics
     */
    static void printStatistics();
    
    /**
     * @brief Clear all fault events
     */
    static void clearAllEvents();
    
    /**
     * @brief Get total fault count
     */
    static size_t getTotalFaultCount();
    
    /**
     * @brief Get recovery success rate
     */
    static float getRecoveryRate();
    
    /**
     * @brief Convert Modbus exception code to description (public for fault_handler)
     */
    static const char* exceptionCodeToString(uint8_t code);
    
private:
    static std::vector<FaultEvent> fault_log;
    static const size_t MAX_LOG_ENTRIES = 50;  // Maximum events to keep in memory
    
    /**
     * @brief Convert FaultType to string
     */
    static const char* faultTypeToString(FaultType type);
    
    /**
     * @brief Convert fault event to JSON object
     */
    static void eventToJSON(const FaultEvent& event, JsonObject& obj);
    
    /**
     * @brief Load fault log from NVS
     */
    static void loadFromNVS();
    
    /**
     * @brief Save fault log to NVS
     */
    static void saveToNVS();
    
    /**
     * @brief Get current ISO 8601 timestamp
     */
    static String getISO8601Timestamp();
};

// Convenience macros for logging
#define LOG_FAULT(type, desc) FaultLogger::logFault(type, desc, __FUNCTION__)
#define LOG_FAULT_RECOVERED(type, desc, action) FaultLogger::logFault(type, desc, __FUNCTION__, true, action)
#define LOG_MODBUS_EXCEPTION(code, recovered) FaultLogger::logFault(FaultType::MODBUS_EXCEPTION, "Modbus Exception " + String(code), __FUNCTION__, recovered, "", code)

#endif // FAULT_LOGGER_H
