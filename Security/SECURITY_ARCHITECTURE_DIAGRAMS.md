# Security Layer Architecture Diagrams

## System Overview

```
┌─────────────────────────────────────────────────────────────────────────┐
│                          ESP32 DEVICE                                    │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  ┌────────────────────────────────────────────────────────────────┐    │
│  │          SENSOR DATA ACQUISITION                                │    │
│  │  Read registers: VAC1, IAC1, IPV1, PAC, IPV2, TEMP             │    │
│  └──────────────────────────┬─────────────────────────────────────┘    │
│                             │ Raw sensor values                         │
│                             ↓                                            │
│  ┌────────────────────────────────────────────────────────────────┐    │
│  │          DATA COMPRESSION                                       │    │
│  │  Smart compression (Dictionary/Temporal/Semantic/Bitpack)      │    │
│  └──────────────────────────┬─────────────────────────────────────┘    │
│                             │ Compressed binary + metadata              │
│                             ↓                                            │
│  ┌────────────────────────────────────────────────────────────────┐    │
│  │          JSON SERIALIZATION                                     │    │
│  │  Build payload with compressed data, timestamps, metadata      │    │
│  └──────────────────────────┬─────────────────────────────────────┘    │
│                             │ Original JSON (1500 bytes)                │
│                             ↓                                            │
│  ┌────────────────────────────────────────────────────────────────┐    │
│  │  ╔═══════════════════════════════════════════════════════╗     │    │
│  │  ║         SECURITY LAYER (security.cpp)                 ║     │    │
│  │  ╠═══════════════════════════════════════════════════════╣     │    │
│  │  ║                                                        ║     │    │
│  │  ║  1. Load/Increment Nonce (NVS)                       ║     │    │
│  │  ║     Current: 10500 → 10501                           ║     │    │
│  │  ║                                                        ║     │    │
│  │  ║  2. Encode Payload                                    ║     │    │
│  │  ║     [Mock Encryption Mode]                            ║     │    │
│  │  ║     JSON → Base64                                     ║     │    │
│  │  ║     1500 bytes → 2000 bytes                          ║     │    │
│  │  ║                                                        ║     │    │
│  │  ║  3. Calculate HMAC-SHA256                            ║     │    │
│  │  ║     Input: [nonce_4bytes][payload_bytes]             ║     │    │
│  │  ║     Key: PSK_HMAC (256-bit)                          ║     │    │
│  │  ║     Output: 32-byte MAC → 64 hex chars               ║     │    │
│  │  ║                                                        ║     │    │
│  │  ║  4. Build Secured JSON                               ║     │    │
│  │  ║     {                                                 ║     │    │
│  │  ║       "nonce": 10501,                                ║     │    │
│  │  ║       "payload": "eyJkZXZpY2...",                   ║     │    │
│  │  ║       "mac": "a7b3c4d5e6f...",                       ║     │    │
│  │  ║       "encrypted": false                             ║     │    │
│  │  ║     }                                                 ║     │    │
│  │  ║                                                        ║     │    │
│  │  ╚═══════════════════════════════════════════════════════╝     │    │
│  └──────────────────────────┬─────────────────────────────────────┘    │
│                             │ Secured JSON (2200 bytes)                 │
│                             ↓                                            │
│  ┌────────────────────────────────────────────────────────────────┐    │
│  │          HTTP CLIENT                                            │    │
│  │  POST to http://server:5001/process                            │    │
│  │  Content-Type: application/json                                │    │
│  └──────────────────────────┬─────────────────────────────────────┘    │
│                             │                                            │
└─────────────────────────────┼────────────────────────────────────────────┘
                              │ HTTPS over WiFi
                              │
                              ↓
┌─────────────────────────────────────────────────────────────────────────┐
│                         CLOUD SERVER (Flask)                             │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  ┌────────────────────────────────────────────────────────────────┐    │
│  │          FLASK ROUTE: /process                                  │    │
│  │  Receive POST request with secured JSON                        │    │
│  └──────────────────────────┬─────────────────────────────────────┘    │
│                             │ Secured JSON                              │
│                             ↓                                            │
│  ┌────────────────────────────────────────────────────────────────┐    │
│  │  ╔═══════════════════════════════════════════════════════╗     │    │
│  │  ║    SECURITY VERIFICATION (server_security_layer.py)  ║     │    │
│  │  ╠═══════════════════════════════════════════════════════╣     │    │
│  │  ║                                                        ║     │    │
│  │  ║  1. Parse JSON                                        ║     │    │
│  │  ║     Extract nonce, payload, mac, encrypted            ║     │    │
│  │  ║                                                        ║     │    │
│  │  ║  2. Anti-Replay Check                                ║     │    │
│  │  ║     if nonce (10501) > last_seen (10500): ✓         ║     │    │
│  │  ║     else: REJECT - Replay Attack!                    ║     │    │
│  │  ║                                                        ║     │    │
│  │  ║  3. Base64 Decode                                    ║     │    │
│  │  ║     "eyJkZXZpY2..." → JSON bytes                     ║     │    │
│  │  ║                                                        ║     │    │
│  │  ║  4. Calculate HMAC                                   ║     │    │
│  │  ║     Input: [nonce_4bytes][payload_bytes]             ║     │    │
│  │  ║     Key: PSK_HMAC (same as ESP32)                    ║     │    │
│  │  ║     Output: 32-byte MAC                              ║     │    │
│  │  ║                                                        ║     │    │
│  │  ║  5. Verify MAC                                       ║     │    │
│  │  ║     if calculated == received: ✓                     ║     │    │
│  │  ║     else: REJECT - Tampered/Wrong Key!              ║     │    │
│  │  ║                                                        ║     │    │
│  │  ║  6. Update Nonce Tracker                             ║     │    │
│  │  ║     last_seen[device] = 10501                        ║     │    │
│  │  ║                                                        ║     │    │
│  │  ╚═══════════════════════════════════════════════════════╝     │    │
│  └──────────────────────────┬─────────────────────────────────────┘    │
│                             │ Original JSON (verified)                  │
│                             ↓                                            │
│  ┌────────────────────────────────────────────────────────────────┐    │
│  │          DATA DECOMPRESSION                                     │    │
│  │  Extract compressed binary, decompress sensor data             │    │
│  └──────────────────────────┬─────────────────────────────────────┘    │
│                             │ Raw sensor values                         │
│                             ↓                                            │
│  ┌────────────────────────────────────────────────────────────────┐    │
│  │          DATA PROCESSING & STORAGE                              │    │
│  │  Store in database, run analytics, trigger alerts              │    │
│  └────────────────────────────────────────────────────────────────┘    │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## HMAC-SHA256 Authentication Flow

```
┌─────────────────────────────────────────────────────────────────────┐
│                       ESP32 DEVICE                                   │
└─────────────────────────────────────────────────────────────────────┘

Step 1: Prepare Data to Sign
┌──────────────────────────────────────────────────────────────┐
│  Nonce (Big-Endian)      │   Original JSON Payload           │
│  ┌──┬──┬──┬──┐           │   ┌─────────────────────────┐    │
│  │ 0x00 │ 0x00 │ 0x29 │ 0x05│   │{"device_id":"ESP32..."}│    │
│  └──┴──┴──┴──┘           │   └─────────────────────────┘    │
│  4 bytes (10501)          │   Variable length                 │
└──────────────────────────────────────────────────────────────┘
                            ↓
Step 2: Calculate HMAC
┌────────────────────────────────────────────────────────────┐
│           mbedTLS HMAC-SHA256 Algorithm                     │
│                                                              │
│  Input:  data (nonce + payload)                            │
│  Key:    PSK_HMAC (32 bytes)                               │
│          [0x2b, 0x7e, 0x15, 0x16, ...]                     │
│                                                              │
│  Process:                                                    │
│  1. Initialize SHA256 with key (HMAC mode)                 │
│  2. Hash the data                                           │
│  3. Finalize with key                                       │
│                                                              │
│  Output: 32-byte MAC                                        │
│          [0xa7, 0xb3, 0xc4, 0xd5, ...]                     │
└────────────────────────────────────────────────────────────┘
                            ↓
Step 3: Convert to Hex String
┌────────────────────────────────────────────────────────────┐
│  MAC bytes → Hex string (64 characters)                    │
│                                                              │
│  [0xa7, 0xb3, 0xc4, 0xd5, ...]                            │
│         ↓                                                   │
│  "a7b3c4d5e6f7890abcdef1234567890abcdef1234..."           │
└────────────────────────────────────────────────────────────┘
                            ↓
                    Include in Secured JSON

═══════════════════════════════════════════════════════════════

┌─────────────────────────────────────────────────────────────────────┐
│                     CLOUD SERVER                                     │
└─────────────────────────────────────────────────────────────────────┘

Step 1: Extract Data from Secured JSON
┌────────────────────────────────────────────────────────────┐
│  received_nonce = 10501                                     │
│  received_payload = "eyJkZXZpY2VfaWQi..." (Base64)        │
│  received_mac = "a7b3c4d5e6f7890abc..."                   │
└────────────────────────────────────────────────────────────┘
                            ↓
Step 2: Decode Payload
┌────────────────────────────────────────────────────────────┐
│  Base64 decode → original_payload_bytes                    │
│  "eyJkZXZpY2..." → b'{"device_id":"ESP32..."}'           │
└────────────────────────────────────────────────────────────┘
                            ↓
Step 3: Reconstruct Data to Sign
┌────────────────────────────────────────────────────────────┐
│  nonce_bytes = (10501).to_bytes(4, 'big')                 │
│  data = nonce_bytes + original_payload_bytes               │
└────────────────────────────────────────────────────────────┘
                            ↓
Step 4: Calculate HMAC (Same Algorithm)
┌────────────────────────────────────────────────────────────┐
│  import hmac, hashlib                                       │
│                                                              │
│  calculated_mac = hmac.new(                                │
│      PSK_HMAC,                                             │
│      data,                                                  │
│      hashlib.sha256                                         │
│  ).hexdigest()                                             │
│                                                              │
│  Result: "a7b3c4d5e6f7890abcdef1234..."                   │
└────────────────────────────────────────────────────────────┘
                            ↓
Step 5: Compare MACs
┌────────────────────────────────────────────────────────────┐
│  if hmac.compare_digest(calculated_mac, received_mac):     │
│      ✓ VERIFIED - Message is authentic and unmodified     │
│  else:                                                      │
│      ✗ REJECT - Message tampered or wrong key             │
└────────────────────────────────────────────────────────────┘
```

---

## Anti-Replay Protection Flow

```
┌─────────────────────────────────────────────────────────────────┐
│                     DEVICE INITIALIZATION                        │
└─────────────────────────────────────────────────────────────────┘

┌────────────────────────┐
│   ESP32 Boots Up       │
└──────────┬─────────────┘
           │
           ↓
┌────────────────────────────────────────────────────────────┐
│  SecurityLayer::init()                                      │
│  → loadNonce()                                             │
│    → Read from NVS namespace "security", key "nonce"       │
│    → If not found, start at 10000                         │
│    → currentNonce = 10500 (example)                       │
└────────────────────────────────────────────────────────────┘

═══════════════════════════════════════════════════════════════

┌─────────────────────────────────────────────────────────────────┐
│                     MESSAGE TRANSMISSION                         │
└─────────────────────────────────────────────────────────────────┘

┌────────────────────────┐
│  Upload Data Event     │
└──────────┬─────────────┘
           │
           ↓
┌────────────────────────────────────────────────────────────┐
│  securePayload() called                                     │
└──────────┬─────────────────────────────────────────────────┘
           │
           ↓
┌────────────────────────────────────────────────────────────┐
│  incrementAndSaveNonce()                                    │
│  ├─ currentNonce++ (10500 → 10501)                        │
│  ├─ Save to NVS (persistent storage)                       │
│  └─ Nonce survives reboot/FOTA                            │
└──────────┬─────────────────────────────────────────────────┘
           │
           ↓
┌────────────────────────────────────────────────────────────┐
│  Include nonce in secured message                           │
│  {"nonce": 10501, "payload": "...", "mac": "..."}         │
└──────────┬─────────────────────────────────────────────────┘
           │
           ↓
       Send to Server

═══════════════════════════════════════════════════════════════

┌─────────────────────────────────────────────────────────────────┐
│                     SERVER VALIDATION                            │
└─────────────────────────────────────────────────────────────────┘

┌────────────────────────────────────────────────────────────┐
│  Receive Message                                            │
│  received_nonce = 10501                                     │
└──────────┬─────────────────────────────────────────────────┘
           │
           ↓
┌────────────────────────────────────────────────────────────┐
│  Check Device Nonce Tracker                                 │
│  device_nonces["ESP32_EcoWatt_Smart"] = 10500              │
│  (last seen nonce)                                         │
└──────────┬─────────────────────────────────────────────────┘
           │
           ↓
┌────────────────────────────────────────────────────────────┐
│  Validate: received_nonce > last_seen_nonce ?              │
│                                                              │
│  Case 1: 10501 > 10500 → ✓ ACCEPT                         │
│          New message, proceed with verification             │
│                                                              │
│  Case 2: 10501 <= 10500 → ✗ REJECT                        │
│          Replay attack! This message was already seen      │
│          Return HTTP 403 Forbidden                          │
└──────────┬─────────────────────────────────────────────────┘
           │
           ↓ (if accepted)
┌────────────────────────────────────────────────────────────┐
│  Verify HMAC (see HMAC flow above)                         │
└──────────┬─────────────────────────────────────────────────┘
           │
           ↓ (if HMAC valid)
┌────────────────────────────────────────────────────────────┐
│  Update Tracker                                             │
│  device_nonces["ESP32_EcoWatt_Smart"] = 10501              │
│  (prevent future replays of this message)                  │
└────────────────────────────────────────────────────────────┘
           │
           ↓
     Process Message
```

---

## Optional Encryption (AES-128-CBC)

### When ENABLE_ENCRYPTION = false (Current Mode)

```
Original JSON
     ↓
Base64 Encode
     ↓
Encoded Payload (mock encryption)
```

### When ENABLE_ENCRYPTION = true

```
┌─────────────────────────────────────────────────────────────┐
│                   ENCRYPTION PROCESS                         │
└─────────────────────────────────────────────────────────────┘

Step 1: Prepare Plaintext
┌────────────────────────────────────────────────────────────┐
│  Original JSON: {"device_id": "ESP32_EcoWatt_Smart", ...}  │
│  Length: 1500 bytes                                         │
└────────────────────────────────────────────────────────────┘
                            ↓
Step 2: Apply PKCS7 Padding
┌────────────────────────────────────────────────────────────┐
│  AES requires data in 16-byte blocks                       │
│  1500 bytes → 1504 bytes (next multiple of 16)            │
│  Padding: [0x04, 0x04, 0x04, 0x04]                        │
└────────────────────────────────────────────────────────────┘
                            ↓
Step 3: AES-128-CBC Encryption
┌────────────────────────────────────────────────────────────┐
│  Algorithm: AES-128 in CBC mode                            │
│  Key: PSK_AES (16 bytes)                                   │
│  IV: Fixed 16-byte initialization vector                   │
│                                                              │
│  Process:                                                    │
│  Block 0: Plaintext[0:16] XOR IV → Encrypt                │
│  Block 1: Plaintext[16:32] XOR Ciphertext[0] → Encrypt    │
│  Block 2: Plaintext[32:48] XOR Ciphertext[1] → Encrypt    │
│  ...                                                        │
│                                                              │
│  Output: 1504 bytes of ciphertext                          │
└────────────────────────────────────────────────────────────┘
                            ↓
Step 4: Base64 Encode
┌────────────────────────────────────────────────────────────┐
│  Ciphertext (binary) → Base64 string                       │
│  1504 bytes → ~2005 characters                            │
└────────────────────────────────────────────────────────────┘
                            ↓
        Include in Secured JSON

═══════════════════════════════════════════════════════════════

┌─────────────────────────────────────────────────────────────┐
│                   DECRYPTION PROCESS                         │
└─────────────────────────────────────────────────────────────┘

Step 1: Base64 Decode
┌────────────────────────────────────────────────────────────┐
│  Base64 string → Binary ciphertext (1504 bytes)            │
└────────────────────────────────────────────────────────────┘
                            ↓
Step 2: AES-128-CBC Decryption
┌────────────────────────────────────────────────────────────┐
│  Algorithm: AES-128 in CBC mode (reverse)                  │
│  Key: PSK_AES (same as ESP32)                              │
│  IV: Fixed IV (same as ESP32)                              │
│                                                              │
│  Process:                                                    │
│  Block 0: Decrypt → XOR IV → Plaintext[0:16]              │
│  Block 1: Decrypt → XOR Ciphertext[0] → Plaintext[16:32]  │
│  ...                                                        │
│                                                              │
│  Output: 1504 bytes of plaintext (with padding)            │
└────────────────────────────────────────────────────────────┘
                            ↓
Step 3: Remove PKCS7 Padding
┌────────────────────────────────────────────────────────────┐
│  Check last byte: 0x04                                     │
│  Remove last 4 bytes                                        │
│  Result: 1500 bytes of original JSON                       │
└────────────────────────────────────────────────────────────┘
```

---

## Memory and Storage Layout

```
┌─────────────────────────────────────────────────────────────┐
│                    ESP32 FLASH MEMORY                        │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  ┌────────────────────────────────────────────────┐        │
│  │  Program Code (.text section)                  │        │
│  │                                                 │        │
│  │  ┌──────────────────────────────────────┐     │        │
│  │  │  security.cpp compiled code          │     │        │
│  │  │  - calculateHMAC()                    │     │        │
│  │  │  - encryptAES()                       │     │        │
│  │  │  - securePayload()                    │     │        │
│  │  └──────────────────────────────────────┘     │        │
│  └────────────────────────────────────────────────┘        │
│                                                              │
│  ┌────────────────────────────────────────────────┐        │
│  │  Constants (.rodata section)                   │        │
│  │                                                 │        │
│  │  PSK_HMAC[32] = {0x2b, 0x7e, ...}            │        │
│  │  PSK_AES[16] = {0x2b, 0x7e, ...}             │        │
│  │  AES_IV[16] = {0x00, 0x01, ...}              │        │
│  └────────────────────────────────────────────────┘        │
│                                                              │
│  ┌────────────────────────────────────────────────┐        │
│  │  NVS Partition (Non-Volatile Storage)          │        │
│  │                                                 │        │
│  │  Namespace: "security"                         │        │
│  │  ┌─────────────────────────────────────┐      │        │
│  │  │  Key: "nonce"                        │      │        │
│  │  │  Type: uint32_t                      │      │        │
│  │  │  Value: 10501 (current nonce)        │      │        │
│  │  │  Size: 4 bytes                        │      │        │
│  │  └─────────────────────────────────────┘      │        │
│  └────────────────────────────────────────────────┘        │
│                                                              │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│                    ESP32 RAM (SRAM)                          │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  Stack (during securePayload() execution):                  │
│  ┌────────────────────────────────────────────────┐        │
│  │  mbedtls_md_context_t ctx        ~200 bytes   │        │
│  │  mbedtls_aes_context aes         ~150 bytes   │        │
│  │  uint8_t hmac[32]                  32 bytes   │        │
│  │  char hmacHex[65]                  65 bytes   │        │
│  │  uint8_t* dataToSign              variable    │        │
│  └────────────────────────────────────────────────┘        │
│                                                              │
│  Heap (allocated buffers):                                  │
│  ┌────────────────────────────────────────────────┐        │
│  │  char securedPayload[4096]        4096 bytes  │        │
│  │  DynamicJsonDocument doc(8192)    8192 bytes  │        │
│  └────────────────────────────────────────────────┘        │
│                                                              │
│  Total RAM for security: ~13 KB (temporary)                 │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

---

## Timeline: Single Upload Cycle

```
Time (ms)  Event
─────────  ─────────────────────────────────────────────────────
0.0        uploadSmartCompressedDataToCloud() called
           
0.1        Build compressed data JSON (1500 bytes)
           
0.2        ┌─────────────────────────────────────────────┐
           │  securePayload() START                       │
           └─────────────────────────────────────────────┘
           
0.3        • Increment nonce: 10500 → 10501
           
0.8        • Save nonce to NVS (flash write ~5ms)
           
6.0        • Base64 encode payload (~0.2ms)
           
6.5        • Prepare data to sign (nonce + payload)
           
7.0        • Calculate HMAC-SHA256 (~1.5ms)
           
8.5        • Convert HMAC to hex string
           
9.0        • Build secured JSON
           
9.5        ┌─────────────────────────────────────────────┐
           │  securePayload() COMPLETE                    │
           └─────────────────────────────────────────────┘
           
10.0       HTTP POST begins
           
150.0      HTTP POST completes (network latency)
           
150.5      Server response received
           
151.0      uploadSmartCompressedDataToCloud() returns

─────────────────────────────────────────────────────────────
Total Time: ~151 ms
Security Overhead: ~9.5 ms (6% of total time)
Network Time: ~140 ms (93% of total time)
```

---

**End of Diagrams**

These diagrams provide a visual understanding of:
- Overall system architecture
- HMAC authentication process
- Anti-replay protection mechanism  
- Optional encryption flow
- Memory layout
- Execution timeline

Refer to `SECURITY_IMPLEMENTATION_GUIDE.md` for detailed explanations.
