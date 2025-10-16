# 🔧 Network Connection Issue - Quick Fix

## 🔴 Problem
ESP32 shows: `connection refused` when trying to reach Flask server at `10.78.228.2:5001`

## 🔍 Diagnosis

### Current Setup:
- **ESP32 WiFi**: Connected to `YasithsRedmi` hotspot
- **Flask Server**: Running at `10.78.228.2:5001` (accessible via curl from laptop)
- **Status**: Connection refused (HTTP -1)

### Root Cause:
**Network mismatch!** ESP32 and laptop are on different networks or the Flask server IP has changed.

## ✅ Solutions

### Option 1: Find Correct IP (Recommended)
```bash
# On macOS/Linux - Find your current IP
ifconfig | grep "inet " | grep -v 127.0.0.1

# Look for the IP on the same network as YasithsRedmi hotspot
# It might be something like 192.168.x.x or 10.x.x.x
```

### Option 2: Check Flask Server IP
```bash
# Flask server should show:
# "Flask server starting on http://0.0.0.0:5001"
# This means it's listening on all interfaces
```

### Option 3: Connect Laptop to Same Hotspot
1. Connect your laptop WiFi to `YasithsRedmi` hotspot
2. Get the new IP address
3. Update URLs in main.cpp

### Option 4: Use Laptop's Hotspot IP
If your laptop is sharing internet via "YasithsRedmi":
1. Find the hotspot gateway IP (usually `192.168.137.1` or similar)
2. Use that IP in main.cpp

## 🛠️ Quick Fix Steps

### Step 1: Find Your Laptop's IP on the Hotspot Network
```bash
# Terminal 1
ifconfig en0 | grep "inet "  # WiFi interface
# OR
ifconfig en1 | grep "inet "  # Alternate WiFi
# OR
ip addr show  # Linux
```

### Step 2: Update main.cpp URLs
Replace `10.78.228.2` with your actual IP address.

### Step 3: Rebuild and Upload
```bash
cd PIO/ECOWATT
pio run -t upload
```

## 📊 Current Status

### ✅ Working:
- Security Manager initialized ✓
- HMAC computation working ✓
- Sequence numbers persisting ✓
- Modbus communication working ✓
- Compression working ✓
- Command queue (Flask endpoints) ✓
- Security validation (Flask) ✓

### ❌ Not Working:
- Network connectivity between ESP32 and Flask

## 🎯 Quick Test

### Test 1: Can ESP32 reach laptop?
```bash
# From ESP32 serial monitor, check WiFi info
# Look for: "WiFi connected" and the local IP

# From laptop, try to ping ESP32's IP
ping <ESP32_IP>
```

### Test 2: Can laptop reach itself on hotspot?
```bash
# Find your IP
ifconfig

# Test Flask locally
curl http://<YOUR_IP>:5001/status
```

### Test 3: Verify both are on same subnet
- ESP32 IP: `192.168.x.x`
- Laptop IP: Should be `192.168.x.y` (same x)

## 💡 Most Common Scenarios

### Scenario A: Laptop is hotspot host
- Hotspot IP is usually: `192.168.137.1` (Windows) or `172.20.10.1` (Mac iOS hotspot)
- **Solution**: Use that gateway IP in main.cpp

### Scenario B: Both connected to external hotspot
- Both devices should have IPs in same range
- **Solution**: Find laptop's actual IP and use it

### Scenario C: IP changed since last test
- Flask was on `10.78.228.2` before, different now
- **Solution**: Run `ifconfig` to get current IP

## 🔧 Files to Update

If IP needs to change, update these:

### 1. main.cpp (6 locations)
```cpp
Line 16:  const char* dataPostURL = "http://NEW_IP:5001/process";
Line 17:  const char* fetchChangesURL = "http://NEW_IP:5001/changes";
Line 134: "http://NEW_IP:5001",    // OTAManager
Line 474: const char* reportURL = "http://NEW_IP:5001/config/report";
Line 1061: const char* getCommandsURL = "http://NEW_IP:5001/command/get";
Line 1155: const char* reportURL = "http://NEW_IP:5001/command/report";
```

### 2. credentials.h (optional - not used)
```cpp
Line 10: #define FLASK_SERVER_URL "http://NEW_IP:5001"
```

### 3. Test scripts (optional)
```python
test_security.py line 12: FLASK_SERVER = "http://NEW_IP:5001"
test_command_queue.py: (same)
```

## 🎬 Next Steps

1. **Find correct IP**: `ifconfig` or check hotspot settings
2. **Update main.cpp**: Replace `10.78.228.2` with correct IP
3. **Rebuild**: `pio run -t upload`
4. **Monitor**: `pio device monitor`
5. **Verify**: Should see "✓ Data uploaded successfully"

---

**Once connected, you'll immediately see:**
- Data uploading successfully
- Commands being retrieved and executed
- Configuration updates working
- All Phase 4 features operational! 🚀
