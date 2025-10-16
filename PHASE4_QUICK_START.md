# Phase 4 - Quick Start Guide

## 🚀 Get Everything Running in 5 Minutes

### Step 1: Start Flask Server (1 min)

```bash
cd /Users/prabathwijethilaka/Sem7/Embedded/EcoWatt_TeamPowerPort/flask
python flask_server_hivemq.py
```

**Expected Output:**
```
Starting EcoWatt Smart Selection Batch Server v3.1
MQTT client initialized successfully
OTA firmware manager initialized successfully
Flask server starting on http://0.0.0.0:5001
```

---

### Step 2: Flash ESP32 (2 min)

```bash
cd /Users/prabathwijethilaka/Sem7/Embedded/EcoWatt_TeamPowerPort/PIO/ECOWATT

# Build and flash
pio run -t upload

# Monitor output
pio device monitor
```

**Expected Output:**
```
Starting ECOWATT
WiFi connected
Security Manager initialized successfully
Current sequence number: 0
OTA Manager initialized successfully
```

---

### Step 3: Test Command Execution (1 min)

**Terminal 1** (Flask server running)

**Terminal 2** (ESP32 monitor running)

**Terminal 3:**
```bash
cd flask
python test_command_queue.py
```

**What to Watch:**
- Commands queued on Flask
- ESP32 retrieves at next upload cycle
- Commands execute (setPower)
- Results reported back
- History logged

---

### Step 4: Test Security (1 min)

```bash
cd flask
python test_security.py
```

**Expected Results:**
- ✓ Valid requests pass
- ✗ Wrong HMAC rejected
- ✗ Replay attacks blocked
- Stats displayed

---

## 🧪 Quick Tests

### Test 1: Queue a Command
```bash
curl -X POST http://10.78.228.2:5001/command/queue \
  -H "Content-Type: application/json" \
  -d '{"device_id":"ESP32_EcoWatt_Smart","command_type":"set_power","parameters":{"power_value":75}}'
```

### Test 2: Check Command History
```bash
curl "http://10.78.228.2:5001/command/history?device_id=ESP32_EcoWatt_Smart"
```

### Test 3: View Security Stats
```bash
curl http://10.78.228.2:5001/security/stats
```

### Test 4: Check Security Log
```bash
curl "http://10.78.228.2:5001/security/log?device_id=ESP32_EcoWatt_Smart&limit=10"
```

---

## 📊 Monitor Everything

### ESP32 Serial Output
Look for:
- `✓ Security validated`
- `Found N pending commands`
- `Executing command: CMD_...`
- `Command result reported successfully`

### Flask Server Logs
Look for:
- `✓ Security validated: Device=... Seq=...`
- `Command queued: CMD_...`
- `Command CMD_... - Status: completed`

---

## 🐛 Troubleshooting

### Commands Not Executing?
```bash
# Check ESP32 WiFi connection
# Check Flask server is running
# Verify URLs match in main.cpp
```

### Security Failing?
```bash
# Reset sequence numbers:
curl -X POST http://10.78.228.2:5001/security/reset \
  -H "Content-Type: application/json" \
  -d '{"device_id":"ESP32_EcoWatt_Smart"}'

# Reset ESP32 (reboot)
```

### ESP32 Not Connecting?
```bash
# Check credentials.h:
# - WIFI_SSID
# - WIFI_PASSWORD
# - Server URLs must match Flask IP
```

---

## 📝 Key URLs to Update

In `main.cpp`:
```cpp
const char* dataPostURL = "http://YOUR_IP:5001/process";
const char* fetchChangesURL = "http://YOUR_IP:5001/changes";
```

In `OTAManager` setup:
```cpp
otaManager = new OTAManager(
    "http://YOUR_IP:5001",
    "ESP32_EcoWatt_Smart",
    FIRMWARE_VERSION
);
```

In command functions:
```cpp
const char* getCommandsURL = "http://YOUR_IP:5001/command/get";
const char* reportURL = "http://YOUR_IP:5001/command/report";
```

---

## ✅ Success Checklist

- [ ] Flask server running
- [ ] ESP32 connected to WiFi
- [ ] Security manager initialized
- [ ] Data uploading with HMAC
- [ ] Commands queued and executed
- [ ] Configuration updates working
- [ ] Logs show no errors

---

## 📖 Full Documentation

1. **PHASE4_COMPLETE_SUMMARY.md** - Complete overview
2. **COMMAND_EXECUTION_DOCUMENTATION.md** - Command details
3. **SECURITY_DOCUMENTATION.md** - Security implementation
4. **FOTA_DOCUMENTATION.md** - FOTA details

---

## 🎥 Demo Video Checklist

### Preparation:
- [ ] Flask server running
- [ ] ESP32 flashed and connected
- [ ] MQTT broker accessible
- [ ] test_command_queue.py ready
- [ ] test_security.py ready

### Recording Sections:
1. [ ] Intro - show project structure
2. [ ] Remote Config - demonstrate updates
3. [ ] Command Execution - queue and execute
4. [ ] Security - show HMAC validation
5. [ ] FOTA - show update process
6. [ ] Summary - recap features

**Target Duration:** 10 minutes max  
**Keep video on throughout demo** ✓

---

## 🎯 What's Working

✅ **Remote Configuration:** Runtime updates with validation  
✅ **Command Execution:** Bidirectional command flow  
✅ **Security Layer:** HMAC + anti-replay protection  
✅ **FOTA:** Code complete, needs testing  

---

## 🚧 What Needs Testing

⚠️ FOTA failure scenarios  
⚠️ Network interruption recovery  
⚠️ Rollback verification  

---

## 💡 Tips

1. **Start fresh:** Reset sequences if testing multiple times
2. **Check logs:** Both ESP32 and Flask provide detailed logs
3. **Use test scripts:** They save time and ensure consistency
4. **Monitor serial:** ESP32 output shows what's happening
5. **MQTT useful:** For config changes and monitoring

---

**Need Help?**
- Check documentation files
- Review test script output
- Monitor ESP32 serial logs
- Check Flask server console

**Ready to demo!** 🎉
