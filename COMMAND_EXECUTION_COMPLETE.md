# Command Execution System - Complete Documentation

## Overview

The Command Execution system allows **EcoWatt Cloud** to send commands to **EcoWatt Device** (ESP32), which then executes them on the **Inverter SIM** and reports results back to the cloud.

## Architecture

```
┌──────────────────┐         ┌──────────────────┐         ┌──────────────────┐
│  EcoWatt Cloud   │         │  EcoWatt Device  │         │   Inverter SIM   │
│  (Flask Server)  │         │     (ESP32)      │         │   (Cloud API)    │
└──────────────────┘         └──────────────────┘         └──────────────────┘
        │                             │                             │
        │  1. Queue Command           │                             │
        │────────────────────────────>│                             │
        │                             │                             │
        │  2. Poll for Commands       │                             │
        │<───────────────────────────│                             │
        │                             │                             │
        │  3. Return Command          │                             │
        │────────────────────────────>│                             │
        │                             │                             │
        │                             │  4. Execute Command         │
        │                             │────────────────────────────>│
        │                             │                             │
        │                             │  5. Return Status           │
        │                             │<────────────────────────────│
        │                             │                             │
        │  6. Report Result           │                             │
        │<───────────────────────────│                             │
        │                             │                             │
```

## Workflow

### Step 1: Queue Command (Cloud)
User or automation queues a command via REST API.

**Endpoint**: `POST /command/queue`

**Request**:
```json
{
  "device_id": "ESP32_EcoWatt_Smart",
  "command_type": "set_power",
  "parameters": {
    "power_value": 5000
  }
}
```

**Response**:
```json
{
  "status": "success",
  "command_id": "CMD_1000_1729281234",
  "message": "Command set_power queued successfully"
}
```

### Step 2: Poll for Commands (ESP32 → Cloud)
At the next upload cycle, ESP32 polls for pending commands.

**Endpoint**: `POST /command/poll`

**Request**:
```json
{
  "device_id": "ESP32_EcoWatt_Smart"
}
```

**Response (command available)**:
```json
{
  "command": {
    "command_id": "CMD_1000_1729281234",
    "command_type": "set_power",
    "parameters": {
      "power_value": 5000
    }
  }
}
```

**Response (no commands)**:
```json
{
  "message": "No pending commands"
}
```

### Step 3: Execute Command (ESP32 → Inverter SIM)
ESP32 receives command, executes it on Inverter SIM.

**ESP32 Code** (already implemented in `main.cpp`):
```cpp
bool executeCommand(const char* commandId, const char* commandType, const char* parameters)
{
    if (strcmp(commandType, "set_power") == 0) {
        int powerValue = paramDoc["power_value"] | 0;
        bool result = setPower(powerValue);  // Calls Inverter SIM
        return result;
    }
    // ... other command types
}
```

### Step 4: Report Result (ESP32 → Cloud)
After execution, ESP32 reports the result back.

**Endpoint**: `POST /command/result`

**Request**:
```json
{
  "command_id": "CMD_1000_1729281234",
  "status": "completed",
  "result": "Command set_power: executed successfully"
}
```

**Response**:
```json
{
  "status": "success",
  "message": "Result recorded successfully",
  "command_id": "CMD_1000_1729281234"
}
```

---

## Flask Server Endpoints

### 1. Queue Command
**POST** `/command/queue`

Queues a new command for execution.

**Request Body**:
```json
{
  "device_id": "ESP32_EcoWatt_Smart",
  "command_type": "set_power" | "write_register",
  "parameters": { ... }
}
```

**Supported Commands**:

#### a) `set_power`
Sets the output power of the inverter.

```json
{
  "command_type": "set_power",
  "parameters": {
    "power_value": 5000  // Power in Watts
  }
}
```

#### b) `write_register`
Writes a value to a specific register.

```json
{
  "command_type": "write_register",
  "parameters": {
    "register_address": 40001,
    "value": 4500
  }
}
```

---

### 2. Poll Command
**POST** `/command/poll`

ESP32 calls this to get the next pending command.

**Request**:
```json
{
  "device_id": "ESP32_EcoWatt_Smart"
}
```

**Response**:
- Returns next queued command OR
- `{"message": "No pending commands"}`

---

### 3. Submit Result
**POST** `/command/result`

ESP32 submits command execution result.

**Request**:
```json
{
  "command_id": "CMD_1000_1729281234",
  "status": "completed" | "failed",
  "result": "Description of result or error"
}
```

---

### 4. Get Command Status
**GET** `/command/status/<command_id>`

Check status of a specific command.

**Example**: `GET /command/status/CMD_1000_1729281234`

**Response**:
```json
{
  "command_id": "CMD_1000_1729281234",
  "device_id": "ESP32_EcoWatt_Smart",
  "command_type": "set_power",
  "parameters": {"power_value": 5000},
  "status": "completed",
  "queued_time": "2025-10-18T14:30:00",
  "sent_time": "2025-10-18T14:31:00",
  "completed_time": "2025-10-18T14:31:05",
  "result": "Command executed successfully"
}
```

---

### 5. Get Command History
**GET** `/command/history?device_id=<id>&limit=<n>`

Get command history with optional filtering.

**Query Parameters**:
- `device_id`: Filter by device (optional)
- `limit`: Max results (default 50)

**Example**: `GET /command/history?device_id=ESP32_EcoWatt_Smart&limit=20`

**Response**:
```json
{
  "commands": [
    {
      "command_id": "CMD_1000_1729281234",
      "device_id": "ESP32_EcoWatt_Smart",
      "command_type": "set_power",
      "status": "completed",
      ...
    },
    ...
  ],
  "count": 15
}
```

---

### 6. Get Pending Commands
**GET** `/command/pending?device_id=<id>`

Get all pending (queued or sent but not completed) commands.

**Response**:
```json
{
  "pending_commands": [
    {
      "command_id": "CMD_1001_1729281300",
      "command_type": "set_power",
      "status": "queued",
      ...
    }
  ],
  "count": 1
}
```

---

### 7. Get Statistics
**GET** `/command/statistics`

Get command execution statistics.

**Response**:
```json
{
  "total_commands": 150,
  "completed": 145,
  "failed": 3,
  "pending": 2,
  "success_rate": 96.67,
  "active_devices": 1,
  "total_queued": 2
}
```

---

### 8. Export Logs
**GET** `/command/logs?device_id=<id>&start_time=<iso>&end_time=<iso>`

Export command logs with optional filtering.

**Query Parameters**:
- `device_id`: Filter by device
- `start_time`: Start time (ISO 8601)
- `end_time`: End time (ISO 8601)

**Response**:
```json
{
  "logs": [
    {
      "timestamp": "2025-10-18T14:30:00",
      "event": "QUEUED",
      "command_id": "CMD_1000_1729281234",
      "device_id": "ESP32_EcoWatt_Smart",
      "command_type": "set_power",
      "parameters": {"power_value": 5000},
      "status": "queued"
    },
    ...
  ],
  "count": 50
}
```

---

## Command Logging

All command events are logged to `flask/commands.log` in JSON format.

**Log Format**:
```json
{
  "timestamp": "2025-10-18T14:30:00.123456",
  "event": "QUEUED" | "SENT" | "COMPLETED" | "FAILED",
  "command_id": "CMD_1000_1729281234",
  "device_id": "ESP32_EcoWatt_Smart",
  "command_type": "set_power",
  "parameters": {"power_value": 5000},
  "status": "queued",
  "result": null
}
```

**Event Types**:
- `QUEUED`: Command queued by user/automation
- `SENT`: Command sent to ESP32
- `COMPLETED`: Command executed successfully
- `FAILED`: Command execution failed

---

## ESP32 Integration

### Existing Implementation

The ESP32 side is already implemented in `main.cpp`:

1. **Command Polling** (`checkForCommands()`):
   - Called every upload cycle
   - Polls `/command/poll` endpoint
   - Receives pending command

2. **Command Execution** (`executeCommand()`):
   - Parses command type and parameters
   - Executes on Inverter SIM
   - Returns success/failure

3. **Result Reporting** (`sendCommandResult()`):
   - Sends result to `/command/result`
   - Includes command ID, status, and result message

### Command Flow in ESP32

```cpp
// In upload cycle (main.cpp line ~215)
if (upload_token) {
    upload_token = false;
    upload_data();
    
    // Check for any queued commands from server
    checkForCommands();  // <-- Command execution happens here
    
    // Apply configuration changes...
}
```

---

## Testing Guide

### Test 1: Queue and Execute Command

1. **Start Flask Server**:
```bash
cd flask
python flask_server_hivemq.py
```

2. **Queue a Command** (via curl or Postman):
```bash
curl -X POST http://localhost:5001/command/queue \
  -H "Content-Type: application/json" \
  -d '{
    "device_id": "ESP32_EcoWatt_Smart",
    "command_type": "set_power",
    "parameters": {"power_value": 5000}
  }'
```

**Expected Response**:
```json
{
  "status": "success",
  "command_id": "CMD_1000_...",
  "message": "Command set_power queued successfully"
}
```

3. **Wait for ESP32 Upload Cycle**

ESP32 automatically polls for commands during upload cycle.

4. **Check Command Status**:
```bash
curl http://localhost:5001/command/status/CMD_1000_...
```

**Expected Response** (after execution):
```json
{
  "command_id": "CMD_1000_...",
  "status": "completed",
  "result": "Command set_power: executed successfully",
  ...
}
```

---

### Test 2: Check Command History

```bash
curl http://localhost:5001/command/history?device_id=ESP32_EcoWatt_Smart
```

**Expected Response**:
```json
{
  "commands": [
    {
      "command_id": "CMD_1000_...",
      "command_type": "set_power",
      "status": "completed",
      ...
    }
  ],
  "count": 1
}
```

---

### Test 3: Check Statistics

```bash
curl http://localhost:5001/command/statistics
```

**Expected Response**:
```json
{
  "total_commands": 1,
  "completed": 1,
  "failed": 0,
  "pending": 0,
  "success_rate": 100.0,
  "active_devices": 1
}
```

---

## Node-RED Dashboard Integration

You can create a Node-RED dashboard for command management:

### Flow 1: Queue Command
```
[inject] → [function: build command] → [http request: /command/queue] → [debug]
```

### Flow 2: Monitor Commands
```
[inject: every 5s] → [http request: /command/pending] → [function: parse] → [ui_table]
```

### Flow 3: View History
```
[ui_button] → [http request: /command/history] → [function: format] → [ui_template]
```

### Sample Node-RED Function
```javascript
// Build command payload
msg.payload = {
    device_id: "ESP32_EcoWatt_Smart",
    command_type: "set_power",
    parameters: {
        power_value: parseInt(msg.payload)  // From slider
    }
};
msg.headers = {
    "Content-Type": "application/json"
};
return msg;
```

---

## Troubleshooting

### Issue 1: Commands Not Being Executed

**Symptoms**: Command stays in "queued" or "sent" status

**Checks**:
1. Verify ESP32 is polling: Check serial monitor for "Checking for queued commands"
2. Check upload cycle timing: Commands are polled during upload cycle
3. Verify network connectivity: ESP32 must reach Flask server

**Solution**: Ensure ESP32's upload timer is working and WiFi is connected.

---

### Issue 2: Command Execution Fails

**Symptoms**: Command status shows "failed"

**Checks**:
1. Check ESP32 serial output for error messages
2. Verify Inverter SIM is accessible
3. Check command parameters are valid

**Solution**: Review ESP32 logs and validate command parameters.

---

### Issue 3: Command Manager Not Initialized

**Symptoms**: `/command/*` endpoints return "Command manager not initialized"

**Solution**:
```python
# Ensure this runs in flask_server_hivemq.py
if init_command_manager():
    print("Command manager initialized successfully")
```

---

## File Structure

```
flask/
├── flask_server_hivemq.py       # Main server (includes command endpoints)
├── command_manager.py           # Command queue and logging logic
├── commands.log                 # Command execution log (auto-created)
└── ...

PIO/ECOWATT/src/
└── main.cpp                     # ESP32 command execution code
    ├── checkForCommands()       # Poll for commands
    ├── executeCommand()         # Execute command
    └── sendCommandResult()      # Report result
```

---

## Security Considerations

### Current Implementation
- Basic authentication (device_id verification)
- Command validation (type and parameters)
- Logging of all command activities

### Future Enhancements
- Add API key authentication for `/command/queue`
- Implement command signing/verification
- Rate limiting to prevent command spam
- Role-based access control

---

## Performance

### Scalability
- **Thread-safe**: Uses locks for concurrent access
- **Memory efficient**: Old commands auto-cleanup after 24 hours
- **Fast lookups**: In-memory command queue with O(1) access

### Limits
- **Queue size**: No hard limit (limited by available RAM)
- **History**: Default 100 commands (configurable)
- **Log file**: Grows indefinitely (implement rotation if needed)

---

## Success Criteria (from Guideline)

✅ **EcoWatt Cloud queues a write command**
- Implemented via `/command/queue` endpoint

✅ **EcoWatt Device receives command at next transmission slot**
- Implemented via `/command/poll` in upload cycle

✅ **EcoWatt Device forwards command to Inverter SIM**
- Implemented in `executeCommand()` function

✅ **Inverter SIM executes and generates status**
- Uses existing `setPower()` function

✅ **EcoWatt Device reports result back to Cloud**
- Implemented via `/command/result` endpoint

✅ **EcoWatt Cloud maintains logs**
- Comprehensive logging to `commands.log`
- History and statistics endpoints

---

## Summary

The Command Execution system is now **fully implemented** with:

1. ✅ **8 REST API endpoints** for command management
2. ✅ **Command Manager** with queue, history, and logging
3. ✅ **ESP32 integration** (already existed, now connected to server)
4. ✅ **Comprehensive logging** with event tracking
5. ✅ **Statistics and monitoring** endpoints
6. ✅ **Thread-safe** implementation
7. ✅ **Error handling** and validation

**Next Steps**:
1. Test the complete flow end-to-end
2. Create Node-RED dashboard (optional but recommended)
3. Document test results
4. Prepare demonstration video

---

**Author**: EcoWatt Team PowerPort  
**Date**: October 18, 2025  
**Status**: Milestone 4 Part 2 - Complete ✅
