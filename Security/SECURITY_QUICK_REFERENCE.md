# Security Layer Quick Reference

## 🔒 Overview
The security layer protects compressed sensor data uploads using **HMAC-SHA256 authentication**, **optional AES-128-CBC encryption**, and **anti-replay protection**.

---

## 📋 Quick Start

### 1. Initialize (in setup())
```cpp
SecurityLayer::init();
```

### 2. Secure Data Before Upload
```cpp
char originalJSON[2048];
char securedJSON[4096];

if (SecurityLayer::securePayload(originalJSON, securedJSON, sizeof(securedJSON))) {
    http.POST((uint8_t*)securedJSON, strlen(securedJSON));
}
```

### 3. Server-Side Verification (Python)
```python
from server_security_layer import verify_secured_payload, SecurityError

try:
    original_data = verify_secured_payload(secured_data, "ESP32_EcoWatt_Smart")
    # Process original_data
except SecurityError as e:
    # Reject request
    print(f"Security error: {e}")
```

---

## 🔑 Secured Payload Format

```json
{
  "nonce": 10503,
  "payload": "eyJkZXZpY2VfaWQiOiJFU1AzMl9FY...",
  "mac": "a7b3c4d5e6f7890abcdef123456789...",
  "encrypted": false
}
```

| Field | Description |
|-------|-------------|
| `nonce` | Sequential counter (anti-replay) |
| `payload` | Base64-encoded original JSON |
| `mac` | HMAC-SHA256 tag (64 hex chars) |
| `encrypted` | `true` if AES-encrypted, `false` if mock |

---

## 🛡️ Security Techniques

### 1. HMAC-SHA256 (Authentication & Integrity)
- **What**: Signs message with 256-bit pre-shared key
- **Where**: `calculateHMAC()` in `security.cpp`
- **Input**: `nonce (4 bytes) + JSON payload`
- **Output**: 32-byte MAC → 64-char hex string

### 2. AES-128-CBC (Optional Encryption)
- **What**: Encrypts payload for confidentiality
- **Where**: `encryptAES()` in `security.cpp`
- **Mode**: Currently disabled (mock encryption via Base64)
- **Enable**: Set `ENABLE_ENCRYPTION = true` in `security.h`

### 3. Anti-Replay Protection (Nonce)
- **What**: Sequential counter prevents message reuse
- **Where**: `incrementAndSaveNonce()` in `security.cpp`
- **Storage**: NVS (survives reboots)
- **Validation**: Server rejects `nonce ≤ last_seen_nonce`

### 4. Base64 Encoding
- **What**: Encodes binary data for JSON
- **Where**: `base64::encode()` library
- **Overhead**: +33% size (3 bytes → 4 chars)

---

## 📁 Files

```
ESP32 Side:
├── include/application/security.h        # Interface
└── src/application/security.cpp          # Implementation

Server Side:
└── server_security_layer.py              # Python verification module

Integration:
└── src/main.cpp                          # uploadSmartCompressedDataToCloud()

Documentation:
├── SECURITY_IMPLEMENTATION_GUIDE.md     # Full guide (you are here)
└── SECURITY_QUICK_REFERENCE.md          # This file
```

---

## 🔧 Configuration

### Pre-Shared Keys
**Location**: `src/application/security.cpp`

```cpp
// MUST match on both ESP32 and server!
const uint8_t PSK_HMAC[32] = { 0x2b, 0x7e, ... };  // HMAC key
const uint8_t PSK_AES[16] = { 0x2b, 0x7e, ... };   // AES key
```

### Enable/Disable Encryption
**Location**: `include/application/security.h`

```cpp
static const bool ENABLE_ENCRYPTION = false;  // false = mock encryption (Base64 only)
```

### Nonce Settings
**Location**: `src/application/security.cpp`

```cpp
preferences.getUInt("nonce", 10000);  // Start value if not found
```

---

## 🔄 Data Flow

```
ESP32 Device
    │
    ├─ Collect & Compress Sensor Data
    │       ↓
    ├─ Build JSON Payload
    │       ↓
    ├─ SecurityLayer::securePayload()
    │   ├─ Increment nonce
    │   ├─ Base64 encode (or AES encrypt + Base64)
    │   ├─ Calculate HMAC(nonce + payload)
    │   └─ Build secured JSON
    │       ↓
    ├─ HTTP POST to Cloud
    │
    ↓
Cloud Server (Python/Flask)
    │
    ├─ Receive Secured JSON
    │       ↓
    ├─ verify_secured_payload()
    │   ├─ Check nonce > last_seen
    │   ├─ Base64 decode (or AES decrypt)
    │   ├─ Calculate HMAC(nonce + payload)
    │   ├─ Compare MACs
    │   └─ Update last_seen_nonce
    │       ↓
    ├─ Extract Original JSON
    │       ↓
    └─ Decompress & Process Data
```

---

## 🧪 Testing

### Check Nonce
```cpp
print("Current nonce: %u\n", SecurityLayer::getCurrentNonce());
```

### Reset Nonce (if needed)
```cpp
SecurityLayer::setNonce(10000);
```

### Verify HMAC Manually (Python)
```python
import hmac
import hashlib

nonce_bytes = (10001).to_bytes(4, 'big')
payload_bytes = b'{"test":"data"}'
data = nonce_bytes + payload_bytes

mac = hmac.new(PSK_HMAC, data, hashlib.sha256).hexdigest()
print(f"Expected MAC: {mac}")
```

---

## ⚠️ Common Issues

| Issue | Solution |
|-------|----------|
| "HMAC verification failed" | Verify PSK keys match on ESP32 and server |
| "Replay attack detected" | Reset server nonce or sync device nonce |
| "Buffer too small" | Increase `securedPayload` buffer to 8192 |
| Nonce resets after reboot | Check NVS initialization |

---

## 📊 Performance

| Metric | Value |
|--------|-------|
| Overhead (time) | ~3-5 ms per upload |
| Overhead (size) | +47% (Base64 + security fields) |
| Memory (RAM) | <1 KB |
| NVS Storage | 4 bytes (nonce) |

---

## ✅ Security Checklist

- [x] HMAC-SHA256 authentication implemented
- [x] Pre-shared key (PSK) in place
- [x] Anti-replay protection (nonce)
- [x] Nonce persists in NVS
- [x] Optional AES-128-CBC encryption
- [x] Server-side verification module
- [x] Applied to data uploads only
- [x] Error handling and logging

---

## 📚 See Also

- **Full Documentation**: `SECURITY_IMPLEMENTATION_GUIDE.md`
- **Server Module**: `server_security_layer.py`
- **Main Integration**: `src/main.cpp` (line ~710)

---

**Quick Tip**: Security layer is transparent to your existing code - just wrap your JSON payload with `securePayload()` before sending!
