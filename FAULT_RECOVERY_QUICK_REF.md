# Fault Recovery Quick Reference
**Milestone 5 Part 2 - Quick Testing Guide**

## Quick Start

### 1. Flash Firmware
```bash
cd PIO/ECOWATT
pio run -t upload -t monitor
```

### 2. Test Fault Injection
```bash
cd ../../flask
python3 inject_fault.py --error CRC_ERROR
```

---

## Fault Injection Commands

### CRC Error
```bash
python3 inject_fault.py --error CRC_ERROR
```

### Modbus Exceptions
```bash
# Slave Device Busy (0x06) - Most common, recoverable
python3 inject_fault.py --error EXCEPTION --code 06

# Slave Device Failure (0x04) - Recoverable
python3 inject_fault.py --error EXCEPTION --code 04

# Illegal Function (0x01) - Non-recoverable
python3 inject_fault.py --error EXCEPTION --code 01
```

### Timeout/Packet Drop
```bash
python3 inject_fault.py --error PACKET_DROP
```

### Corrupt Frame
```bash
python3 inject_fault.py --error CORRUPT
```

### Delayed Response
```bash
python3 inject_fault.py --error DELAY --delay 2000
```

### Production API (affects next request)
```bash
python3 inject_fault.py --error EXCEPTION --code 06 --production
```

### Clear Error Flags
```bash
python3 inject_fault.py --clear
```

---

## All Exception Codes Quick Test
```bash
#!/bin/bash
for code in 01 02 03 04 05 06 08 0A 0B; do
    echo "Testing exception code $code..."
    python3 inject_fault.py --error EXCEPTION --code $code
    sleep 15  # Wait for ESP32 poll
done
```

---

## Remote Commands (via MQTT)

### Get Fault Log
```json
{"command": "get_fault_log", "count": 10}
```

### Get Statistics
```json
{"command": "get_fault_stats"}
```

### Clear Log
```json
{"command": "clear_fault_log"}
```

---

## Serial Output Patterns

### Success Pattern
```
[INFO] Injecting fault via Error Emulation API
[SUCCESS] Fault injected successfully
```

Then on ESP32:
```
[ERROR] FAULT: CRC validation failed
  Recovered: YES
  Recovery: Retry request
[SUCCESS] ✓ Recovery successful on retry 1
```

### Failure Pattern
```
[ERROR] FAULT: Modbus exception 0x01: Illegal Function
  Recovered: NO
  Recovery: None - configuration error
[ERROR] Fault not recoverable
```

---

## Expected Results

| Error Type | Detection Time | Retries | Recovery Rate |
|------------|---------------|---------|---------------|
| CRC_ERROR | Immediate | 1-2 | 100% |
| CORRUPT | Immediate | 1-2 | 100% |
| PACKET_DROP | 10s timeout | 2-3 | 95%+ |
| EXCEPTION 06 | Immediate | 1-2 | 100% |
| EXCEPTION 01 | Immediate | 0 | 0% (permanent) |

---

## Troubleshooting

### Script Not Working
```bash
# Check Python version
python3 --version  # Should be 3.x

# Install requests
pip3 install requests

# Make executable
chmod +x inject_fault.py

# Test API connection
curl -X POST http://20.15.114.131:8080/api/inverter/error \
  -H "Content-Type: application/json" \
  -d '{"slaveAddress":17,"functionCode":4,"errorType":"CRC_ERROR","exceptionCode":0,"delayMs":0}'
```

### ESP32 Not Detecting
- Check serial monitor (115200 baud)
- Verify `FaultLogger::init()` and `FaultHandler::init()` in setup()
- Ensure frame validation enabled in `readMultipleRegisters()`

### API Returns 400
- Check payload format (use examples above)
- Verify API endpoint reachable
- Check exception code is integer (not string)

---

## Test Sequence (5 Minutes)

```bash
# Terminal 1: Monitor ESP32
pio device monitor -b 115200

# Terminal 2: Inject faults
cd flask

# Test 1: CRC Error
python3 inject_fault.py --error CRC_ERROR
# Wait 15 seconds for ESP32 poll

# Test 2: Exception
python3 inject_fault.py --error EXCEPTION --code 06
# Wait 15 seconds

# Test 3: Timeout
python3 inject_fault.py --error PACKET_DROP
# Wait 15 seconds

# Check fault log
# Send MQTT command: {"command": "get_fault_log", "count": 5}
```

Expected: 3 faults logged, all recovered

---

## Quick Verification Checklist

- [ ] Script runs without errors
- [ ] API returns success (Status 200)
- [ ] ESP32 detects fault (serial shows [ERROR])
- [ ] Recovery attempted (shows retry delay)
- [ ] Fault logged (can query via MQTT)
- [ ] Statistics updated (recovery rate shown)

---

## Common Issues

### "Status: 400" Error
**Fix**: Update payload format to match API spec (slaveAddress, functionCode, errorType, exceptionCode, delayMs)

### "Connection Failed" Error
**Fix**: Check network connection, verify API endpoint: http://20.15.114.131:8080

### "No Retry" on ESP32
**Fix**: Check if error is non-recoverable (codes 01, 02, 03 don't retry)

### "Empty Log" 
**Fix**: Ensure `FaultLogger::init()` called in setup(), check NVS partition

---

## API Endpoints

### Error Emulation (Dev Testing)
```
POST http://20.15.114.131:8080/api/inverter/error
Content-Type: application/json
NO AUTH REQUIRED

{
  "slaveAddress": 17,
  "functionCode": 4,
  "errorType": "CRC_ERROR",
  "exceptionCode": 0,
  "delayMs": 0
}
```

### Add Error Flag (Production Testing)
```
POST http://20.15.114.131:8080/api/user/error-flag/add
Content-Type: application/json
Authorization: <API_TOKEN>

{
  "errorType": "EXCEPTION",
  "exceptionCode": 6,
  "delayMs": 0
}
```

---

## Success Criteria

✅ Fault injection script works  
✅ All 5 error types tested  
✅ Recovery rate > 95% for transient errors  
✅ Non-recoverable errors skip retry  
✅ Fault log accessible via MQTT  
✅ Statistics show correct counts  
✅ Log persists after reboot  

---

## Files Modified

- `src/main.cpp` - Init + remote commands
- `src/peripheral/acquisition.cpp` - Frame validation
- `src/driver/protocol_adapter.cpp` - HTTP error handling

## Files Created

- `include/application/fault_logger.h`
- `src/application/fault_logger.cpp`
- `include/application/fault_handler.h`
- `src/application/fault_handler.cpp`
- `flask/inject_fault.py`

---

**Ready to Test!** 🚀

Run: `python3 inject_fault.py --error CRC_ERROR`
