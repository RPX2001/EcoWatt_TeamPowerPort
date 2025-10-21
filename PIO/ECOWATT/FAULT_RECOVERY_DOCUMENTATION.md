# Fault Recovery System Documentation

## Overview
The Fault Recovery System provides comprehensive error detection, logging, and recovery mechanisms for Modbus RTU communication with the inverter simulator. It handles timeouts, CRC errors, malformed frames, packet drops, delays, and Modbus exceptions with intelligent retry logic.

---

## Table of Contents
1. [Features](#features)
2. [Fault Types](#fault-types)
3. [Modbus Exception Codes](#modbus-exception-codes)
4. [Architecture](#architecture)
5. [Integration](#integration)
6. [Usage Examples](#usage-examples)
7. [Cloud Upload](#cloud-upload)
8. [Command Interface](#command-interface)
9. [Testing](#testing)

---

## Features

### ✅ Comprehensive Error Detection
- **Timeout Detection**: Detects when inverter doesn't respond within expected time
- **CRC Validation**: Validates Modbus RTU CRC-16 checksums
- **Malformed Frame Detection**: Identifies invalid frame structures
- **Exception Detection**: Parses Modbus exception responses
- **Delay Monitoring**: Tracks response times exceeding 2 seconds
- **Packet Drop Detection**: Identifies incomplete responses

### ✅ Intelligent Retry Logic
- **Exponential Backoff**: 100ms → 200ms → 400ms → 800ms (max 2s)
- **Configurable Retry Limits**:
  - Timeouts: 3 retries
  - CRC Errors: 3 retries
  - Modbus Exceptions: 2 retries (for recoverable exceptions)
- **Exception-Based Recovery**: Different strategies for different exception types

### ✅ Event Logging
- **Circular Buffer**: Stores up to 100 most recent fault events
- **Detailed Information**: Timestamp, type, description, exception code, slave address, register address, retry count, recovery status
- **NVS Persistence**: Critical faults stored in non-volatile storage

### ✅ Statistics Tracking
- Total faults counter
- Recovered faults counter
- Unresolved faults counter
- Per-fault-type analysis

### ✅ Cloud Integration
- Automatic fault log upload every 60 seconds (if faults exist)
- JSON-formatted fault logs with full event history
- Manual upload via command interface

---

## Fault Types

```cpp
enum FaultType {
    FAULT_NONE,                // No fault
    FAULT_TIMEOUT,             // Communication timeout
    FAULT_CRC_ERROR,           // CRC validation failed
    FAULT_CORRUPT_RESPONSE,    // Response data corrupted
    FAULT_PACKET_DROP,         // Incomplete packet received
    FAULT_DELAY,               // Response time exceeded threshold
    FAULT_MODBUS_EXCEPTION,    // Modbus protocol exception
    FAULT_MALFORMED_FRAME,     // Invalid frame structure
    FAULT_BUFFER_OVERFLOW      // Buffer capacity exceeded
};
```

---

## Modbus Exception Codes

| Code | Name | Description | Recoverable? |
|------|------|-------------|--------------|
| 0x01 | Illegal Function | Function code not supported | ❌ No |
| 0x02 | Illegal Data Address | Register address out of range | ❌ No |
| 0x03 | Illegal Data Value | Invalid data value | ❌ No |
| 0x04 | Slave Device Failure | Hardware failure detected | ⚠️ Limited |
| 0x05 | Acknowledge | Processing long request | ✅ Yes |
| 0x06 | Slave Device Busy | Temporarily unavailable | ✅ Yes |
| 0x08 | Memory Parity Error | Memory error detected | ⚠️ Limited |
| 0x0A | Gateway Path Unavailable | Gateway routing failed | ⚠️ Limited |
| 0x0B | Gateway Target Failed | Gateway target unresponsive | ⚠️ Limited |

### Exception Response Format
```
[Slave Address][Function Code + 0x80][Exception Code][CRC Low][CRC High]
```

**Example**: Exception 0x02 for function 0x03
```
11 83 02 C0 F1
```
- `11` = Slave address
- `83` = Function code 0x03 with error bit (0x03 | 0x80 = 0x83)
- `02` = Exception code (Illegal Data Address)
- `C0 F1` = CRC-16

---

## Architecture

### Core Components

#### 1. FaultRecovery Class (`fault_recovery.h/cpp`)
Static class providing all fault recovery functionality.

**Key Methods:**
```cpp
// Initialization
static void init();

// Fault Logging
static void logFault(FaultType type, const char* description, 
                    uint8_t exceptionCode = 0, uint8_t slaveAddr = 0,
                    uint8_t funcCode = 0, uint16_t regAddr = 0);
static void markRecovered();

// Validation
static bool validateCRC(const uint8_t* frame, size_t length);
static bool isModbusException(const uint8_t* response, size_t length, 
                              uint8_t* exceptionCode = nullptr);
static bool isMalformedFrame(const uint8_t* frame, size_t length);

// Error Handling
static bool handleTimeout(uint16_t regAddr, uint8_t retryCount);
static bool handleCRCError(const uint8_t* frame, size_t length, uint8_t retryCount);
static bool handleModbusException(uint8_t exceptionCode, uint8_t slaveAddr,
                                  uint8_t funcCode, uint16_t regAddr);
static void handlePacketDrop(size_t expectedBytes, size_t receivedBytes);
static void handleDelay(unsigned long expectedTime, unsigned long actualTime);

// Logging & Statistics
static std::vector<FaultEvent> getFaultLog();
static bool getFaultLogJSON(char* output, size_t maxSize);
static void printFaultLog();
static void clearFaultLog();
static void getFaultStatistics(uint32_t* totalFaults, uint32_t* recoveredFaults,
                               uint32_t* unresolvedFaults);
```

#### 2. Integration Points

**acquisition.cpp** - Enhanced with fault recovery:
```cpp
DecodedValues readRequest(const RegID* regs, size_t regCount) {
    // Retry loop with fault recovery
    while (retryCount <= FaultRecovery::MAX_TIMEOUT_RETRIES && !success) {
        // Send request
        bool ok = adapter.readRegister(frame, responseFrame, sizeof(responseFrame));
        
        if (!ok) {
            // Handle timeout with retry logic
            if (FaultRecovery::handleTimeout(startAddr, retryCount)) {
                retryCount++;
                continue;
            } else {
                return errorResult;
            }
        }
        
        // Validate CRC
        if (!FaultRecovery::validateCRC(frameBytes, frameLength)) {
            if (FaultRecovery::handleCRCError(frameBytes, frameLength, retryCount)) {
                retryCount++;
                continue;
            }
        }
        
        // Check for Modbus exception
        uint8_t exceptionCode = 0;
        if (FaultRecovery::isModbusException(frameBytes, frameLength, &exceptionCode)) {
            bool shouldRetry = FaultRecovery::handleModbusException(
                exceptionCode, 0x11, 0x03, startAddr);
            if (shouldRetry) {
                retryCount++;
                continue;
            }
        }
        
        // Mark recovery if successful after retries
        if (retryCount > 0) {
            FaultRecovery::markRecovered();
        }
        
        success = true;
    }
}
```

**main.cpp** - Automatic fault log upload:
```cpp
void setup() {
    // Initialize fault recovery
    FaultRecovery::init();
    
    // Setup timer for fault log uploads (60 seconds)
    fault_log_timer = timerBegin(4, 80, true);
    timerAttachInterrupt(fault_log_timer, &onFaultLogTimer, true);
    timerAlarmWrite(fault_log_timer, FAULT_LOG_UPLOAD_INTERVAL, true);
    timerAlarmEnable(fault_log_timer);
}

void loop() {
    if (fault_log_token) {
        fault_log_token = false;
        
        uint32_t totalFaults = 0;
        FaultRecovery::getFaultStatistics(&totalFaults, nullptr, nullptr);
        
        if (totalFaults > 0) {
            uploadFaultLogToCloud();
        }
    }
}
```

---

## Usage Examples

### Example 1: Handling Timeout in Register Read

```cpp
// Acquisition code automatically handles timeouts
const RegID selection[] = {REG_PAC, REG_VAC1, REG_IAC1};
uint16_t data[3];

DecodedValues result = readRequest(selection, 3);

// If timeout occurs:
// 1. FaultRecovery::logFault(FAULT_TIMEOUT, "Read timeout for register 0x...", ...)
// 2. Exponential backoff delay applied
// 3. Retry up to 3 times
// 4. If recovered, FaultRecovery::markRecovered() called
```

### Example 2: CRC Error Handling

```cpp
// Automatic CRC validation in readRequest()
// If CRC fails:
// 1. Frame data logged for debugging
// 2. FaultRecovery::logFault(FAULT_CRC_ERROR, "CRC validation failed...", ...)
// 3. Retry up to 3 times with exponential backoff
```

### Example 3: Modbus Exception Handling

```cpp
// When exception response received:
// Example: 11 83 02 C0 F1 (Illegal Data Address)

// Automatic detection:
uint8_t exceptionCode = 0;
if (FaultRecovery::isModbusException(frameBytes, length, &exceptionCode)) {
    // exceptionCode = 0x02
    
    bool shouldRetry = FaultRecovery::handleModbusException(
        exceptionCode, 0x11, 0x03, registerAddress);
    
    // For 0x02 (Illegal Data Address), shouldRetry = false (config error)
    // For 0x06 (Slave Device Busy), shouldRetry = true (wait and retry)
}
```

### Example 4: Manual Fault Log Inspection

```cpp
// In setup() or command handler
FaultRecovery::printFaultLog();

// Output:
// ╔═══════════════════════════════════════════════════════╗
// ║              FAULT RECOVERY LOG                       ║
// ╠═══════════════════════════════════════════════════════╣
// ║ Total Faults: 12                                      ║
// ║ Recovered:    8                                       ║
// ║ Unresolved:   4                                       ║
// ║ Log Entries:  12 / 100                                ║
// ╠═══════════════════════════════════════════════════════╣
// ║ [1]                                                   ║
// ║   Time:   12345678 ms                                 ║
// ║   Type:   Timeout                                     ║
// ║   Desc:   Read timeout for register 0x0006            ║
// ║   Status: ✓ RECOVERED                                 ║
// ║   Retries: 2                                          ║
// ╟───────────────────────────────────────────────────────╢
// ...
```

---

## Cloud Upload

### Endpoint Configuration

Add to your Flask server:

```python
@app.route('/faults', methods=['POST'])
def receive_fault_log():
    fault_data = request.get_json()
    
    device_id = fault_data.get('device_id')
    timestamp = fault_data.get('timestamp')
    total_faults = fault_data.get('total_faults')
    recovered = fault_data.get('recovered_faults')
    unresolved = fault_data.get('unresolved_faults')
    events = fault_data.get('fault_events', [])
    
    # Store in database
    for event in events:
        db.store_fault_event(
            device_id=device_id,
            timestamp=event['timestamp'],
            fault_type=event['type'],
            description=event['description'],
            recovered=event['recovered'],
            retry_count=event['retry_count'],
            exception_code=event.get('exception_code'),
            slave_address=event.get('slave_address')
        )
    
    return jsonify({"status": "success"}), 200
```

### JSON Format

```json
{
  "device_id": "ESP32_EcoWatt_Smart",
  "timestamp": 1234567890,
  "total_faults": 12,
  "recovered_faults": 8,
  "unresolved_faults": 4,
  "log_size": 12,
  "fault_events": [
    {
      "timestamp": 12345678,
      "type": "Timeout",
      "type_code": 1,
      "description": "Read timeout for register 0x0006 (retry 2/3)",
      "recovered": true,
      "retry_count": 2,
      "slave_address": 17,
      "function_code": 3,
      "register_address": 6
    },
    {
      "timestamp": 12367890,
      "type": "Modbus Exception",
      "type_code": 6,
      "description": "Modbus exception from slave 0x11",
      "recovered": false,
      "retry_count": 0,
      "exception_code": 2,
      "exception_desc": "Illegal Data Address",
      "slave_address": 17,
      "function_code": 3,
      "register_address": 999
    }
  ]
}
```

### Upload Frequency
- **Automatic**: Every 60 seconds if faults exist
- **Manual**: Via command interface
- **On-Demand**: Call `uploadFaultLogToCloud()` function

---

## Command Interface

### Available Commands

#### 1. Get Fault Log
```json
{
  "command_id": "cmd_123",
  "command_type": "get_fault_log",
  "parameters": {}
}
```
Prints formatted fault log to serial output.

#### 2. Clear Fault Log
```json
{
  "command_id": "cmd_124",
  "command_type": "clear_fault_log",
  "parameters": {}
}
```
Clears all fault events and resets counters.

#### 3. Upload Fault Log
```json
{
  "command_id": "cmd_125",
  "command_type": "upload_fault_log",
  "parameters": {}
}
```
Manually triggers immediate fault log upload to cloud.

---

## Testing

### Test Scenarios

#### 1. Timeout Error
**Trigger**: Use API to simulate inverter non-response
```bash
# Expected behavior:
# - Fault logged with FAULT_TIMEOUT
# - 3 retry attempts with exponential backoff
# - Either recovery or failure after 3 attempts
```

#### 2. CRC Error
**Trigger**: Use API to corrupt response frame
```bash
# Expected behavior:
# - CRC validation fails
# - Frame data logged for debugging
# - 3 retry attempts
# - Original frame data printed to serial
```

#### 3. Modbus Exception - Slave Busy (0x06)
**Trigger**: Use API to return exception response
```bash
# Response: 11 83 06 XX XX
# Expected behavior:
# - Exception detected and parsed
# - Logged with exception code and description
# - 500ms delay applied
# - Retry attempted (recoverable exception)
```

#### 4. Modbus Exception - Illegal Address (0x02)
**Trigger**: Request invalid register address
```bash
# Response: 11 83 02 XX XX
# Expected behavior:
# - Exception detected and logged
# - No retry (configuration error, not recoverable)
# - Error returned to caller
```

#### 5. Delay Detection
**Trigger**: Use API to introduce response delay >2 seconds
```bash
# Expected behavior:
# - Delay logged with actual vs expected time
# - Request still completes
# - Fault recorded for analysis
```

### Validation Checklist

- [ ] Timeouts trigger retry with exponential backoff
- [ ] CRC errors are detected and logged with frame data
- [ ] Modbus exceptions parsed correctly with proper exception codes
- [ ] Recoverable vs non-recoverable exceptions handled differently
- [ ] Fault log stored in circular buffer (max 100 events)
- [ ] Critical faults persisted to NVS
- [ ] Fault log uploaded to cloud every 60 seconds
- [ ] Statistics tracked accurately
- [ ] Recovery marked when retry succeeds
- [ ] Command interface responds to get/clear/upload commands

---

## Performance Impact

### Memory Usage
- **Fault Log Buffer**: ~40KB (100 events × ~400 bytes each)
- **NVS Storage**: Minimal (only critical faults)
- **Code Size**: ~10KB additional flash

### CPU Overhead
- **CRC Validation**: ~50μs per frame
- **Exception Detection**: ~10μs per frame
- **Logging**: ~100μs per event
- **Retry Delays**: Configurable (100ms base, exponential)

### Network Impact
- **Fault Log Upload**: ~2-8KB per upload (depends on event count)
- **Upload Frequency**: 60 seconds (only if faults exist)

---

## Troubleshooting

### Issue: Too Many Retries
**Solution**: Check retry limits in `fault_recovery.h`:
```cpp
static const uint8_t MAX_TIMEOUT_RETRIES = 3;
static const uint8_t MAX_CRC_RETRIES = 3;
static const uint8_t MAX_EXCEPTION_RETRIES = 2;
```

### Issue: Fault Log Buffer Full
**Behavior**: Circular buffer automatically removes oldest events
**Solution**: Increase `MAX_LOG_SIZE` in `fault_recovery.h` if needed

### Issue: Excessive Delays
**Solution**: Adjust base retry delay:
```cpp
static const unsigned long BASE_RETRY_DELAY = 100;  // Base delay in ms
```

### Issue: Cloud Upload Failures
**Check**:
- WiFi connectivity
- Server endpoint availability
- JSON buffer size (8192 bytes default)

---

## Best Practices

1. **Monitor Fault Statistics**: Check total/recovered/unresolved ratios regularly
2. **Analyze Exception Patterns**: Recurring exceptions may indicate configuration issues
3. **Tune Retry Delays**: Adjust based on inverter response characteristics
4. **Review Unrecovered Faults**: Investigate non-recoverable exceptions for root cause
5. **Clear Fault Log Periodically**: After analysis, clear logs to reduce memory usage
6. **Test with Simulated Errors**: Use API error injection before production deployment

---

## Version History

**v1.0.0** (October 21, 2025)
- Initial implementation
- Support for 8 fault types
- 9 Modbus exception codes
- Exponential backoff retry logic
- Cloud integration with JSON upload
- Command interface for fault log management
- NVS persistence for critical faults

---

## Contact & Support

For issues or questions about the fault recovery system:
- Check fault log: `FaultRecovery::printFaultLog()`
- Review statistics: `FaultRecovery::getFaultStatistics()`
- Enable debug output in `debug.cpp`

---

**End of Documentation**
