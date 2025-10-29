# FOTA (Firmware Over-The-Air) Verification Plan

## Complete FOTA Flow Test

### Prerequisites
✅ Flask server running on port 5001
✅ Frontend running on port 5173
✅ ESP32 connected and running firmware v1.0.4
✅ WiFi connectivity established

---

## Test Scenarios

### Scenario 1: Complete FOTA Success Flow

**Steps:**

1. **Upload Firmware (Frontend)**
   - Navigate to FOTA page → Upload tab
   - Select firmware binary file (e.g., firmware_1.0.5.bin)
   - Enter version: `1.0.5`
   - Click Upload
   - **Expected**: Success message, firmware appears in Manage tab

2. **Verify Firmware Uploaded (Backend)**
   ```bash
   curl http://localhost:5001/ota/firmwares
   ```
   **Expected**: JSON response with firmware_1.0.5 in list

3. **Check ESP32 Current Version**
   - Monitor ESP32 serial output on boot
   - **Expected**: Shows "Current Version: 1.0.4"

4. **Trigger OTA Check on ESP32**
   - Wait for periodic check (60 minutes) OR
   - Manually trigger via command endpoint
   - **Expected**: ESP32 detects update available

5. **Monitor OTA Download (ESP32 Serial)**
   ```
   === CHECKING FOR FIRMWARE UPDATES ===
   Checking updates for device: ESP32_001
   Current version: 1.0.4
   ✓ Update available: v1.0.5
   
   === INITIATING OTA UPDATE ===
   Firmware version: 1.0.5
   Total chunks: 613
   
   === DOWNLOADING FIRMWARE ===
   Chunk 0/613 downloaded (2048 bytes)
   Chunk 1/613 downloaded (2048 bytes)
   ...
   Chunk 612/613 downloaded (1234 bytes)
   
   *** DOWNLOAD COMPLETED ***
   Installing firmware...
   ✓ OTA update completed successfully
   Rebooting in 2 seconds...
   ```

6. **ESP32 Reboots**
   - ESP32 boots with new firmware
   - **Expected**: Shows "Current Version: 1.0.5"

7. **ESP32 Reports Status**
   ```
   === REPORTING OTA COMPLETION STATUS TO FLASK ===
   POST /ota/ESP32_001/complete
   Payload: {"version":"1.0.5","status":"success","timestamp":1761760719}
   Response (200): {"success":true,...}
   ✓ OTA completion status reported successfully
   ```

8. **Verify on Frontend**
   - Navigate to FOTA → Progress tab
   - Check OTA status for ESP32_001
   - **Expected**: Shows "Status: success, Version: 1.0.5"

---

### Scenario 2: OTA Rollback Test

**Steps:**

1. **Upload Bad Firmware**
   - Upload corrupted firmware as v1.0.6
   - Initiate OTA

2. **ESP32 Downloads and Installs**
   - Firmware downloads successfully
   - ESP32 reboots

3. **Boot Fails → Automatic Rollback**
   ```
   Boot failed with new firmware
   Rolling back to previous partition...
   Rebooted with version: 1.0.5
   ```

4. **ESP32 Reports Rollback**
   ```
   POST /ota/ESP32_001/complete
   {"version":"1.0.5","status":"rolled_back","error_msg":"Firmware validation failed"}
   ```

5. **Verify on Frontend**
   - **Expected**: Status shows "rolled_back", device still running v1.0.5

---

### Scenario 3: OTA Resume After Network Failure

**Steps:**

1. **Start OTA Download**
   - ESP32 starts downloading chunks

2. **Simulate Network Failure**
   - Disconnect WiFi mid-download (e.g., at chunk 200/613)

3. **ESP32 Saves Progress**
   ```
   Network error during download
   Progress saved to NVS: chunk 200
   ```

4. **Reconnect WiFi**
   - ESP32 reconnects to WiFi

5. **Resume Download**
   ```
   === RESUMING OTA DOWNLOAD ===
   Resuming from chunk 200/613
   Chunk 200/613 downloaded...
   ...
   Chunk 612/613 downloaded
   Download completed
   ```

6. **Complete Installation**
   - ESP32 installs and reboots successfully

---

## API Endpoint Verification

### Frontend → Flask Endpoints

1. **Upload Firmware**
   ```bash
   curl -X POST http://localhost:5001/ota/upload \
     -F "file=@firmware_1.0.5.bin" \
     -F "version=1.0.5"
   ```
   **Expected**: HTTP 201, firmware uploaded

2. **List Firmwares**
   ```bash
   curl http://localhost:5001/ota/firmwares
   ```
   **Expected**: HTTP 200, array of firmware objects

3. **Get Firmware Manifest**
   ```bash
   curl http://localhost:5001/ota/firmware/1.0.5/manifest
   ```
   **Expected**: HTTP 200, manifest object with version, size, hash

4. **Delete Firmware**
   ```bash
   curl -X DELETE http://localhost:5001/ota/firmware/1.0.5
   ```
   **Expected**: HTTP 200, firmware deleted

5. **Get OTA Status**
   ```bash
   curl http://localhost:5001/ota/status/ESP32_001
   ```
   **Expected**: HTTP 200, current OTA status

6. **Get All OTA Status**
   ```bash
   curl http://localhost:5001/ota/status
   ```
   **Expected**: HTTP 200, status for all devices

7. **Get OTA Statistics**
   ```bash
   curl http://localhost:5001/ota/stats
   ```
   **Expected**: HTTP 200, statistics object

---

### ESP32 → Flask Endpoints

1. **Check for Update**
   ```bash
   curl "http://localhost:5001/ota/check/ESP32_001?version=1.0.4"
   ```
   **Expected**: HTTP 200, `{"update_available": true/false, ...}`

2. **Initiate OTA**
   ```bash
   curl -X POST http://localhost:5001/ota/initiate/ESP32_001 \
     -H "Content-Type: application/json" \
     -d '{"firmware_version":"1.0.5"}'
   ```
   **Expected**: HTTP 201, `{"session_id": "...", ...}`

3. **Get Firmware Chunk**
   ```bash
   curl "http://localhost:5001/ota/chunk/ESP32_001?version=1.0.5&chunk=0"
   ```
   **Expected**: HTTP 200, `{"chunk_data": "base64...", "chunk_index": 0, ...}`

4. **Report Completion**
   ```bash
   curl -X POST http://localhost:5001/ota/ESP32_001/complete \
     -H "Content-Type: application/json" \
     -d '{"version":"1.0.5","status":"success","timestamp":1234567890}'
   ```
   **Expected**: HTTP 200, `{"success": true, ...}`

---

## Checklist

### Backend (Flask)
- [x] `/ota/upload` - Upload firmware binary
- [x] `/ota/firmwares` - List available firmwares
- [x] `/ota/firmware/{version}/manifest` - Get firmware manifest
- [x] `/ota/firmware/{version}` - Delete firmware (DELETE)
- [x] `/ota/check/{device_id}` - Check for updates
- [x] `/ota/initiate/{device_id}` - Initiate OTA session
- [x] `/ota/chunk/{device_id}` - Get firmware chunk
- [x] `/ota/{device_id}/complete` - Receive OTA completion status
- [x] `/ota/status/{device_id}` - Get OTA status
- [x] `/ota/status` - Get all OTA status
- [x] `/ota/stats` - Get OTA statistics
- [x] `/ota/cancel/{device_id}` - Cancel OTA session

### Frontend (React)
- [x] `uploadFirmware(file, version)` - Upload firmware
- [x] `getFirmwareList()` - List firmwares
- [x] `getFirmwareManifest(version)` - Get manifest
- [x] `deleteFirmware(version)` - Delete firmware
- [x] `checkForUpdate(deviceId, version)` - Check updates
- [x] `initiateOTA(deviceId, version)` - Initiate OTA
- [x] `getOTAStatus(deviceId)` - Get device OTA status
- [x] `getAllOTAStatus()` - Get all devices OTA status
- [x] `getOTAStats()` - Get OTA statistics
- [x] `cancelOTA(deviceId)` - Cancel OTA

### ESP32 (C++)
- [x] `checkForUpdate()` - Check for firmware updates
- [x] `performOTA()` - Download and install firmware
- [x] `reportOTACompletionStatus()` - Report status after reboot
- [x] Resume capability (saves/loads progress from NVS)
- [x] Automatic rollback on boot failure

---

## Known Issues & Notes

1. **ESP32 Complete Endpoint Difference**
   - Frontend uses: `POST /ota/complete/{device_id}` (with session_id)
   - ESP32 uses: `POST /ota/{device_id}/complete` (with status report)
   - Both are valid, serve different purposes

2. **OTA Check Frequency**
   - Default: Every 60 minutes
   - Configurable via `/config/{device_id}` endpoint (`firmware_check_interval`)

3. **Chunk Size**
   - Fixed at 2048 bytes
   - Allows reliable download over unstable connections

4. **Security**
   - Firmware hash validation (SHA256)
   - Optional encryption support (AES)
   - Rollback protection

---

## Test Results

| Test Case | Status | Date | Notes |
|-----------|--------|------|-------|
| Upload firmware via frontend | ⏳ Pending | - | - |
| List firmwares | ⏳ Pending | - | - |
| ESP32 check for update | ⏳ Pending | - | - |
| ESP32 download firmware | ⏳ Pending | - | - |
| ESP32 install and reboot | ⏳ Pending | - | - |
| ESP32 report success | ⏳ Pending | - | - |
| Frontend monitor status | ⏳ Pending | - | - |
| Resume after network failure | ⏳ Pending | - | - |
| Automatic rollback | ⏳ Pending | - | - |

---

## Next Steps

1. Flash updated ESP32 firmware with config fix
2. Test firmware upload via frontend
3. Trigger OTA update on ESP32
4. Monitor complete flow
5. Document results
