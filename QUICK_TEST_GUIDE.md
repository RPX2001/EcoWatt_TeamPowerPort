# 🚀 Quick Test Guide - Fault Recovery System
**5-Minute Quick Start**

---

## ✅ Step 1: Flash Firmware (2 minutes)

### Upload to ESP32
```bash
cd /Users/prabathwijethilaka/Sem7/Embedded/EcoWatt_TeamPowerPort/PIO/ECOWATT
pio run -t upload -t monitor
```

### What You Should See in ESP32 Log:
```
╔═══════════════════════════════════════╗
║  FAULT LOGGER INITIALIZATION          ║
╚═══════════════════════════════════════╝
  [INFO] Fault logger initialized
  Max log entries: 50
  [SUCCESS] ✓ Fault logger ready

╔═══════════════════════════════════════╗
║  FAULT HANDLER INITIALIZATION         ║
╚═══════════════════════════════════════╝
  Max retries: 3
  Base retry delay: 500 ms
  [SUCCESS] ✓ Fault handler ready

... (WiFi connecting, MQTT connecting) ...

╔═══════════════════════════════════════╗
║       MODBUS POLL CYCLE START         ║
╚═══════════════════════════════════════╝
```

✅ **Success Indicator**: You see "Fault logger ready" and "Fault handler ready"

---

## ✅ Step 2: Test CRC Error (1 minute)

### Open New Terminal
```bash
cd /Users/prabathwijethilaka/Sem7/Embedded/EcoWatt_TeamPowerPort/flask
python3 inject_fault.py --error CRC_ERROR
```

### What You Should See in Script Output:
```
╔═══════════════════════════════════════════════════════╗
║     EcoWatt Fault Injection Testing Tool             ║
║     Milestone 5 Part 2: Fault Recovery               ║
╚═══════════════════════════════════════════════════════╝

[INFO] Injecting fault via Error Emulation API
       URL: http://20.15.114.131:8080/api/inverter/error
       Type: CRC_ERROR
       Payload: {
  "slaveAddress": 17,
  "functionCode": 4,
  "errorType": "CRC_ERROR",
  "exceptionCode": 0,
  "delayMs": 0
}
[SUCCESS] Fault injected successfully
          Response: {
  "frame": "118400423A"
}

✓ Fault injection complete
[NEXT] Monitor ESP32 serial output for fault detection and recovery
```

### What You Should See in ESP32 Log:
```
╔═══════════════════════════════════════╗
║       MODBUS POLL CYCLE START         ║
╚═══════════════════════════════════════╝

  [ERROR] FAULT: CRC validation failed
  Module: Acquisition
  Recovered: YES
  Recovery: Retry request (transient error)

  [INFO] Recovery delay: 500 ms

╔═══════════════════════════════════════╗
║       MODBUS POLL CYCLE START         ║  <-- RETRY!
╚═══════════════════════════════════════╝

  [SUCCESS] ✓ Data saved to buffer       <-- RECOVERED!
```

✅ **Success Indicator**: 
- Fault detected
- Shows "Recovered: YES"
- Next poll succeeds

---

## ✅ Step 3: Test Modbus Exception (1 minute)

### Inject Slave Device Busy Error
```bash
python3 inject_fault.py --error EXCEPTION --code 06
```

### What You Should See in ESP32 Log:
```
╔═══════════════════════════════════════╗
║       MODBUS POLL CYCLE START         ║
╚═══════════════════════════════════════╝

  [ERROR] FAULT: Modbus exception 0x06: Slave Device Busy
  Exception Code: 0x06 (Slave Device Busy)
  Module: Acquisition
  Recovered: YES
  Recovery: Wait for slave to become ready
  Retries: 1

  [INFO] Recovery delay: 500 ms

╔═══════════════════════════════════════╗
║       MODBUS POLL CYCLE START         ║  <-- RETRY!
╚═══════════════════════════════════════╝

  [SUCCESS] ✓ Data saved to buffer       <-- RECOVERED!
```

✅ **Success Indicator**:
- Exception code detected (0x06)
- Exception description shown ("Slave Device Busy")
- Automatic retry after delay
- Recovery successful

---

## ✅ Step 4: Test Unrecoverable Error (30 seconds)

### Inject Illegal Function Error
```bash
python3 inject_fault.py --error EXCEPTION --code 01
```

### What You Should See in ESP32 Log:
```
╔═══════════════════════════════════════╗
║       MODBUS POLL CYCLE START         ║
╚═══════════════════════════════════════╝

  [ERROR] FAULT: Modbus exception 0x01: Illegal Function
  Exception Code: 0x01 (Illegal Function)
  Module: Acquisition
  Recovered: NO
  Recovery: None - configuration error

  [ERROR] Fault not recoverable
  [ERROR] Failed to read inverter data after 0 retries
```

✅ **Success Indicator**:
- Fault detected
- Shows "Recovered: NO"
- **NO RETRY** (permanent error)
- Marked as "not recoverable"

---

## ✅ Step 5: Test All Error Types (2 minutes)

### Quick Test Script
```bash
# Test all 5 error types
python3 inject_fault.py --error CRC_ERROR
sleep 15

python3 inject_fault.py --error EXCEPTION --code 06
sleep 15

python3 inject_fault.py --error CORRUPT
sleep 15

python3 inject_fault.py --error PACKET_DROP
sleep 15

python3 inject_fault.py --error DELAY --delay 2000
sleep 15
```

### What You Should See:
Each error should be:
1. ✅ **Detected** (ESP32 logs the fault)
2. ✅ **Classified** (correct fault type shown)
3. ✅ **Recovered** (shows retry and success)

---

## 📊 Expected Serial Output Patterns

### ✅ Successful Fault Recovery Pattern
```
[ERROR] FAULT: <description>
  Module: <module_name>
  Recovered: YES
  Recovery: <recovery_action>
  
[INFO] Recovery delay: <delay> ms

<--- WAIT FOR RETRY --->

[SUCCESS] ✓ Data saved to buffer
```

### ❌ Unrecoverable Fault Pattern
```
[ERROR] FAULT: <description>
  Module: <module_name>
  Recovered: NO
  Recovery: None - <reason>
  
[ERROR] Fault not recoverable
[ERROR] Failed to read inverter data
```

### ⚠️ Timeout Pattern
```
[ERROR] FAULT: Timeout after 10000 ms
  Module: <module_name>
  Recovered: YES
  Recovery: Retry request
  
[INFO] Recovery delay: 1000 ms  <-- Longer delay for timeouts

<--- WAIT FOR RETRY --->

[SUCCESS] ✓ Data saved to buffer
```

---

## 🔍 What Each Test Validates

| Test | Validates | Expected Result |
|------|-----------|-----------------|
| **CRC_ERROR** | CRC validation logic | ✅ Detected, retried, recovered |
| **EXCEPTION 06** | Exception detection & retry | ✅ Detected, retried, recovered |
| **EXCEPTION 01** | Non-recoverable detection | ✅ Detected, NO retry, failed |
| **CORRUPT** | Frame validation | ✅ Detected, retried, recovered |
| **PACKET_DROP** | Timeout handling | ✅ Detected after 10s, retried, recovered |
| **DELAY** | Response time handling | ✅ Normal (if delay < 10s) |

---

## 🎯 Quick Success Checklist

After running tests, verify:

- [ ] ✅ Initialization messages appear at boot
- [ ] ✅ Fault logger shows "ready"
- [ ] ✅ Fault handler shows "ready"
- [ ] ✅ CRC errors detected and recovered
- [ ] ✅ Modbus exceptions detected with correct codes
- [ ] ✅ Exception descriptions match codes
- [ ] ✅ Recoverable errors show "Recovered: YES"
- [ ] ✅ Non-recoverable errors show "Recovered: NO"
- [ ] ✅ Retry delays shown (500ms, 1000ms, etc.)
- [ ] ✅ Next poll succeeds after recovery

---

## ⚠️ Troubleshooting

### Problem: No fault messages appear
**Solution**: Check if fault handler initialized:
```
Look for: "FAULT HANDLER INITIALIZATION" in log
If missing: Check main.cpp has FaultHandler::init()
```

### Problem: Faults not detected
**Solution**: Check frame validation is enabled:
```
Look for: Frame validation in readMultipleRegisters()
If missing: Check acquisition.cpp integration
```

### Problem: Script fails with 400 error
**Solution**: Check API payload format:
```bash
# Verify correct format being sent
python3 inject_fault.py --error CRC_ERROR
# Should show "slaveAddress", "functionCode", etc.
```

### Problem: No retry attempts
**Solution**: Check error type:
```
Codes 01, 02, 03 = Non-recoverable (no retry expected)
Codes 04-0B = Recoverable (should retry)
CRC, CORRUPT, TIMEOUT = Always retry
```

---

## 🎬 Demo Video Checklist

Record your screen showing:
1. ✅ ESP32 booting with initialization messages
2. ✅ Terminal running `python3 inject_fault.py --error CRC_ERROR`
3. ✅ Script showing "SUCCESS"
4. ✅ ESP32 log showing fault detection
5. ✅ ESP32 log showing retry delay
6. ✅ ESP32 log showing successful recovery
7. ✅ Repeat for exception code 06
8. ✅ Show non-recoverable error (code 01) with no retry

**Duration**: 3-5 minutes  
**Key Points**: Show detection → retry → recovery cycle

---

## 📈 Performance Expectations

| Metric | Expected Value |
|--------|----------------|
| Recovery Rate | > 95% for transient errors |
| Retry Count | 1-2 retries average |
| CRC Error Recovery | ~1 second (500ms delay + poll) |
| Exception 06 Recovery | ~1 second |
| Timeout Recovery | ~12 seconds (10s timeout + 1s retry) |
| Non-recoverable Errors | 0% recovery (expected) |

---

## 🏁 Final Validation

After all tests, your ESP32 log should show:
```
╔═══════════════════════════════════════╗
║  FAULT STATISTICS                     ║
╚═══════════════════════════════════════╝
  Total Faults: 5
  Recovered: 4 (80.0%)
  Failed: 1

  Fault Breakdown:
    CRC Error: 1
    Modbus Exception: 2
    Corrupt Frame: 1
    Timeout: 1
```

✅ **You're ready for milestone submission if**:
- Initialization messages appear
- Faults are detected
- Recovery attempts shown
- Next polls succeed
- Statistics make sense

---

## 📝 Quick Command Reference

```bash
# Basic Tests
python3 inject_fault.py --error CRC_ERROR
python3 inject_fault.py --error EXCEPTION --code 06
python3 inject_fault.py --error CORRUPT
python3 inject_fault.py --error PACKET_DROP
python3 inject_fault.py --error DELAY --delay 2000

# Production API (affects next request only)
python3 inject_fault.py --error EXCEPTION --code 04 --production

# Clear error flags
python3 inject_fault.py --clear

# All exception codes test
for code in 01 02 03 04 05 06 08 0A 0B; do
    python3 inject_fault.py --error EXCEPTION --code $code
    sleep 15
done
```

---

## ✅ Success = You See This Pattern

```
1. Script: [SUCCESS] Fault injected
   ↓
2. ESP32: [ERROR] FAULT: <description>
   ↓
3. ESP32: [INFO] Recovery delay: 500 ms
   ↓
4. ESP32: [SUCCESS] ✓ Data saved to buffer
```

**If you see this pattern → Your implementation is working! 🎉**

---

**Time to Test**: ~5-10 minutes  
**Next**: Document results and create demo video  
**Status**: Ready to test!
