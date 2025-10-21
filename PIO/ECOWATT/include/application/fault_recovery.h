#ifndef FAULT_RECOVERY_H
#define FAULT_RECOVERY_H

#include <Arduino.h>
#include <vector>
#include <Preferences.h>

/**
 * @brief Modbus Exception Codes
 */
enum ModbusException {
    MODBUS_EXCEPTION_ILLEGAL_FUNCTION = 0x01,
    MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS = 0x02,
    MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE = 0x03,
    MODBUS_EXCEPTION_SLAVE_DEVICE_FAILURE = 0x04,
    MODBUS_EXCEPTION_ACKNOWLEDGE = 0x05,
    MODBUS_EXCEPTION_SLAVE_DEVICE_BUSY = 0x06,
    MODBUS_EXCEPTION_MEMORY_PARITY_ERROR = 0x08,
    MODBUS_EXCEPTION_GATEWAY_PATH_UNAVAILABLE = 0x0A,
    MODBUS_EXCEPTION_GATEWAY_TARGET_FAILED = 0x0B
};

/**
 * @brief Fault Types
 */
enum FaultType {
    FAULT_NONE = 0,
    FAULT_TIMEOUT,
    FAULT_CRC_ERROR,
    FAULT_CORRUPT_RESPONSE,
    FAULT_PACKET_DROP,
    FAULT_DELAY,
    FAULT_MODBUS_EXCEPTION,
    FAULT_MALFORMED_FRAME,
    FAULT_BUFFER_OVERFLOW
};

/**
 * @brief Fault Event Structure
 */
struct FaultEvent {
    unsigned long timestamp;          // Milliseconds since boot
    FaultType type;                   // Type of fault
    uint8_t exceptionCode;            // Modbus exception code (if applicable)
    char description[128];            // Human-readable description
    uint8_t slaveAddress;             // Modbus slave address
    uint8_t functionCode;             // Modbus function code
    uint16_t registerAddress;         // Register address (if applicable)
    bool recovered;                   // Whether fault was recovered
    uint8_t retryCount;               // Number of retries attempted
};

/**
 * @brief Fault Recovery Manager
 * 
 * Handles all types of inverter communication faults:
 * - Timeouts
 * - CRC errors
 * - Corrupt responses
 * - Packet drops
 * - Delays
 * - Modbus exceptions
 * - Malformed frames
 * - Buffer overflows
 */
class FaultRecovery {
public:
    /**
     * @brief Initialize fault recovery system
     */
    static void init();
    
    /**
     * @brief Log a fault event
     * 
     * @param type Fault type
     * @param description Human-readable description
     * @param exceptionCode Modbus exception code (0 if not applicable)
     * @param slaveAddr Slave address
     * @param funcCode Function code
     * @param regAddr Register address
     */
    static void logFault(FaultType type, const char* description, 
                        uint8_t exceptionCode = 0, uint8_t slaveAddr = 0x01,
                        uint8_t funcCode = 0, uint16_t regAddr = 0);
    
    /**
     * @brief Mark the last fault as recovered
     */
    static void markRecovered();
    
    /**
     * @brief Get all fault events
     * 
     * @return Vector of fault events
     */
    static std::vector<FaultEvent> getFaultLog();
    
    /**
     * @brief Get fault events in JSON format
     * 
     * @param output Output buffer
     * @param maxSize Maximum buffer size
     * @return true if successful
     */
    static bool getFaultLogJSON(char* output, size_t maxSize);
    
    /**
     * @brief Print fault log to serial
     */
    static void printFaultLog();
    
    /**
     * @brief Clear fault log
     */
    static void clearFaultLog();
    
    /**
     * @brief Get fault statistics
     */
    static void getFaultStatistics(uint32_t* totalFaults, uint32_t* recoveredFaults,
                                   uint32_t* unresolvedFaults);
    
    /**
     * @brief Check if response is a Modbus exception
     * 
     * @param response Response buffer
     * @param length Response length
     * @param exceptionCode Output parameter for exception code
     * @return true if exception detected
     */
    static bool isModbusException(const uint8_t* response, size_t length, uint8_t* exceptionCode);
    
    /**
     * @brief Validate CRC of Modbus RTU frame
     * 
     * @param frame Frame buffer
     * @param length Frame length
     * @return true if CRC is valid
     */
    static bool validateCRC(const uint8_t* frame, size_t length);
    
    /**
     * @brief Check if frame is malformed
     * 
     * @param frame Frame buffer
     * @param length Frame length
     * @return true if frame is malformed
     */
    static bool isMalformedFrame(const uint8_t* frame, size_t length);
    
    /**
     * @brief Get description for Modbus exception code
     * 
     * @param code Exception code
     * @return Description string
     */
    static const char* getExceptionDescription(uint8_t code);
    
    /**
     * @brief Get description for fault type
     * 
     * @param type Fault type
     * @return Description string
     */
    static const char* getFaultTypeDescription(FaultType type);
    
    /**
     * @brief Handle timeout fault with retry logic
     * 
     * @param regAddr Register address that timed out
     * @param retryCount Current retry count
     * @return true if should retry
     */
    static bool handleTimeout(uint16_t regAddr, uint8_t retryCount);
    
    /**
     * @brief Handle CRC error with retry logic
     * 
     * @param frame Frame that failed CRC
     * @param length Frame length
     * @param retryCount Current retry count
     * @return true if should retry
     */
    static bool handleCRCError(const uint8_t* frame, size_t length, uint8_t retryCount);
    
    /**
     * @brief Handle packet drop
     * 
     * @param expectedBytes Expected response size
     * @param receivedBytes Actual received size
     */
    static void handlePacketDrop(size_t expectedBytes, size_t receivedBytes);
    
    /**
     * @brief Handle excessive delay
     * 
     * @param expectedTime Expected response time (ms)
     * @param actualTime Actual response time (ms)
     */
    static void handleDelay(unsigned long expectedTime, unsigned long actualTime);
    
    /**
     * @brief Handle Modbus exception
     * 
     * @param exceptionCode Exception code
     * @param slaveAddr Slave address
     * @param funcCode Function code
     * @param regAddr Register address
     * @return true if recoverable
     */
    static bool handleModbusException(uint8_t exceptionCode, uint8_t slaveAddr,
                                     uint8_t funcCode, uint16_t regAddr);
    
    /**
     * @brief Get maximum retries for a fault type
     * 
     * @param type Fault type
     * @return Maximum retries
     */
    static uint8_t getMaxRetries(FaultType type);
    
    /**
     * @brief Calculate retry delay with exponential backoff
     * 
     * @param retryCount Current retry count
     * @return Delay in milliseconds
     */
    static unsigned long getRetryDelay(uint8_t retryCount);

    // Configuration constants (public for external access)
    static const uint8_t MAX_TIMEOUT_RETRIES = 3;
    static const uint8_t MAX_CRC_RETRIES = 3;
    static const uint8_t MAX_EXCEPTION_RETRIES = 2;
    static const unsigned long BASE_RETRY_DELAY = 100;  // Base delay in ms

private:
    static std::vector<FaultEvent> faultLog;
    static const size_t MAX_LOG_SIZE = 100;  // Maximum number of events to keep
    
    // Statistics
    static uint32_t totalFaults;
    static uint32_t recoveredFaults;
    
    /**
     * @brief Calculate CRC16 for Modbus RTU
     */
    static uint16_t calculateCRC16(const uint8_t* data, size_t length);
    
    /**
     * @brief Add fault event to log with circular buffer behavior
     */
    static void addToLog(const FaultEvent& event);
    
    /**
     * @brief Persist fault log to NVS (for critical faults)
     */
    static void persistCriticalFault(const FaultEvent& event);
};

#endif // FAULT_RECOVERY_H
