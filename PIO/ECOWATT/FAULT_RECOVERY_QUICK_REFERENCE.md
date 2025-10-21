# Fault Recovery Quick Reference

## 📋 Quick Start

### Initialize
```cpp
void setup() {
    FaultRecovery::init();  // Call once during setup
}
```

### Check Fault Status
```cpp
uint32_t total, recovered, unresolved;
FaultRecovery::getFaultStatistics(&total, &recovered, &unresolved);
```

### View Fault Log
```cpp
FaultRecovery::printFaultLog();
```

---

## 🔍 Fault Types & Actions

| Type | When It Occurs | Retries | Action |
|------|----------------|---------|--------|
| TIMEOUT | No inverter response | 3 | Exponential backoff, retry |
| CRC_ERROR | Checksum mismatch | 3 | Log frame data, retry |
| CORRUPT_RESPONSE | Data mismatch | 3 | Log and retry |
| PACKET_DROP | Incomplete frame | 0 | Log only |
| DELAY | Response >2 seconds | 0 | Log actual time |
| MODBUS_EXCEPTION | Exception response | 2 | Check if recoverable |
| MALFORMED_FRAME | Invalid structure | 3 | Validate and retry |
| BUFFER_OVERFLOW | Frame too large | 0 | Log and abort |

---

## 🚨 Modbus Exceptions

| Code | Name | Retry? | Why |
|------|------|--------|-----|
| 0x01 | Illegal Function | ❌ | Function not supported |
| 0x02 | Illegal Address | ❌ | Register doesn't exist |
| 0x03 | Illegal Value | ❌ | Invalid data value |
| 0x04 | Device Failure | ⚠️ | Hardware error |
| 0x05 | Acknowledge | ✅ | Processing request |
| 0x06 | Device Busy | ✅ | Temporarily unavailable |
| 0x08 | Memory Error | ⚠️ | Memory parity issue |
| 0x0A | Gateway Unavailable | ⚠️ | Routing failed |
| 0x0B | Gateway Failed | ⚠️ | Target unresponsive |

---

## ⚙️ Configuration

```cpp
// Retry limits (in fault_recovery.h)
MAX_TIMEOUT_RETRIES = 3;
MAX_CRC_RETRIES = 3;
MAX_EXCEPTION_RETRIES = 2;
BASE_RETRY_DELAY = 100;  // ms

// Log settings
MAX_LOG_SIZE = 100;  // events

// Upload interval (in main.cpp)
FAULT_LOG_UPLOAD_INTERVAL = 60000000ULL;  // 60s
```

---

## 📡 Cloud Upload

### Automatic
- Every 60 seconds (if faults exist)
- Endpoint: `FLASK_SERVER_URL/faults`
- Format: JSON

### Manual
```cpp
uploadFaultLogToCloud();
```

### JSON Structure
```json
{
  "device_id": "ESP32_EcoWatt_Smart",
  "timestamp": 1234567890,
  "total_faults": 12,
  "recovered_faults": 8,
  "unresolved_faults": 4,
  "fault_events": [...]
}
```

---

## 🎮 Commands

### Via Server
```json
{"command_type": "get_fault_log", "parameters": {}}
{"command_type": "clear_fault_log", "parameters": {}}
{"command_type": "upload_fault_log", "parameters": {}}
```

### Via Serial
```cpp
FaultRecovery::printFaultLog();
FaultRecovery::clearFaultLog();
uploadFaultLogToCloud();
```

---

## 🧪 Testing Checklist

- [ ] Timeout: No response from inverter
- [ ] CRC Error: Corrupted frame checksum
- [ ] Exception 0x06: Device busy
- [ ] Exception 0x02: Invalid address
- [ ] Delay: Response >2 seconds
- [ ] Malformed: Invalid frame structure
- [ ] Recovery: Successful retry after fault
- [ ] Cloud Upload: Fault log reaches server
- [ ] Commands: Get/clear/upload work

---

## 🔧 Troubleshooting

### Too Many Retries
→ Increase `MAX_TIMEOUT_RETRIES`

### Buffer Full
→ Increase `MAX_LOG_SIZE` or upload more frequently

### Slow Retries
→ Decrease `BASE_RETRY_DELAY`

### Cloud Upload Fails
→ Check WiFi, server endpoint, buffer size

### Log Not Showing
→ Call `FaultRecovery::printFaultLog()`

---

## 📊 Statistics

```cpp
uint32_t total, recovered, unresolved;
FaultRecovery::getFaultStatistics(&total, &recovered, &unresolved);

float recovery_rate = (float)recovered / total * 100.0;
Serial.printf("Recovery Rate: %.1f%%\n", recovery_rate);
```

---

## 🎯 Best Practices

1. ✅ Monitor fault statistics regularly
2. ✅ Clear logs after analysis
3. ✅ Test with simulated errors first
4. ✅ Tune retry delays for your hardware
5. ✅ Check cloud uploads are working
6. ✅ Investigate recurring exceptions
7. ✅ Review unresolved faults

---

## 📝 Example Output

```
⚠️ FAULT DETECTED ⚠️
Type: Timeout
Time: 12345678 ms
Description: Read timeout for register 0x0006 (retry 2/3)
Slave: 0x11, Function: 0x03, Register: 0x0006

⏳ Retry in 200 ms...

✓ Successfully recovered after 2 retries
```

---

## 🚀 Ready to Test!

**Files Created:**
- `fault_recovery.h` - Interface
- `fault_recovery.cpp` - Implementation
- `FAULT_RECOVERY_DOCUMENTATION.md` - Full docs
- `FAULT_RECOVERY_SUMMARY.md` - Implementation summary
- `FAULT_RECOVERY_QUICK_REFERENCE.md` - This guide

**Build Status:** ✅ SUCCESS

**Next:** Test with your inverter simulator API!

---

## 📞 Quick Help

**View faults:** `FaultRecovery::printFaultLog();`  
**Check stats:** `FaultRecovery::getFaultStatistics(...);`  
**Clear log:** `FaultRecovery::clearFaultLog();`  
**Upload now:** `uploadFaultLogToCloud();`

**Full docs:** See `FAULT_RECOVERY_DOCUMENTATION.md`
