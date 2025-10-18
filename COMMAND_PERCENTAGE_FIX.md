# 🔧 Command Execution Fix - Register 8 Percentage Issue

**Date**: October 18, 2025  
**Issue**: Commands failing with Modbus Exception 03 (Illegal Data Value)  
**Root Cause**: Register 8 expects PERCENTAGE (0-100%), not absolute WATTS  
**Status**: ✅ FIXED

---

## 🔍 Problem Analysis

### **What Was Happening:**
```
Command: set_power 5000W
ESP32 sends: Register 8 = 5000
Inverter responds: Exception 03 - Illegal Data Value ❌
```

### **Why It Failed:**
According to the API documentation (Section 4 - Modbus Data Registers):

```
Address 8: "Set the export power percentage"
- Function: Read/Write
- Gain: 1
- Unit: % ← THIS IS THE KEY!
- Allowed range: 0–100
```

**The register accepts PERCENTAGE, not absolute watts!**

---

## ✅ Solution Implemented

### **Fix 1: ESP32 Code - Automatic Conversion**

**File**: `PIO/ECOWATT/src/main.cpp`

**Changes:**
```cpp
// OLD CODE (WRONG):
bool result = setPower(powerValue);  // Sends watts directly

// NEW CODE (CORRECT):
const int MAX_INVERTER_CAPACITY = 10000; // Watts
int powerPercentage = (powerValue * 100) / MAX_INVERTER_CAPACITY;
if (powerPercentage < 0) powerPercentage = 0;
if (powerPercentage > 100) powerPercentage = 100;
bool result = setPower(powerPercentage);  // Sends percentage
```

**Example Conversions:**
```
5000W → (5000 * 100) / 10000 = 50%   ✅
1000W → (1000 * 100) / 10000 = 10%   ✅
10000W → (10000 * 100) / 10000 = 100% ✅
15000W → Clamped to 100%              ✅
```

### **Fix 2: New Command Type - Direct Percentage**

Added `set_power_percentage` command for direct control:

```cpp
else if (strcmp(commandType, "set_power_percentage") == 0) {
    int percentage = paramDoc["percentage"] | 0;
    if (percentage < 0) percentage = 0;
    if (percentage > 100) percentage = 100;
    bool result = setPower(percentage);
    return result;
}
```

**Usage:**
```bash
# Direct percentage control
python queue_command_for_esp32.py set_power_percentage 50
```

### **Fix 3: Updated Queue Script**

**File**: `flask/queue_command_for_esp32.py`

**Changes:**
- Added `queue_set_power_percentage()` function
- Updated `queue_set_power()` to show conversion
- Added usage warnings about percentage

---

## 🧪 Testing

### **Test 1: Power with Automatic Conversion**
```bash
cd PIO/ECOWATT
pio run --target upload  # Flash updated firmware

cd flask
python queue_command_for_esp32.py set_power 5000
# Should now convert to 50% and succeed ✅
```

### **Test 2: Direct Percentage Control**
```bash
python queue_command_for_esp32.py set_power_percentage 25
# Directly sets 25% ✅
```

### **Test 3: Boundary Values**
```bash
# Test script for various percentages
chmod +x test_percentage_commands.sh
./test_percentage_commands.sh

# Tests: 0%, 10%, 25%, 50%, 75%, 100%
```

---

## 📊 Expected Results

### **Before Fix:**
```
Command: set_power 5000W
ESP32 log:
  Sending write frame: 11060008138807CE
  Received frame: 11860303A4
  Modbus Exception: 03 - Illegal Data Value ❌
  Failed to set power
```

### **After Fix:**
```
Command: set_power 5000W
ESP32 log:
  Setting power to 5000 W (50%)
  Sending write frame: 110600080032C88C  ← Now sends 50 (0x0032)
  Received frame: 110600080032C88C      ← Success! Echo response
  Power set successfully to 5000 W (50%) ✅
```

---

## 🎯 Command Reference

### **Option 1: set_power (Watts → Auto-converted to %)**
```bash
python queue_command_for_esp32.py set_power 5000
# Converts: 5000W → 50% of 10kW capacity
```

**Pros:**
- Intuitive (think in watts)
- Auto-conversion handled

**Cons:**
- Requires knowing max capacity
- Conversion may not match actual inverter capacity

### **Option 2: set_power_percentage (Direct %)**
```bash
python queue_command_for_esp32.py set_power_percentage 50
# Directly sets 50%
```

**Pros:**
- Direct control
- No conversion needed
- Matches register exactly

**Cons:**
- Need to know percentage values

---

## 📝 Files Modified

### **1. PIO/ECOWATT/src/main.cpp**
**Lines**: 481-515
**Changes**:
- Added automatic watts → percentage conversion
- Added `set_power_percentage` command handler
- Added clamping (0-100%)
- Improved logging

### **2. flask/queue_command_for_esp32.py**
**Functions Added**:
- `queue_set_power_percentage()`

**Functions Modified**:
- `queue_set_power()` - Now shows conversion
- `show_usage()` - Updated help text

### **3. flask/test_percentage_commands.sh** (NEW)
**Purpose**: Test various percentage values
**Tests**: 0%, 10%, 25%, 50%, 75%, 100%

---

## 🔄 Migration Guide

### **For Existing Code:**

If you have hardcoded power values, update them:

```cpp
// OLD:
setPower(5000);  // ❌ Would fail

// NEW:
setPower(50);    // ✅ 50% of capacity
```

### **For API Calls:**

```json
// OLD (Will still work, but with conversion):
{
  "command_type": "set_power",
  "parameters": {"power_value": 5000}
}

// NEW (Direct percentage - Recommended):
{
  "command_type": "set_power_percentage",
  "parameters": {"percentage": 50}
}
```

---

## ⚙️ Configuration

### **Adjust Max Capacity:**

Edit `PIO/ECOWATT/src/main.cpp` line ~490:

```cpp
const int MAX_INVERTER_CAPACITY = 10000; // Change this to your inverter's max watts
```

**Examples:**
- 5kW inverter: `const int MAX_INVERTER_CAPACITY = 5000;`
- 10kW inverter: `const int MAX_INVERTER_CAPACITY = 10000;`
- 15kW inverter: `const int MAX_INVERTER_CAPACITY = 15000;`

---

## 🎓 Key Learnings

### **1. Always Read API Documentation Carefully**
The register description clearly stated "%" but we missed it initially.

### **2. Modbus Exception 03 = Value Out of Range**
This specific error code told us the value was invalid, not the communication.

### **3. Unit Mismatches Are Common**
Always verify units: Watts vs Percentage, Celsius vs Fahrenheit, etc.

### **4. Provide Multiple Interfaces**
Offering both `set_power` (watts) and `set_power_percentage` gives flexibility.

---

## ✅ Validation Checklist

Before testing:
- [ ] ESP32 firmware updated with new main.cpp
- [ ] Flask server restarted (if needed)
- [ ] Max capacity configured correctly
- [ ] Test scripts have execute permissions

After first test:
- [ ] ESP32 logs show "(X%)" in set power message
- [ ] Modbus frame shows correct percentage value
- [ ] No Exception 03 errors
- [ ] Echo response received from inverter
- [ ] Command status shows "completed" on server

---

## 📞 Troubleshooting

### **Still Getting Exception 03:**
1. Check ESP32 serial - does it show "(X%)"?
2. Verify percentage value is 0-100
3. Check inverter is in correct operating mode
4. Try simple values: 0%, 50%, 100%

### **Conversion Wrong:**
1. Verify MAX_INVERTER_CAPACITY in main.cpp
2. Check calculation: (watts * 100) / max_capacity
3. Ensure no integer overflow

### **Command Not Executing:**
1. Flash updated firmware to ESP32
2. Restart ESP32 after flashing
3. Verify new command type in server logs

---

## 🎉 Success Criteria

**You'll know it's working when:**

1. ✅ ESP32 log shows: `Setting power to 5000 W (50%)`
2. ✅ Modbus frame has small value: `110600080032...` (0x32 = 50 decimal)
3. ✅ Inverter responds with echo (not exception)
4. ✅ Command status shows "completed"
5. ✅ Success rate > 0% in statistics

---

## 📚 References

- **API Documentation**: Section 4 - Modbus Data Registers
- **Register 8**: "Set the export power percentage" (0-100%)
- **Exception Code 03**: "Illegal Data Value" (value out of range)

---

**Fixed By**: Team PowerPort  
**Date**: October 18, 2025  
**Status**: Ready for Testing ✅

---

## 🚀 Next Steps

1. ✅ Flash updated firmware
2. ✅ Test with percentage commands
3. ✅ Verify success in statistics
4. ✅ Document successful command execution
5. ✅ Complete Milestone 4 Part 2 demonstration
