# Command Execution API - Quick Reference

## Base URL
```
http://localhost:5001
```

## Endpoints Summary

| Method | Endpoint | Description |
|--------|----------|-------------|
| POST | `/command/queue` | Queue a new command |
| POST | `/command/poll` | Get next pending command (ESP32) |
| POST | `/command/result` | Submit execution result (ESP32) |
| GET | `/command/status/<command_id>` | Get command status |
| GET | `/command/history` | Get command history |
| GET | `/command/pending` | Get pending commands |
| GET | `/command/statistics` | Get execution statistics |
| GET | `/command/logs` | Export command logs |

---

## 1. Queue Command

**POST** `/command/queue`

### Supported Commands

#### a) Set Power
```bash
curl -X POST http://localhost:5001/command/queue \
  -H "Content-Type: application/json" \
  -d '{
    "device_id": "ESP32_EcoWatt_Smart",
    "command_type": "set_power",
    "parameters": {
      "power_value": 5000
    }
  }'
```

#### b) Write Register
```bash
curl -X POST http://localhost:5001/command/queue \
  -H "Content-Type: application/json" \
  -d '{
    "device_id": "ESP32_EcoWatt_Smart",
    "command_type": "write_register",
    "parameters": {
      "register_address": 40001,
      "value": 4500
    }
  }'
```

**Response:**
```json
{
  "status": "success",
  "command_id": "CMD_1000_1729281234",
  "message": "Command queued successfully"
}
```

---

## 2. Poll Command (ESP32)

**POST** `/command/poll`

```bash
curl -X POST http://localhost:5001/command/poll \
  -H "Content-Type: application/json" \
  -d '{
    "device_id": "ESP32_EcoWatt_Smart"
  }'
```

**Response (command available):**
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

**Response (no commands):**
```json
{
  "message": "No pending commands"
}
```

---

## 3. Submit Result (ESP32)

**POST** `/command/result`

```bash
curl -X POST http://localhost:5001/command/result \
  -H "Content-Type: application/json" \
  -d '{
    "command_id": "CMD_1000_1729281234",
    "status": "completed",
    "result": "Command executed successfully"
  }'
```

**Response:**
```json
{
  "status": "success",
  "message": "Result recorded successfully",
  "command_id": "CMD_1000_1729281234"
}
```

---

## 4. Get Command Status

**GET** `/command/status/<command_id>`

```bash
curl http://localhost:5001/command/status/CMD_1000_1729281234
```

**Response:**
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

## 5. Get Command History

**GET** `/command/history?device_id=<id>&limit=<n>`

```bash
# All devices, last 50 commands
curl http://localhost:5001/command/history

# Specific device, last 20 commands
curl http://localhost:5001/command/history?device_id=ESP32_EcoWatt_Smart&limit=20
```

**Response:**
```json
{
  "commands": [
    {
      "command_id": "CMD_1000_1729281234",
      "device_id": "ESP32_EcoWatt_Smart",
      "command_type": "set_power",
      "status": "completed",
      "queued_time": "2025-10-18T14:30:00",
      "result": "Command executed successfully"
    }
  ],
  "count": 1,
  "device_id_filter": "ESP32_EcoWatt_Smart"
}
```

---

## 6. Get Pending Commands

**GET** `/command/pending?device_id=<id>`

```bash
# All devices
curl http://localhost:5001/command/pending

# Specific device
curl http://localhost:5001/command/pending?device_id=ESP32_EcoWatt_Smart
```

**Response:**
```json
{
  "pending_commands": [
    {
      "command_id": "CMD_1001_1729281300",
      "command_type": "set_power",
      "status": "queued",
      "queued_time": "2025-10-18T14:35:00"
    }
  ],
  "count": 1
}
```

---

## 7. Get Statistics

**GET** `/command/statistics`

```bash
curl http://localhost:5001/command/statistics
```

**Response:**
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

## 8. Export Logs

**GET** `/command/logs?device_id=<id>&start_time=<iso>&end_time=<iso>`

```bash
# All logs
curl http://localhost:5001/command/logs

# Filtered by device
curl http://localhost:5001/command/logs?device_id=ESP32_EcoWatt_Smart

# Filtered by time range
curl "http://localhost:5001/command/logs?start_time=2025-10-18T00:00:00&end_time=2025-10-18T23:59:59"
```

**Response:**
```json
{
  "logs": [
    {
      "timestamp": "2025-10-18T14:30:00",
      "event": "QUEUED",
      "command_id": "CMD_1000_1729281234",
      "device_id": "ESP32_EcoWatt_Smart",
      "command_type": "set_power",
      "status": "queued"
    }
  ],
  "count": 1
}
```

---

## Command States

| State | Description |
|-------|-------------|
| `queued` | Command queued, waiting to be sent |
| `sent` | Command sent to ESP32, waiting for execution |
| `completed` | Command executed successfully |
| `failed` | Command execution failed |

---

## Event Types (in logs)

| Event | Description |
|-------|-------------|
| `QUEUED` | Command queued by user/automation |
| `SENT` | Command sent to ESP32 |
| `COMPLETED` | Command executed successfully |
| `FAILED` | Command execution failed |

---

## Python Example

```python
import requests
import json

# Queue a command
response = requests.post(
    'http://localhost:5001/command/queue',
    json={
        'device_id': 'ESP32_EcoWatt_Smart',
        'command_type': 'set_power',
        'parameters': {'power_value': 5000}
    }
)

command_id = response.json()['command_id']
print(f"Queued: {command_id}")

# Check status
status_response = requests.get(
    f'http://localhost:5001/command/status/{command_id}'
)

print(f"Status: {status_response.json()['status']}")
```

---

## Node-RED Example

```javascript
// Queue command from Node-RED
msg.url = "http://localhost:5001/command/queue";
msg.method = "POST";
msg.headers = {"Content-Type": "application/json"};
msg.payload = {
    device_id: "ESP32_EcoWatt_Smart",
    command_type: "set_power",
    parameters: {
        power_value: flow.get("target_power") || 5000
    }
};
return msg;
```

---

## Testing

### Run Test Suite
```bash
cd flask
python test_command_execution.py
```

### Manual Testing
1. Start Flask server:
   ```bash
   python flask/flask_server_hivemq.py
   ```

2. Queue a command:
   ```bash
   curl -X POST http://localhost:5001/command/queue \
     -H "Content-Type: application/json" \
     -d '{"device_id":"ESP32_EcoWatt_Smart","command_type":"set_power","parameters":{"power_value":5000}}'
   ```

3. Check pending commands:
   ```bash
   curl http://localhost:5001/command/pending
   ```

4. Wait for ESP32 to poll and execute

5. Check statistics:
   ```bash
   curl http://localhost:5001/command/statistics
   ```

---

## Files

```
flask/
├── flask_server_hivemq.py          # Main server with endpoints
├── command_manager.py              # Command logic and logging
├── commands.log                    # Command execution log
└── test_command_execution.py       # Test suite
```

---

## Status Codes

| Code | Description |
|------|-------------|
| 200 | Success |
| 400 | Bad request (missing/invalid parameters) |
| 404 | Command not found |
| 500 | Server error |

---

**Last Updated**: October 18, 2025  
**Version**: 1.0  
**Status**: Complete ✅
