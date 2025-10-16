# 🔧 Stack Overflow Fix - Security Layer

## Issue Identified

The ESP32 was experiencing **stack overflow** crashes with the following error:
```
***ERROR*** A stack overflow in task loopTask has been detected.
```

## Root Cause

Two issues were causing stack overflow:

### 1. **Large Stack Buffer (8192 bytes)**
```cpp
// BEFORE (Bad - causes stack overflow)
char securedPayload[8192];  // Too large for stack!
```

### 2. **Large StaticJsonDocument on Stack (8192 bytes)**
```cpp
// BEFORE (Bad - causes stack overflow)
StaticJsonDocument<8192> doc;  // Too large for stack!
```

**Total stack usage**: ~16KB+ (way too much for ESP32 task stack)

---

## Solution Applied

### Fix 1: Use Heap Allocation for Buffer
**File**: `src/main.cpp`

```cpp
// AFTER (Good - uses heap)
char* securedPayload = new char[8192];  // Heap allocation
if (!securedPayload) {
    print("Failed to allocate memory for secured payload!\n");
    return;
}

// ... use the buffer ...

delete[] securedPayload;  // Clean up
```

### Fix 2: Use DynamicJsonDocument
**File**: `src/application/security.cpp`

```cpp
// AFTER (Good - uses heap)
DynamicJsonDocument* doc = new DynamicJsonDocument(8192);
if (!doc) {
    print("Security Layer: Failed to allocate JSON document\n");
    return false;
}

(*doc)["nonce"] = nonce;
(*doc)["payload"] = encodedPayload;
(*doc)["mac"] = hmacHex;
(*doc)["encrypted"] = ENABLE_ENCRYPTION;

size_t jsonLen = serializeJson(*doc, securedPayload, securedPayloadSize);

delete doc;  // Clean up
```

---

## Memory Impact

### Before (Stack):
- Stack buffer: 8192 bytes
- StaticJsonDocument: 8192 bytes
- **Total stack: ~16KB** ❌ (Exceeds ESP32 task stack limit)

### After (Heap):
- Heap buffer: 8192 bytes
- DynamicJsonDocument: 8192 bytes
- **Total heap: ~16KB** ✅ (ESP32 has 240KB free heap)
- **Stack usage: <1KB** ✅

---

## Changes Made

### 1. `PIO/ECOWATT/src/main.cpp`
```diff
- char securedPayload[8192];
+ char* securedPayload = new char[8192];
+ if (!securedPayload) {
+     print("Failed to allocate memory!\n");
+     return;
+ }
  
  // ... use buffer ...
  
+ delete[] securedPayload;
```

### 2. `PIO/ECOWATT/src/application/security.cpp`
```diff
- StaticJsonDocument<8192> doc;
- doc["nonce"] = nonce;
+ DynamicJsonDocument* doc = new DynamicJsonDocument(8192);
+ if (!doc) {
+     return false;
+ }
+ (*doc)["nonce"] = nonce;
  
- size_t jsonLen = serializeJson(doc, ...);
+ size_t jsonLen = serializeJson(*doc, ...);
+ delete doc;
```

---

## Testing

### Expected Results After Fix:

✅ **No more stack overflow errors**
✅ **No more rebooting loops**
✅ **Successful data upload with security layer**
✅ **Nonce increments normally**

### Serial Monitor Should Show:
```
Security Layer: Initialized with nonce = 10000
...
Register mapping built: 9 registers
UPLOADING COMPRESSED DATA TO FLASK SERVER
Security Layer: Payload secured with nonce 10001 (size: XXXX bytes)
Security Layer: Payload secured successfully
Upload successful to Flask server!
```

### What Should NOT Appear:
```
***ERROR*** A stack overflow in task loopTask has been detected.  ❌
Backtrace: ...  ❌
Rebooting...  ❌
```

---

## Build and Upload

```bash
cd "d:\Documents\UoM\Semester 7\Embedded\Firmware\PIO\ECOWATT"
pio run --target upload
pio device monitor
```

---

## Why This Fix Works

### ESP32 Task Stack Limitations
- Default FreeRTOS task stack: **8KB - 16KB**
- Our previous usage: **~16KB+** (overflow!)
- After fix: **<1KB** (safe!)

### Heap vs Stack on ESP32
- **Stack**: Limited per task, fixed size, faster access
- **Heap**: Shared pool, ~240KB available, slower but flexible

### Best Practice
- Use stack for: Small variables (<100 bytes)
- Use heap for: Large buffers, dynamic data, JSON documents

---

## Additional Improvements Made

### Better Error Handling
```cpp
// Check allocation success
if (!securedPayload) {
    print("Failed to allocate memory!\n");
    return;
}

// Ensure proper cleanup
delete[] securedPayload;
```

### More Detailed Logging
```cpp
print("Security Layer: Payload secured with nonce %u (size: %zu bytes)\n", 
      nonce, jsonLen);
```

---

## Status

✅ **Fix Applied**
✅ **Compilation Ready**
✅ **Ready to Upload and Test**

The security layer should now work without stack overflow issues!

---

**Fix Applied**: October 16, 2025
**Files Modified**: 2 (main.cpp, security.cpp)
**Issue**: Stack overflow due to large stack allocations
**Solution**: Heap allocation with proper cleanup
