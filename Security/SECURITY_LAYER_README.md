# Security Layer Integration - Flask Server

This document explains how the security layer has been integrated into the Flask server to verify and decrypt data received from the ESP32 EcoWatt device.

## Overview

The ESP32 device applies a security layer to compressed data before uploading to the Flask server. The security layer provides:

1. **Authentication & Integrity**: HMAC-SHA256 ensures data hasn't been tampered with
2. **Anti-Replay Protection**: Nonce-based system prevents replay attacks
3. **Optional Encryption**: AES-128-CBC encryption (currently disabled in mock mode)

## Security Flow

### On ESP32 Device (Sender)
1. Compress sensor data using smart compression
2. Create JSON payload with compressed data
3. Apply security layer:
   - Increment nonce counter
   - Calculate HMAC-SHA256 over (nonce + payload)
   - Optionally encrypt payload with AES-128-CBC
   - Base64 encode the payload
   - Create secured JSON: `{nonce, payload, mac, encrypted}`
4. Send secured payload to Flask server

### On Flask Server (Receiver)
1. Receive secured payload
2. Parse secured JSON structure
3. Verify nonce (anti-replay check)
4. Decode base64 payload
5. Decrypt if encrypted (AES-128-CBC)
6. Verify HMAC-SHA256
7. If verification succeeds, extract original JSON
8. Decompress data and process normally

## Secured Payload Format

```json
{
  "nonce": 10001,
  "payload": "eyJkZXZpY2VfaWQiOi4uLn0=",
  "mac": "a1b2c3d4e5f6...",
  "encrypted": false
}
```

**Fields:**
- `nonce`: Monotonically increasing counter for anti-replay protection
- `payload`: Base64-encoded data (encrypted or plaintext)
- `mac`: HMAC-SHA256 hex string for authentication
- `encrypted`: Boolean indicating if AES encryption was used

## Pre-Shared Keys (PSK)

**IMPORTANT**: The following keys are hardcoded for development. In production, these must be securely provisioned!

```python
# HMAC Key (32 bytes for SHA256)
PSK_HMAC = bytes([
    0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
    0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c,
    0x76, 0x2e, 0x71, 0x60, 0xf3, 0x8b, 0x4d, 0xa5,
    0x6a, 0x78, 0x4d, 0x90, 0x45, 0x19, 0x0c, 0xfe
])

# AES Key (16 bytes for AES-128)
PSK_AES = bytes([
    0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
    0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c
])

# AES IV (16 bytes)
AES_IV = bytes([
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f
])
```

These keys **must match exactly** between ESP32 and Flask server.

## Code Implementation

### Flask Server Function

The main security function is `verify_and_remove_security()` in `flask_server_hivemq.py`:

```python
def verify_and_remove_security(secured_payload_str):
    """
    Verify HMAC and decrypt (if encrypted) the secured payload from ESP32.
    
    Returns:
        tuple: (success: bool, decrypted_payload: str or None, error_message: str or None)
    """
    # 1. Parse secured JSON
    # 2. Check nonce for replay protection
    # 3. Decode base64 payload
    # 4. Decrypt if encrypted
    # 5. Verify HMAC
    # 6. Return decrypted payload
```

### Integration with /process Endpoint

The `/process` endpoint now automatically detects secured payloads and removes the security layer:

```python
@app.route('/process', methods=['POST'])
def process_compressed_data():
    # Check if payload has security layer
    if all(key in data for key in ['nonce', 'payload', 'mac']):
        # Remove security layer
        success, decrypted_payload, error = verify_and_remove_security(raw_data)
        
        if not success:
            return jsonify({'error': error}), 401
        
        # Continue with decompression...
```

## Installation

Install required Python packages:

```bash
pip install -r requirements.txt
```

Key package: `pycryptodome` for AES encryption/decryption

## Testing

### 1. Unit Test
Run the security layer test script:

```bash
python test_security.py
```

This creates a test secured payload and saves it to `test_secured_payload.json`.

### 2. End-to-End Test
Test with the Flask server:

```bash
# Start Flask server
python flask_server_hivemq.py

# In another terminal, send test payload
curl -X POST http://localhost:5001/process \
     -H 'Content-Type: application/json' \
     -d @test_secured_payload.json
```

### 3. ESP32 Integration Test
Upload firmware to ESP32 and monitor serial output. You should see:
- "Payload secured with nonce XXX"
- Successful upload to Flask server
- Flask server logs showing "Security layer removed successfully!"

## Security Considerations

### Current Implementation (Development Mode)
- ✓ HMAC-SHA256 for authentication
- ✓ Nonce-based anti-replay protection
- ✗ AES encryption disabled (mock mode)
- ✗ Hardcoded PSKs

### Production Recommendations
1. **Enable AES Encryption**: Set `ENABLE_ENCRYPTION = true` in ESP32 code
2. **Secure Key Storage**: Use ESP32 secure boot and flash encryption
3. **Key Rotation**: Implement periodic key rotation mechanism
4. **TLS/HTTPS**: Use HTTPS for transport layer security
5. **Nonce Persistence**: Store nonce in secure storage on server
6. **Rate Limiting**: Implement rate limiting to prevent DoS attacks

## Anti-Replay Protection

The nonce mechanism prevents replay attacks:

1. **ESP32**: Increments nonce for each transmission and persists to NVS
2. **Server**: Tracks last valid nonce and rejects older nonces
3. **Thread-Safe**: Uses locks to prevent race conditions

If a nonce is received that is ≤ last_valid_nonce, the request is rejected as a potential replay attack.

## Error Handling

Possible security verification errors:

| Error | Cause | Solution |
|-------|-------|----------|
| "Missing required security fields" | Malformed JSON | Check payload structure |
| "Replay attack detected" | Old/duplicate nonce | ESP32 may need nonce reset |
| "Failed to decode base64 payload" | Corrupted data | Check data transmission |
| "AES decryption failed" | Wrong key or corrupted data | Verify PSK_AES matches |
| "HMAC verification failed" | Data tampered or wrong key | Verify PSK_HMAC matches |

## Debugging

Enable detailed logging:

```python
logging.basicConfig(level=logging.DEBUG)
```

Key log messages:
- "Detected secured payload - removing security layer..."
- "Security verified: nonce=XXX, encrypted=YYY"
- "Security layer removed successfully!"

## Code Mapping

| ESP32 File | Flask File | Function |
|------------|------------|----------|
| `security.cpp` | `flask_server_hivemq.py` | Security implementation |
| `SecurityLayer::securePayload()` | `verify_and_remove_security()` | Main security function |
| `SecurityLayer::calculateHMAC()` | `hmac.new(PSK_HMAC, ...)` | HMAC calculation |
| `SecurityLayer::encryptAES()` | `AES.new(PSK_AES, ...)` | AES encryption/decryption |
| `PSK_HMAC[32]` | `PSK_HMAC` | HMAC key |
| `PSK_AES[16]` | `PSK_AES` | AES key |
| `currentNonce` | `last_valid_nonce` | Nonce tracking |

## Summary

The security layer integration provides end-to-end data protection from ESP32 device to Flask server:

1. **Data Integrity**: HMAC ensures data hasn't been modified
2. **Authentication**: Shared keys verify sender identity
3. **Replay Protection**: Nonces prevent replay attacks
4. **Optional Confidentiality**: AES encryption (when enabled)

The Flask server automatically detects secured payloads, verifies them, and extracts the original data for decompression and processing.
