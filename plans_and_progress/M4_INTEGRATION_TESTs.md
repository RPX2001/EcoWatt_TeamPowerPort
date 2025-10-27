# M4 Integration Tests - README

## Overview

This directory contains the M4 integration tests that validate the complete end-to-end functionality of Milestone 4 features:

1. **Security Layer** - HMAC verification, anti-replay protection
2. **Command Execution** - Set power, write registers with security
3. **Remote Configuration** - Update device config over the air
4. **FOTA** - Firmware Over-The-Air updates

## Test Architecture

The M4 integration tests consist of two coordinated components:

### 1. Flask Server Test (`test_m4_flask_integration.py`)
- Location: `flask/tests/integration_tests/test_m4_integration/test_m4_flask_integration.py`
- Framework: pytest with Unity-style test organization
- Tests: 18 test cases covering all M4 features
- Can run standalone as pytest suite OR as integration server

### 2. ESP32 Device Test (`test_main.cpp`)
- Location: `PIO/ECOWATT/test/test_m4_integration/test_main.cpp`
- Framework: Unity Test Framework
- Tests: 10 test cases with real WiFi and HTTP communication
- Coordinates with Flask server for end-to-end validation

## Test Flow

```
┌─────────────────────┐          ┌─────────────────────┐
│  Flask Server       │          │  ESP32 Device       │
│  (localhost:5001)   │◄────────►│  (Real Hardware)    │
└─────────────────────┘   WiFi   └─────────────────────┘
         │                                  │
         │  1. Start Flask server           │
         │  2. Wait for ESP32               │
         │                                  │  3. Connect WiFi
         │                                  │  4. Run tests
         │  5. Receive secured uploads  ◄───┤
         │  6. Validate HMAC/nonce          │
         │  7. Queue commands           ────►  8. Poll commands
         │  9. Serve config updates     ────►  10. Retrieve config
         │  11. Provide FOTA info       ────►  12. Check updates
         │                                  │
         │  13. Log results                 │  14. Display results
         └──────────────────────────────────┘
```

## Running the Tests

### Option 1: Run as Pytest Suite (Flask Side Only)

```bash
cd flask
pytest tests/integration_tests/test_m4_integration/test_m4_flask_integration.py -v
```

**Expected Output:**
```
test_01_flask_server_health ........................ PASSED
test_02_security_layer_initialized ................. PASSED
test_03_secured_upload_valid_hmac .................. PASSED
test_04_secured_upload_invalid_hmac ................ PASSED
test_05_secured_upload_tampered_payload ............ PASSED
test_06_anti_replay_duplicate_nonce ................ PASSED
test_07_anti_replay_old_nonce ...................... PASSED
test_08_command_queue_set_power .................... PASSED
test_09_command_poll_pending ....................... PASSED
test_10_command_execution_result ................... PASSED
test_11_config_update_valid ........................ PASSED
test_12_config_retrieve ............................ PASSED
test_13_fota_check_update .......................... PASSED
test_14_fota_firmware_info ......................... PASSED
test_16_workflow_secured_upload_with_command ....... PASSED
test_17_workflow_config_then_fota .................. PASSED
test_18_security_statistics ........................ PASSED

=================== 18 passed in 2.34s ===================
```

### Option 2: Run Full Integration (Flask + ESP32)

#### Step 1: Start Flask Integration Server

```bash
cd flask
python tests/integration_tests/test_m4_integration/test_m4_flask_integration.py
```

**Expected Output:**
```
======================================================================
               M4 INTEGRATION TEST - FLASK SERVER
======================================================================
Server: http://0.0.0.0:5001
Device ID: ESP32_EcoWatt_Smart
Firmware Version: 1.0.4
======================================================================

Test Categories:
  1. Security - HMAC verification & anti-replay
  2. Command Execution - Set power, write registers
  3. Remote Configuration - Update device config
  4. FOTA - Firmware download & update

Waiting for ESP32 to connect...
======================================================================

 * Running on http://0.0.0.0:5001
```

#### Step 2: Run ESP32 Integration Test

```bash
cd PIO/ECOWATT
pio test -e esp32dev -f test_m4_integration
```

**Expected Output:**
```
======================================================================
               M4 INTEGRATION TEST - ESP32 SIDE
======================================================================
Device ID: ESP32_EcoWatt_Smart
Firmware: v1.0.4
WiFi: Galaxy A32B46A
Server: http://192.168.194.249:5001
======================================================================

Test Categories:
  1. Connectivity - WiFi & Server
  2. Security - HMAC & Anti-Replay
  3. Commands - Power & Registers
  4. Configuration - Remote Updates
  5. FOTA - Firmware Updates
======================================================================

[TEST 1] WiFi Connection
========================================
[WiFi] Connecting to: Galaxy A32B46A
...
[WiFi] Connected!
[WiFi] IP: 192.168.194.123
[PASS] WiFi connected successfully

[TEST 2] Flask Server Health
========================================
[PASS] Server is healthy

[TEST 3] Secured Upload - Valid HMAC
========================================
[PASS] Valid HMAC accepted

... (10 tests run)

======================================================================
                        TEST RESULTS
======================================================================
Total Tests:  10
Passed:       10
Failed:       0
Pass Rate:    100.0%
======================================================================
```

## Configuration

### WiFi Settings

Update in `PIO/ECOWATT/include/config/test_config.h`:

```cpp
#define WIFI_SSID "Galaxy A32B46A"
#define WIFI_PASSWORD "aubz5724"
#define FLASK_SERVER_IP "192.168.194.249"  // Your local IP
#define FLASK_SERVER_PORT 5001
```

### Security Settings

Both Flask and ESP32 use matching PSK:

```python
# Flask: tests/integration_tests/test_m4_integration/test_m4_flask_integration.py
PSK_KEY = "EcoWattSecureKey2024"
```

```cpp
// ESP32: test/test_m4_integration/test_main.cpp
#define M4_PSK_HMAC "EcoWattSecureKey2024"
```

## Test Categories

### 1. Server Health & Setup
- `test_01_flask_server_health` - Server responds correctly
- `test_02_security_layer_initialized` - Security layer ready

### 2. Security - HMAC Verification
- `test_03_secured_upload_valid_hmac` - Valid HMAC accepted
- `test_04_secured_upload_invalid_hmac` - Invalid HMAC rejected
- `test_05_secured_upload_tampered_payload` - Tampered data detected

### 3. Security - Anti-Replay Protection
- `test_06_anti_replay_duplicate_nonce` - Duplicate nonce rejected
- `test_07_anti_replay_old_nonce` - Old nonce rejected

### 4. Command Execution
- `test_08_command_queue_set_power` - Queue power command
- `test_09_command_poll_pending` - Poll for pending commands
- `test_10_command_execution_result` - Report execution result

### 5. Remote Configuration
- `test_11_config_update_valid` - Update device config
- `test_12_config_retrieve` - Retrieve current config

### 6. FOTA (Firmware Over-The-Air)
- `test_13_fota_check_update` - Check for updates
- `test_14_fota_firmware_info` - Get firmware metadata
- `test_15_fota_download_firmware` - Download binary (skipped - needs firmware)

### 7. Combined Workflows
- `test_16_workflow_secured_upload_with_command` - Upload → Queue → Poll
- `test_17_workflow_config_then_fota` - Config update → FOTA check

### 8. Statistics and Monitoring
- `test_18_security_statistics` - Security metrics tracking

## Troubleshooting

### WiFi Connection Issues

**Problem:** ESP32 can't connect to WiFi
**Solution:**
1. Check SSID/password in `test_config.h`
2. Verify WiFi is 2.4GHz (ESP32 doesn't support 5GHz)
3. Check signal strength

### Server Connection Issues

**Problem:** ESP32 can't reach Flask server
**Solution:**
1. Verify Flask server is running: `curl http://localhost:5001/health`
2. Check firewall allows port 5001
3. Update `FLASK_SERVER_IP` with correct local IP: `ip addr show` (Linux) or `ipconfig` (Windows)
4. Ensure ESP32 and computer are on same network

### HMAC Validation Failures

**Problem:** All secured uploads rejected
**Solution:**
1. Verify PSK_KEY matches on both sides
2. Check nonce generation (should be sequential)
3. Ensure message format: `payload + nonce + device_id`

### No Commands Received

**Problem:** ESP32 polls but gets no commands
**Solution:**
1. Queue a command first using `queue_command_for_esp32.py`
2. Check device_id matches: `ESP32_EcoWatt_Smart`
3. Verify Flask command handler is working

## Manual Testing

### Queue a Command

```bash
cd flask
python scripts/queue_command_for_esp32.py set_power 5000
```

### Monitor MQTT Messages

```bash
cd flask
python scripts/mqtt_subscriber.py --device ESP32_EcoWatt_Smart
```

### Check Server Health

```bash
curl http://localhost:5001/health
```

### Check Security Stats

```bash
curl http://localhost:5001/security/stats
```

## Success Criteria

✅ All tests pass (Flask: 18/18, ESP32: 10/10)
✅ Security validation works (HMAC verified, replays blocked)
✅ Commands queued and polled successfully
✅ Configuration updates applied
✅ FOTA check returns correct status
✅ Combined workflows function properly

## Next Steps

After integration tests pass:

1. **Generate Firmware** - Use `scripts/generate_keys.py` and `scripts/prepare_firmware.py`
2. **Test FOTA Download** - Enable `test_15_fota_download_firmware`
3. **Create Demo Video** - Record integration test execution
4. **Documentation** - Update project documentation with test results

## Support

For issues or questions:
- Check logs in Flask terminal
- Check ESP32 serial monitor output
- Review test code comments
- Refer to M3 integration tests for reference pattern
