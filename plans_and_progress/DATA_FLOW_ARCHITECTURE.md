# EcoWatt Data Flow Architecture

## Industry Standard Implementation (Per Milestone 3 Requirements)

### **Complete System Flow**

```
┌─────────────────────────────────────────────────────────────────────┐
│                         ESP32 DEVICE (SLAVE)                         │
└─────────────────────────────────────────────────────────────────────┘

1. ACQUISITION PHASE (Continuous)
   ├─ Poll Inverter SIM every 5 seconds
   ├─ Read registers (voltage, current, etc.)
   ├─ Store raw samples in local buffer
   └─ Continue until 15-minute cycle completes

2. COMPRESSION PHASE (Before Upload)
   ├─ Collect all buffered samples
   ├─ Apply compression algorithm (Delta/RLE/Dictionary)
   ├─ Generate compressed binary packets
   ├─ Store compressed packets with metadata
   └─ Result: SmartCompressedData objects in ringBuffer

3. UPLOAD PREPARATION (Every 15 minutes)
   ├─ Drain all compressed packets from ringBuffer
   ├─ Build JSON payload with:
   │  ├─ Device ID
   │  ├─ Timestamp
   │  ├─ Compressed packets array [
   │  │  ├─ compressed_binary (base64)
   │  │  ├─ decompression_metadata
   │  │  └─ performance_metrics
   │  │  ]
   │  └─ Session summary
   └─ Serialize to JSON string

4. SECURITY LAYER (Encryption + Authentication)
   ├─ Input: JSON string
   ├─ Increment nonce (anti-replay)
   ├─ Base64 encode payload
   ├─ Calculate HMAC-SHA256(nonce + payload)
   ├─ Build secured JSON: {
   │    "nonce": 12345,
   │    "payload": "base64_encoded_json",
   │    "mac": "hmac_hex",
   │    "encrypted": false,
   │    "compressed": false  ← JSON contains compressed data, not raw binary
   │  }
   └─ Output: Secured JSON string

5. TRANSMISSION
   ├─ HTTP POST to /aggregated/{device_id}
   ├─ Content-Type: application/json
   └─ Body: Secured JSON from step 4

┌─────────────────────────────────────────────────────────────────────┐
│                      ECOWATT CLOUD (MASTER)                          │
└─────────────────────────────────────────────────────────────────────┘

6. RECEPTION & SECURITY VALIDATION
   ├─ Receive HTTP POST
   ├─ Parse secured JSON
   ├─ Verify nonce (anti-replay check)
   ├─ Base64 decode payload
   ├─ Calculate HMAC-SHA256(nonce + decoded_payload)
   ├─ Compare with received MAC
   ├─ If valid: Return decoded payload (JSON string)
   └─ If invalid: Reject with 401

7. PAYLOAD PROCESSING
   ├─ Parse decrypted JSON
   ├─ Extract compressed_data array
   ├─ For each packet:
   │  ├─ Base64 decode compressed_binary
   │  ├─ Decompress using metadata.method
   │  ├─ Verify lossless recovery
   │  └─ Extract original samples
   └─ Store samples in database

8. RESPONSE
   ├─ Send 200 OK with success message
   ├─ Include pending configuration updates
   └─ Include queued commands (if any)
```

---

## Current Implementation Status

### ✅ **What's Working:**
1. **Acquisition**: Polling Inverter SIM correctly
2. **Compression**: Delta/RLE/Dictionary compression working
3. **Buffering**: RingBuffer stores compressed packets
4. **Security**: HMAC-SHA256 authentication working
5. **Transport**: HTTP POST to Flask server

### ⚠️ **Current Issue:**
- **Double Base64 encoding**: Compressed binary is Base64 encoded in JSON, then the entire JSON is Base64 encoded again by security layer
- **Not a critical problem**: This works but adds ~33% overhead
- **Solution**: Already implemented - security layer marks `compressed=false` to indicate payload is JSON, not raw binary

---

## Data Format Specifications

### **ESP32 → Server (Upload)**

```json
{
  "nonce": 12345,
  "payload": "eyJkZXZpY2VfaWQiOiAiRVNQMzJfMDAxIiwgLi4ufQ==",  // Base64(JSON)
  "mac": "a1b2c3d4e5f6...",
  "encrypted": false,
  "compressed": false  // Indicates payload is JSON, not raw binary
}
```

**Decoded payload (JSON):**
```json
{
  "device_id": "ESP32_001",
  "timestamp": 1234567890,
  "data_type": "compressed_sensor_batch",
  "total_samples": 3,
  "register_mapping": {
    "0": "Voltage",
    "1": "Current"
  },
  "compressed_data": [
    {
      "compressed_binary": "eNqrVg...",  // Base64(compressed binary)
      "decompression_metadata": {
        "method": "DICTIONARY",
        "register_count": 2,
        "original_size_bytes": 36,
        "compressed_size_bytes": 20,
        "timestamp": 1234567890,
        "register_layout": [0, 1]
      },
      "performance_metrics": {
        "academic_ratio": 0.556,
        "compression_time_us": 4710,
        "savings_percent": 44.4,
        "lossless_verified": true
      }
    }
  ],
  "session_summary": {
    "total_original_bytes": 36,
    "total_compressed_bytes": 20,
    "overall_academic_ratio": 0.556,
    "overall_savings_percent": 44.4
  }
}
```

### **Server → ESP32 (Configuration)**

Server can send configuration updates in the response:

```json
{
  "success": true,
  "message": "Data processed",
  "pending_config": {
    "sampling_rate_ms": 3000,  // Change from 5s to 3s
    "registers": [0, 1, 2]     // Add frequency register
  },
  "pending_commands": [
    {
      "command_id": "cmd_001",
      "type": "write_register",
      "register": 5,
      "value": 100
    }
  ]
}
```

---

## Compression Methods Implemented

1. **NONE**: No compression (testing baseline)
2. **DELTA**: Delta encoding for slowly changing values
3. **RLE**: Run-length encoding for repeated values
4. **DICTIONARY**: Dictionary-based compression (best for structured data)

---

## Security Features

1. **Authentication**: HMAC-SHA256 with pre-shared key
2. **Integrity**: MAC verification prevents tampering
3. **Anti-Replay**: Sequential nonce prevents replay attacks
4. **Confidentiality**: Optional AES-128-CBC encryption (currently disabled)

---

## Performance Metrics

Based on test data:
- **Original size**: 36 bytes (3 samples × 12 bytes)
- **Compressed size**: 20 bytes
- **Compression ratio**: 55.6% (44.4% savings)
- **Compression time**: ~4.7 ms
- **Lossless**: 100% verified

---

## Alignment with Project Requirements

### **Milestone 3 Requirements:**

✅ **Part 1: Buffer Implementation**
- Local buffer stores samples without data loss
- Modular and separate from acquisition/transport

✅ **Part 2: Compression Algorithm and Benchmarking**
- Multiple algorithms implemented (Delta, RLE, Dictionary)
- Compression ratio measured and documented
- Lossless recovery verified

✅ **Part 3: Uplink Packetizer and Upload Cycle**
- 15-minute tick → finalize → encrypt/HMAC → transmit ✅
- Retry logic implemented
- Placeholder for encryption (to be enhanced in Milestone 4)

✅ **Part 4: Cloud API Endpoint**
- Flask server endpoint implemented
- Accepts and processes uploaded data
- Stores data and returns acknowledgment

---

## Next Steps (Milestone 4)

1. **Full AES Encryption**: Enable real AES-128-CBC encryption
2. **Remote Configuration**: Implement runtime config updates
3. **Command Execution**: Implement write commands to Inverter SIM
4. **FOTA**: Implement firmware-over-the-air updates

---

## Code References

- **ESP32 Acquisition**: `PIO/ECOWATT/src/peripheral/acquisition.cpp`
- **ESP32 Compression**: `PIO/ECOWATT/src/peripheral/compression.cpp`
- **ESP32 Upload**: `PIO/ECOWATT/src/application/data_uploader.cpp`
- **ESP32 Security**: `PIO/ECOWATT/src/application/security.cpp`
- **Server Security**: `flask/handlers/security_handler.py`
- **Server Compression**: `flask/handlers/compression_handler.py`
- **Server Routes**: `flask/routes/aggregation_routes.py`
