# 👀 Visual Test Reference - What You Should See
**ESP32 Serial Output Examples**

---

## ✅ BOOT SEQUENCE (What you see first)

```
══════════════════════════════════════════════════════════
   _____ __________  _       _____   ____________
  / ____|____  / _ \| |     | ____| | ____| ____|
 | |        / / | | | |     | |     | |__  | |__  
 | |       / /| | | | |     | |___  |  __| |  __| 
  \____| |____\___/|_|      \____/  |____| |____|
══════════════════════════════════════════════════════════

╔═══════════════════════════════════════╗
║  SYSTEM INITIALIZATION                ║
╚═══════════════════════════════════════╝
  [INFO] Initializing NVS
  [SUCCESS] ✓ NVS initialized

╔═══════════════════════════════════════╗
║  FAULT LOGGER INITIALIZATION          ║  👈 LOOK FOR THIS!
╚═══════════════════════════════════════╝
  [INFO] Fault logger initialized
  Max log entries: 50
  [SUCCESS] ✓ Fault logger ready          👈 SUCCESS!

╔═══════════════════════════════════════╗
║  FAULT HANDLER INITIALIZATION         ║  👈 LOOK FOR THIS!
╚═══════════════════════════════════════╝
  Max retries: 3
  Base retry delay: 500 ms
  [SUCCESS] ✓ Fault handler ready         👈 SUCCESS!

╔═══════════════════════════════════════╗
║  WIFI CONNECTION                      ║
╚═══════════════════════════════════════╝
  [INFO] Connecting to WiFi: YourSSID
  [SUCCESS] ✓ WiFi connected
  IP: 192.168.1.100

╔═══════════════════════════════════════╗
║  MQTT CONNECTION                      ║
╚═══════════════════════════════════════╝
  [INFO] Connecting to MQTT broker
  [SUCCESS] ✓ MQTT connected
```

✅ **If you see this → System initialized correctly!**

---

## 📋 TEST 1: CRC Error Recovery

### Terminal 1 (Inject Fault):
```bash
$ python3 inject_fault.py --error CRC_ERROR
```

### Terminal 1 Output:
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
[SUCCESS] Fault injected successfully ✓               👈 GOOD!
          Response: {
  "frame": "118400423A"
}
          Frame: 118400423A

✓ Fault injection complete
[NEXT] Monitor ESP32 serial output for fault detection
```

### Terminal 2 (ESP32 Serial Monitor):
```
╔═══════════════════════════════════════╗
║       MODBUS POLL CYCLE START         ║
╚═══════════════════════════════════════╝
  [INFO] Sending Modbus request...
  [INFO] Response received: 118400423A

  [ERROR] FAULT: CRC validation failed    👈 DETECTED!
  Module: Acquisition
  Recovered: YES                          👈 WILL RETRY!
  Recovery: Retry request (transient error)

  [INFO] Recovery delay: 500 ms          👈 WAIT 500ms

╔═══════════════════════════════════════╗
║       MODBUS POLL CYCLE START         ║  👈 RETRY ATTEMPT
╚═══════════════════════════════════════╝
  [INFO] Sending Modbus request...
  [INFO] Response received: 1104040904002A2870

  [SUCCESS] ✓ Frame validation passed    👈 RECOVERED!
  [SUCCESS] ✓ Data saved to buffer

  Vac1: 230.40 V
  Iac1: 4.20 A
  Fac1: 50.00 Hz
  Vpv1: 400.00 V
```

✅ **Success Pattern**:
1. Fault detected ✓
2. Shows "Recovered: YES" ✓
3. Waits 500ms ✓
4. Retries ✓
5. Next poll succeeds ✓

---

## 📋 TEST 2: Modbus Exception (Recoverable)

### Terminal 1:
```bash
$ python3 inject_fault.py --error EXCEPTION --code 06
```

### Terminal 2 (ESP32):
```
╔═══════════════════════════════════════╗
║       MODBUS POLL CYCLE START         ║
╚═══════════════════════════════════════╝

  [ERROR] FAULT: Modbus exception 0x06: Slave Device Busy  👈 DETECTED!
  Exception Code: 0x06 (Slave Device Busy)                 👈 CODE + DESC!
  Module: Acquisition
  Recovered: YES                                           👈 WILL RETRY!
  Recovery: Wait for slave to become ready
  Retries: 1

  [INFO] Recovery delay: 500 ms

╔═══════════════════════════════════════╗
║       MODBUS POLL CYCLE START         ║  👈 RETRY!
╚═══════════════════════════════════════╝

  [SUCCESS] ✓ Frame validation passed    👈 RECOVERED!
  [SUCCESS] ✓ Data saved to buffer
```

✅ **Success Pattern**:
1. Exception code extracted (0x06) ✓
2. Description shown ("Slave Device Busy") ✓
3. Marked as recoverable ✓
4. Retry successful ✓

---

## 📋 TEST 3: Modbus Exception (Non-Recoverable)

### Terminal 1:
```bash
$ python3 inject_fault.py --error EXCEPTION --code 01
```

### Terminal 2 (ESP32):
```
╔═══════════════════════════════════════╗
║       MODBUS POLL CYCLE START         ║
╚═══════════════════════════════════════╝

  [ERROR] FAULT: Modbus exception 0x01: Illegal Function  👈 DETECTED!
  Exception Code: 0x01 (Illegal Function)
  Module: Acquisition
  Recovered: NO                                            👈 WON'T RETRY!
  Recovery: None - configuration error

  [ERROR] Fault not recoverable                           👈 EXPECTED!
  [ERROR] Failed to read inverter data after 0 retries

╔═══════════════════════════════════════╗
║       WAITING FOR NEXT POLL CYCLE     ║  👈 MOVES ON
╚═══════════════════════════════════════╝
```

✅ **Success Pattern**:
1. Exception detected ✓
2. Shows "Recovered: NO" ✓
3. **NO RETRY** (correct behavior) ✓
4. Moves to next cycle ✓

---

## 📋 TEST 4: Corrupt Frame

### Terminal 1:
```bash
$ python3 inject_fault.py --error CORRUPT
```

### Terminal 2 (ESP32):
```
╔═══════════════════════════════════════╗
║       MODBUS POLL CYCLE START         ║
╚═══════════════════════════════════════╝

  [ERROR] FAULT: Frame corruption detected               👈 DETECTED!
  Invalid Frame: [2 bytes] 11 FF                         👈 SHOWS FRAME!
  Module: Acquisition
  Recovered: YES                                          👈 WILL RETRY!
  Recovery: Retry request (transient error)

  [INFO] Recovery delay: 500 ms

╔═══════════════════════════════════════╗
║       MODBUS POLL CYCLE START         ║  👈 RETRY!
╚═══════════════════════════════════════╝

  [SUCCESS] ✓ Frame validation passed    👈 RECOVERED!
```

✅ **Success Pattern**:
1. Corruption detected ✓
2. Invalid frame shown ✓
3. Immediate retry ✓
4. Recovery successful ✓

---

## 📋 TEST 5: Timeout/Packet Drop

### Terminal 1:
```bash
$ python3 inject_fault.py --error PACKET_DROP
```

### Terminal 2 (ESP32):
```
╔═══════════════════════════════════════╗
║       MODBUS POLL CYCLE START         ║
╚═══════════════════════════════════════╝
  [INFO] Sending Modbus request...
  [INFO] Waiting for response...
  [INFO] Waiting... (1s)
  [INFO] Waiting... (2s)
  [INFO] Waiting... (3s)
  ...
  [INFO] Waiting... (10s)

  [ERROR] FAULT: Timeout after 10000 ms                  👈 TIMEOUT!
  Module: Protocol Adapter
  Recovered: YES                                          👈 WILL RETRY!
  Recovery: Retry with exponential backoff

  [INFO] Recovery delay: 1000 ms                         👈 LONGER DELAY!

╔═══════════════════════════════════════╗
║       MODBUS POLL CYCLE START         ║  👈 RETRY!
╚═══════════════════════════════════════╝

  [SUCCESS] ✓ Response received          👈 RECOVERED!
```

✅ **Success Pattern**:
1. Waits 10 seconds (timeout) ✓
2. Timeout detected ✓
3. Longer retry delay (1000ms) ✓
4. Recovery successful ✓

---

## 📋 TEST 6: Delayed Response

### Terminal 1:
```bash
$ python3 inject_fault.py --error DELAY --delay 2000
```

### Terminal 2 (ESP32):
```
╔═══════════════════════════════════════╗
║       MODBUS POLL CYCLE START         ║
╚═══════════════════════════════════════╝
  [INFO] Sending Modbus request...
  [INFO] Waiting for response...
  [INFO] Response delayed...              👈 SLOW RESPONSE
  [INFO] Response received (after 2000ms) 👈 BUT WITHIN TIMEOUT!

  [SUCCESS] ✓ Frame validation passed     👈 NO FAULT (NORMAL)
  [SUCCESS] ✓ Data saved to buffer

  Note: Delay < timeout (10s), so no fault logged
```

✅ **Success Pattern**:
1. Waits for delayed response ✓
2. Receives response within timeout ✓
3. **NO FAULT** logged (correct!) ✓
4. Normal operation continues ✓

---

## 🎯 Color-Coded Output Meanings

```
  [INFO]    ℹ️  Informational message (blue)
  [SUCCESS] ✅ Operation succeeded (green)
  [WARNING] ⚠️  Warning message (yellow)
  [ERROR]   ❌ Error detected (red)
```

---

## 🔍 Key Indicators of Success

### ✅ Fault Detection Working:
```
  [ERROR] FAULT: <clear description>
  Module: <correct module name>
```

### ✅ Recovery System Working:
```
  Recovered: YES
  Recovery: <appropriate strategy>
  [INFO] Recovery delay: <delay> ms
```

### ✅ Retry Logic Working:
```
╔═══════════════════════════════════════╗
║       MODBUS POLL CYCLE START         ║  <-- This appears again
╚═══════════════════════════════════════╝
```

### ✅ Final Recovery:
```
  [SUCCESS] ✓ Frame validation passed
  [SUCCESS] ✓ Data saved to buffer
```

---

## ❌ What BAD Output Looks Like

### ❌ Fault Not Detected:
```
╔═══════════════════════════════════════╗
║       MODBUS POLL CYCLE START         ║
╚═══════════════════════════════════════╝
  [INFO] Response received
  <no fault message>                     ❌ MISSING!
  [ERROR] Failed to parse response       ❌ WRONG!
```

### ❌ No Retry Attempt:
```
  [ERROR] FAULT: CRC validation failed
  Recovered: YES
  <next poll cycle doesn't happen>        ❌ NO RETRY!
```

### ❌ Initialization Failed:
```
<boot messages>
<no "FAULT LOGGER INITIALIZATION">       ❌ MISSING!
<no "FAULT HANDLER INITIALIZATION">      ❌ MISSING!
```

---

## 📊 Statistics View (After Multiple Tests)

You can see accumulated stats in the log:

```
╔═══════════════════════════════════════╗
║  FAULT STATISTICS                     ║
╚═══════════════════════════════════════╝
  Total Faults:       8
  Recovered:          7 (87.5%)          👈 GOOD RECOVERY RATE!
  Failed:             1

  Fault Breakdown:
    Modbus Exception: 3                  👈 TRACKED BY TYPE
    Timeout:          2
    CRC Error:        2
    Corrupt Frame:    1
```

---

## 🎬 Demo Video - What to Record

1. **Boot Sequence** (show initialization)
2. **Script Terminal** (show injection command)
3. **ESP32 Terminal** (show fault detection)
4. **Recovery Process** (show retry and success)
5. **Multiple Tests** (show different error types)

**Recording Tip**: Split screen with script on left, ESP32 log on right

---

## ✅ Final Checklist - You're Ready If You See:

- [ ] ✅ "Fault logger ready" at boot
- [ ] ✅ "Fault handler ready" at boot
- [ ] ✅ Script shows "SUCCESS" for fault injection
- [ ] ✅ ESP32 shows "[ERROR] FAULT: <description>"
- [ ] ✅ ESP32 shows "Recovered: YES" for transient errors
- [ ] ✅ ESP32 shows "Recovered: NO" for permanent errors (01-03)
- [ ] ✅ ESP32 shows "[INFO] Recovery delay: <ms>"
- [ ] ✅ ESP32 shows retry poll cycle starting
- [ ] ✅ ESP32 shows "[SUCCESS] ✓ Data saved to buffer"
- [ ] ✅ Exception codes and descriptions match

---

## 🚀 You're Good to Go!

**If your output matches these patterns → Your implementation is working perfectly!**

Now go test and collect evidence for your milestone submission! 🎉
