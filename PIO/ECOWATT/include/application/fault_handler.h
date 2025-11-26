/**
 * @file fault_handler.h
 * @brief Fault detection and recovery for Modbus communication
 * 
 * Handles:
 * - Modbus exceptions (codes 0x01-0x0B)
 * - CRC validation errors
 * - Corrupt/malformed frames
 * - Timeout conditions
 * - Buffer overflow
 * 
 * @version 1.0.0
 * @date 2025-10-19
 */

#ifndef FAULT_HANDLER_H
#define FAULT_HANDLER_H

#include <Arduino.h>
#include <HTTPClient.h>
#include "application/fault_logger.h"

/**
 * @brief Modbus response validation result
 */
enum class ValidationResult {
    VALID,              ///< Frame is valid
    EXCEPTION,          ///< Modbus exception received
    CRC_ERROR,          ///< CRC check failed
    CORRUPT_FRAME,      ///< Frame is malformed
    TIMEOUT,            ///< No response received
    BUFFER_OVERFLOW,    ///< Buffer too small
    HTTP_ERROR          ///< HTTP communication error
};

/**
 * @brief Frame validation details
 */
struct FrameValidation {
    ValidationResult result;
    uint8_t exception_code;    ///< If result == EXCEPTION
    String error_description;
    bool recovered;
    String recovery_action;
};

/**
 * @brief Fault detection and recovery handler
 */
class FaultHandler {
public:
    /**
     * @brief Initialize fault handler
     */
    static void init();
    
    /**
     * @brief Validate Modbus response frame
     * @param frame Raw response bytes
     * @param frame_length Length of response
     * @param expected_slave Expected slave address
     * @param expected_function Expected function code
     * @param expected_length Expected frame length
     * @return Validation result with details
     */
    static FrameValidation validateModbusFrame(
        const uint8_t* frame,
        size_t frame_length,
        uint8_t expected_slave = 0x11,
        uint8_t expected_function = 0x04,
        size_t expected_length = 0
    );
    
    /**
     * @brief Check if frame is Modbus exception
     * @param frame Raw response bytes
     * @param frame_length Length of response
     * @return True if exception frame detected
     */
    static bool isModbusException(const uint8_t* frame, size_t frame_length);
    
    /**
     * @brief Extract exception code from exception frame
     * @param frame Raw exception frame
     * @return Exception code (0x01-0x0B)
     */
    static uint8_t getExceptionCode(const uint8_t* frame);
    
    /**
     * @brief Validate CRC checksum
     * @param frame Frame to validate
     * @param frame_length Length of frame
     * @return True if CRC is valid
     */
    static bool validateCRC(const uint8_t* frame, size_t frame_length);
    
    /**
     * @brief Calculate CRC-16 (Modbus)
     * @param data Data buffer
     * @param length Data length
     * @return CRC value
     */
    static uint16_t calculateCRC(const uint8_t* data, size_t length);
    
    /**
     * @brief Detect frame corruption
     * @param frame Frame to check
     * @param frame_length Length of frame
     * @return True if frame appears corrupted
     */
    static bool isFrameCorrupt(const uint8_t* frame, size_t frame_length);
    
    /**
     * @brief Handle HTTP error response
     * @param http_code HTTP status code
     * @param module Module name for logging
     * @return True if error was handled
     */
    static bool handleHTTPError(int http_code, const String& module = "HTTP");
    
    /**
     * @brief Handle timeout condition
     * @param module Module name for logging
     * @param timeout_ms Timeout duration in ms
     * @return True if timeout was handled
     */
    static bool handleTimeout(const String& module, unsigned long timeout_ms);
    
    /**
     * @brief Execute recovery strategy for fault
     * @param validation Validation result
     * @param retry_count Current retry attempt
     * @param module Module name for logging
     * @return True if recovery successful
     */
    static bool recoverFromFault(
        const FrameValidation& validation,
        uint8_t retry_count,
        const String& module
    );
    
    /**
     * @brief Get recommended retry delay for fault type
     * @param validation Validation result
     * @param retry_count Current retry attempt
     * @return Delay in milliseconds
     */
    static unsigned long getRetryDelay(
        const FrameValidation& validation,
        uint8_t retry_count
    );
    
    /**
     * @brief Check if fault is recoverable
     * @param validation Validation result
     * @return True if retry should be attempted
     */
    static bool isRecoverable(const FrameValidation& validation);
    
    /**
     * @brief Print frame for debugging
     * @param frame Frame to print
     * @param frame_length Length of frame
     * @param label Optional label
     */
    static void printFrame(
        const uint8_t* frame,
        size_t frame_length,
        const String& label = "Frame"
    );

private:
    static const uint8_t MAX_RETRIES = 3;           ///< Maximum retry attempts
    static const unsigned long BASE_RETRY_DELAY = 500;  ///< Base retry delay (ms)
    
    /**
     * @brief Get recovery strategy description
     * @param validation Validation result
     * @return Strategy description
     */
    static String getRecoveryStrategy(const FrameValidation& validation);
};

/**
 * @brief Convenience macros for fault handling
 */
#define VALIDATE_FRAME(frame, len, slave, func, expected_len) \
    FaultHandler::validateModbusFrame(frame, len, slave, func, expected_len)

#define CHECK_HTTP_ERROR(code, module) \
    FaultHandler::handleHTTPError(code, module)

#define LOG_TIMEOUT(module, timeout_ms) \
    FaultHandler::handleTimeout(module, timeout_ms)

#endif // FAULT_HANDLER_H
