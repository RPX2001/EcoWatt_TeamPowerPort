# 🚀 Command Execution - Quick Start Guide

**Get up and running in 5 minutes!**

---

## Step 1: Start the Flask Server

```bash
cd flask
python flask_server_hivemq.py
```

**Expected Output:**
```
Starting EcoWatt Smart Selection Batch Server v3.1
============================================================
MQTT client initialized successfully
OTA firmware manager initialized successfully
Command manager initialized successfully
============================================================
Command endpoints: /command/queue, /command/poll, /command/result, /command/status, /command/history
============================================================
 * Running on http://0.0.0.0:5001
```

---

## Step 2: Test Command Queueing

Open a **new terminal** and run:

```bash
# Queue a set_power command
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

**Expected Response:**
```json
{
  "status": "success",
  "command_id": "CMD_1000_1729281234",
  "message": "Command set_power queued successfully for ESP32_EcoWatt_Smart"
}
```

**✅ Success!** Your command is now queued.

---

## Step 3: Check Pending Commands

```bash
curl http://localhost:5001/command/pending
```

**Expected Response:**
```json
{
  "pending_commands": [
    {
      "command_id": "CMD_1000_1729281234",
      "command_type": "set_power",
      "status": "queued",
      "queued_time": "2025-10-18T..."
    }
  ],
  "count": 1
}
```

---

## Step 4: Simulate ESP32 Polling

The ESP32 will automatically poll during its upload cycle, but you can simulate it:

```bash
curl -X POST http://localhost:5001/command/poll \
  -H "Content-Type: application/json" \
  -d '{
    "device_id": "ESP32_EcoWatt_Smart"
  }'
```

**Expected Response:**
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

---

## Step 5: Simulate ESP32 Result Submission

After ESP32 executes the command, it reports back:

```bash
curl -X POST http://localhost:5001/command/result \
  -H "Content-Type: application/json" \
  -d '{
    "command_id": "CMD_1000_1729281234",
    "status": "completed",
    "result": "Command set_power: executed successfully"
  }'
```

**Expected Response:**
```json
{
  "status": "success",
  "message": "Result recorded successfully",
  "command_id": "CMD_1000_1729281234"
}
```

---

## Step 6: Check Command Status

```bash
curl http://localhost:5001/command/status/CMD_1000_1729281234
```

**Expected Response:**
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
  "result": "Command set_power: executed successfully"
}
```

---

## Step 7: View Statistics

```bash
curl http://localhost:5001/command/statistics
```

**Expected Response:**
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

## 🧪 Run Automated Test Suite

Instead of manual testing, run the comprehensive test suite:

```bash
cd flask
python test_command_execution.py
```

**This will automatically:**
1. ✅ Queue commands
2. ✅ Poll for commands
3. ✅ Submit results
4. ✅ Check status
5. ✅ Get history
6. ✅ Get statistics
7. ✅ Export logs
8. ✅ Test failure scenarios
9. ✅ Test write_register command
10. ✅ Verify all endpoints

---

## 📱 Test with Real ESP32

### 1. Flash ESP32 with Current Firmware
```bash
cd PIO/ECOWATT
pio run --target upload
pio device monitor
```

### 2. Queue a Command from Flask
```bash
curl -X POST http://localhost:5001/command/queue \
  -H "Content-Type: application/json" \
  -d '{
    "device_id": "ESP32_EcoWatt_Smart",
    "command_type": "set_power",
    "parameters": {"power_value": 6000}
  }'
```

### 3. Watch ESP32 Serial Monitor

**You should see:**
```
Checking for queued commands from server...
Received command: set_power (ID: CMD_1000_...)
Executing command: set_power
Parameters: {"power_value":6000}
Setting power to 6000 W
Power set successfully to 6000 W
Sending command result to server...
Command result sent successfully
```

### 4. Verify Result on Server
```bash
curl http://localhost:5001/command/history?device_id=ESP32_EcoWatt_Smart
```

---

## 🐛 Troubleshooting

### Issue: "Command manager not initialized"
**Solution:**
```bash
# Check server startup logs for:
"Command manager initialized successfully"

# If missing, check command_manager.py exists:
ls flask/command_manager.py
```

---

### Issue: "Connection refused"
**Solution:**
```bash
# Check if Flask server is running:
curl http://localhost:5001/status

# If not running, start it:
python flask/flask_server_hivemq.py
```

---

### Issue: ESP32 not receiving commands
**Solution:**
1. Check ESP32 is connected to WiFi
2. Verify upload cycle is working (check serial monitor)
3. Ensure Flask server URL is correct in credentials.h
4. Check upload timer is triggering (every 15 minutes or test interval)

---

### Issue: Commands stay in "queued" state
**Solution:**
1. ESP32 must poll `/command/poll` during upload cycle
2. Check ESP32 serial monitor for "Checking for queued commands"
3. Verify `checkForCommands()` is called in `main.cpp` upload cycle

---

## 📊 Monitoring Dashboard (Optional)

### Create Simple Web Dashboard

Create `flask/templates/commands.html`:
```html
<!DOCTYPE html>
<html>
<head>
    <title>EcoWatt Commands</title>
    <script>
        function refreshStats() {
            fetch('/command/statistics')
                .then(r => r.json())
                .then(data => {
                    document.getElementById('stats').innerHTML = 
                        JSON.stringify(data, null, 2);
                });
        }
        setInterval(refreshStats, 5000);
        refreshStats();
    </script>
</head>
<body>
    <h1>EcoWatt Command Statistics</h1>
    <pre id="stats"></pre>
</body>
</html>
```

Access at: `http://localhost:5001/commands.html`

---

## 🎯 What to Test

### Test Scenario 1: Successful Command
1. ✅ Queue command
2. ✅ ESP32 polls and receives
3. ✅ ESP32 executes successfully
4. ✅ ESP32 reports success
5. ✅ Verify status = "completed"

### Test Scenario 2: Failed Command
1. ✅ Queue command with invalid parameters
2. ✅ ESP32 polls and receives
3. ✅ ESP32 execution fails
4. ✅ ESP32 reports failure
5. ✅ Verify status = "failed"

### Test Scenario 3: Multiple Commands
1. ✅ Queue 3 commands
2. ✅ ESP32 receives them one by one
3. ✅ All commands executed in order
4. ✅ All results recorded correctly

---

## 📝 Expected Log Format

Check `flask/commands.log`:
```json
{"timestamp": "2025-10-18T14:30:00", "event": "QUEUED", "command_id": "CMD_1000_...", ...}
{"timestamp": "2025-10-18T14:31:00", "event": "SENT", "command_id": "CMD_1000_...", ...}
{"timestamp": "2025-10-18T14:31:05", "event": "COMPLETED", "command_id": "CMD_1000_...", ...}
```

---

## ✅ Success Checklist

- [ ] Flask server starts without errors
- [ ] Command manager initialized successfully
- [ ] Can queue commands via `/command/queue`
- [ ] Can poll commands via `/command/poll`
- [ ] Can submit results via `/command/result`
- [ ] Can view status via `/command/status/<id>`
- [ ] Can view history via `/command/history`
- [ ] Statistics show correct counts
- [ ] Logs are being written to `commands.log`
- [ ] ESP32 receives and executes commands
- [ ] End-to-end flow works completely

---

## 🚀 Next Steps After Testing

1. ✅ Verify all endpoints work
2. ✅ Test with real ESP32 device
3. ✅ Document any issues found
4. ✅ Take screenshots for demonstration
5. ✅ Prepare video demonstration
6. ➡️ **Start Milestone 5** (Power Management)

---

## 📚 Additional Resources

- **Full Documentation**: `COMMAND_EXECUTION_COMPLETE.md`
- **API Reference**: `COMMAND_API_REFERENCE.md`
- **Implementation Summary**: `COMMAND_EXECUTION_SUMMARY.md`
- **Test Script**: `flask/test_command_execution.py`

---

## 💡 Tips

1. **Use `jq` for pretty JSON**:
   ```bash
   curl http://localhost:5001/command/statistics | jq
   ```

2. **Watch logs in real-time**:
   ```bash
   tail -f flask/commands.log | jq
   ```

3. **Monitor ESP32 serial**:
   ```bash
   pio device monitor
   ```

4. **Save command ID for later**:
   ```bash
   CMD_ID=$(curl -s -X POST http://localhost:5001/command/queue \
     -H "Content-Type: application/json" \
     -d '{"device_id":"ESP32_EcoWatt_Smart","command_type":"set_power","parameters":{"power_value":5000}}' \
     | jq -r '.command_id')
   
   echo "Command ID: $CMD_ID"
   
   # Check status later
   curl http://localhost:5001/command/status/$CMD_ID | jq
   ```

---

**Ready to test? Start with Step 1!** 🚀

---

**Status**: Ready for testing ✅  
**Estimated Testing Time**: 15-30 minutes  
**Difficulty**: Easy - all scripts provided  
**Date**: October 18, 2025
