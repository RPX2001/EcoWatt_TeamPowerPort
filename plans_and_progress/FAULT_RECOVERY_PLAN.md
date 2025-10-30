# Fault Injection and Recovery Implementation Plan

## Current Status Analysis

### What's Already Implemented

#### Flask Backend (Partial)
- **fault_handler.py**: Basic fault injection logic for Modbus responses (CRC corruption, truncation, garbage, timeout, exception)
- **fault_routes.py**: Dual backend routing (Inverter SIM API + Local faults)
- **fault_injector.py**: Core fault injection utilities
- **Inverter SIM Integration**: Routes to external API at `http://20.15.114.131:8080/api/inverter/error`

#### Front-end (Basic UI)
- **FaultInjection.jsx**: UI component with fault type selection, device selection, fault history

#### ESP32 (Gaps Identified)
- ❌ No structured fault recovery JSON reporting
- ❌ Limited detection for Milestone 5 fault types
- ⚠️ Has diagnostics.cpp but not integrated with fault recovery reporting

### What's Missing (Milestone 5 Requirements)

According to Project Outline Part 2: Fault Recovery:

**Required Fault Types** (from Milestone 5 Resources):
1. **Malformed CRC frames** - Inverter SIM fault
2. **Truncated payloads** - Inverter SIM fault  
3. **Buffer overflow triggers** - Inverter SIM fault
4. **Random byte garbage** - Inverter SIM fault

**Required Recovery Mechanism**:
- ESP32 must **detect** these faults
- ESP32 must **recover** (retry, reset connection, etc.)
- ESP32 must **send recovery JSON** to Flask server

**Sample Recovery Log Format** (from Project Outline image):
```json
{
  "device_id": "ESP32_EcoWatt_Smart",
  "timestamp": 1698527500,
  "fault_type": "crc_error",
  "recovery_action": "retry_read",
  "success": true,
  "details": "CRC validation failed, retried successfully after 1 attempt"
}
```

## Architecture Decision: Best Method for Recovery Reporting

### Option 1: Send Recovery JSON Immediately When Fault Occurs ✅ RECOMMENDED
**Pros:**
- Real-time visibility of faults
- Simpler logic (no buffering needed)
- Matches milestone requirements for "event log"
- Better for demonstration

**Cons:**
- More network requests
- Could fail if network is down during recovery

### Option 2: Periodic Batch Upload of All Recovery Events
**Pros:**
- Fewer network requests
- Works offline, uploads later

**Cons:**
- Delayed visibility
- More complex buffering logic
- Not aligned with "real-time" fault testing

**DECISION**: Use Option 1 - Send recovery JSON immediately via dedicated endpoint.

---

## Implementation Plan

### 1. ESP32 Fault Detection & Recovery

**File:** `PIO/ECOWATT/src/application/fault_recovery.cpp` (NEW)

```cpp
// Recovery JSON structure
struct FaultRecovery {
    String device_id;
    uint32_t timestamp;
    String fault_type;      // "crc_error", "truncated", "buffer_overflow", "garbage"
    String recovery_action; // "retry_read", "reset_connection", "discard_data"
    bool success;
    String details;
};

// Send recovery event to Flask
bool sendRecoveryEvent(const FaultRecovery& recovery) {
    String payload = createRecoveryJSON(recovery);
    return httpPostToFlask("/fault/recovery", payload);
}

// Detect and recover from faults
void handleModbusReadError(ModbusErrorType error) {
    FaultRecovery recovery;
    recovery.device_id = "ESP32_EcoWatt_Smart";
    recovery.timestamp = getCurrentTimestamp();
    
    switch(error) {
        case CRC_ERROR:
            recovery.fault_type = "crc_error";
            recovery.recovery_action = "retry_read";
            recovery.success = retryModbusRead();  // Attempt recovery
            break;
        case TRUNCATED_PAYLOAD:
            recovery.fault_type = "truncated";
            recovery.recovery_action = "request_full_payload";
            recovery.success = retryModbusRead();
            break;
        // ... handle other faults
    }
    
    sendRecoveryEvent(recovery);  // Send to Flask
}
```

**Integration Points:**
- Hook into existing Modbus read logic in sensor polling
- Add CRC validation checks
- Add payload length validation
- Add garbage data detection

### 2. Flask Recovery Endpoint

**File:** `flask/routes/fault_routes.py`

```python
# Storage for recovery events (in-memory for now, can be DB later)
recovery_events = []  # List of recovery dicts per device

@fault_bp.route('/fault/recovery', methods=['POST'])
def receive_recovery():
    """
    Receive fault recovery event from ESP32
    
    Request Body:
    {
      "device_id": "ESP32_EcoWatt_Smart",
      "timestamp": 1698527500,
      "fault_type": "crc_error",
      "recovery_action": "retry_read",
      "success": true,
      "details": "CRC validation failed, retried successfully after 1 attempt"
    }
    """
    try:
        data = request.get_json()
        
        # Validate required fields
        required = ['device_id', 'timestamp', 'fault_type', 'recovery_action', 'success']
        if not all(k in data for k in required):
            return jsonify({'success': False, 'error': 'Missing required fields'}), 400
        
        # Store recovery event
        recovery_events.append(data)
        
        # Update statistics
        fault_statistics['total_recovered'] += 1
        if data['success']:
            fault_statistics['successful_recoveries'] += 1
        
        logger.info(f"Recovery event from {data['device_id']}: {data['fault_type']} - {data['recovery_action']}")
        
        return jsonify({
            'success': True,
            'message': 'Recovery event recorded'
        }), 200
        
    except Exception as e:
        logger.error(f"Error receiving recovery event: {e}")
        return jsonify({'success': False, 'error': str(e)}), 500


@fault_bp.route('/fault/recovery/<device_id>', methods=['GET'])
def get_recovery_history(device_id):
    """Get recovery history for a device"""
    device_recoveries = [r for r in recovery_events if r['device_id'] == device_id]
    return jsonify({
        'device_id': device_id,
        'total_events': len(device_recoveries),
        'events': device_recoveries
    })
```

### 3. Front-end Enhancement

**File:** `front-end/src/components/testing/FaultInjection.jsx`

Add recovery event viewer:

```jsx
// State for recovery events
const [recoveryEvents, setRecoveryEvents] = useState([]);

// Poll for recovery events
useQuery({
  queryKey: ['recovery-events', deviceId],
  queryFn: () => getRecoveryHistory(deviceId),
  refetchInterval: 3000  // Refresh every 3 seconds
});

// Display recovery events
<Card>
  <CardHeader title="Recovery Events" />
  <CardContent>
    {recoveryEvents.map((event, idx) => (
      <Alert severity={event.success ? 'success' : 'error'} key={idx}>
        <Typography variant="body2">
          {new Date(event.timestamp * 1000).toLocaleString()}
        </Typography>
        <Typography variant="body1">
          <strong>{event.fault_type}</strong>: {event.recovery_action}
        </Typography>
        <Typography variant="body2">{event.details}</Typography>
      </Alert>
    ))}
  </CardContent>
</Card>
```

---

## Testing Workflow

### End-to-End Test Scenario

1. **User Action:** Front-end → Inject "CRC_ERROR" fault via Inverter SIM API
2. **Inverter SIM:** Returns corrupted CRC frame on next read
3. **ESP32 Detection:** Modbus read detects CRC mismatch
4. **ESP32 Recovery:** Retries read operation
5. **ESP32 Report:** Sends recovery JSON to Flask `/fault/recovery`
6. **Flask Storage:** Stores recovery event
7. **Front-end Display:** Recovery event appears in UI (auto-refresh)

### Success Criteria
- ✅ All 4 fault types can be injected
- ✅ ESP32 detects each fault type
- ✅ ESP32 recovers successfully (or fails gracefully)
- ✅ Recovery JSON sent to Flask
- ✅ Front-end displays recovery events
- ✅ Can view recovery history per device

---

## Files to Create/Modify

### ESP32
- [ ] **CREATE** `src/application/fault_recovery.cpp`
- [ ] **CREATE** `include/application/fault_recovery.h`
- [ ] **MODIFY** `src/core/modbus.cpp` - Add fault detection hooks
- [ ] **MODIFY** `src/application/sensor_poll.cpp` - Integrate fault recovery

### Flask
- [ ] **MODIFY** `routes/fault_routes.py` - Add recovery endpoints
- [ ] **MODIFY** `handlers/fault_handler.py` - Update statistics
- [ ] **CREATE** `models/fault_recovery.py` (optional DB model)

### Front-end
- [ ] **MODIFY** `components/testing/FaultInjection.jsx` - Add recovery viewer
- [ ] **CREATE** `api/faults.js` - Add recovery API calls

### Documentation
- [ ] **UPDATE** `plans_and_progress/FLASK_TESTS.md`
- [ ] **UPDATE** `plans_and_progress/ESP32_TESTS.md`
- [ ] **CREATE** `plans_and_progress/FAULT_RECOVERY.md`

---

## justfile Commands

```makefile
# Test fault injection
test-fault-crc:
    curl -X POST http://localhost:5000/fault/inject \
      -H "Content-Type: application/json" \
      -d '{"errorType": "CRC_ERROR", "slaveAddress": 1, "functionCode": 3}'

# View recovery events
view-recoveries:
    curl http://localhost:5000/fault/recovery/ESP32_EcoWatt_Smart | jq

# Clear all fault state
clear-faults:
    curl -X POST http://localhost:5000/fault/clear
```

---

## Implementation Progress

### Completed Tasks ✅
1. ✅ **Analyzed existing fault injection code** (fault_handler.py, fault_routes.py, fault_injector.py, FaultInjection.jsx)
2. ✅ **Designed recovery JSON format** (matches Milestone 5 requirements)
3. ✅ **Implemented Flask recovery endpoints** (POST /fault/recovery, GET /fault/recovery/<device_id>, GET /fault/recovery/all, POST /fault/recovery/clear)
4. ✅ **Created Flask justfile commands** (test-fault-crc, view-recoveries, test-fault-sequence, clear-recoveries)
5. ✅ **Implemented ESP32 fault_recovery module** (fault_recovery.h, fault_recovery.cpp with CRC validation, payload checks, garbage detection)
6. ✅ **Integrated fault recovery into acquisition.cpp** (Enhanced readRequest() with fault detection and recovery logic)
7. ✅ **Added fault recovery initialization** (Added to system_initializer.cpp boot sequence)
8. ✅ **Created PIO justfile commands** (test-m5, test-fault-recovery for ESP32 testing)

### In Progress 🔨
9. **Test ESP32 fault recovery compilation** - NEXT
   - Build firmware with `just build` from PIO/ECOWATT directory
   - Verify no compilation errors
   - Flash to device and monitor serial output

### Pending Tasks ⏳
10. **Enhance front-end UI** (FaultInjection.jsx)
   - Add recovery events viewer with auto-refresh
   - Display events as MUI Alerts (success/error)
   - Show statistics dashboard
   
11. **Test each fault type end-to-end**
   - UI inject → Inverter SIM corrupts → ESP32 detects → ESP32 recovers → Flask stores → UI displays
   - Verify all 4 fault types: CRC_ERROR, TRUNCATE, GARBAGE, BUFFER_OVERFLOW

12. **Documentation updates**
   - Update API Service Documentation with recovery endpoints
   - Add testing procedures to README

13. **Prepare demonstration video**
   - Show fault injection through UI
   - Show recovery events appearing in real-time
   - Highlight statistics dashboard

---

## Questions & Clarifications

**Q: Should recovery events be persisted to database?**
A: Start with in-memory storage, add DB later if needed for long-term logging.

**Q: What if ESP32 can't send recovery JSON due to network failure?**
A: Buffer locally, send when network recovers. Log to serial for debugging.

**Q: How many retries before giving up?**
A: Max 3 retries with exponential backoff (1s, 2s, 4s). Document in recovery JSON.

---

## Alignment with Milestone 5 Rubric

| Criterion | Implementation |
|-----------|----------------|
| End-to-end fault recovery (40%) | ✅ All 4 fault types covered |
| Code modularity (25%) | ✅ Separate fault_recovery module |
| Documentation quality (5%) | ✅ This plan + updated docs |
| Video demonstration (10%) | 🎥 Show fault injection → recovery in UI |

---

**Status:** Ready to implement
**Author:** EcoWatt Team  
**Date:** October 30, 2025
