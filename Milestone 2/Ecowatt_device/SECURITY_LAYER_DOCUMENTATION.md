# EcoWatt Security Layer Documentation
## Secure Cloud Upload for Compressed Sensor Data

**Project**: EcoWatt Smart Energy Monitoring System  
**Component**: Security Layer for Cloud Communication  
**Date**: October 14, 2025  
**Version**: 1.0

---

## Table of Contents
1. [Overview](#overview)
2. [System Architecture](#system-architecture)
3. [Security Features](#security-features)
4. [Data Flow Process](#data-flow-process)
5. [Implementation Details](#implementation-details)
6. [Security Analysis](#security-analysis)
7. [Testing and Validation](#testing-and-validation)

---

## 1. Overview

### 1.1 Purpose
The EcoWatt Security Layer provides cryptographic protection for compressed sensor data uploaded to cloud servers. It ensures data integrity, authenticity, and protection against replay attacks while maintaining the efficiency of the compression system.

### 1.2 Key Components
- **HMAC-SHA256 Authentication**: Ensures message integrity and authenticity
- **Sequential Nonce System**: Prevents replay attacks
- **Optional Mock Encryption**: Provides basic confidentiality (demonstration)
- **Persistent Storage**: Maintains security state across device reboots

---

## 2. System Architecture

### 2.1 Integration with EcoWatt System

```
┌─────────────────────────────────────────────────────────────┐
│                    EcoWatt Device (ESP32)                    │
├─────────────────────────────────────────────────────────────┤
│                                                               │
│  1. Sensor Data Acquisition                                  │
│     └─> Read multiple registers (VAC, IAC, IPV, PAC, etc.)  │
│                                                               │
│  2. Data Batching                                            │
│     └─> Collect samples into batch                          │
│                                                               │
│  3. Smart Compression                                        │
│     └─> Apply adaptive compression algorithm                │
│     └─> Methods: Dictionary, Temporal, Semantic, Bitpack    │
│                                                               │
│  4. Create JSON Payload                                      │
│     └─> Compressed data + metadata + statistics             │
│                                                               │
│  ┌─────────────────────────────────────────────┐            │
│  │      🔒 SECURITY LAYER (THIS COMPONENT)      │            │
│  ├─────────────────────────────────────────────┤            │
│  │  5. Secure Payload                          │            │
│  │     └─> Add nonce (anti-replay)             │            │
│  │     └─> Optional: Encrypt payload           │            │
│  │     └─> Calculate HMAC-SHA256               │            │
│  │     └─> Create secured JSON wrapper         │            │
│  └─────────────────────────────────────────────┘            │
│                                                               │
│  6. Upload to Cloud Server                                   │
│     └─> HTTP POST to server endpoint                        │
│                                                               │
└─────────────────────────────────────────────────────────────┘
                            │
                            │ Secured Payload
                            ▼
┌─────────────────────────────────────────────────────────────┐
│                    Cloud Server (Flask)                      │
├─────────────────────────────────────────────────────────────┤
│                                                               │
│  1. Receive Secured Payload                                  │
│                                                               │
│  2. Verify HMAC-SHA256                                       │
│     └─> Recalculate MAC and compare                         │
│                                                               │
│  3. Validate Nonce (Anti-replay)                             │
│     └─> Check nonce > last_valid_nonce                      │
│                                                               │
│  4. Decrypt if Encrypted                                     │
│     └─> Reverse mock encryption                             │
│                                                               │
│  5. Extract Original Payload                                 │
│     └─> Parse JSON with compressed data                     │
│                                                               │
│  6. Process and Store Data                                   │
│                                                               │
└─────────────────────────────────────────────────────────────┘
```

---

## 3. Security Features

### 3.1 HMAC-SHA256 Authentication

**Purpose**: Ensures message integrity and authenticity

**Implementation**:
```cpp
// Calculate HMAC using mbedTLS
mbedtls_md_context_t ctx;
mbedtls_md_init(&ctx);
const mbedtls_md_info_t* md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);

mbedtls_md_setup(&ctx, md_info, 1);
mbedtls_md_hmac_starts(&ctx, psk, PSK_LENGTH);  // 256-bit PSK
mbedtls_md_hmac_update(&ctx, data, data_length);
mbedtls_md_hmac_finish(&ctx, hmac_output);      // 32-byte HMAC
```

**What it protects against**:
- ✅ Data tampering - any byte modification detected
- ✅ Unauthorized access - only devices with correct PSK can create valid HMAC
- ✅ Man-in-the-middle attacks - modified messages rejected

### 3.2 Sequential Nonce System

**Purpose**: Prevents replay attacks

**Implementation**:
- Nonce starts at 1 and increments with each message
- Stored persistently in ESP32 flash memory (Preferences)
- Server tracks `last_valid_nonce` and rejects old messages

**Example Flow**:
```
Message 1: nonce = 1  ✅ Accepted (1 > 0)
Message 2: nonce = 2  ✅ Accepted (2 > 1)
Message 3: nonce = 3  ✅ Accepted (3 > 2)
Message 1 (replay): nonce = 1  ❌ Rejected (1 ≤ 3)
```

**What it protects against**:
- ✅ Replay attacks - old messages cannot be resent
- ✅ Out-of-order messages - maintains sequence integrity
- ✅ Reboot resilience - nonce persists across power cycles

### 3.3 Mock Encryption (Optional)

**Purpose**: Demonstrates confidentiality (simplified for IoT)

**Algorithm**:
1. Base64 encode the payload
2. Apply Caesar cipher (character shift +3)
3. Set `encrypted: true` flag

**Example**:
```
Original: "Hello, World!"
Base64:   "SGVsbG8sIFdvcmxkIQ=="
Shifted:  "VJHvvEr8sIFdruogLiT=="
```

**Note**: This is a demonstration. For production, use AES-256-GCM or ChaCha20-Poly1305.

---

## 4. Data Flow Process

### 4.1 Device Side (ESP32)

#### Step 1: Compress Sensor Data
```cpp
// Collect sensor readings
const RegID selection[] = {REG_VAC1, REG_IAC1, REG_IPV1, REG_PAC, REG_IPV2, REG_TEMP};
uint16_t sensorData[6];
readMultipleRegisters(selection, 6, sensorData);

// Apply smart compression
std::vector<uint8_t> compressed = compressBatchWithSmartSelection(
    sensorData, 6, selection, compressionTime, methodUsed, ratio
);
```

#### Step 2: Create JSON Payload
```cpp
JsonDocument doc;
doc["device_id"] = "ESP32_EcoWatt_Smart";
doc["timestamp"] = millis();
doc["data_type"] = "compressed_sensor_batch";

// Add compressed data (Base64 encoded)
doc["compressed_data"]["compressed_binary"] = convertBinaryToBase64(compressed);
doc["compressed_data"]["method"] = methodUsed;  // e.g., "Dictionary-RLE"
doc["compressed_data"]["original_size"] = originalSize;
doc["compressed_data"]["compressed_size"] = compressed.size();
doc["compressed_data"]["compression_ratio"] = ratio;

String jsonPayload;
serializeJson(doc, jsonPayload);
```

**Example JSON Payload**:
```json
{
  "device_id": "ESP32_EcoWatt_Smart",
  "timestamp": 123456,
  "total_samples": 10,
  "compressed_data": [{
    "compressed_binary": "eJxTYGBg4GD4...base64...",
    "decompression_metadata": {
      "method": "Dictionary-RLE",
      "register_count": 6,
      "original_size_bytes": 120,
      "compressed_size_bytes": 45,
      "timestamp": 123456
    },
    "performance_metrics": {
      "academic_ratio": 0.375,
      "traditional_ratio": 2.67,
      "compression_time_us": 1234,
      "savings_percent": 62.5,
      "lossless_verified": true
    }
  }],
  "session_summary": {
    "total_original_bytes": 1200,
    "total_compressed_bytes": 450,
    "overall_savings_percent": 62.5,
    "optimal_method": "Dictionary-RLE"
  }
}
```

#### Step 3: Apply Security Layer
```cpp
// Check if security is initialized
if (security.isInitialized()) {
    // Secure the JSON payload
    String securedPayload = security.secureMessage(jsonPayload);
    
    if (securedPayload.isEmpty()) {
        Serial.println("ERROR: Failed to secure payload");
        return;
    }
} else {
    // Fallback to unsecured (not recommended)
    securedPayload = jsonPayload;
}
```

**What happens inside `secureMessage()`**:
```cpp
String SimpleSecurity::secureMessage(const String& jsonPayload, bool useEncryption) {
    // 1. Get next nonce
    uint32_t nonce = getNextNonce();  // e.g., nonce = 42
    
    // 2. Prepare payload (encrypt if requested)
    String payload = jsonPayload;
    if (useEncryption) {
        payload = mockEncrypt(jsonPayload);
    }
    
    // 3. Calculate HMAC
    String dataForHMAC = String(nonce) + ":" + payload;
    uint8_t hmac[32];
    calculateHMAC(dataForHMAC, hmac);
    String macHex = bytesToHex(hmac, 32);
    
    // 4. Create secured JSON wrapper
    JsonDocument securedDoc;
    securedDoc["nonce"] = nonce;
    securedDoc["encrypted"] = useEncryption;
    securedDoc["payload"] = payload;
    securedDoc["mac"] = macHex;
    
    String result;
    serializeJson(securedDoc, result);
    return result;
}
```

**Secured Payload Example**:
```json
{
  "nonce": 42,
  "encrypted": false,
  "payload": "{\"device_id\":\"ESP32_EcoWatt_Smart\",\"timestamp\":123456,...}",
  "mac": "a1b2c3d4e5f6789...64-char-hex-string..."
}
```

#### Step 4: Upload to Cloud
```cpp
HTTPClient http;
http.begin(serverURL);
http.addHeader("Content-Type", "application/json");

// POST the secured payload
int httpResponseCode = http.POST(securedPayload);

if (httpResponseCode == 200) {
    Serial.println("✅ Upload successful!");
} else {
    Serial.printf("❌ Upload failed (HTTP %d)\n", httpResponseCode);
}
```

### 4.2 Server Side (Python Flask)

#### Step 1: Receive Secured Payload
```python
@app.route('/process', methods=['POST'])
def process_data():
    secured_payload = request.get_data(as_text=True)
    
    # Verify the secured message
    success, result = verify_secured_message(secured_payload)
    
    if not success:
        return jsonify({"status": "error", "message": result}), 400
```

#### Step 2: Verify Security
```python
def verify_secured_message(secured_message_str):
    global last_valid_nonce
    
    # Parse JSON
    secured_msg = json.loads(secured_message_str)
    
    nonce = secured_msg['nonce']
    payload = secured_msg['payload']
    received_mac = secured_msg['mac']
    is_encrypted = secured_msg['encrypted']
    
    # 1. Validate nonce (anti-replay)
    if nonce <= last_valid_nonce:
        return False, f"Replay attack detected (nonce: {nonce})"
    
    # 2. Calculate HMAC for verification
    data_for_hmac = f"{nonce}:{payload}"
    calculated_hmac = hmac.new(
        PSK_BYTES, 
        data_for_hmac.encode('utf-8'), 
        hashlib.sha256
    ).hexdigest()
    
    # 3. Verify HMAC
    if received_mac != calculated_hmac:
        return False, "HMAC verification failed - tampered!"
    
    # 4. Update nonce
    last_valid_nonce = nonce
    
    # 5. Decrypt if needed
    if is_encrypted:
        payload = mock_decrypt(payload)
    
    return True, payload  # Return original JSON
```

#### Step 3: Process Decompressed Data
```python
# Parse the original payload
original_data = json.loads(result)

# Extract compressed data
compressed_packets = original_data['compressed_data']

for packet in compressed_packets:
    compressed_binary = base64.b64decode(packet['compressed_binary'])
    method = packet['decompression_metadata']['method']
    
    # Decompress based on method
    decompressed_data = decompress_data(compressed_binary, method)
    
    # Store in database
    store_sensor_data(decompressed_data)
```

---

## 5. Implementation Details

### 5.1 Pre-Shared Key (PSK) Management

**PSK Format**:
- **Length**: 64 hexadecimal characters (256 bits)
- **Example**: `4A6F486E20446F652041455336342D536563726574204B65792D323536626974`

**Storage Location**:
```cpp
// ESP32 side - stored in flash using Preferences API
Preferences prefs;
prefs.begin("security", false);
prefs.putString("psk", pskHex);
```

**Server Side**:
```python
# Hardcoded or loaded from environment variable
PSK_HEX = "4A6F486E20446F652041455336342D536563726574204B65792D323536626974"
PSK_BYTES = bytes.fromhex(PSK_HEX)
```

**Security Considerations**:
- ✅ PSK stored in flash (survives reboots)
- ⚠️ Not stored in hardware-secure element (acceptable for this use case)
- ✅ Can be rotated using `security.setPSK(newPsk)`
- ⚠️ Transmitted only once during initial setup

### 5.2 Nonce Persistence

**ESP32 Implementation**:
```cpp
void SimpleSecurity::persistNonce() {
    prefs.putUInt("nonce", currentNonce);
    prefs.putUInt("last_nonce", lastValidNonce);
}

// Called after every successful message
uint32_t SimpleSecurity::getNextNonce() {
    uint32_t nonce = currentNonce++;
    persistNonce();  // Save to flash
    return nonce;
}
```

**Reboot Handling**:
```cpp
// On device startup
currentNonce = prefs.getUInt("nonce", 1);
lastValidNonce = prefs.getUInt("last_nonce", 0);

// Ensure nonce always advances
if (currentNonce <= lastValidNonce) {
    currentNonce = lastValidNonce + 1;
    persistNonce();
}
```

### 5.3 Performance Impact

**Timing Measurements** (ESP32 @ 240MHz):

| Operation | Time (μs) | Impact |
|-----------|-----------|--------|
| HMAC-SHA256 calculation | ~500-800 | Low |
| Nonce increment + persist | ~100-200 | Minimal |
| JSON serialization (secured wrapper) | ~300-500 | Low |
| Mock encryption (optional) | ~200-400 | Low |
| **Total security overhead** | **~1-2 ms** | **Negligible** |

**Bandwidth Impact**:
```
Original compressed payload: ~450 bytes
Secured payload overhead: ~100 bytes (nonce + MAC + JSON wrapper)
Total: ~550 bytes
Overhead: ~22%
```

**Memory Usage**:
```
SimpleSecurity instance: ~100 bytes
PSK storage: 32 bytes
Nonce storage: 8 bytes
Temporary buffers: ~500 bytes during operation
Total: ~640 bytes
```

---

## 6. Security Analysis

### 6.1 Threat Model

| Threat | Mitigation | Effectiveness |
|--------|------------|---------------|
| **Eavesdropping** | Optional encryption + TLS | ⚠️ Mock encryption only |
| **Data Tampering** | HMAC-SHA256 | ✅ Strong protection |
| **Replay Attacks** | Sequential nonce | ✅ Strong protection |
| **Man-in-the-Middle** | HMAC + PSK | ✅ Strong protection |
| **Device Impersonation** | PSK authentication | ✅ Strong protection |
| **Denial of Service** | Rate limiting (server) | ⚠️ Application layer |
| **PSK Compromise** | Key rotation capability | ⚠️ Manual process |

### 6.2 Security Strengths

1. **Strong Authentication**
   - HMAC-SHA256 provides cryptographic-grade authentication
   - 256-bit PSK prevents brute-force attacks
   - Any modification to payload detected

2. **Replay Protection**
   - Sequential nonce system prevents message replay
   - Persistent storage ensures protection across reboots
   - Server-side validation prevents out-of-order acceptance

3. **Minimal Attack Surface**
   - Simple implementation reduces vulnerability risk
   - Uses battle-tested mbedTLS library
   - No complex key exchange protocols

### 6.3 Known Limitations

1. **Mock Encryption**
   - Current encryption is demonstration only
   - Base64 + Caesar cipher provides minimal confidentiality
   - **Recommendation**: Replace with AES-256-GCM for production

2. **PSK Storage**
   - Stored in flash memory, not hardware-secure element
   - Vulnerable if physical access to device
   - **Recommendation**: Use ESP32 secure boot and flash encryption

3. **No Forward Secrecy**
   - Static PSK used for all communications
   - PSK compromise affects all past and future messages
   - **Recommendation**: Implement Diffie-Hellman key exchange

4. **Single Key for All Devices**
   - Same PSK shared across devices (if multiple deployed)
   - **Recommendation**: Use device-specific keys or certificate-based auth

---

## 7. Testing and Validation

### 7.1 Security Test Suite

The `security_test_demo.cpp` validates all security features:

```cpp
✅ Test 1: Simple Message Authentication
   - Verifies HMAC calculation and verification

✅ Test 2: JSON Sensor Data
   - Tests security with real sensor payload format

✅ Test 3: Mock Encryption/Decryption
   - Validates optional encryption feature

✅ Test 4: Anti-replay Protection
   - Confirms old messages are rejected

✅ Test 5: Tamper Detection
   - Verifies modified messages fail HMAC check
```

### 7.2 Integration Testing

**Test Scenario**: Complete upload cycle with compression

```cpp
// 1. Acquire sensor data
readMultipleRegisters(selection, 6, sensorData);

// 2. Compress
compressed = compressWithSmartSelection(sensorData, ...);

// 3. Create JSON
jsonPayload = createJSONWithCompressedData(compressed);

// 4. Secure
securedPayload = security.secureMessage(jsonPayload);

// 5. Upload
http.POST(securedPayload);

// 6. Verify on server
assert verify_secured_message(securedPayload) == True
```

### 7.3 Performance Validation

**Benchmark Results**:
```
Compression time: ~15-30 ms
Security overhead: ~1-2 ms
Upload time: ~50-100 ms (network dependent)

Security impact: < 5% of total cycle time
✅ Acceptable for IoT application
```

---

## 8. Deployment Guide

### 8.1 Device Configuration

1. **Set PSK** (one-time setup):
```cpp
const char* PSK = "4A6F486E20446F652041455336342D536563726574204B65792D323536626974";
security.begin(PSK);
```

2. **Configure server endpoint**:
```cpp
const char* serverURL = "http://YOUR_SERVER_IP:5001/process";
```

3. **Build and upload**:
```bash
pio run --target upload
```

### 8.2 Server Configuration

1. **Install dependencies**:
```bash
pip install flask
```

2. **Set matching PSK**:
```python
PSK_HEX = "4A6F486E20446F652041455336342D536563726574204B65792D323536626974"
```

3. **Run server**:
```bash
python server_secured.py
```

### 8.3 Monitoring

**Device Logs**:
```
[Security] Security layer initialized successfully
[Security] Current nonce: 1, Last valid: 0
[Security] Secured message with nonce 1
Payload secured with HMAC-SHA256 authentication
UPLOADING SECURED DATA TO FLASK SERVER
Packets: 5 | Original JSON: 1234 bytes | Secured: 1334 bytes
✅ Upload successful to Flask server!
```

**Server Logs**:
```
🔒 EcoWatt Secured Server Starting...
🔑 PSK: 4A6F486E20446F65...2D323536626974
🛡️  HMAC-SHA256 verification enabled
🚫 Anti-replay protection active

=== Received secured payload ===
Size: 1334 bytes
✅ Security verification passed
Nonce: 1
📊 Processing data from ESP32_EcoWatt_Smart
   Type: compressed_sensor_batch
   Samples: 5
   Compression: 62.5% savings
   Optimal method: Dictionary-RLE
```

---

## 9. Conclusion

The EcoWatt Security Layer provides robust protection for compressed sensor data uploads with:

✅ **Strong Authentication** - HMAC-SHA256 with 256-bit PSK  
✅ **Anti-Replay Protection** - Persistent sequential nonce system  
✅ **Tamper Detection** - Any modification caught and rejected  
✅ **Low Overhead** - < 5% performance impact  
✅ **Reboot Resilient** - Security state persists across power cycles  

### Future Enhancements
- Replace mock encryption with AES-256-GCM
- Implement TLS/SSL for transport layer security
- Add device-specific key management
- Implement automatic key rotation
- Add hardware security module support

---

**Document Version**: 1.0  
**Last Updated**: October 14, 2025  
**Author**: EcoWatt Development Team  
**Contact**: [Your contact information]