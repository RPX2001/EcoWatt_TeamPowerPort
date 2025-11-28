/**
 * @file fault_recovery.h
 * @brief Fault Detection and Recovery Module
 * 
 * Detects and recovers from Modbus communication faults:
 * - CRC errors
 * - Truncated payloads
 * - Garbage data
 * - Buffer overflows
 * 
 * Sends recovery events to Flask server as JSON.
 */

#ifndef FAULT_RECOVERY_H
#define FAULT_RECOVERY_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "driver/debug.h"
#include "application/data_uploader.h"
#include "application/credentials.h"

// ============================================
// Constants
// ============================================

#define MAX_RECOVERY_RETRIES 3
#define INITIAL_RETRY_DELAY_MS 1000
#define MAX_RETRY_DELAY_MS 4000

// ============================================
// Fault Types
// ============================================

enum class FaultType {
    NONE = 0,
    CRC_ERROR,          // Malformed CRC frames
    TRUNCATED_PAYLOAD,  // Incomplete response
    GARBAGE_DATA,       // Random byte corruption
    BUFFER_OVERFLOW,    // Response too large
    TIMEOUT,            // No response
    MODBUS_EXCEPTION    // Inverter exception code
};

// ============================================
// Recovery Actions
// ============================================

enum class RecoveryAction {
    RETRY_READ,         // Retry the Modbus read
    RESET_CONNECTION,   // Reset TCP connection
    SKIP_SAMPLE,        // Skip this sample, continue
    REBOOT_DEVICE       // Last resort
};

// ============================================
// Recovery Event Structure
// ============================================

struct FaultRecoveryEvent {
    char device_id[32];
    uint64_t timestamp;
    FaultType fault_type;
    RecoveryAction recovery_action;
    bool success;
    char details[128];
    uint8_t retry_count;
};

// ============================================
// Module Functions
// ============================================

/**
 * @brief Initialize fault recovery module
 */
void initFaultRecovery();

/**
 * @brief Validate Modbus CRC checksum
 * 
 * @param frameHex Response frame in HEX format
 * @return true if CRC valid, false if CRC error
 */
bool validateModbusCRC(const char* frameHex);

/**
 * @brief Validate payload length
 * 
 * @param frameHex Response frame in HEX format
 * @param expectedByteCount Expected number of data bytes
 * @return true if length correct, false if truncated
 */
bool validatePayloadLength(const char* frameHex, uint8_t expectedByteCount);

/**
 * @brief Check for garbage data in response
 * 
 * @param frameHex Response frame in HEX format
 * @return true if valid hex format, false if garbage detected
 */
bool checkForGarbage(const char* frameHex);

/**
 * @brief Check for buffer overflow risk
 * 
 * @param frameHex Response frame
 * @param bufferSize Size of receiving buffer
 * @return true if safe, false if overflow risk
 */
bool checkBufferOverflow(const char* frameHex, size_t bufferSize);

/**
 * @brief Detect fault type from Modbus response
 * 
 * @param frameHex Response frame in HEX format
 * @param expectedByteCount Expected data length
 * @param bufferSize Buffer size used
 * @return Detected FaultType
 */
FaultType detectFault(const char* frameHex, uint8_t expectedByteCount, size_t bufferSize);

/**
 * @brief Execute recovery action with retries
 * 
 * @param fault Detected fault type
 * @param retryFunction Function to retry (e.g., readRequest)
 * @param retryCount Output: number of retries performed
 * @return true if recovery successful, false if failed
 */
bool executeRecovery(FaultType fault, std::function<bool()> retryFunction, uint8_t& retryCount);

/**
 * @brief Send recovery event to Flask server
 * 
 * @param event FaultRecoveryEvent to send
 * @return true if sent successfully, false if failed
 */
bool sendRecoveryEvent(const FaultRecoveryEvent& event);

/**
 * @brief Get string representation of fault type
 * 
 * @param fault Fault type enum
 * @return String name of fault (e.g., "crc_error")
 */
const char* getFaultTypeName(FaultType fault);

/**
 * @brief Get string representation of recovery action
 * 
 * @param action Recovery action enum
 * @return String name of action (e.g., "retry_read")
 */
const char* getRecoveryActionName(RecoveryAction action);

#endif // FAULT_RECOVERY_H
