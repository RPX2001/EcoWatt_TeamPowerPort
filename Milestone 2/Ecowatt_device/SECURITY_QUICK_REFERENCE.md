# EcoWatt Security Layer - Quick Reference Guide
## How Security Works When Uploading Compressed Data

---

## 🔄 Complete Data Flow Diagram

```
┌──────────────────────────────────────────────────────────────────┐
│  STEP 1: SENSOR DATA COLLECTION                                  │
├──────────────────────────────────────────────────────────────────┤
│  ESP32 reads sensor registers:                                   │
│  • VAC1 (Voltage) = 230V                                         │
│  • IAC1 (Current) = 5.2A                                         │
│  • IPV1 (PV Current) = 3.1A                                      │
│  • PAC (Power) = 1196W                                           │
│  • IPV2 (PV Current 2) = 2.8A                                    │
│  • TEMP (Temperature) = 45°C                                     │
│                                                                   │
│  Raw Data: [230, 52, 31, 1196, 28, 45] (12 bytes)               │
└──────────────────────────────────────────────────────────────────┘
                            ↓
┌──────────────────────────────────────────────────────────────────┐
│  STEP 2: SMART COMPRESSION                                       │
├──────────────────────────────────────────────────────────────────┤
│  Apply adaptive compression algorithm:                           │
│  • Analyzes data patterns                                        │
│  • Selects optimal method (Dictionary/Temporal/Semantic)         │
│  • Compresses data                                               │
│                                                                   │
│  Original Size: 120 bytes (10 samples × 6 registers)             │
│  Compressed Size: 45 bytes                                       │
│  Compression Ratio: 62.5% savings                                │
│  Method Used: "Dictionary-RLE"                                   │
└──────────────────────────────────────────────────────────────────┘
                            ↓
┌──────────────────────────────────────────────────────────────────┐
│  STEP 3: CREATE JSON PAYLOAD                                     │
├──────────────────────────────────────────────────────────────────┤
│  {                                                                │
│    "device_id": "ESP32_EcoWatt_Smart",                           │
│    "timestamp": 1729800000,                                      │
│    "compressed_data": [{                                         │
│      "compressed_binary": "eJxTYGBg4GD4z8DwH4j+A...",           │
│      "decompression_metadata": {                                 │
│        "method": "Dictionary-RLE",                               │
│        "original_size_bytes": 120,                               │
│        "compressed_size_bytes": 45,                              │
│        "register_count": 6                                       │
│      },                                                           │
│      "performance_metrics": {                                    │
│        "compression_ratio": 0.375,                               │
│        "savings_percent": 62.5                                   │
│      }                                                            │
│    }]                                                             │
│  }                                                                │
│                                                                   │
│  JSON Size: ~450 bytes                                           │
└──────────────────────────────────────────────────────────────────┘
                            ↓
╔══════════════════════════════════════════════════════════════════╗
║  🔒 STEP 4: APPLY SECURITY LAYER (THE KEY STEP!)                ║
╠══════════════════════════════════════════════════════════════════╣
║                                                                   ║
║  ┌────────────────────────────────────────────────────┐         ║
║  │ 4.1: Get Next Nonce (Anti-Replay)                  │         ║
║  │     currentNonce = 42 (stored in flash)            │         ║
║  │     nonce = currentNonce++                         │         ║
║  │     Save to Preferences (persistent storage)       │         ║
║  └────────────────────────────────────────────────────┘         ║
║                          ↓                                        ║
║  ┌────────────────────────────────────────────────────┐         ║
║  │ 4.2: Optional Encryption (if enabled)              │         ║
║  │     payload = mockEncrypt(jsonPayload)             │         ║
║  │     • Base64 encode                                │         ║
║  │     • Apply character shifting                     │         ║
║  └────────────────────────────────────────────────────┘         ║
║                          ↓                                        ║
║  ┌────────────────────────────────────────────────────┐         ║
║  │ 4.3: Calculate HMAC-SHA256                         │         ║
║  │     data = "42:{compressed_json_payload}"          │         ║
║  │     hmac = HMAC-SHA256(data, PSK)                  │         ║
║  │     mac_hex = "a1b2c3d4e5f6...32bytes"             │         ║
║  │                                                     │         ║
║  │     Uses 256-bit Pre-Shared Key (PSK):             │         ║
║  │     "4A6F486E20446F652041455336342D536563..."       │         ║
║  └────────────────────────────────────────────────────┘         ║
║                          ↓                                        ║
║  ┌────────────────────────────────────────────────────┐         ║
║  │ 4.4: Create Secured JSON Wrapper                   │         ║
║  │     {                                               │         ║
║  │       "nonce": 42,                                 │         ║
║  │       "encrypted": false,                          │         ║
║  │       "payload": "{compressed_json...}",           │         ║
║  │       "mac": "a1b2c3d4e5f6...64-hex-chars"         │         ║
║  │     }                                               │         ║
║  └────────────────────────────────────────────────────┘         ║
║                                                                   ║
║  Secured Payload Size: ~550 bytes (+100 bytes overhead)          ║
╚══════════════════════════════════════════════════════════════════╝
                            ↓
┌──────────────────────────────────────────────────────────────────┐
│  STEP 5: UPLOAD TO CLOUD SERVER                                  │
├──────────────────────────────────────────────────────────────────┤
│  HTTP POST to http://server:5001/process                         │
│  Content-Type: application/json                                  │
│                                                                   │
│  Secured payload sent over network →                             │
└──────────────────────────────────────────────────────────────────┘
                            ↓
╔══════════════════════════════════════════════════════════════════╗
║  🛡️ STEP 6: SERVER-SIDE VERIFICATION                            ║
╠══════════════════════════════════════════════════════════════════╣
║                                                                   ║
║  ┌────────────────────────────────────────────────────┐         ║
║  │ 6.1: Parse Secured Message                         │         ║
║  │     Extract: nonce, payload, mac, encrypted flag   │         ║
║  └────────────────────────────────────────────────────┘         ║
║                          ↓                                        ║
║  ┌────────────────────────────────────────────────────┐         ║
║  │ 6.2: Validate Nonce (Anti-Replay Check)            │         ║
║  │     if (nonce <= last_valid_nonce):                │         ║
║  │         ❌ REJECT - Replay attack!                 │         ║
║  │     else:                                           │         ║
║  │         ✅ Accept nonce                            │         ║
║  │         last_valid_nonce = nonce                   │         ║
║  └────────────────────────────────────────────────────┘         ║
║                          ↓                                        ║
║  ┌────────────────────────────────────────────────────┐         ║
║  │ 6.3: Verify HMAC (Integrity Check)                 │         ║
║  │     data = "42:{payload}"                          │         ║
║  │     calculated_mac = HMAC-SHA256(data, PSK)        │         ║
║  │                                                     │         ║
║  │     if (calculated_mac == received_mac):           │         ║
║  │         ✅ PASS - Message authentic & intact       │         ║
║  │     else:                                           │         ║
║  │         ❌ FAIL - Message tampered!                │         ║
║  └────────────────────────────────────────────────────┘         ║
║                          ↓                                        ║
║  ┌────────────────────────────────────────────────────┐         ║
║  │ 6.4: Decrypt if Encrypted                          │         ║
║  │     if (encrypted == true):                        │         ║
║  │         payload = mockDecrypt(payload)             │         ║
║  └────────────────────────────────────────────────────┘         ║
║                          ↓                                        ║
║  ┌────────────────────────────────────────────────────┐         ║
║  │ 6.5: Extract Original Payload                      │         ║
║  │     original_json = parse(payload)                 │         ║
║  │     ✅ Security verification complete!             │         ║
║  └────────────────────────────────────────────────────┘         ║
╚══════════════════════════════════════════════════════════════════╝
                            ↓
┌──────────────────────────────────────────────────────────────────┐
│  STEP 7: DECOMPRESS & PROCESS DATA                               │
├──────────────────────────────────────────────────────────────────┤
│  • Extract compressed binary (Base64 decode)                     │
│  • Apply decompression based on method                           │
│  • Recover original sensor readings                              │
│  • Store in database                                             │
│                                                                   │
│  ✅ Data successfully received, verified, and processed!         │
└──────────────────────────────────────────────────────────────────┘
```

---

## 🔑 Key Security Components

### 1️⃣ **HMAC-SHA256 Authentication**
```
Purpose: Ensures data integrity and authenticity

Input:  "42:{compressed_data_json}"
Key:    256-bit Pre-Shared Key (PSK)
Output: 32-byte HMAC tag (64 hex characters)

Example HMAC: a1b2c3d4e5f67890abcdef1234567890abcdef1234567890abcdef1234567890
```

### 2️⃣ **Sequential Nonce System**
```
Message 1: nonce = 1   ✅ Accepted (1 > 0)
Message 2: nonce = 2   ✅ Accepted (2 > 1)
Message 3: nonce = 3   ✅ Accepted (3 > 2)
Message 1: nonce = 1   ❌ REJECTED (replay attack - 1 ≤ 3)

Stored in ESP32 flash → Survives reboots!
```

### 3️⃣ **Mock Encryption (Optional)**
```
Step 1: Base64 Encode
  "Hello" → "SGVsbG8="

Step 2: Character Shift (+3)
  "SGVsbG8=" → "VJHvvEr8="

Decryption: Reverse process (shift -3, then decode)
```

---

## 📊 Security Layer Impact

### Performance Metrics
```
┌──────────────────────────────┬──────────┬────────────┐
│ Operation                    │ Time     │ Impact     │
├──────────────────────────────┼──────────┼────────────┤
│ Data Compression             │ 15-30 ms │ Base       │
│ Security Layer (HMAC + JSON) │  1-2 ms  │ +5%        │
│ Network Upload               │ 50-100ms │ Variable   │
├──────────────────────────────┼──────────┼────────────┤
│ Total Cycle Time             │ 66-132ms │ Minimal    │
└──────────────────────────────┴──────────┴────────────┘

✅ Security overhead: < 5% of total time
```

### Bandwidth Impact
```
┌──────────────────────────────┬───────────┐
│ Original Compressed Payload  │ 450 bytes │
│ Security Overhead            │ 100 bytes │
│ (nonce + MAC + JSON wrapper) │           │
├──────────────────────────────┼───────────┤
│ Total Secured Payload        │ 550 bytes │
│ Overhead Percentage          │ ~22%      │
└──────────────────────────────┴───────────┘

✅ Acceptable for IoT bandwidth constraints
```

---

## 🛡️ What Each Security Feature Protects Against

```
┌─────────────────────────┬────────────────────┬────────────┐
│ Threat                  │ Protection         │ Status     │
├─────────────────────────┼────────────────────┼────────────┤
│ Data Tampering          │ HMAC-SHA256        │ ✅ Strong  │
│ Replay Attacks          │ Sequential Nonce   │ ✅ Strong  │
│ Unauthorized Access     │ PSK Authentication │ ✅ Strong  │
│ Man-in-the-Middle       │ HMAC + PSK         │ ✅ Strong  │
│ Message Injection       │ HMAC + Nonce       │ ✅ Strong  │
│ Eavesdropping           │ Mock Encryption    │ ⚠️  Weak   │
└─────────────────────────┴────────────────────┴────────────┘
```

---

## 💻 Code Implementation Highlights

### ESP32 Side (Security Application)
```cpp
// Initialize security layer once in setup()
SimpleSecurity security;
const char* PSK = "4A6F486E20446F652041455336342D...";
security.begin(PSK);

// Before uploading compressed data
String jsonPayload = createCompressedDataJSON();
String securedPayload = security.secureMessage(jsonPayload);

// Upload secured payload
http.POST(securedPayload);
```

### Server Side (Security Verification)
```python
def verify_secured_message(secured_msg):
    # Parse JSON
    nonce = secured_msg['nonce']
    payload = secured_msg['payload']
    received_mac = secured_msg['mac']
    
    # Check nonce
    if nonce <= last_valid_nonce:
        return False, "Replay attack"
    
    # Verify HMAC
    calculated_mac = hmac.new(PSK, f"{nonce}:{payload}", sha256).hexdigest()
    if received_mac != calculated_mac:
        return False, "Tampered"
    
    # Update nonce
    last_valid_nonce = nonce
    
    return True, payload  # Original data
```

---

## 🎯 Summary

The EcoWatt security layer wraps your compressed sensor data with:

1. **🔢 Nonce** - Prevents replay attacks, increments with each message
2. **🔐 HMAC** - Cryptographic signature proving authenticity
3. **📦 Wrapper** - JSON structure containing all security metadata
4. **💾 Persistence** - Nonce survives reboots via ESP32 flash storage

**Result**: Your compressed data arrives at the server:
✅ Authenticated (from legitimate device)  
✅ Intact (not modified in transit)  
✅ Fresh (not replayed from old capture)  
✅ Efficient (minimal overhead)

---

**For complete technical details, see**: `SECURITY_LAYER_DOCUMENTATION.md`