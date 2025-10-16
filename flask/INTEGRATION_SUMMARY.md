# Security Layer Integration - Summary of Changes

## Overview
This document summarizes the changes made to integrate the security layer removal functionality into the Flask server.

## Files Modified

### 1. `flask_server_hivemq.py` (MODIFIED)

#### Added Imports
```python
import hmac
import hashlib
from Crypto.Cipher import AES
from Crypto.Util.Padding import unpad
```

#### Added Security Layer Constants (Lines ~342-373)
```python
# Pre-shared keys matching ESP32
PSK_HMAC = bytes([0x2b, 0x7e, ...])  # 32 bytes
PSK_AES = bytes([0x2b, 0x7e, ...])   # 16 bytes
AES_IV = bytes([0x00, 0x01, ...])     # 16 bytes

# Nonce tracking
last_valid_nonce = 10000
nonce_lock = threading.Lock()
```

#### Added Security Function (Lines ~375-440)
```python
def verify_and_remove_security(secured_payload_str):
    """
    Verify HMAC and decrypt (if encrypted) the secured payload from ESP32.
    
    Steps:
    1. Parse secured JSON {nonce, payload, mac, encrypted}
    2. Anti-replay check (nonce must be > last_valid_nonce)
    3. Base64 decode payload
    4. Decrypt with AES-128-CBC if encrypted=true
    5. Verify HMAC-SHA256
    6. Return decrypted JSON payload
    
    Returns:
        (success: bool, decrypted_payload: str, error: str)
    """
```

#### Modified `/process` Endpoint (Lines ~875-920)
```python
@app.route('/process', methods=['POST'])
def process_compressed_data():
    # NEW: Get raw data and check for secured payload
    raw_data = request.data.decode('utf-8')
    potential_secured = json.loads(raw_data)
    
    # NEW: Detect and remove security layer
    if all(key in potential_secured for key in ['nonce', 'payload', 'mac']):
        logger.info("Detected secured payload - removing security layer...")
        success, decrypted_payload, error = verify_and_remove_security(raw_data)
        
        if not success:
            return jsonify({'error': error}), 401
        
        data = json.loads(decrypted_payload)
        logger.info("Security layer removed successfully!")
    else:
        # Unsecured payload - process normally
        data = potential_secured
    
    # Continue with existing decompression logic...
```

## Files Created

### 2. `requirements.txt` (NEW)
Dependencies required for the Flask server:
- Flask==2.3.3
- paho-mqtt==1.6.1
- pycryptodome==3.19.0 (for AES/HMAC)
- requests==2.31.0

### 3. `test_security.py` (NEW)
Test script to verify security layer functionality:
- Creates test secured payloads
- Mimics ESP32 security behavior
- Generates test data for endpoint testing
- Saves test payload to `test_secured_payload.json`

### 4. `SECURITY_LAYER_README.md` (NEW)
Comprehensive documentation covering:
- Security flow (ESP32 → Flask)
- Secured payload format
- Pre-shared keys (PSKs)
- Implementation details
- Testing procedures
- Security considerations
- Error handling
- Code mapping between ESP32 and Flask

### 5. `SETUP_GUIDE.md` (NEW)
Quick start guide for setting up the Flask server:
- Installation instructions
- Running the server
- Testing procedures
- Configuration options
- Troubleshooting tips

## Key Features Implemented

### ✓ Security Layer Verification
- HMAC-SHA256 authentication
- Anti-replay protection with nonce tracking
- AES-128-CBC decryption support (when enabled)
- Thread-safe nonce management

### ✓ Automatic Detection
- Detects secured vs unsecured payloads
- Backward compatible with existing unsecured data

### ✓ Error Handling
- Detailed error messages for debugging
- Proper HTTP status codes (401 for auth failure)
- Comprehensive logging

### ✓ Security Best Practices
- Constant-time HMAC comparison (prevents timing attacks)
- Thread-safe nonce updates
- Clear separation of security and business logic

## Data Flow

### Before (Unsecured)
```
ESP32 → Compressed JSON → Flask /process → Decompress → Process
```

### After (Secured)
```
ESP32 → Compress → Secure (HMAC+AES) → Flask /process → 
  Verify Security → Decrypt → Decompress → Process
```

## Mapping: ESP32 ↔ Flask

| ESP32 Component | Flask Component | Purpose |
|----------------|-----------------|---------|
| `SecurityLayer::securePayload()` | `verify_and_remove_security()` | Main security function |
| `SecurityLayer::calculateHMAC()` | `hmac.new(PSK_HMAC, ...)` | HMAC calculation |
| `SecurityLayer::encryptAES()` | `AES.new(...).decrypt()` | AES encryption/decryption |
| `PSK_HMAC[32]` | `PSK_HMAC` bytes | HMAC shared key |
| `PSK_AES[16]` | `PSK_AES` bytes | AES shared key |
| `AES_IV[16]` | `AES_IV` bytes | AES initialization vector |
| `currentNonce` | `last_valid_nonce` | Anti-replay nonce |
| `incrementAndSaveNonce()` | Nonce validation | Nonce management |

## Testing Checklist

- [ ] Install dependencies: `pip install -r requirements.txt`
- [ ] Run test script: `python test_security.py`
- [ ] Start Flask server: `python flask_server_hivemq.py`
- [ ] Test with curl: `curl -X POST http://localhost:5001/process -d @test_secured_payload.json`
- [ ] Upload ESP32 firmware with security enabled
- [ ] Verify end-to-end: ESP32 → Flask with security
- [ ] Check logs for "Security layer removed successfully!"
- [ ] Test anti-replay: send same nonce twice (should fail)

## Security Notes

### Current Status (Development)
✓ HMAC authentication enabled
✓ Nonce-based replay protection enabled
✗ AES encryption in mock mode (disabled)
✗ Keys are hardcoded (development only)

### For Production
⚠️ Enable real AES encryption
⚠️ Use secure key provisioning
⚠️ Implement key rotation
⚠️ Add TLS/HTTPS transport security
⚠️ Persist nonce in secure database
⚠️ Add rate limiting

## Integration Complete ✓

The Flask server now:
1. ✓ Accepts both secured and unsecured payloads
2. ✓ Automatically detects security layer
3. ✓ Verifies HMAC authentication
4. ✓ Checks nonce for replay attacks
5. ✓ Decrypts AES-encrypted data (when enabled)
6. ✓ Extracts original JSON for decompression
7. ✓ Provides detailed error messages
8. ✓ Maintains backward compatibility

## Questions?

Refer to:
- `SECURITY_LAYER_README.md` - Detailed technical documentation
- `SETUP_GUIDE.md` - Installation and setup instructions
- `test_security.py` - Example code and testing
