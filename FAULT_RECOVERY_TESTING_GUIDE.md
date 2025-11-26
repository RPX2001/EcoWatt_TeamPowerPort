# Fault Recovery Testing Guide
**Milestone 5 Part 2: Fault Recovery Implementation**

## Overview
This guide provides comprehensive instructions for testing the fault recovery mechanisms implemented in the EcoWatt system.

## System Architecture

### Fault Logging System (`fault_logger.h/cpp`)
- **Purpose**: Persistent event logging with NVS storage
- **Capacity**: 50 events (ring buffer)
- **Format**: JSON with ISO 8601 timestamps
- **Statistics**: Total faults, recovery rate, fault breakdown

### Fault Handler (`fault_handler.h/cpp`)
- **Purpose**: Detection and recovery of Modbus/HTTP errors
- **Features**:
  - Modbus exception detection (function code & 0x80)
  - CRC-16 validation
  - Frame corruption detection
  - Timeout handling
  - Automatic retry with exponential backoff

### Integration Points
1. **Modbus Polling** (`acquisition.cpp`)
   - Frame validation after each read
   - Automatic retry on transient errors
   - Fault logging with recovery tracking

2. **HTTP Communication** (`protocol_adapter.cpp`)
   - HTTP error handling
   - Timeout detection
   - Connection retry logic

3. **Remote Commands** (`main.cpp`)
   - `get_fault_log` - Retrieve fault events as JSON
   - `clear_fault_log` - Clear fault event log
   - `get_fault_stats` - Get fault statistics

---

## Fault Injection Tool

### Location
```bash
flask/inject_fault.py
```

### Installation
```bash
cd flask
pip3 install requests
chmod +x inject_fault.py
```

### Usage

#### 1. Error Emulation API (Development Testing)
Direct error injection without affecting subsequent requests.

**Test CRC Error:**
```bash
python3 inject_fault.py --error CRC_ERROR
```

**Test Modbus Exception (Slave Device Busy):**
```bash
python3 inject_fault.py --error EXCEPTION --code 06
```

**Test Corrupt Frame:**
```bash
python3 inject_fault.py --error CORRUPT
```

**Test Packet Drop/Timeout:**
```bash
python3 inject_fault.py --error PACKET_DROP
```

**Test Delayed Response (2 seconds):**
```bash
python3 inject_fault.py --error DELAY --delay 2000
```

#### 2. Add Error Flag API (Production-Like Testing)
Sets a flag that triggers error on the **next** ESP32 request.

**Test Exception with Flag:**
```bash
python3 inject_fault.py --error EXCEPTION --code 04 --production
```

**Test CRC Error with Flag:**
```bash
python3 inject_fault.py --error CRC_ERROR --production
```

**Clear Error Flags:**
```bash
python3 inject_fault.py --clear
```

---

## Supported Error Types

### 1. EXCEPTION - Modbus Exception Response
**Description**: Server returns exception frame with error bit set (function code | 0x80)

**Frame Format:**
```
[Slave Address] [Function Code + 0x80] [Exception Code] [CRC]
Example: 11 84 06 42 3A (Slave Device Busy)
```

**Exception Codes:**
| Code | Meaning | Recoverable |
|------|---------|-------------|
| 0x01 | Illegal Function | No (config error) |
| 0x02 | Illegal Data Address | No (config error) |
| 0x03 | Illegal Data Value | No (config error) |
| 0x04 | Slave Device Failure | Yes |
| 0x05 | Acknowledge | Yes |
| 0x06 | Slave Device Busy | Yes |
| 0x08 | Memory Parity Error | Yes |
| 0x0A | Gateway Path Unavailable | Yes |
| 0x0B | Gateway Target Failed to Respond | Yes |

**Testing:**
```bash
# Test each exception code
for code in 01 02 03 04 05 06 08 0A 0B; do
    python3 inject_fault.py --error EXCEPTION --code $code
    sleep 15  # Wait for next ESP32 poll
done
```

**Expected Behavior:**
- ESP32 detects exception (function code & 0x80)
- Logs fault with exception code and description
- Attempts recovery for codes 04-0B
- Does not retry for codes 01-03 (permanent errors)

---

### 2. CRC_ERROR - Invalid Checksum
**Description**: Frame with incorrect CRC-16 checksum

**Testing:**
```bash
python3 inject_fault.py --error CRC_ERROR
```

**Expected Behavior:**
- ESP32 calculates CRC and detects mismatch
- Logs "CRC Error" fault
- Retries immediately (transient error)
- Recovers on next successful poll

**Serial Output:**
```
FAULT: CRC validation failed
  Module: Acquisition
  Recovered: YES
  Recovery: Retry request (transient error)
```

---

### 3. CORRUPT - Malformed Frame
**Description**: Frame with invalid structure, length, or content

**Testing:**
```bash
python3 inject_fault.py --error CORRUPT
```

**Expected Behavior:**
- ESP32 detects structural issues (length mismatch, invalid format)
- Logs "Corrupt Frame" fault
- Retries immediately
- Recovers on next successful poll

**Detection Criteria:**
- Frame too short (< 4 bytes)
- All zeros or all 0xFF
- Wrong slave address
- Wrong function code
- Length mismatch with byte count

---

### 4. PACKET_DROP - Timeout
**Description**: No response from server (simulates network timeout)

**Testing:**
```bash
python3 inject_fault.py --error PACKET_DROP
```

**Expected Behavior:**
- HTTP client times out (HTTP code -1)
- Logs "Modbus Timeout" fault
- Retries with exponential backoff (500ms, 1000ms, 2000ms)
- Recovers when server responds

**Retry Strategy:**
- Retry 1: Wait 500ms
- Retry 2: Wait 1000ms
- Retry 3: Wait 2000ms
- Max retries: 3

---

### 5. DELAY - Slow Response
**Description**: Server delays response (tests timeout handling)

**Testing:**
```bash
# Test 2-second delay
python3 inject_fault.py --error DELAY --delay 2000

# Test 10-second delay (should timeout if HTTP timeout < 10s)
python3 inject_fault.py --error DELAY --delay 10000
```

**Expected Behavior:**
- ESP32 waits for response
- If delay > HTTP timeout: Logs timeout fault
- If delay < HTTP timeout: Normal operation

---

## Testing Scenarios

### Scenario 1: Single Fault Recovery
**Objective**: Verify detection and recovery from a single fault

**Steps:**
1. Start ESP32 with fault recovery enabled
2. Inject single fault:
   ```bash
   python3 inject_fault.py --error CRC_ERROR
   ```
3. Monitor serial output for fault detection
4. Wait for next poll (should succeed)
5. Verify recovery in fault log

**Expected Result:**
```json
{
  "events": [
    {
      "timestamp": "2025-10-19T10:30:45.123Z",
      "event": "CRC validation failed",
      "type": "CRC Error",
      "module": "Acquisition",
      "recovered": true,
      "recovery_action": "Retry request (transient error)",
      "retry_count": 1
    }
  ],
  "total_count": 1,
  "recovery_rate": 100.0
}
```

---

### Scenario 2: Multiple Consecutive Faults
**Objective**: Test retry mechanism with multiple failures

**Steps:**
1. Inject fault using production API (affects next request):
   ```bash
   python3 inject_fault.py --error EXCEPTION --code 06 --production
   ```
2. ESP32 polls → receives error
3. ESP32 retries → normal response
4. Verify fault logged with retry count

**Expected Result:**
- Fault detected on first poll
- Retry after 500ms delay
- Success on retry
- Single fault entry with `retry_count: 1`

---

### Scenario 3: Unrecoverable Fault
**Objective**: Verify system handles permanent errors correctly

**Steps:**
1. Inject illegal function error:
   ```bash
   python3 inject_fault.py --error EXCEPTION --code 01
   ```
2. Monitor serial output
3. Verify no retry attempted

**Expected Result:**
```
FAULT: Modbus exception 0x01: Illegal Function
  Module: Acquisition
  Recovered: NO
  Recovery: None - configuration error
```

---

### Scenario 4: Fault Log Management
**Objective**: Test remote fault log commands

**Steps:**
1. Inject multiple faults (5-10 different types)
2. Retrieve fault log via MQTT:
   ```json
   {
     "command": "get_fault_log",
     "count": 5
   }
   ```
3. Verify JSON response with events
4. Get statistics:
   ```json
   {
     "command": "get_fault_stats"
   }
   ```
5. Clear log:
   ```json
   {
     "command": "clear_fault_log"
   }
   ```

**Expected Result:**
- `get_fault_log`: Returns recent N events
- `get_fault_stats`: Returns total count and recovery rate
- `clear_fault_log`: Empties log, next query returns 0 events

---

### Scenario 5: Log Persistence
**Objective**: Verify fault log survives reboot

**Steps:**
1. Inject several faults
2. Verify logged events
3. **Reboot ESP32** (power cycle or reset)
4. After reboot, query fault log
5. Verify events persist

**Expected Result:**
- All logged events present after reboot
- NVS persistence working correctly

---

### Scenario 6: Ring Buffer Overflow
**Objective**: Test log capacity limits (50 events)

**Steps:**
1. Inject 60+ faults over time
2. Query fault log
3. Verify only most recent 50 events retained

**Expected Result:**
- Log contains exactly 50 events
- Oldest events removed (FIFO)
- No memory overflow

---

## Remote Commands

### Get Fault Log
**Command:**
```json
{
  "command": "get_fault_log",
  "count": 10
}
```

**Response:**
```json
{
  "events": [
    {
      "timestamp": "2025-10-19T10:30:45.123Z",
      "event": "CRC validation failed",
      "type": "CRC Error",
      "module": "Acquisition",
      "recovered": true,
      "recovery_action": "Retry request",
      "timestamp_ms": 12345678
    }
  ],
  "showing": 10,
  "total_count": 15
}
```

---

### Get Fault Statistics
**Command:**
```json
{
  "command": "get_fault_stats"
}
```

**Response:**
```json
{
  "total_faults": 25,
  "recovered": 22,
  "failed": 3,
  "recovery_rate": 88.0,
  "breakdown": {
    "modbus_exception": 8,
    "timeout": 5,
    "crc_error": 7,
    "corrupt_frame": 3,
    "http_error": 2
  }
}
```

---

### Clear Fault Log
**Command:**
```json
{
  "command": "clear_fault_log"
}
```

**Response:**
```json
{
  "status": "success",
  "message": "Fault log cleared"
}
```

---

## Serial Output Examples

### Successful Detection and Recovery
```
╔═══════════════════════════════════════╗
║       MODBUS POLL CYCLE START         ║
╚═══════════════════════════════════════╝

[ERROR] FAULT: CRC validation failed
  Exception Code: 0x00
  Module: Acquisition
  Recovered: YES
  Recovery: Retry request (transient error)
  Retries: 1

[INFO] Recovery delay: 500 ms
[SUCCESS] ✓ Fault recovered - retry successful

╔═══════════════════════════════════════╗
║          DATA SAVED TO BUFFER         ║
╚═══════════════════════════════════════╝
```

---

### Modbus Exception Detection
```
[ERROR] FAULT: Modbus exception 0x06: Slave Device Busy
  Exception Code: 0x06 (Slave Device Busy)
  Module: Acquisition
  Recovered: YES
  Recovery: Wait for slave to become ready
  Retries: 2

[INFO] Recovery delay: 1000 ms
[SUCCESS] ✓ Recovery successful on retry 2
```

---

### Unrecoverable Error
```
[ERROR] FAULT: Modbus exception 0x01: Illegal Function
  Exception Code: 0x01 (Illegal Function)
  Module: Acquisition
  Recovered: NO
  Recovery: None - configuration error

[ERROR] Fault not recoverable
[ERROR] Max retries exceeded
```

---

## Performance Metrics

### Recovery Success Rate
**Target**: > 95% for transient errors

**Calculation**:
```
Recovery Rate = (Recovered Faults / Total Faults) × 100%
```

**Query**:
```json
{
  "command": "get_fault_stats"
}
```

---

### Retry Statistics
**Metrics to Track**:
- Average retry count per fault
- Max retry count observed
- Retry delays applied

**Expected Values**:
- CRC/Corrupt: 1-2 retries
- Timeout: 2-3 retries
- Exception 0x06: 1-2 retries

---

## Troubleshooting

### Issue: Faults not detected
**Check:**
1. Fault handler initialized in `setup()`
2. Frame validation enabled in `readMultipleRegisters()`
3. Serial monitor baud rate correct (115200)

---

### Issue: No retry attempts
**Check:**
1. `isRecoverable()` returning true for fault type
2. Retry count < MAX_RETRIES (3)
3. Recovery delay not too long

---

### Issue: Log not persisting
**Check:**
1. NVS partition configured in `platformio.ini`
2. NVS initialization successful
3. Sufficient NVS space available

---

### Issue: Fault injection not working
**Check:**
1. API endpoint reachable: `http://20.15.114.131:8080`
2. Correct API key if using production API
3. Payload format matches API specification
4. Network connectivity

---

## Test Checklist

- [ ] **CRC Error Detection**
  - [ ] Fault detected and logged
  - [ ] Immediate retry attempted
  - [ ] Recovery successful
  
- [ ] **Modbus Exceptions**
  - [ ] All codes 01-0B tested
  - [ ] Recoverable exceptions retry
  - [ ] Non-recoverable exceptions skip retry
  - [ ] Exception descriptions correct
  
- [ ] **Timeout Handling**
  - [ ] HTTP timeout detected
  - [ ] Exponential backoff applied
  - [ ] Recovery after network restore
  
- [ ] **Frame Corruption**
  - [ ] Invalid structure detected
  - [ ] Retry attempted
  - [ ] Recovery successful
  
- [ ] **Remote Commands**
  - [ ] `get_fault_log` returns JSON
  - [ ] `get_fault_stats` shows metrics
  - [ ] `clear_fault_log` empties log
  
- [ ] **Persistence**
  - [ ] Log survives reboot
  - [ ] Statistics preserved
  - [ ] Ring buffer works correctly
  
- [ ] **Performance**
  - [ ] Recovery rate > 95%
  - [ ] Retry delays appropriate
  - [ ] No memory leaks

---

## Integration Verification

### End-to-End Test
1. Start system with clean fault log
2. Inject 10 different faults (various types)
3. Verify detection and logging (serial output)
4. Query fault log remotely
5. Verify statistics calculation
6. Reboot ESP32
7. Verify log persistence
8. Clear fault log
9. Verify empty log

**Duration**: ~20 minutes  
**Success Criteria**: All faults detected, logged, and recovered (where applicable)

---

## Documentation

### Code Documentation
- `fault_logger.h` - Fault event logging API
- `fault_handler.h` - Fault detection and recovery API
- `inject_fault.py` - Fault injection tool

### Integration Points
- `main.cpp` - Initialization and remote commands
- `acquisition.cpp` - Modbus frame validation
- `protocol_adapter.cpp` - HTTP error handling

---

## Milestone Compliance

### Requirements Met
✅ Handle Modbus exceptions (codes 0x01-0x0B)  
✅ Handle CRC validation errors  
✅ Handle corrupt/malformed frames  
✅ Handle timeouts  
✅ Handle buffer overflow  
✅ Maintain local event log (JSON format)  
✅ NVS persistence (survives reboot)  
✅ Remote log retrieval commands  
✅ Fault injection testing tools  
✅ Recovery strategies with retry logic  
✅ Statistics tracking (recovery rate)

---

## Next Steps

1. **Flash Updated Firmware** to ESP32
2. **Run Test Suite** using scenarios above
3. **Document Results** with screenshots/logs
4. **Measure Performance** (recovery rate, retry counts)
5. **Create Demo Video** showing fault injection and recovery
6. **Prepare Report** for Milestone 5 Part 2 submission

---

## Support

For issues or questions:
- Check serial output at 115200 baud
- Verify API connectivity with curl/Postman
- Review fault log for error patterns
- Contact instructor for API issues

**Last Updated**: October 19, 2025  
**Version**: 1.0.0  
**Milestone**: 5 Part 2 - Fault Recovery
