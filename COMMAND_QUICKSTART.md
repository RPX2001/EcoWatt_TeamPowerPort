# Quick Start - Command Execution System

## 🚀 Getting Started in 5 Minutes

### Step 1: Start the Flask Server
```bash
cd flask
python flask_server_hivemq.py
```

**Expected output**:
```
Command manager initialized successfully
Command endpoints: /command/queue, /command/poll, /command/result...
```

---

### Step 2: Test with Provided Script
```bash
python flask/test_command_execution.py
```

This runs 10 automated tests and verifies all endpoints work.

---

### Step 3: Queue a Command Manually

**Using curl**:
```bash
curl -X POST http://localhost:5001/command/queue \
  -H "Content-Type: application/json" \
  -d '{
    "device_id": "ESP32_EcoWatt_Smart",
    "command_type": "set_power",
    "parameters": {"power_value": 5000}
  }'
```

**Using Python**:
```python
import requests

response = requests.post('http://localhost:5001/command/queue', json={
    'device_id': 'ESP32_EcoWatt_Smart',
    'command_type': 'set_power',
    'parameters': {'power_value': 5000}
})

print(response.json())
# Output: {"status": "success", "command_id": "CMD_1000_...", ...}
```

---

### Step 4: Check Command Status
```bash
curl http://localhost:5001/command/statistics
```

**Output**:
```json
{
  "total_commands": 1,
  "completed": 0,
  "pending": 1,
  "success_rate": 0.0
}
```

---

### Step 5: ESP32 Will Automatically Execute

The ESP32 firmware already has command execution built in:
- Every upload cycle (~15 min), ESP32 calls `checkForCommands()`
- ESP32 polls `/command/poll` endpoint
- Receives pending command
- Executes on Inverter SIM
- Reports result via `/command/result`

**No changes needed to ESP32 code!**

---

## 📚 Documentation

| File | Description |
|------|-------------|
| `COMMAND_EXECUTION_COMPLETE.md` | Full documentation (680 lines) |
| `COMMAND_API_REFERENCE.md` | Quick API reference (450 lines) |
| `COMMAND_EXECUTION_SUMMARY.md` | Implementation summary |
| `PROJECT_STATUS_SUMMARY.md` | Updated project status |

---

## 🧪 All Available Endpoints

```
POST   /command/queue          # Queue a command
POST   /command/poll           # Get next command (ESP32)
POST   /command/result         # Submit result (ESP32)
GET    /command/status/<id>    # Get command status
GET    /command/history        # Get command history
GET    /command/pending        # Get pending commands
GET    /command/statistics     # Get statistics
GET    /command/logs           # Export logs
```

---

## 💡 Example Commands

### Set Power to 5000W
```json
{
  "device_id": "ESP32_EcoWatt_Smart",
  "command_type": "set_power",
  "parameters": {
    "power_value": 5000
  }
}
```

### Write Register
```json
{
  "device_id": "ESP32_EcoWatt_Smart",
  "command_type": "write_register",
  "parameters": {
    "register_address": 40001,
    "value": 4500
  }
}
```

---

## ✅ Verification Checklist

- [x] Flask server starts without errors
- [x] `/command/queue` accepts commands
- [x] `/command/poll` returns queued commands
- [x] `/command/result` records results
- [x] `/command/statistics` shows correct counts
- [x] Commands logged to `flask/commands.log`
- [x] Test script runs successfully
- [x] ESP32 code integrates (no changes needed)

---

## 🎯 Status

**Milestone 4 - Part 2: COMPLETE ✅**

- ✅ Command queueing
- ✅ Command polling
- ✅ Command execution
- ✅ Result reporting
- ✅ Command logging
- ✅ Statistics tracking
- ✅ History management
- ✅ Documentation

**Next**: Milestone 5 (Power Management & Fault Recovery)

---

**For detailed information, see**: `COMMAND_EXECUTION_COMPLETE.md`
