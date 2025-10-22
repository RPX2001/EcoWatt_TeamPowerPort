# EcoWatt Complete System Testing Guide
## Wokwi + Flask + FOTA Fault Injection

**Date**: October 22, 2025  
**Status**: ✅ Firmware Built, Ready for Testing

---

## 🎯 Test Setup Overview

### System Components
1. **Flask Server** - Running on localhost:5001
2. **ESP32 (Wokwi)** - Simulated ESP32 with WiFi
3. **MQTT Broker** - broker.hivemq.com:1883
4. **OTA Fault Injection** - Server-side fault injection

### Network Configuration
- **Wokwi WiFi**: Wokwi-GUEST (no password)
- **Flask URL**: http://localhost:5001
- **Port Forwarding**: Host:5001 → ESP32

---

## 📋 Test Sequence

### Terminal 1: Flask Server
```bash
cd /home/akitha/Desktop/EcoWatt_TeamPowerPort/flask
python3 flask_server_modular.py
```

**Expected Output**:
```
======================================================================
EcoWatt Smart Server v2.0.0 - Modular Architecture
======================================================================
Features:
  ✓ Modular architecture (Routes → Handlers → Utilities)
  ✓ Compression handling (Dictionary, Temporal, Semantic, Bit-packed)
  ✓ Security validation with replay protection
  ✓ OTA firmware updates with chunking
  ✓ Command execution queue
  ✓ Fault injection testing
  ✓ Diagnostics tracking
======================================================================
Server starting... Press Ctrl+C to stop
======================================================================
```

---

### Terminal 2: Wokwi Simulator
```bash
cd /home/akitha/Desktop/EcoWatt_TeamPowerPort/PIO/ECOWATT
just run-wokwi
```

**OR** (direct command):
```bash
cd /home/akitha/Desktop/EcoWatt_TeamPowerPort/PIO/ECOWATT
~/.platformio/penv/bin/platformio run -e wokwi -t wokwi
```

**Expected Output**:
```
=== OTA Manager Initialization ===
Device ID: ESP32_EcoWatt_Smart
Current Version: 1.0.3
Server URL: http://localhost:5001
OTA Manager initialized successfully
=====================================

Connecting to WiFi: Wokwi-GUEST
WiFi connected!
IP Address: 192.168.1.100
```

---

### Terminal 3: Test FOTA Fault Injection

#### Test 1: Check Flask Server Health
```bash
curl http://localhost:5001/health
```

**Expected**:
```json
{
  "status": "healthy",
  "timestamp": "2025-10-22T..."
}
```

#### Test 2: Check OTA Status
```bash
curl http://localhost:5001/ota/status
```

**Expected**:
```json
{
  "success": true,
  "active_sessions": 0,
  "statistics": {
    "total_sessions": 0,
    "completed_updates": 0,
    "failed_updates": 0
  }
}
```

#### Test 3: Enable Fault Injection - Corrupt Chunk
```bash
curl -X POST http://localhost:5001/ota/test/enable \
  -H "Content-Type: application/json" \
  -d '{
    "fault_type": "corrupt_chunk",
    "target_device": "ESP32_EcoWatt_Smart",
    "target_chunk": 5
  }'
```

**Expected**:
```json
{
  "success": true,
  "message": "Fault injection enabled: corrupt_chunk",
  "fault_config": {
    "fault_type": "corrupt_chunk",
    "target_device": "ESP32_EcoWatt_Smart",
    "target_chunk": 5
  }
}
```

#### Test 4: Check Fault Injection Status
```bash
curl http://localhost:5001/ota/test/status
```

**Expected**:
```json
{
  "success": true,
  "fault_injection": {
    "enabled": true,
    "fault_type": "corrupt_chunk",
    "target_device": "ESP32_EcoWatt_Smart",
    "target_chunk": 5,
    "fault_count": 0
  }
}
```

#### Test 5: Disable Fault Injection
```bash
curl -X POST http://localhost:5001/ota/test/disable
```

**Expected**:
```json
{
  "success": true,
  "message": "Fault injection disabled (Faults injected: 0)"
}
```

---

## 🧪 FOTA Test Scenarios

### Scenario 1: Normal OTA Update (No Faults)
**Steps**:
1. Ensure fault injection is disabled
2. ESP32 checks for update: `GET /ota/check/ESP32_EcoWatt_Smart?version=1.0.3`
3. Server responds with latest version (1.0.4)
4. ESP32 initiates OTA: `POST /ota/initiate/ESP32_EcoWatt_Smart`
5. ESP32 downloads chunks: `GET /ota/chunk/ESP32_EcoWatt_Smart?chunk=N`
6. ESP32 completes OTA: `POST /ota/complete/ESP32_EcoWatt_Smart`

**Expected Result**: ✅ Update successful, ESP32 reboots with v1.0.4

---

### Scenario 2: Corrupted Chunk Test
**Steps**:
1. Enable fault injection:
   ```bash
   curl -X POST http://localhost:5001/ota/test/enable \
     -H "Content-Type: application/json" \
     -d '{"fault_type": "corrupt_chunk", "target_chunk": 5}'
   ```
2. ESP32 initiates OTA update
3. ESP32 downloads chunk #5
4. Server corrupts chunk #5 data
5. ESP32 verifies HMAC

**Expected Result**: 
- ❌ HMAC verification fails on chunk #5
- ⚠️ ESP32 logs: "HMAC verification failed for chunk 5"
- 🔄 ESP32 retries chunk #5 download
- ✅ After disabling fault injection, download succeeds

**Monitor in Flask logs**:
```
⚠️  Injecting chunk corruption (fault #1)
```

---

### Scenario 3: Bad Hash Test
**Steps**:
1. Enable fault injection:
   ```bash
   curl -X POST http://localhost:5001/ota/test/enable \
     -H "Content-Type: application/json" \
     -d '{"fault_type": "bad_hash"}'
   ```
2. ESP32 downloads all chunks successfully
3. ESP32 verifies firmware hash
4. Server provides incorrect hash

**Expected Result**:
- ❌ Hash verification fails
- ⚠️ ESP32 logs: "Firmware hash mismatch!"
- 🛑 Update cancelled, firmware rejected
- 📊 otaFailureCount++

---

### Scenario 4: Network Timeout Test
**Steps**:
1. Enable fault injection:
   ```bash
   curl -X POST http://localhost:5001/ota/test/enable \
     -H "Content-Type: application/json" \
     -d '{"fault_type": "timeout"}'
   ```
2. ESP32 initiates OTA
3. Server delays response > OTA_TIMEOUT_MS (30 seconds)

**Expected Result**:
- ⏱️ ESP32 detects timeout
- ⚠️ ESP32 logs: "OTA timeout detected"
- 💾 Progress saved to NVS
- 🔄 Can resume later

---

### Scenario 5: Rollback Test
**Prerequisites**: 
- ESP32 running known-good firmware v1.0.3
- Bad firmware v1.0.4 prepared

**Steps**:
1. Enable fault injection to make v1.0.4 fail hash check
2. ESP32 attempts OTA to v1.0.4
3. Update fails
4. ESP32 detects boot failure

**Expected Result**:
- ❌ OTA fails
- 🔄 ESP32 rolls back to v1.0.3
- ✅ ESP32 boots successfully on v1.0.3
- 📊 otaRollbackCount++

---

## 📊 Monitoring & Verification

### Check OTA Statistics
```bash
curl http://localhost:5001/ota/stats
```

**Expected**:
```json
{
  "success": true,
  "statistics": {
    "total_sessions": 5,
    "completed_updates": 1,
    "failed_updates": 3,
    "active_sessions": 0,
    "total_bytes_transferred": 1021845
  }
}
```

### Check Fault Injection Stats
```bash
curl http://localhost:5001/ota/test/status
```

### Monitor ESP32 Serial Output
Watch Wokwi Terminal for:
- ✅ `OTA Manager initialized successfully`
- ✅ `WiFi connected!`
- ⚠️ `HMAC verification failed` (during fault injection)
- ✅ `Firmware verified successfully`
- ✅ `OTA update completed successfully`

---

## 🔧 Troubleshooting

### Issue: Flask server won't start
**Solution**: Check if port 5001 is in use
```bash
lsof -i :5001
kill -9 <PID>
```

### Issue: Wokwi can't connect to Flask
**Solution**: Check wokwi.toml port forwarding
```toml
[[net.forward]]
from = "host"
to = "esp32"
port = 5001
```

### Issue: ESP32 WiFi connection fails
**Solution**: Check credentials.h
```cpp
#define WIFI_SSID "Wokwi-GUEST"
#define WIFI_PASSWORD ""
```

### Issue: OTA always fails
**Solution**: Disable fault injection first
```bash
curl -X POST http://localhost:5001/ota/test/disable
```

---

## ✅ Success Criteria

### Phase 4 Testing Complete When:
- [x] ESP32 firmware builds successfully
- [ ] Flask server starts without errors
- [ ] Wokwi connects to WiFi successfully
- [ ] ESP32 can reach Flask server
- [ ] Normal OTA update completes successfully
- [ ] Fault injection corrupts chunk successfully
- [ ] ESP32 rejects corrupted chunk
- [ ] Bad hash is rejected
- [ ] Timeout is handled gracefully
- [ ] Rollback mechanism works
- [ ] All statistics are tracked correctly

---

## 📝 Test Results Template

```
Test Date: 2025-10-22
Tester: Team PowerPort

┌─────────────────────────────────────────────────────────┐
│ Test Scenario                  │ Result │ Notes         │
├─────────────────────────────────────────────────────────┤
│ Normal OTA Update              │ [ ]    │               │
│ Corrupted Chunk Rejection      │ [ ]    │               │
│ Bad Hash Rejection             │ [ ]    │               │
│ Network Timeout Handling       │ [ ]    │               │
│ Incomplete Download Recovery   │ [ ]    │               │
│ Rollback Mechanism             │ [ ]    │               │
│ Statistics Accuracy            │ [ ]    │               │
└─────────────────────────────────────────────────────────┘

Overall Phase 4 Status: [ ] PASS / [ ] FAIL

Notes:
_____________________________________________________________
_____________________________________________________________
```

---

## 🚀 Quick Start Commands

**Terminal 1 - Flask Server**:
```bash
cd /home/akitha/Desktop/EcoWatt_TeamPowerPort/flask && python3 flask_server_modular.py
```

**Terminal 2 - Wokwi**:
```bash
cd /home/akitha/Desktop/EcoWatt_TeamPowerPort/PIO/ECOWATT && just run-wokwi
```

**Terminal 3 - Test Commands**:
```bash
# Health check
curl http://localhost:5001/health

# Enable fault injection
curl -X POST http://localhost:5001/ota/test/enable \
  -H "Content-Type: application/json" \
  -d '{"fault_type": "corrupt_chunk"}'

# Check status
curl http://localhost:5001/ota/test/status

# Disable fault injection
curl -X POST http://localhost:5001/ota/test/disable
```

---

**Status**: ✅ READY FOR TESTING
