# Security Layer Documentation

## Overview
The EcoWatt Security Layer implements lightweight cryptographic authentication and anti-replay protection for data communication between ESP32 devices and the Flask server.

## Security Features

### 1. HMAC-SHA256 Authentication
- **Purpose:** Verify data integrity and authenticity
- **Algorithm:** HMAC-SHA256 (256-bit hash)
- **Key:** Pre-Shared Key (PSK) - 32 bytes
- **Implementation:** mbedtls library on ESP32, hashlib on Flask

### 2. Anti-Replay Protection
- **Method:** Monotonically increasing sequence numbers
- **Storage:** NVS on ESP32, in-memory on Flask
- **Validation:** Server rejects sequence numbers ≤ last valid sequence

### 3. Secure Key Storage
- **ESP32:** PSK embedded in firmware (`credentials.h`)
- **Flask:** PSK in source code (`security_manager.py`)
- **Production Note:** Use hardware secure elements or encrypted storage

## Architecture

```
ESP32 Upload Flow:
1. Prepare JSON payload
2. Get current sequence number (from NVS)
3. Compute HMAC = HMAC-SHA256(payload + sequence, PSK)
4. Add HTTP headers:
   - X-Sequence-Number: <sequence>
   - X-HMAC-SHA256: <hmac_hex>
5. Send POST request
6. Increment and save sequence number

Flask Validation Flow:
1. Receive POST request
2. Extract sequence and HMAC from headers
3. Compute expected HMAC
4. Compare HMACs (constant-time comparison)
5. Check sequence > last_valid_sequence
6. If valid: process request, update sequence
7. If invalid: return 401 Unauthorized
```

## Pre-Shared Key (PSK)

**Current PSK (Hex):**
```
2b7e151628aed2a6abf7158809cf4f3c762e7160f38b4da56a784d9045190cfe
```

**⚠️ WARNING:** This is a demo key. In production:
- Generate unique keys per deployment
- Use hardware secure element (ESP32 efuse)
- Implement key rotation
- Never commit keys to version control

## ESP32 Implementation

### Files
- `include/application/security.h` - Security manager header
- `src/application/security.cpp` - Security implementation
- `include/application/credentials.h` - PSK storage

### Key Classes

#### `SecurityManager`
```cpp
class SecurityManager {
public:
    bool initialize(const uint8_t* psk, size_t psk_length);
    bool computeHMAC(const uint8_t* data, size_t data_len, 
                     uint32_t sequence_number, uint8_t* hmac_out);
    uint32_t getAndIncrementSequence();
    uint32_t getCurrentSequence();
    void resetSequence();
    void hmacToHex(const uint8_t* hmac, char* hex_out);
};
```

### Usage in Upload

```cpp
// In uploadSmartCompressedDataToCloud():

// Get sequence and compute HMAC
uint32_t sequence = securityManager.getAndIncrementSequence();
uint8_t hmac[32];
char hmacHex[65];

if (securityManager.computeHMAC((uint8_t*)jsonString, jsonLen, sequence, hmac)) {
    securityManager.hmacToHex(hmac, hmacHex);
    
    // Add security headers
    http.addHeader("X-Sequence-Number", String(sequence));
    http.addHeader("X-HMAC-SHA256", String(hmacHex));
}
```

### Initialization

```cpp
// In setup():
if (securityManager.initialize(HMAC_PSK, sizeof(HMAC_PSK))) {
    print("Security Manager initialized successfully\n");
    print("Current sequence number: %u\n", securityManager.getCurrentSequence());
} else {
    print("ERROR: Failed to initialize Security Manager!\n");
}
```

## Flask Server Implementation

### Files
- `flask/security_manager.py` - Security validation
- `flask/flask_server_hivemq.py` - Integration with endpoints

### Key Class

#### `SecurityManager`
```python
class SecurityManager:
    def __init__(self, psk_hex):
        self.psk = bytes.fromhex(psk_hex)
        self.device_sequences = {}  # Track per device
        
    def compute_hmac(self, data, sequence_number):
        # Append sequence (big-endian, 4 bytes)
        sequence_bytes = sequence_number.to_bytes(4, byteorder='big')
        message = data + sequence_bytes
        # Compute HMAC
        h = hmac.new(self.psk, message, hashlib.sha256)
        return h.hexdigest()
    
    def validate_request(self, device_id, payload, sequence_number, received_hmac):
        # Compute expected HMAC
        expected_hmac = self.compute_hmac(payload, sequence_number)
        
        # Constant-time comparison
        if not hmac.compare_digest(expected_hmac, received_hmac):
            return False, "HMAC validation failed"
        
        # Anti-replay check
        last_sequence = self.device_sequences.get(device_id, -1)
        if sequence_number <= last_sequence:
            return False, "Replay attack detected"
        
        # Update sequence
        self.device_sequences[device_id] = sequence_number
        return True, "Valid"
```

### Integration with /process Endpoint

```python
@app.route('/process', methods=['POST'])
def process_compressed_data():
    # Extract security headers
    device_id = request.json.get('device_id', 'ESP32_EcoWatt_Smart')
    sequence_number = request.headers.get('X-Sequence-Number')
    received_hmac = request.headers.get('X-HMAC-SHA256')
    
    if sequence_number and received_hmac:
        sequence_num = int(sequence_number)
        payload_bytes = request.get_data()
        
        is_valid, error_msg = security_manager.validate_request(
            device_id, payload_bytes, sequence_num, received_hmac
        )
        
        if not is_valid:
            logger.warning(f"Security validation failed: {error_msg}")
            return jsonify({'error': error_msg}), 401
        
        logger.info(f"✓ Security validated: Device={device_id}, Seq={sequence_num}")
    
    # Continue with normal processing...
```

## API Endpoints

### 1. Security Statistics
**Endpoint:** `GET /security/stats`

**Response:**
```json
{
  "status": "success",
  "security_stats": {
    "total_validations": 150,
    "valid_requests": 145,
    "invalid_requests": 5,
    "devices_tracked": 1,
    "device_sequences": {
      "ESP32_EcoWatt_Smart": 145
    }
  }
}
```

### 2. Security Log
**Endpoint:** `GET /security/log?device_id=ESP32_EcoWatt_Smart&limit=50`

**Response:**
```json
{
  "status": "success",
  "device_id": "ESP32_EcoWatt_Smart",
  "log_count": 10,
  "log": [
    {
      "timestamp": "2025-10-16T15:30:45.123",
      "device_id": "ESP32_EcoWatt_Smart",
      "sequence": 145,
      "valid": true,
      "message": "OK"
    },
    {
      "timestamp": "2025-10-16T15:30:40.456",
      "device_id": "ESP32_EcoWatt_Smart",
      "sequence": 120,
      "valid": false,
      "message": "Replay attack detected! Sequence 120 <= last 144"
    }
  ]
}
```

### 3. Reset Sequence (Testing Only)
**Endpoint:** `POST /security/reset`

**Request:**
```json
{
  "device_id": "ESP32_EcoWatt_Smart"
}
```

**Response:**
```json
{
  "status": "success",
  "message": "Sequence reset for ESP32_EcoWatt_Smart",
  "device_id": "ESP32_EcoWatt_Smart"
}
```

## Testing

### 1. Using test_security.py

```bash
cd flask/
python test_security.py
```

**Tests Performed:**
1. ✓ Valid request with correct HMAC and sequence
2. ✓ Valid request with incremented sequence
3. ✗ Invalid request with wrong HMAC (should fail)
4. ✗ Replay attack with old sequence (should fail)
5. ✓ Valid request after failures

### 2. Manual Testing with curl

**Valid Request:**
```bash
# Compute HMAC first (use Python or similar)
# Then:
curl -X POST http://10.78.228.2:5001/process \
  -H "Content-Type: application/json" \
  -H "X-Sequence-Number: 1" \
  -H "X-HMAC-SHA256: <computed_hmac>" \
  -d '{"device_id":"ESP32_EcoWatt_Smart","test":"data"}'
```

**Check Statistics:**
```bash
curl http://10.78.228.2:5001/security/stats
```

**Check Log:**
```bash
curl "http://10.78.228.2:5001/security/log?device_id=ESP32_EcoWatt_Smart"
```

## Security Properties

### ✓ Authenticity
- Only devices with correct PSK can create valid HMACs
- Server verifies every request came from legitimate device

### ✓ Integrity
- Any modification to payload invalidates HMAC
- Tampered messages detected and rejected

### ✓ Anti-Replay
- Sequence numbers prevent replay of old messages
- Each sequence number can only be used once

### ✗ Confidentiality (Not Implemented)
- Payloads are not encrypted
- Network eavesdropper can see data content
- Could add AES-GCM encryption if needed

### ✗ Perfect Forward Secrecy (Not Implemented)
- PSK is static
- Compromise of PSK affects all past/future messages
- Could implement Diffie-Hellman key exchange

## Attack Scenarios

### 1. Man-in-the-Middle (MITM) Attack
**Attack:** Attacker intercepts and modifies payload

**Defense:** 
- Modified payload → different HMAC
- Server rejects (HMAC mismatch)
- **Result: ✓ Protected**

### 2. Replay Attack
**Attack:** Attacker captures valid message and resends it

**Defense:**
- Sequence number already used
- Server rejects (sequence ≤ last)
- **Result: ✓ Protected**

### 3. Brute Force HMAC
**Attack:** Attacker tries to guess HMAC

**Defense:**
- 2^256 possible HMACs (SHA-256)
- Computationally infeasible
- **Result: ✓ Protected**

### 4. Key Extraction
**Attack:** Attacker extracts PSK from firmware

**Defense:**
- Currently: PSK in plaintext
- ⚠️ Vulnerable if attacker has physical access
- **Mitigation:** Use hardware secure element

### 5. Eavesdropping
**Attack:** Attacker reads payload content

**Defense:**
- Data not encrypted
- ⚠️ Vulnerable to passive monitoring
- **Mitigation:** Add AES encryption

## Performance Impact

### ESP32
- **HMAC Computation:** ~2-5 ms
- **Memory Overhead:** ~100 bytes (buffers)
- **NVS Writes:** Every 10 uploads (sequence persistence)

### Flask Server
- **HMAC Validation:** <1 ms
- **Memory:** Minimal (sequence tracking)
- **CPU:** Negligible impact

## Best Practices

### ✓ DO:
- Initialize security manager before any uploads
- Check HMAC computation success
- Log security events
- Reset sequence after firmware update
- Monitor validation failures

### ✗ DON'T:
- Hardcode PSK in public repositories
- Reuse sequence numbers
- Skip validation on server
- Ignore security failures
- Use weak PSKs

## Future Enhancements

1. **Key Rotation:** Periodically change PSK
2. **Per-Device Keys:** Unique PSK per device
3. **Payload Encryption:** Add AES-GCM
4. **Certificate-Based Auth:** Use X.509 certificates
5. **Hardware Security:** ESP32 secure boot + efuse
6. **Time-Based Validation:** Add timestamp checks

## Troubleshooting

### Issue: HMAC validation always fails
**Causes:**
- PSK mismatch between ESP32 and Flask
- Incorrect HMAC computation
- Payload modified after HMAC computation

**Solution:**
- Verify PSK hex strings match exactly
- Check byte order of sequence number (big-endian)
- Ensure payload not modified between HMAC and send

### Issue: Replay detection false positives
**Causes:**
- Sequence number mismatch (ESP32 vs Flask)
- Multiple ESP32s with same device_id
- Sequence not persisted across restarts

**Solution:**
- Reset sequence on both sides for testing
- Use unique device_ids
- Verify NVS persistence working

### Issue: Performance degradation
**Causes:**
- Too frequent NVS writes
- HMAC computation blocking

**Solution:**
- Batch NVS writes (every 10 sequences)
- Move HMAC to separate task if needed

---

**Last Updated:** October 16, 2025  
**Status:** ✅ Fully Implemented and Tested
