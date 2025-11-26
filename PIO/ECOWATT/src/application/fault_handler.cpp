/**
 * @file fault_handler.cpp
 * @brief Implementation of fault detection and recovery
 * 
 * @version 1.0.0
 * @date 2025-10-19
 */

#include "application/fault_handler.h"
#include "peripheral/formatted_print.h"

/**
 * @brief Initialize fault handler
 */
void FaultHandler::init() {
    PRINT_SECTION("FAULT HANDLER INITIALIZATION");
    Serial.printf("  Max retries: %u\n", MAX_RETRIES);
    Serial.printf("  Base retry delay: %lu ms\n", BASE_RETRY_DELAY);
    PRINT_SUCCESS("Fault handler ready");
}

/**
 * @brief Validate complete Modbus response frame
 */
FrameValidation FaultHandler::validateModbusFrame(
    const uint8_t* frame,
    size_t frame_length,
    uint8_t expected_slave,
    uint8_t expected_function,
    size_t expected_length
) {
    FrameValidation result;
    result.result = ValidationResult::VALID;
    result.exception_code = 0;
    result.recovered = false;
    
    // Check for null pointer
    if (!frame) {
        result.result = ValidationResult::CORRUPT_FRAME;
        result.error_description = "Null frame pointer";
        return result;
    }
    
    // Check minimum frame length (slave + function + CRC = 4 bytes minimum)
    if (frame_length < 4) {
        result.result = ValidationResult::CORRUPT_FRAME;
        result.error_description = "Frame too short (< 4 bytes)";
        return result;
    }
    
    // Check if buffer overflow would occur
    if (expected_length > 0 && frame_length < expected_length) {
        result.result = ValidationResult::BUFFER_OVERFLOW;
        result.error_description = String("Expected ") + String(expected_length) + 
                                  " bytes, got " + String(frame_length);
        return result;
    }
    
    // Check slave address
    if (expected_slave > 0 && frame[0] != expected_slave) {
        result.result = ValidationResult::CORRUPT_FRAME;
        result.error_description = String("Wrong slave address: 0x") + 
                                  String(frame[0], HEX) + " (expected 0x" + 
                                  String(expected_slave, HEX) + ")";
        return result;
    }
    
    // Check for Modbus exception (function code has bit 7 set)
    if (isModbusException(frame, frame_length)) {
        result.result = ValidationResult::EXCEPTION;
        result.exception_code = getExceptionCode(frame);
        result.error_description = String("Modbus exception 0x") + 
                                  String(result.exception_code, HEX) + ": " +
                                  FaultLogger::exceptionCodeToString(result.exception_code);
        return result;
    }
    
    // Validate function code
    if (expected_function > 0 && frame[1] != expected_function) {
        result.result = ValidationResult::CORRUPT_FRAME;
        result.error_description = String("Wrong function code: 0x") + 
                                  String(frame[1], HEX) + " (expected 0x" + 
                                  String(expected_function, HEX) + ")";
        return result;
    }
    
    // Validate CRC
    if (!validateCRC(frame, frame_length)) {
        result.result = ValidationResult::CRC_ERROR;
        result.error_description = "CRC validation failed";
        return result;
    }
    
    // Check for frame corruption indicators
    if (isFrameCorrupt(frame, frame_length)) {
        result.result = ValidationResult::CORRUPT_FRAME;
        result.error_description = "Frame corruption detected";
        return result;
    }
    
    result.error_description = "Frame valid";
    return result;
}

/**
 * @brief Check if frame is Modbus exception
 */
bool FaultHandler::isModbusException(const uint8_t* frame, size_t frame_length) {
    if (!frame || frame_length < 3) return false;
    
    // Exception frame: function code has bit 7 set (0x80)
    return (frame[1] & 0x80) != 0;
}

/**
 * @brief Extract exception code from exception frame
 */
uint8_t FaultHandler::getExceptionCode(const uint8_t* frame) {
    if (!frame) return 0;
    
    // Exception code is in byte 2
    // Frame format: [Slave][Function+0x80][Exception Code][CRC-L][CRC-H]
    return frame[2];
}

/**
 * @brief Calculate CRC-16 (Modbus)
 */
uint16_t FaultHandler::calculateCRC(const uint8_t* data, size_t length) {
    uint16_t crc = 0xFFFF;
    
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    
    return crc;
}

/**
 * @brief Validate CRC checksum
 */
bool FaultHandler::validateCRC(const uint8_t* frame, size_t frame_length) {
    if (!frame || frame_length < 4) return false;
    
    // Calculate CRC for all bytes except last 2 (which contain the CRC)
    uint16_t calculated_crc = calculateCRC(frame, frame_length - 2);
    
    // Extract CRC from frame (little-endian)
    uint16_t frame_crc = frame[frame_length - 2] | (frame[frame_length - 1] << 8);
    
    return calculated_crc == frame_crc;
}

/**
 * @brief Detect frame corruption
 */
bool FaultHandler::isFrameCorrupt(const uint8_t* frame, size_t frame_length) {
    if (!frame || frame_length < 4) return true;
    
    // Check for all zeros (common corruption pattern)
    bool all_zeros = true;
    for (size_t i = 0; i < frame_length; i++) {
        if (frame[i] != 0) {
            all_zeros = false;
            break;
        }
    }
    if (all_zeros) return true;
    
    // Check for all 0xFF (common corruption pattern)
    bool all_ff = true;
    for (size_t i = 0; i < frame_length; i++) {
        if (frame[i] != 0xFF) {
            all_ff = false;
            break;
        }
    }
    if (all_ff) return true;
    
    // For Read Multiple Registers (0x04), check byte count
    if (frame[1] == 0x04 && frame_length >= 3) {
        uint8_t byte_count = frame[2];
        size_t expected_length = 3 + byte_count + 2;  // slave + func + count + data + CRC
        
        if (frame_length != expected_length) {
            return true;  // Length mismatch indicates corruption
        }
    }
    
    return false;
}

/**
 * @brief Handle HTTP error response
 */
bool FaultHandler::handleHTTPError(int http_code, const String& module) {
    String description = String("HTTP error ") + String(http_code);
    
    switch (http_code) {
        case -1:
            description = "HTTP connection failed";
            break;
        case 400:
            description = "Bad Request (400)";
            break;
        case 401:
            description = "Unauthorized (401)";
            break;
        case 404:
            description = "Not Found (404)";
            break;
        case 500:
            description = "Internal Server Error (500)";
            break;
        case 503:
            description = "Service Unavailable (503)";
            break;
    }
    
    bool recoverable = (http_code == 503 || http_code == -1);
    String recovery = recoverable ? "Retry connection" : "None - permanent error";
    
    FaultLogger::logFault(
        FaultType::HTTP_ERROR,
        description,
        module,
        false,  // Not recovered yet
        recovery,
        0,
        0
    );
    
    return recoverable;
}

/**
 * @brief Handle timeout condition
 */
bool FaultHandler::handleTimeout(const String& module, unsigned long timeout_ms) {
    String description = String("Timeout after ") + String(timeout_ms) + " ms";
    
    FaultLogger::logFault(
        FaultType::MODBUS_TIMEOUT,
        description,
        module,
        false,
        "Retry request",
        0,
        0
    );
    
    return true;  // Timeouts are always recoverable
}

/**
 * @brief Execute recovery strategy for fault
 */
bool FaultHandler::recoverFromFault(
    const FrameValidation& validation,
    uint8_t retry_count,
    const String& module
) {
    // Determine if we should retry
    if (!isRecoverable(validation)) {
        Serial.printf("  [ERROR] Fault not recoverable: %s\n", validation.error_description.c_str());
        return false;
    }
    
    if (retry_count >= MAX_RETRIES) {
        PRINT_ERROR("Max retries exceeded");
        return false;
    }
    
    // Get recovery strategy
    String strategy = getRecoveryStrategy(validation);
    
    // Log recovery attempt
    FaultType fault_type;
    switch (validation.result) {
        case ValidationResult::EXCEPTION:
            fault_type = FaultType::MODBUS_EXCEPTION;
            break;
        case ValidationResult::CRC_ERROR:
            fault_type = FaultType::CRC_ERROR;
            break;
        case ValidationResult::CORRUPT_FRAME:
            fault_type = FaultType::CORRUPT_FRAME;
            break;
        case ValidationResult::TIMEOUT:
            fault_type = FaultType::MODBUS_TIMEOUT;
            break;
        case ValidationResult::BUFFER_OVERFLOW:
            fault_type = FaultType::BUFFER_OVERFLOW;
            break;
        case ValidationResult::HTTP_ERROR:
            fault_type = FaultType::HTTP_ERROR;
            break;
        default:
            fault_type = FaultType::UNKNOWN;
    }
    
    FaultLogger::logFault(
        fault_type,
        validation.error_description,
        module,
        false,  // Will update if recovery succeeds
        strategy,
        validation.exception_code,
        retry_count
    );
    
    // Execute recovery delay
    unsigned long delay_ms = getRetryDelay(validation, retry_count);
    Serial.printf("  [INFO] Recovery delay: %lu ms\n", delay_ms);
    delay(delay_ms);
    
    return true;
}

/**
 * @brief Get recommended retry delay for fault type
 */
unsigned long FaultHandler::getRetryDelay(
    const FrameValidation& validation,
    uint8_t retry_count
) {
    // Exponential backoff based on retry count
    unsigned long delay = BASE_RETRY_DELAY * (1 << retry_count);
    
    // Adjust based on fault type
    switch (validation.result) {
        case ValidationResult::EXCEPTION:
            // Slave device busy or acknowledge - shorter delay
            if (validation.exception_code == 0x05 || validation.exception_code == 0x06) {
                delay = BASE_RETRY_DELAY * 2;
            }
            break;
            
        case ValidationResult::TIMEOUT:
            // Longer delay for timeouts
            delay *= 2;
            break;
            
        case ValidationResult::CRC_ERROR:
        case ValidationResult::CORRUPT_FRAME:
            // Immediate retry for corruption (transient error)
            delay = BASE_RETRY_DELAY;
            break;
            
        default:
            break;
    }
    
    // Cap maximum delay at 10 seconds
    if (delay > 10000) delay = 10000;
    
    return delay;
}

/**
 * @brief Check if fault is recoverable
 */
bool FaultHandler::isRecoverable(const FrameValidation& validation) {
    switch (validation.result) {
        case ValidationResult::VALID:
            return false;  // No recovery needed
            
        case ValidationResult::EXCEPTION:
            // Some exceptions are not recoverable
            switch (validation.exception_code) {
                case 0x01:  // Illegal Function
                case 0x02:  // Illegal Data Address
                case 0x03:  // Illegal Data Value
                    return false;  // Configuration error
                default:
                    return true;   // Transient errors
            }
            
        case ValidationResult::CRC_ERROR:
        case ValidationResult::CORRUPT_FRAME:
        case ValidationResult::TIMEOUT:
        case ValidationResult::HTTP_ERROR:
            return true;  // All potentially transient
            
        case ValidationResult::BUFFER_OVERFLOW:
            return false;  // Configuration error
            
        default:
            return false;
    }
}

/**
 * @brief Get recovery strategy description
 */
String FaultHandler::getRecoveryStrategy(const FrameValidation& validation) {
    switch (validation.result) {
        case ValidationResult::EXCEPTION:
            if (validation.exception_code == 0x06) {
                return "Wait for slave to become ready";
            }
            return "Retry request";
            
        case ValidationResult::CRC_ERROR:
        case ValidationResult::CORRUPT_FRAME:
            return "Retry request (transient error)";
            
        case ValidationResult::TIMEOUT:
            return "Retry with exponential backoff";
            
        case ValidationResult::HTTP_ERROR:
            return "Retry HTTP connection";
            
        case ValidationResult::BUFFER_OVERFLOW:
            return "None - buffer too small";
            
        default:
            return "Unknown";
    }
}

/**
 * @brief Print frame for debugging
 */
void FaultHandler::printFrame(
    const uint8_t* frame,
    size_t frame_length,
    const String& label
) {
    // Use printf to avoid macro conflicts with Serial.print()
    Serial.printf("  %s: [%u bytes] ", label.c_str(), frame_length);
    
    for (size_t i = 0; i < frame_length; i++) {
        if (i > 0) Serial.printf(" ");
        Serial.printf("%02X", frame[i]);
    }
    Serial.printf("\n");
}
