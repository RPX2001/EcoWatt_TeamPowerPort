# Quick Start: Wokwi Testing Guide

## Current Status ✅
- **Flask Server**: Running on http://127.0.0.1:5001
- **ESP32 Firmware**: Built successfully (78% flash, 17.1% RAM)
- **Wokwi Configuration**: WiFi enabled, port forwarding configured

## Option 1: VS Code Wokwi Extension (Recommended)

### Step 1: Install Wokwi Extension
```bash
# In VS Code, press Ctrl+P and paste:
ext install wokwi.wokwi-vscode
```

### Step 2: Start Simulator
1. Open `/PIO/ECOWATT/` folder in VS Code
2. Press **F1** (Command Palette)
3. Type: **"Wokwi: Start Simulator"**
4. The simulator will start with your built firmware

### Step 3: Monitor Serial Output
- Serial output will appear in VS Code terminal
- Look for WiFi connection messages
- Monitor OTA update process

## Option 2: Wokwi CLI (If Installed)

```bash
cd /home/akitha/Desktop/EcoWatt_TeamPowerPort/PIO/ECOWATT

# Install Wokwi CLI (if not installed)
npm install -g wokwi-cli

# Run simulator
wokwi-cli --firmware .pio/build/wokwi/firmware.bin
```

## Option 3: Manual Testing (Without Simulator)

If you can't run Wokwi, you can still test the Flask endpoints:

### Test Fault Injection Endpoints

```bash
# Enable fault injection
curl -X POST http://127.0.0.1:5001/fault/enable \
  -H "Content-Type: application/json" \
  -d '{"fault_type": "corrupt_chunk", "enabled": true}'

# Check fault status
curl http://127.0.0.1:5001/fault/status

# Get available fault types
curl http://127.0.0.1:5001/fault/available

# Disable fault injection
curl -X POST http://127.0.0.1:5001/fault/disable
```

### Test OTA Endpoints

```bash
# Check OTA status
curl http://127.0.0.1:5001/ota/status

# Initiate OTA (mock device)
curl -X POST http://127.0.0.1:5001/ota/initiate/ESP32_TEST_001 \
  -H "Content-Type: application/json" \
  -d '{"current_version": "1.0.0", "hardware_version": "v1.0"}'

# Check for available updates
curl http://127.0.0.1:5001/ota/check/ESP32_TEST_001
```

## Expected Behavior

### 1. ESP32 Startup (in Wokwi)
```
[INFO] EcoWatt System Starting...
[INFO] Connecting to WiFi: Wokwi-GUEST
[INFO] WiFi connected! IP: 10.0.0.1
[INFO] Connecting to Flask server: http://localhost:5001
[INFO] OTA Manager initialized
```

### 2. Flask Server Logs
```
2025-10-22 21:15:00 - werkzeug - INFO - 10.0.0.1 - - "GET /health HTTP/1.1" 200
2025-10-22 21:15:05 - handlers.ota_handler - INFO - OTA check request from ESP32_TEST_001
```

### 3. OTA Update Flow
1. ESP32 checks for updates: `GET /ota/check/<device_id>`
2. Server responds with firmware info
3. ESP32 initiates update: `POST /ota/initiate/<device_id>`
4. ESP32 downloads chunks: `GET /ota/chunk/<device_id>`
5. ESP32 verifies and applies update
6. ESP32 reports completion: `POST /ota/complete/<device_id>`

## Troubleshooting

### Flask Server Not Responding
```bash
# Check if server is running
curl http://127.0.0.1:5001/health

# If not, restart:
cd /home/akitha/Desktop/EcoWatt_TeamPowerPort/flask
python3 flask_server_modular.py
```

### ESP32 Can't Connect to WiFi (in Wokwi)
- Verify `wokwi.toml` has `wifi = true`
- Check credentials in `include/application/credentials.h`:
  - SSID: "Wokwi-GUEST"
  - Password: "" (empty)
  - URL: "http://localhost:5001"

### Port Forwarding Issues
Check `wokwi.toml`:
```toml
[[net.forward]]
from = "host"
to = "esp32"
protocol = "tcp"
targetPort = 5001
```

## Test Scenarios

### Scenario 1: Normal OTA Update ✅
```bash
# No faults - should succeed
curl http://127.0.0.1:5001/ota/status
# Start Wokwi simulator
# Watch ESP32 download and apply update
```

### Scenario 2: Corrupted Chunk Test ⚠️
```bash
# Enable corrupt_chunk fault
curl -X POST http://127.0.0.1:5001/fault/enable \
  -H "Content-Type: application/json" \
  -d '{"fault_type": "corrupt_chunk", "enabled": true}'

# Start OTA - should detect corruption via HMAC
# ESP32 should retry chunk download
```

### Scenario 3: Network Timeout Test ⏱️
```bash
# Enable timeout fault
curl -X POST http://127.0.0.1:5001/fault/enable \
  -H "Content-Type: application/json" \
  -d '{"fault_type": "timeout", "enabled": true}'

# ESP32 should handle timeout gracefully
# Should resume from last successful chunk
```

### Scenario 4: Bad Hash Test ❌
```bash
# Enable bad_hash fault
curl -X POST http://127.0.0.1:5001/fault/enable \
  -H "Content-Type: application/json" \
  -d '{"fault_type": "bad_hash", "enabled": true}'

# ESP32 should reject firmware after download
# Should rollback to previous version
```

### Scenario 5: Incomplete Transfer Test 🔄
```bash
# Enable incomplete fault
curl -X POST http://127.0.0.1:5001/fault/enable \
  -H "Content-Type: application/json" \
  -d '{"fault_type": "incomplete", "enabled": true}'

# ESP32 should detect missing chunks
# Should not attempt to flash incomplete firmware
```

## Next Steps

1. ✅ Flask server running
2. ✅ ESP32 firmware built
3. ⏳ Start Wokwi simulator (use VS Code extension or CLI)
4. ⏳ Test basic connectivity (WiFi → Flask)
5. ⏳ Test OTA update flow (normal case)
6. ⏳ Test fault injection scenarios (5 scenarios)
7. ⏳ Document test results
8. ⏳ Update IMPROVEMENT_PLAN.md with Phase 4 completion

## Resources
- Wokwi VS Code Extension: https://marketplace.visualstudio.com/items?itemName=wokwi.wokwi-vscode
- Wokwi Documentation: https://docs.wokwi.com/vscode/getting-started
- Full Testing Guide: See `WOKWI_TESTING_GUIDE.md`
