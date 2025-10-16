# Security Layer Data Flow Diagram

## Complete Data Flow: ESP32 → Flask Server

```
┌─────────────────────────────────────────────────────────────────────────┐
│                           ESP32 DEVICE                                   │
└─────────────────────────────────────────────────────────────────────────┘
                                   │
                                   ▼
                    ┌──────────────────────────┐
                    │   Read Sensor Data       │
                    │   (6 registers)          │
                    └──────────────────────────┘
                                   │
                                   ▼
                    ┌──────────────────────────┐
                    │   Smart Compression      │
                    │   (Dictionary/Temporal)  │
                    └──────────────────────────┘
                                   │
                                   ▼
                    ┌──────────────────────────┐
                    │   Create JSON Payload    │
                    │   {compressed_data, ...} │
                    └──────────────────────────┘
                                   │
                    ┌──────────────┴───────────────┐
                    │   SECURITY LAYER (ESP32)     │
                    ├──────────────────────────────┤
                    │ 1. Increment nonce           │
                    │ 2. Base64 encode payload     │
                    │ 3. Calculate HMAC-SHA256     │
                    │    over (nonce + payload)    │
                    │ 4. Create secured JSON:      │
                    │    {nonce, payload, mac}     │
                    └──────────────┬───────────────┘
                                   │
                                   ▼
                    ┌──────────────────────────┐
                    │   HTTP POST to Flask     │
                    │   /process endpoint      │
                    └──────────────────────────┘
                                   │
                                   │ Network
                                   │
┌─────────────────────────────────────────────────────────────────────────┐
│                         FLASK SERVER                                     │
└─────────────────────────────────────────────────────────────────────────┘
                                   │
                                   ▼
                    ┌──────────────────────────┐
                    │   Receive POST Request   │
                    │   at /process            │
                    └──────────────────────────┘
                                   │
                                   ▼
                    ┌──────────────────────────┐
                    │   Parse JSON             │
                    │   Check for security     │
                    │   fields (nonce, mac)    │
                    └──────────────────────────┘
                                   │
                    ┌──────────────┴───────────────┐
                    │                              │
               YES  │   Has security layer?        │  NO
                    │                              │
        ┌───────────┴──────────┐         ┌────────┴────────┐
        │                      │         │                 │
        ▼                      │         ▼                 │
┌──────────────────┐           │   ┌──────────────────┐   │
│ SECURITY LAYER   │           │   │ Use data as-is   │   │
│ VERIFICATION     │           │   │ (unsecured)      │   │
├──────────────────┤           │   └────────┬─────────┘   │
│ 1. Check nonce   │           │            │             │
│    > last_nonce  │           │            │             │
│ 2. Base64 decode │           │            │             │
│ 3. Decrypt AES   │           │            │             │
│    (if enabled)  │           │            │             │
│ 4. Verify HMAC   │           │            │             │
│ 5. Extract JSON  │           │            │             │
└────────┬─────────┘           │            │             │
         │                     │            │             │
         ▼                     │            │             │
    ┌────────┐                │            │             │
    │ Valid? │                │            │             │
    └───┬────┘                │            │             │
        │                     │            │             │
   YES  │          NO         │            │             │
        │          │          │            │             │
        ▼          ▼          │            │             │
┌──────────┐  ┌──────────┐   │            │             │
│ Extract  │  │ Return   │   │            │             │
│ Payload  │  │ 401      │   │            │             │
└────┬─────┘  │ Error    │   │            │             │
     │        └──────────┘   │            │             │
     └────────────────────────┼────────────┘             │
                              │                          │
                              ▼                          │
                   ┌──────────────────────┐              │
                   │  Decompression       │              │
                   │  (Smart/Dictionary)  │              │
                   └──────────────────────┘              │
                              │                          │
                              ▼                          │
                   ┌──────────────────────┐              │
                   │  Process Register    │              │
                   │  Data & Mapping      │              │
                   └──────────────────────┘              │
                              │                          │
                              ▼                          │
                   ┌──────────────────────┐              │
                   │  Publish to MQTT     │              │
                   │  (HiveMQ broker)     │              │
                   └──────────────────────┘              │
                              │                          │
                              ▼                          │
                   ┌──────────────────────┐              │
                   │  Return Success      │              │
                   │  200 OK              │◄─────────────┘
                   └──────────────────────┘
```

## Security Layer Details

### Secured Payload Structure
```json
{
  "nonce": 10001,
  "payload": "eyJkZXZpY2VfaWQiOiJFU1AzMl9FY29XYXR0X1NtYXJ0IiwiY29tcHJlc3NlZF9kYXRhIjpbXX0=",
  "mac": "a1b2c3d4e5f6789...(64 hex chars)",
  "encrypted": false
}
```

### HMAC Calculation
```
Data to Sign = [nonce (4 bytes, big-endian)] + [JSON payload (UTF-8)]
HMAC-SHA256  = HMAC(PSK_HMAC, Data to Sign)
```

### Anti-Replay Protection
```
Server maintains: last_valid_nonce = 10000

Request 1: nonce = 10001 → Accept ✓ (update last_valid_nonce = 10001)
Request 2: nonce = 10002 → Accept ✓ (update last_valid_nonce = 10002)
Request 3: nonce = 10001 → Reject ✗ (replay attack!)
Request 4: nonce = 10003 → Accept ✓ (update last_valid_nonce = 10003)
```

## Error Scenarios

### 1. Invalid HMAC
```
ESP32 sends with tampered data
   ↓
Flask verifies HMAC
   ↓
HMAC mismatch
   ↓
Return 401: "HMAC verification failed"
```

### 2. Replay Attack
```
Attacker resends old packet (nonce = 10001)
   ↓
Flask checks: 10001 <= last_valid_nonce (10005)
   ↓
Return 401: "Replay attack detected"
```

### 3. Missing Security Fields
```
Malformed JSON without 'nonce' or 'mac'
   ↓
Flask detects missing fields
   ↓
Return 401: "Missing required security fields"
```

### 4. Decryption Failure
```
ESP32 sends encrypted data with wrong key
   ↓
Flask attempts AES decryption
   ↓
Decryption fails (padding error)
   ↓
Return 401: "AES decryption failed"
```

## Success Flow Summary

```
ESP32                           Flask Server
  │                                  │
  │ 1. Compress data                 │
  │ 2. Secure (HMAC)                 │
  │ 3. POST /process                 │
  ├──────────────────────────────────▶
  │                                  │ 4. Check security fields
  │                                  │ 5. Verify nonce
  │                                  │ 6. Verify HMAC
  │                                  │ 7. Extract payload
  │                                  │ 8. Decompress
  │                                  │ 9. Process data
  │                                  │ 10. Publish MQTT
  │                                  │
  │◀──────────────────────────────────┤
  │         200 OK                   │
  │                                  │
```

## Key Components

### ESP32 Side (security.cpp)
- `SecurityLayer::init()` - Initialize security
- `SecurityLayer::securePayload()` - Apply security layer
- `SecurityLayer::calculateHMAC()` - HMAC-SHA256
- `SecurityLayer::encryptAES()` - AES-128-CBC (optional)

### Flask Side (flask_server_hivemq.py)
- `verify_and_remove_security()` - Main verification function
- `hmac.new(PSK_HMAC, ...)` - HMAC verification
- `AES.new(...).decrypt()` - AES decryption (optional)
- `nonce_lock` - Thread-safe nonce tracking

## Configuration Match Required

Both ESP32 and Flask must have identical:
- ✓ PSK_HMAC (32 bytes)
- ✓ PSK_AES (16 bytes)
- ✓ AES_IV (16 bytes)
- ✓ Encryption enabled/disabled flag

If any mismatch → Authentication fails!
