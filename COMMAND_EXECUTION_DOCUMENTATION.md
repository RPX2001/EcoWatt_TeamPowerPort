# Command Execution System Documentation

## Overview
The Command Execution System enables bidirectional communication between EcoWatt Cloud and ESP32 devices, allowing remote control of inverter operations.

## Architecture

### Command Flow
```
Cloud (User/Dashboard) 
  ↓ [Queue Command]
Flask Server (/command/queue)
  ↓ [Store in Queue]
Command Queue (in-memory, thread-safe)
  ↓ [Wait for ESP32]
ESP32 Upload Cycle
  ↓ [Request Commands: /command/get]
Flask Server
  ↓ [Return Pending Commands]
ESP32
  ↓ [Execute on Inverter SIM]
Inverter SIM (via setPower/writeRegister)
  ↓ [Get Result]
ESP32
  ↓ [Report Result: /command/report]
Flask Server
  ↓ [Log Result]
Command History
```

## Flask Server API Endpoints

### 1. Queue Command
**Endpoint:** `POST /command/queue`

**Purpose:** Queue a new command for a device to execute

**Request Body:**
```json
{
  "device_id": "ESP32_EcoWatt_Smart",
  "command_type": "set_power",
  "parameters": {
    "power_value": 50
  }
}
```

**Valid Command Types:**
- `set_power` - Set inverter power output
- `write_register` - Write to specific register
- `read_register` - Read from specific register (future)

**Response (Success):**
```json
{
  "status": "success",
  "message": "Command queued successfully",
  "command_id": "CMD_1_1729123456",
  "device_id": "ESP32_EcoWatt_Smart",
  "command_type": "set_power",
  "queued_time": "2025-10-16T10:30:56.123456"
}
```

**Response (Error):**
```json
{
  "error": "Invalid command_type. Valid types: ['set_power', 'write_register', 'read_register']"
}
```

### 2. Get Pending Commands
**Endpoint:** `POST /command/get`

**Purpose:** Retrieve pending commands for a device (called by ESP32)

**Request Body:**
```json
{
  "device_id": "ESP32_EcoWatt_Smart"
}
```

**Response:**
```json
{
  "status": "success",
  "device_id": "ESP32_EcoWatt_Smart",
  "command_count": 2,
  "commands": [
    {
      "command_id": "CMD_1_1729123456",
      "command_type": "set_power",
      "parameters": {
        "power_value": 50
      },
      "status": "executing",
      "queued_time": "2025-10-16T10:30:56.123456",
      "execution_start": "2025-10-16T10:31:10.234567"
    },
    {
      "command_id": "CMD_2_1729123457",
      "command_type": "set_power",
      "parameters": {
        "power_value": 100
      },
      "status": "executing",
      "queued_time": "2025-10-16T10:30:58.345678",
      "execution_start": "2025-10-16T10:31:10.234567"
    }
  ]
}
```

### 3. Report Command Result
**Endpoint:** `POST /command/report`

**Purpose:** Device reports execution result (called by ESP32)

**Request Body:**
```json
{
  "device_id": "ESP32_EcoWatt_Smart",
  "command_id": "CMD_1_1729123456",
  "status": "completed",
  "result": "Successfully set power to 50 W",
  "timestamp": 123456789
}
```

**For Failed Commands:**
```json
{
  "device_id": "ESP32_EcoWatt_Smart",
  "command_id": "CMD_2_1729123457",
  "status": "failed",
  "result": "Failed to set power value",
  "error_message": "WiFi connection lost during execution",
  "timestamp": 123456790
}
```

**Response:**
```json
{
  "status": "success",
  "message": "Command result recorded",
  "command_id": "CMD_1_1729123456"
}
```

### 4. Get Command History
**Endpoint:** `GET /command/history?device_id=ESP32_EcoWatt_Smart`

**Purpose:** Retrieve command history for monitoring

**Response:**
```json
{
  "status": "success",
  "device_id": "ESP32_EcoWatt_Smart",
  "command_count": 3,
  "commands": [
    {
      "command_id": "CMD_1_1729123456",
      "command_type": "set_power",
      "parameters": {"power_value": 50},
      "status": "completed",
      "queued_time": "2025-10-16T10:30:56.123456",
      "execution_start": "2025-10-16T10:31:10.234567",
      "completed_time": "2025-10-16T10:31:12.456789",
      "execution_time_ms": 2222,
      "result": "Successfully set power to 50 W"
    }
  ]
}
```

### 5. Get Command Status
**Endpoint:** `GET /command/status?command_id=CMD_1_1729123456`

**Purpose:** Check status of a specific command

**Response:**
```json
{
  "status": "success",
  "command": {
    "command_id": "CMD_1_1729123456",
    "command_type": "set_power",
    "status": "completed",
    "result": "Successfully set power to 50 W"
  }
}
```

## ESP32 Implementation

### Command Check Function
Location: `PIO/ECOWATT/src/main.cpp` - `checkAndExecuteCommands()`

**When Called:** During every upload cycle (after `upload_data()`)

**Process:**
1. Send POST request to `/command/get`
2. Parse response to get pending commands
3. For each command:
   - Execute based on command_type
   - Capture result (success/failure)
   - Report result via POST to `/command/report`

### Supported Commands

#### 1. set_power
```cpp
if (strcmp(commandType, "set_power") == 0) {
    uint16_t powerValue = parameters["power_value"] | 0;
    success = setPower(powerValue);
    resultMessage = success ? 
        "Successfully set power to X W" : 
        "Failed to set power value";
}
```

#### 2. write_register
```cpp
if (strcmp(commandType, "write_register") == 0) {
    uint16_t regAddress = parameters["register_address"] | 0;
    uint16_t value = parameters["value"] | 0;
    
    if (regAddress == 8) {  // Power register
        success = setPower(value);
        resultMessage = success ? 
            "Register write successful" : 
            "Register write failed";
    } else {
        resultMessage = "Unsupported register address";
        success = false;
    }
}
```

## Testing Guide

### 1. Using test_command_queue.py

```bash
cd flask/
python test_command_queue.py
```

This will:
- Queue 3 test commands
- Check their status
- Display command history

### 2. Manual Testing with curl

**Queue a Command:**
```bash
curl -X POST http://10.78.228.2:5001/command/queue \
  -H "Content-Type: application/json" \
  -d '{
    "device_id": "ESP32_EcoWatt_Smart",
    "command_type": "set_power",
    "parameters": {"power_value": 75}
  }'
```

**Check Command History:**
```bash
curl "http://10.78.228.2:5001/command/history?device_id=ESP32_EcoWatt_Smart"
```

**Check Specific Command:**
```bash
curl "http://10.78.228.2:5001/command/status?command_id=CMD_1_1729123456"
```

### 3. Integration Testing

1. **Start Flask Server:**
   ```bash
   cd flask/
   python flask_server_hivemq.py
   ```

2. **Flash ESP32:**
   ```bash
   cd PIO/ECOWATT/
   pio run -t upload
   pio device monitor
   ```

3. **Queue Commands:**
   ```bash
   python test_command_queue.py
   ```

4. **Observe ESP32 Serial Output:**
   - Wait for upload cycle
   - Watch "Checking for pending commands..."
   - See command execution
   - Verify result reporting

## Command States

1. **pending** - Command queued, waiting for device
2. **executing** - Device retrieved command, executing
3. **completed** - Command executed successfully
4. **failed** - Command execution failed

## Thread Safety

All command operations use `command_lock` to ensure thread-safe access:
- Queue modifications
- Status updates
- History logging

## Logging

### Flask Server Logs:
```
INFO: Command queued: CMD_1_1729123456 for device ESP32_EcoWatt_Smart - set_power
INFO: Device ESP32_EcoWatt_Smart retrieved 2 pending commands
INFO: Command CMD_1_1729123456 for device ESP32_EcoWatt_Smart - Status: completed, Result: Successfully set power to 50 W
```

### ESP32 Serial Logs:
```
Checking for pending commands from cloud...
Found 2 pending commands to execute
Executing command: CMD_1_1729123456 (Type: set_power)
  Setting power to 50 W
Reporting command result to cloud...
  Command result reported successfully
```

## Future Enhancements

1. **Command Timeout:** Auto-fail commands not executed within X minutes
2. **Command Priority:** High/medium/low priority queue
3. **Scheduled Commands:** Execute at specific time
4. **Batch Commands:** Group multiple commands
5. **Command Validation:** Pre-validate parameters before queuing
6. **Command Templates:** Predefined command sets
7. **Read Commands:** Support for reading data on-demand

## Error Handling

### Common Errors:
1. **WiFi Not Connected:** Command check skipped
2. **HTTP Timeout:** Retry on next cycle
3. **Invalid Command Type:** Reported as failed
4. **Inverter SIM Error:** Reported with error message
5. **JSON Parse Error:** Logged, command skipped

## Security Considerations

**Current Implementation:**
- No authentication on command endpoints (Phase 4 will add)
- No encryption of command data (Phase 4 will add)
- No command signature verification (Phase 4 will add)

**Planned for Security Phase:**
- HMAC authentication for commands
- Encrypted command parameters
- Anti-replay protection
- Command authorization levels

## Integration with Other Systems

### Configuration Updates:
Commands execute **after** configuration updates in the upload cycle:
```cpp
upload_data();              // 1. Upload buffered data
checkAndExecuteCommands();  // 2. Execute commands
// Apply config changes     // 3. Update configurations
```

### FOTA Updates:
Commands are **paused** during FOTA operations (timers disabled)

## Troubleshooting

### Commands Not Executing:
1. Check Flask server is running
2. Verify ESP32 WiFi connection
3. Check URL matches (`dataPostURL` and `fetchChangesURL`)
4. Monitor serial output for errors

### Commands Stuck in "pending":
1. ESP32 not reaching upload cycle
2. HTTP request failing
3. Check `/command/get` endpoint availability

### Commands Not Reporting Results:
1. Check `/command/report` endpoint
2. Verify JSON serialization
3. Check network connectivity during execution

---

**Last Updated:** October 16, 2025  
**Status:** ✅ Fully Implemented and Tested
