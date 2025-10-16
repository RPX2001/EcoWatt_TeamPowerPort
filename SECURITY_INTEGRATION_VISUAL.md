# Security Layer Integration - Visual Overview

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              ESP32 FIRMWARE                                  │
│                         (PIO/ECOWATT/src/main.cpp)                          │
└─────────────────────────────────────────────────────────────────────────────┘
                                    │
                        setup() {   │
                          ...       │
                          SecurityLayer::init(); ✅ NEW
                          ...       │
                        }           │
                                    │
                        uploadSmartCompressedDataToCloud() {
                          │
                          ▼
          ┌─────────────────────────────────────────────────┐
          │ 1. Compress sensor data                         │
          │    (Dictionary/Temporal/Semantic/BitPack)       │
          └─────────────────────────────────────────────────┘
                          │
                          ▼
          ┌─────────────────────────────────────────────────┐
          │ 2. Build JSON payload                           │
          │    {device_id, timestamp, compressed_data[]}    │
          └─────────────────────────────────────────────────┘
                          │
                          ▼
          ╔═════════════════════════════════════════════════╗
          ║ 3. SecurityLayer::securePayload() ✅ NEW       ║
          ║    - Increment nonce (10001 → 10002...)        ║
          ║    - Base64 encode payload                      ║
          ║    - Calculate HMAC-SHA256                      ║
          ║    - Build secured JSON                         ║
          ╚═════════════════════════════════════════════════╝
                          │
                          ▼
          ┌─────────────────────────────────────────────────┐
          │ 4. HTTP POST to Flask server                    │
          │    Content: {nonce, payload, mac, encrypted}    │
          └─────────────────────────────────────────────────┘
                          │
                          │ Network (WiFi)
                          │
                          ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                           FLASK SERVER                                       │
│                   (flask/flask_server_hivemq.py)                            │
└─────────────────────────────────────────────────────────────────────────────┘
                          │
          @app.route('/process', methods=['POST'])
          def process_compressed_data():
                          │
                          ▼
          ╔═════════════════════════════════════════════════╗
          ║ 1. Detect Security Layer ✅ NEW                ║
          ║    if is_secured_payload(data):                ║
          ║      verify_secured_payload()                  ║
          ╚═════════════════════════════════════════════════╝
                          │
                          ▼
          ┌─────────────────────────────────────────────────┐
          │ server_security_layer.py ✅ NEW                 │
          │                                                 │
          │ verify_secured_payload():                       │
          │   ├─ Check nonce > last_valid_nonce            │
          │   ├─ Decode Base64 payload                     │
          │   ├─ Calculate HMAC-SHA256                     │
          │   ├─ Compare MACs (constant time)              │
          │   ├─ Update last_valid_nonce                   │
          │   └─ Return original JSON                      │
          └─────────────────────────────────────────────────┘
                          │
                          ▼
          ┌─────────────────────────────────────────────────┐
          │ 2. Decompress sensor data                       │
          │    (Dictionary/Temporal/etc.)                   │
          └─────────────────────────────────────────────────┘
                          │
                          ▼
          ┌─────────────────────────────────────────────────┐
          │ 3. Process and publish to MQTT                  │
          └─────────────────────────────────────────────────┘


═══════════════════════════════════════════════════════════════════════════════
                            FILES CREATED/MODIFIED
═══════════════════════════════════════════════════════════════════════════════

ESP32 SIDE:
  ✅ NEW:      include/application/security.h
  ✅ NEW:      src/application/security.cpp
  ✅ MODIFIED: src/main.cpp

FLASK SIDE:
  ✅ NEW:      flask/server_security_layer.py
  ✅ MODIFIED: flask/flask_server_hivemq.py

DOCUMENTATION:
  ✅ NEW:      PIO/ECOWATT/SECURITY_IMPLEMENTATION_TEST.md
  ✅ NEW:      SECURITY_IMPLEMENTATION_COMPLETE.md
  ✅ NEW:      SECURITY_INTEGRATION_VISUAL.md (this file)


═══════════════════════════════════════════════════════════════════════════════
                         SECURED PAYLOAD STRUCTURE
═══════════════════════════════════════════════════════════════════════════════

BEFORE SECURITY LAYER (Original JSON):
┌─────────────────────────────────────────────────────────────────────────┐
│ {                                                                       │
│   "device_id": "ESP32_EcoWatt_Smart",                                  │
│   "timestamp": 123456789,                                              │
│   "data_type": "compressed_sensor_batch",                              │
│   "total_samples": 5,                                                  │
│   "compressed_data": [                                                 │
│     {                                                                  │
│       "compressed_binary": "eyJ...",                                   │
│       "decompression_metadata": {...},                                 │
│       "performance_metrics": {...}                                     │
│     }                                                                  │
│   ],                                                                   │
│   "session_summary": {...}                                             │
│ }                                                                       │
└─────────────────────────────────────────────────────────────────────────┘
                              │
                              │ SecurityLayer::securePayload()
                              ▼
AFTER SECURITY LAYER (Secured JSON):
┌─────────────────────────────────────────────────────────────────────────┐
│ {                                                                       │
│   "nonce": 10001,                        ← Anti-replay counter         │
│   "payload": "eyJkZXZpY2VfaWQiOiJF...",  ← Base64(Original JSON)       │
│   "mac": "a7b3c4d5e6f7890abcdef12...",   ← HMAC-SHA256(nonce+payload) │
│   "encrypted": false                     ← Mock mode (Base64 only)     │
│ }                                                                       │
└─────────────────────────────────────────────────────────────────────────┘


═══════════════════════════════════════════════════════════════════════════════
                           SECURITY MECHANISMS
═══════════════════════════════════════════════════════════════════════════════

1. HMAC-SHA256 (Authentication & Integrity)
   ┌──────────────────────────────────────┐
   │ Input:  nonce (4 bytes) + payload    │
   │ Key:    PSK_HMAC (32 bytes)          │
   │ Output: 32-byte MAC → 64 hex chars   │
   └──────────────────────────────────────┘

2. Anti-Replay Protection (Nonce)
   ┌──────────────────────────────────────┐
   │ ESP32:  nonce++ before each send     │
   │ Server: if nonce <= last: reject     │
   │ NVS:    persists across reboots      │
   └──────────────────────────────────────┘

3. Base64 Encoding (JSON-safe)
   ┌──────────────────────────────────────┐
   │ Binary → ASCII characters            │
   │ Safe for JSON transmission           │
   │ +33% size overhead                   │
   └──────────────────────────────────────┘

4. Optional AES-128-CBC (Confidentiality)
   ┌──────────────────────────────────────┐
   │ Currently: Mock mode (disabled)      │
   │ Can enable: ENABLE_ENCRYPTION=true   │
   │ Mode: CBC with PKCS7 padding         │
   └──────────────────────────────────────┘


═══════════════════════════════════════════════════════════════════════════════
                            KEY CONFIGURATION
═══════════════════════════════════════════════════════════════════════════════

ESP32 (src/application/security.cpp):                                          
┌────────────────────────────────────────────────────────────────────────────┐
│ const uint8_t SecurityLayer::PSK_HMAC[32] = {                             │
│     0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,                       │
│     0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c,                       │
│     0x76, 0x2e, 0x71, 0x60, 0xf3, 0x8b, 0x4d, 0xa5,                       │
│     0x6a, 0x78, 0x4d, 0x90, 0x45, 0x19, 0x0c, 0xfe                        │
│ };                                                                         │
└────────────────────────────────────────────────────────────────────────────┘
                                    ║
                                    ║ MUST MATCH EXACTLY!
                                    ║
Flask (flask/server_security_layer.py):                                        
┌────────────────────────────────────────────────────────────────────────────┐
│ PSK_HMAC = bytes([                                                         │
│     0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,                       │
│     0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c,                       │
│     0x76, 0x2e, 0x71, 0x60, 0xf3, 0x8b, 0x4d, 0xa5,                       │
│     0x6a, 0x78, 0x4d, 0x90, 0x45, 0x19, 0x0c, 0xfe                        │
│ ])                                                                         │
└────────────────────────────────────────────────────────────────────────────┘


═══════════════════════════════════════════════════════════════════════════════
                              TESTING GUIDE
═══════════════════════════════════════════════════════════════════════════════

STEP 1: Build and Upload Firmware
  $ cd "PIO/ECOWATT"
  $ pio run --target upload
  $ pio device monitor
  
  Expected Output:
  ✓ "Security Layer: Initialized with nonce = 10000"
  ✓ "Security Layer: Payload secured with nonce 10001"
  ✓ "Upload successful to Flask server!"

STEP 2: Start Flask Server
  $ cd "flask"
  $ python flask_server_hivemq.py
  
  Expected Output:
  ✓ "🔒 Detected secured payload - removing security layer..."
  ✓ "[Security] ✓ Verification successful: nonce=10001"
  ✓ "✓ Security verification successful!"

STEP 3: Verify Nonce Persistence
  - Note last nonce from serial monitor
  - Press RESET button on ESP32
  - Verify nonce continues incrementing (doesn't reset to 10000)

STEP 4: Test Security Module Standalone
  $ cd "flask"
  $ python server_security_layer.py
  
  Expected Output:
  ✓ "✓ Verification successful!"
  ✓ "Original data: {..."


═══════════════════════════════════════════════════════════════════════════════
                            SUCCESS CRITERIA
═══════════════════════════════════════════════════════════════════════════════

✅ Firmware compiles without errors
✅ Serial monitor shows security initialization
✅ Payloads are secured before sending
✅ Nonce increments sequentially
✅ Nonce persists across reboots
✅ Flask server detects secured payloads
✅ HMAC verification succeeds
✅ Original data is extracted correctly
✅ Decompression works after security verification
✅ Data flows to MQTT successfully


═══════════════════════════════════════════════════════════════════════════════
                              STATUS: COMPLETE ✅
═══════════════════════════════════════════════════════════════════════════════

🎉 The security layer has been FULLY IMPLEMENTED and is ready for testing!

Implementation Date: October 16, 2025
Firmware Status: ✅ Compiled Successfully
Server Status: ✅ Integrated and Ready
Documentation: ✅ Complete
```
