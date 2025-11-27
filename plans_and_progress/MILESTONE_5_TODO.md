# Milestone 5: Fault Recovery, Power Optimization & Final Integration - TODO List

## Status Legend
- ✅ = Completed
- 🔄 = In Progress  
- ⏳ = Not Started
- ❌ = Blocked/Issues

---

## Part 0: Logging Improvements 🔄 (IN PROGRESS)

### ESP32 Logging System
- [x] ✅ Create new logger.h/logger.cpp with NTP-based timestamps
  - Uses getLocalTime() for real-time timestamps (HH:MM:SS format)
  - Falls back to millis() if NTP unavailable
  - Log levels: DEBUG, INFO, WARN, ERROR, NONE
  - Module tags: BOOT, WIFI, UPLOAD, POWER, SECURITY, etc.
  - Success symbol: ✓, Error: ✗, Warning: [!]
- [x] ✅ Updated system_initializer.cpp with new logging
- [x] ✅ Updated data_uploader.cpp with new logging
- [x] ✅ Updated power_management.cpp with new logging
- [x] ✅ Updated command_executor.cpp with new logging
- [x] ✅ Updated peripheral_power.cpp with new logging
- [ ] 🔄 Update nvs.cpp with new logging
- [ ] 🔄 Update OTAManager.cpp with new logging
- [ ] 🔄 Update compression files with new logging
- [ ] 🔄 Update security.cpp with new logging (handle ArduinoJson print conflicts)

### Flask Logging System
- [x] ✅ Enhanced logger_utils.py with ColoredFormatter and FileFormatter
  - ANSI color codes for console output
  - Plain text for file logs
  - Request ID tracking with contextvars
  - log_success() helper function
  - Consistent timestamp format (HH:MM:SS)
- [x] ✅ Updated flask_server_modular.py with new logging
- [x] ✅ Updated diagnostics_handler.py with new logging
- [x] ✅ Updated compression_handler.py with new logging
- [ ] 🔄 Update database.py with new logging
- [ ] 🔄 Update remaining handlers with new logging
- [ ] 🔄 Update routes with new logging

### Compression System Fixes (Nov 26, 2025)
- [x] ✅ **CRITICAL BUG FIXED**: ESP32 compression marker bytes missing
  - **Issue**: `storeAsRawBinary()` was skipping header for small datasets (≤8 values)
  - **Impact**: Server received data without compression markers (0x48 instead of 0x00)
  - **Fix**: Eliminated raw binary mode entirely - always use bit-packing with marker
  - **Files Modified**: 
    - `PIO/ECOWATT/src/application/compression.cpp` - Removed storeAsRawBinary(), forced bit-packing
    - `PIO/ECOWATT/include/application/compression.h` - Removed storeAsRawBinary() declaration
    - `flask/utils/compression_utils.py` - Removed decompress_raw_binary(), deprecated 0x00 marker
- [x] ✅ Verified compression/decompression compatibility
  - All Python test scripts pass (dictionary, temporal, bit-packing)
  - Markers match between ESP32 (0xD0, 0x70, 0x71, 0x01) and Flask
  - Bit ordering confirmed (MSB-first for bit-packing, LSB-first for dictionary bitmask)
- [x] ✅ Enforced compression policy
  - ESP32 ALWAYS compresses with proper markers (minimum 8-bit bit-packing)
  - Raw binary (0x00 marker) is deprecated and rejected by server
  - Ensures consistent data format and proper decompression

---

## Part 1: Power Management and Measurement 🔄 (IN PROGRESS)

### Power Optimization Implementation
- [x] ✅ Implement Light CPU Idle (uses `delay()` which allows CPU idle states)
  - **Status:** COMPLETE - delay() permits CPU to enter idle between interrupts
  - **Note:** True esp_light_sleep_start() disabled due to watchdog timer conflicts
  - **Energy Savings:** ~40-50mA during sleep periods vs 200mA baseline
- [x] ✅ Implement Dynamic Clock Scaling (240/160/80 MHz)
  - **Status:** COMPLETE - Conditional scaling based on operation type
  - **240 MHz:** WiFi transmissions (HIGH_PERFORMANCE mode)
  - **160 MHz:** Modbus polling, data processing (NORMAL mode)
  - **80 MHz:** Idle/waiting (LOW_POWER mode, when freq scaling enabled)
  - **Energy Savings:** ~60mA savings in LOW mode with 80MHz vs 200mA baseline
- [x] ✅ Implement WiFi Modem Sleep (`WiFi.setSleep(WIFI_PS_MAX_MODEM)`)
  - **Status:** COMPLETE - WiFi sleeps between DTIM beacons
  - **Energy Savings:** ~30% reduction in WiFi current consumption
- [x] ✅ Implement Peripheral Gating (UART power control)
  - **Status:** COMPLETE - UART enabled only during Modbus polls
  - **Implementation:** PeripheralPower::enableUART() / disableUART()
  - **Tracking:** Duty cycle monitoring in power reports
- [x] ✅ Document technique compatibility
  - **WiFi Modem Sleep:** Compatible with all techniques
  - **CPU Freq Scaling:** Requires 240MHz for WiFi ops, 160MHz+ for stability
  - **Light CPU Idle:** delay() is WiFi-safe, avoids watchdog issues
  - **Peripheral Gating:** Compatible with all techniques
- [x] ✅ Create power state management module (power_management.cpp/h)

### Power Measurement
- [x] ✅ Implement power reporting endpoint POST /power/energy/<device_id>
- [x] ✅ Add power metrics to device telemetry (sent every 5 minutes)
- [ ] ⏳ Create methodology documentation (estimation vs hardware measurement)
- [ ] ⏳ Generate comparison report: optimized vs baseline power consumption
- [ ] ⏳ Add frontend screenshots showing Power Mode Distribution statistics

---

## Part 2: Fault Recovery ✅ (COMPLETE)

### Device Auto-Registration on Server Reconnection ✅ (Nov 27, 2025)
- [x] ✅ **SERVER-SIDE SOLUTION**: Automatic device re-registration
  - **Problem**: ESP32 only registered on ESP32 restart, not when server restarted
  - **Solution**: Server-side auto-registration when device communicates
  - **Implementation**:
    - Created `ensure_device_registered()` helper function in `device_routes.py`
    - Auto-registers devices on ANY communication with server (not just initial registration)
    - Updates last_seen timestamp on every data upload
    - Updates firmware version if provided
    - Persists to SQLite database (survives server restarts)
  - **Files Modified**:
    - `flask/routes/device_routes.py` - Added ensure_device_registered() helper
    - `flask/routes/aggregation_routes.py` - Uses helper on data upload
  - **Behavior**:
    1. Device sends data to `/aggregated/<device_id>` endpoint
    2. Server checks if device exists in database
    3. If not exists: auto-register with device_id, location="Auto-registered"
    4. If exists: update last_seen timestamp and firmware version
    5. Process data normally
  - **Benefits**:
    - Works even after server restarts (database persists)
    - No ESP32 code changes needed
    - Handles any device communication endpoint
    - Automatic firmware version tracking
  - **Testing**: Just restart server - devices re-register on next data upload

---

## Part 2: Fault Recovery ✅ (COMPLETE - Previous Items)

### Inverter SIM Fault Injection ✅ (COMPLETE - ALIGNED WITH MILESTONE 5)

**Backend Implementation (Flask):**
- [x] ✅ **Dual Backend Routing** - `fault_routes.py` routes faults to correct backend
  - Inverter SIM API for Modbus faults (EXCEPTION, CRC_ERROR, CORRUPT, PACKET_DROP, DELAY)
  - Local Flask for application faults (OTA, Network)
- [x] ✅ **Inverter SIM Integration** - Uses correct API endpoint
  - **Endpoint:** `POST http://20.15.114.131:8080/api/user/error-flag/add`
  - **Purpose:** Sets error flag for NEXT ESP32 Modbus request
  - **Payload:** `{errorType, exceptionCode, delayMs}` (no slaveAddress/functionCode needed)
- [x] ✅ **Exception Codes Supported** - All Milestone 5 exception codes
  - 01: Illegal Function
  - 02: Illegal Data Address
  - 03: Illegal Data Value
  - 04: Slave Device Failure
  - 05-0B: Other Modbus exceptions
- [x] ✅ **Fault Types Implemented:**
  - **EXCEPTION** - Valid Modbus exception frames with exception codes
  - **CRC_ERROR** - Malformed CRC frames (Milestone 5 requirement)
  - **CORRUPT** - Random byte garbage (Milestone 5 requirement)
  - **PACKET_DROP** - Dropped packets (no response)
  - **DELAY** - Response delays in milliseconds

### Local Fault Injection ✅ (COMPLETE - MILESTONE 5)

**OTA Fault Injection (`ota_handler.py`):**
- [x] ✅ **Partial Download Simulation**
  - `partial_download`: Interrupts download at configurable percentage
  - `network_interrupt`: Stops after specific chunk number
  - **Parameters:** `max_chunk_percent`, `interrupt_after_chunk`
- [x] ✅ **Hash/Signature Faults**
  - `bad_hash`: Incorrect SHA256 hash in manifest
  - `bad_signature`: Wrong signature in manifest
  - `hash_mismatch`: Final hash verification failure
- [x] ✅ **Chunk-Level Faults**
  - `corrupt_chunk`: Corrupts specific chunk data (flips random bits)
  - `incomplete`: Random chunk drops during download
  - **Parameters:** `target_chunk`, `drop_probability`
- [x] ✅ **Manifest Corruption**
  - `manifest_corrupt`: Invalid manifest data (corrupted fields)
  - **Parameters:** `manifest_field` (which field to corrupt)
- [x] ✅ **Network Delays**
  - `timeout`: Delays chunk delivery to simulate slow network
  - **Parameters:** `delay_ms`

**Network Fault Injection (`fault_handler.py`):**
- [x] ✅ **Connection Faults**
  - `timeout`: Connection timeout (delay then 504 error)
  - `disconnect`: Connection drop (immediate 503 error)
  - **Parameters:** `timeout_ms` (default: 30000ms)
- [x] ✅ **Performance Faults**
  - `slow`: Slow network speed (adds delay to responses)
  - `intermittent`: Random intermittent failures
  - **Parameters:** `delay_ms`, `failure_rate` (0.0-1.0)
- [x] ✅ **Endpoint Targeting**
  - Can target specific endpoints (e.g., `/power/upload`, `/ota/chunk`)
  - Configurable probability (0-100%)
  - **Parameters:** `target_endpoint`, `probability`

**API Endpoints:**
- [x] ✅ `POST /fault/inject` - Inject fault (routes to correct backend)
- [x] ✅ `GET /fault/types` - Get available fault types with examples
- [x] ✅ `GET /fault/status` - Get active faults status
- [x] ✅ `POST /fault/clear` - Clear all or specific faults
- [x] ✅ `GET /fault/history` - Get fault injection history

### ESP32 Fault Detection ⏳ (NEEDS IMPLEMENTATION)

According to Milestone 5 Resources, the Inverter SIM API should support:
- [ ] ⏳ **Malformed CRC frames** - Trigger via API endpoint
- [ ] ⏳ **Truncated payloads** - Trigger via API endpoint  
- [ ] ⏳ **Buffer overflow triggers** - Trigger via API endpoint
- [ ] ⏳ **Random byte garbage** - Trigger via API endpoint

**Actions Required:**
- [ ] 🔄 Review Inverter SIM API documentation for fault injection endpoints
- [ ] 🔄 Implement fault triggers in frontend UI (integrate with existing fault injection page)
- [ ] 🔄 Remove local Flask fault injection endpoints (not needed per milestone requirements)
- [ ] 🔄 Update ESP32 to handle Inverter SIM faults gracefully
- [ ] 🔄 Add Inverter SIM fault recovery logging to database

### Network Fault Recovery ✅ (COMPLETE)
- [x] ✅ HTTP timeout handling
- [x] ✅ Retry logic with exponential backoff
- [x] ✅ Connection failure recovery
- [x] ✅ Invalid response handling

### ESP32 Fault Recovery ✅ (COMPLETE)
- [x] ✅ Protocol adapter recovery
- [x] ✅ WiFi reconnection
- [x] ✅ NTP sync failure recovery
- [x] ✅ Fault reporting to Flask backend
- [x] ✅ Recovery timestamps (NTP-based)

### Backend Cleanup Required ✅ (MQTT REMOVED)
- [x] ✅ **Remove MQTT utils** - Deleted mqtt_utils.py, removed from __init__.py
- [x] ✅ **Remove MQTT from Flask server** - Removed initialization and config
- [x] ✅ **Remove MQTT from routes** - Removed publish_mqtt calls from aggregation_routes.py
- [x] ✅ **Remove MQTT dependency** - Removed paho-mqtt from requirements.txt
- [ ] 🔄 **Simplify fault injection** - Keep only necessary fault types:
  - Network errors (timeout, connection failure)
  - Inverter SIM faults (via API trigger, not local simulation)
  - Security failures (HMAC mismatch, nonce replay)
- [ ] 🔄 **Remove deprecated endpoints**:
  - Local fault injection triggers (replace with Inverter SIM API calls)
  - Unused diagnostic routes

---

## Part 3: Final Integration and Fault Testing ⏳

### Integration Checklist (From Milestone 5 Resources)
- [ ] ⏳ Data acquisition and buffering
- [ ] ⏳ Secure transmission  
- [ ] ⏳ Remote configuration
- [ ] ⏳ Command execution
- [ ] ⏳ FOTA update (success)
- [ ] ⏳ FOTA update (failure + rollback)
- [ ] ⏳ Power optimization comparison
- [ ] ⏳ Fault injection (network error)
- [ ] ⏳ Fault injection (Inverter SIM)

### Testing Requirements
- [ ] ⏳ End-to-end test with all features enabled
- [ ] ⏳ Fault injection test scenarios documented
- [ ] ⏳ Recovery time measurements
- [ ] ⏳ Power consumption measurements
- [ ] ⏳ FOTA rollback demonstration

---

## Part 4: Frontend UI Improvements ✅ (MOSTLY COMPLETE)

### Layout & Consistency Issues
- [x] ✅ **Fix tab width inconsistency** - Set minWidth: 800px on main content container in App.jsx
  - Solution: All tabs now have consistent minimum width

- [x] ✅ **Fix Configuration tab layout**
  - Solution: Redesigned ConfigForm.jsx with proper Box sections instead of Grid-only layout
  - Added emoji section headers (⏱️ Timing, 🗜️ Data Processing, 📊 Modbus, ⚡ Power)
  - Organized sections with better spacing and dividers
  - Improved power saving techniques layout with Grid
  - Better button alignment (right-aligned with proper spacing)

- [x] ✅ **Fix footer positioning**
  - Footer already uses mt: 'auto'
  - Main container uses minHeight: calc(100vh - 64px)
  - Footer stays at bottom consistently

- [ ] ⏳ **Overall UI polish**:
  - Consistent spacing between sections
  - Consistent button styles
  - Consistent card/panel styles
  - Loading states for all async operations

### MQTT Removal from Frontend
- [x] ✅ Remove MQTT from fault injection dropdown (FaultInjection.jsx)
- [x] ✅ Remove mqtt_disconnect preset from faults.js
- [x] ✅ Update fault type comment (network|command|ota only)

### Fault Injection UI Integration
- [ ] 🔄 Add Inverter SIM fault injection controls
  - Malformed CRC trigger button
  - Truncated payload trigger button
  - Buffer overflow trigger button
  - Random garbage trigger button
- [ ] 🔄 Remove local fault injection controls (deprecated)
- [ ] 🔄 Show fault injection history from Inverter SIM

---

## Compression Fix (COMPLETED) ✅

- [x] ✅ Fixed bit-packing header order mismatch
- [x] ✅ Fixed MSB-first vs LSB-first bit unpacking  
- [x] ✅ Verified decompression matches ESP32 values
- [x] ✅ Added support for 0x70/0x71 temporal markers (for future use)

---

## Deliverables (From Milestone 5 Requirements)

### Code & Documentation
- [ ] ⏳ Source code with power management implementation
- [ ] ⏳ Fault recovery documentation
- [ ] ⏳ Power measurement methodology document
- [ ] ⏳ Test scenarios and results

### Video Demonstration
- [ ] ⏳ Power optimization demo (before/after comparison)
- [ ] ⏳ Network fault injection + recovery
- [ ] ⏳ Inverter SIM fault injection + recovery
- [ ] ⏳ FOTA success demonstration
- [ ] ⏳ FOTA failure + rollback demonstration
- [ ] ⏳ Live demonstration of complete system

---

## Priority Order

### High Priority (Blocks Milestone Completion)
1. 🔄 Align Inverter SIM fault injection with milestone requirements
2. 🔄 Remove MQTT from backend and frontend
3. ⏳ Implement power management techniques
4. ⏳ Complete integration testing checklist

### Medium Priority (Quality Improvements)
5. 🔄 Fix frontend UI layout issues
6. 🔄 Clean up deprecated backend code
7. ⏳ Power measurement and documentation

### Low Priority (Nice to Have)
8. 🔄 UI polish and consistency improvements
9. ⏳ Additional test scenarios
10. ⏳ Performance optimizations

---

## Next Steps

1. **Review Inverter SIM API documentation** - Get exact fault injection endpoints
2. **Clean up MQTT code** - Remove from both backend and frontend
3. **Redesign fault injection page** - Focus on Inverter SIM + network faults only
4. **Fix frontend layout** - Tab width, config form, footer positioning
5. **Implement power management** - Start with light sleep and clock scaling
6. **Run integration tests** - Verify all milestone checklist items
