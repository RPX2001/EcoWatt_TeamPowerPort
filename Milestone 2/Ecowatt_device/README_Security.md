# EcoWatt Security Layer Implementation

This document explains the lightweight security layer implementation for the NodeMCU/ESP32 platform that secures data uploads to the cloud.

## Features

The security layer implements the following features as required:

### ✅ **Authentication and Integrity**
- **HMAC-SHA256** digital signatures for message authentication
- **Pre-shared Key (PSK)** based authentication (256-bit key)
- Tamper detection - any modification to the message is detected

### ✅ **Confidentiality (Simplified)**
- **Mock encryption** using base64 encoding with character shifting
- Optional encryption flag in secured messages
- Can be easily replaced with real encryption (AES, ChaCha20) if needed

### ✅ **Anti-Replay Protection**
- **Sequential nonce** system prevents message replay attacks
- Persistent nonce storage using ESP32 Preferences (survives reboots)
- Configurable nonce window for out-of-order message tolerance

### ✅ **Secured Payload Format**
Messages follow the required format:
```json
{
  "nonce": 11003,
  "payload": "<original JSON data>",
  "encrypted": false,
  "mac": "<HMAC-SHA256 tag in hex>"
}
```

## Implementation

### 1. Security Layer Integration

The security layer is integrated into your main EcoWatt system as follows:

```cpp
#include "simple_security.h"

// Global security instance
SimpleSecurity security;

void setupSystem() {
    // ... existing setup code ...
    
    // Initialize security with 256-bit PSK (64 hex chars)
    if (!security.begin("4A6F486E20446F652041455336342D536563726574204B65792D323536626974")) {
        Serial.println("ERROR: Security initialization failed!");
    }
}

void uploadSmartCompressedDataToCloud() {
    // ... create your JSON payload ...
    
    // Secure the payload
    String securedPayload = security.secureMessage(jsonString);
    
    // Send secured payload instead of raw JSON
    int httpResponseCode = http.POST(securedPayload);
}
```

### 2. Server-Side Verification

A Python Flask server (`server_secured.py`) is provided that can verify secured messages:

```python
# Verify the secured message
success, original_payload = verify_secured_message(secured_payload)

if success:
    # Process the verified data
    data = json.loads(original_payload)
    # ... handle your sensor data ...
else:
    # Reject the message
    return "Security verification failed", 400
```

### 3. Key Management

The PSK is stored persistently in ESP32 flash memory:

- **Initial Setup**: PSK provided in `security.begin(pskHex)`
- **Persistent Storage**: Automatically saved to ESP32 Preferences
- **Auto-load**: PSK loaded from storage on subsequent boots
- **Format**: 64 hexadecimal characters (256 bits)

Example PSK generation (Python):
```python
import secrets
psk = secrets.token_hex(32)  # 32 bytes = 256 bits
print(f"PSK: {psk}")
```

### 4. Nonce Management

Sequential nonce system with persistent storage:

- **Increment**: Each message gets `nonce = currentNonce++`
- **Validation**: Server accepts only `receivedNonce > lastValidNonce`
- **Persistence**: Nonces survive device reboots using Preferences
- **Recovery**: After reboot, nonce continues from last stored value

## Security Analysis

### Threat Model Protection

| Threat | Protection | Implementation |
|--------|------------|----------------|
| **Message Tampering** | HMAC-SHA256 verification | Any byte change invalidates HMAC |
| **Replay Attacks** | Sequential nonce validation | Old messages rejected |
| **Eavesdropping** | Optional mock encryption | Base64 + character shifting |
| **Key Compromise** | PSK rotation capability | `setPSK()` method available |
| **Device Reset** | Persistent nonce storage | ESP32 Preferences used |

### Limitations

This is a **lightweight implementation** suitable for IoT devices:

1. **PSK Storage**: Keys stored in flash (not hardware-secured)
2. **Mock Encryption**: Demonstration only - use real encryption for sensitive data
3. **No Key Exchange**: PSK must be pre-shared (no dynamic key agreement)
4. **Single PSK**: One key for all communications (suitable for single-tenant systems)

## Usage Examples

### Basic Message Security

```cpp
SimpleSecurity sec;
sec.begin("your-256-bit-psk-in-hex");

// Secure a message
String original = "Hello World";
String secured = sec.secureMessage(original);

// Verify a message  
String verified = sec.unsecureMessage(secured);
// Returns original message if valid, empty string if invalid
```

### JSON Data Security

```cpp
// Create sensor data JSON
JsonDocument doc;
doc["temperature"] = 25.4;
doc["humidity"] = 60.2;
String json;
serializeJson(doc, json);

// Secure the JSON
String securedJson = security.secureMessage(json);

// Send to server
http.POST(securedJson);
```

### With Mock Encryption

```cpp
// Enable mock encryption
String encrypted = security.secureMessage(jsonData, true);
// Sets "encrypted": true in the secured message
```

## Testing

Run the security test demo:

1. Upload `security_test_demo.cpp` to your ESP32
2. Open Serial Monitor
3. Observe test results for:
   - Message authentication
   - JSON handling  
   - Mock encryption/decryption
   - Anti-replay protection
   - Tamper detection

## Server Setup

1. **Install Python dependencies**:
   ```bash
   pip install flask
   ```

2. **Run the secured server**:
   ```bash
   python server_secured.py
   ```

3. **Update ESP32 server URL** to match your server IP:
   ```cpp
   const char* serverURL = "http://YOUR_SERVER_IP:5001/process";
   ```

## Production Recommendations

For production deployment, consider:

1. **Hardware Security**: Use ESP32's secure boot and flash encryption
2. **Real Encryption**: Replace mock encryption with AES-256-GCM or ChaCha20-Poly1305
3. **Key Rotation**: Implement periodic PSK rotation
4. **Certificate Pinning**: Use HTTPS with certificate validation
5. **Rate Limiting**: Implement server-side rate limiting
6. **Monitoring**: Add security event logging and monitoring

## Troubleshooting

### Common Issues

1. **"Security initialization failed"**
   - Check PSK format (must be exactly 64 hex characters)
   - Verify Preferences namespace availability

2. **"HMAC verification failed"**
   - PSK mismatch between ESP32 and server
   - Message corruption during transmission
   - Wrong HMAC calculation

3. **"Nonce replay detected"**
   - Clock synchronization issues
   - Old messages being retransmitted
   - Nonce storage corruption

### Debug Output

Enable detailed security logging:
```cpp
// The security layer automatically prints debug info to Serial
// Look for "[Security]" prefixed messages
```

## File Structure

```
├── include/
│   └── simple_security.h          # Security layer header
├── src/
│   ├── simple_security.cpp        # Security layer implementation  
│   └── main.cpp                   # Main application with security integration
├── server_secured.py              # Python server with security verification
├── security_test_demo.cpp         # Security layer test suite
└── README_Security.md             # This documentation
```

---

**⚠️ Important**: This implementation provides a lightweight security layer suitable for IoT applications. For highly sensitive data, consider additional security measures and cryptographic review.