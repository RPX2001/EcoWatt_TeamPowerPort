# Milestone 5: Fault Recovery, Power Optimization & Final Integration - TODO List

## Status Legend
- ✅ = Completed
- 🔄 = In Progress  
- ⏳ = Not Started
- ❌ = Blocked/Issues

---

## Part 1: Power Management and Measurement ⏳

### Power Optimization Implementation
- [ ] ⏳ Implement Light CPU Idle (use `delay()` in polling loops)
- [ ] ⏳ Implement Dynamic Clock Scaling (160 MHz ↔ 80 MHz)
- [ ] ⏳ Implement Light Sleep Mode (`wifi_set_sleep_type(LIGHT_SLEEP_T)`)
- [ ] ⏳ Implement Peripheral Gating (power down WiFi, UART, ADC when idle)
- [ ] ⏳ Document which techniques are compatible/incompatible
- [ ] ⏳ Create power state management module

### Power Measurement
- [ ] ⏳ Implement power reporting endpoint (average current, energy saved)
- [ ] ⏳ Add power metrics to device telemetry
- [ ] ⏳ Create methodology documentation (if hardware measurement not available)
- [ ] ⏳ Compare power consumption: optimized vs baseline

---

## Part 2: Fault Recovery ✅ (MOSTLY COMPLETE - NEEDS CLEANUP)

### Inverter SIM Fault Injection ⏳ (NEEDS ALIGNMENT WITH MILESTONE 5)

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
