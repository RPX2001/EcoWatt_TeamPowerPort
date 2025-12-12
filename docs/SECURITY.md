# 🔐 EcoWatt Security Guide

Comprehensive documentation of security implementations, threat model, and best practices.

---

## Table of Contents

- [Security Overview](#security-overview)
- [Threat Model](#threat-model)
- [Upload Security](#upload-security)
- [FOTA Security](#fota-security)
- [Key Management](#key-management)
- [Attack Mitigations](#attack-mitigations)
- [Security Best Practices](#security-best-practices)

---

## Security Overview

### Security Layers

EcoWatt implements **defense-in-depth** security with multiple independent layers:

```
┌──────────────────────────────────────────────────┐
│              Security Layers                     │
├──────────────────────────────────────────────────┤
│                                                  │
│  Layer 1: Transport Security (HTTPS/TLS)        │
│  Layer 2: Anti-Replay Protection (Nonce)        │
│  Layer 3: Message Authentication (HMAC-SHA256)  │
│  Layer 4: Confidentiality (AES-128-CBC)         │
│  Layer 5: Firmware Authenticity (RSA-2048)      │
│                                                  │
└──────────────────────────────────────────────────┘
```

### Security Goals

| Goal | Implementation | Status |
|:-----|:--------------|:-------|
| **Confidentiality** | AES-128-CBC encryption | ✅ Supported |
| **Integrity** | HMAC-SHA256 signatures | ✅ Active |
| **Authenticity** | Pre-shared keys | ✅ Active |
| **Non-Repudiation** | Logging & audit trail | ✅ Active |
| **Availability** | Fault tolerance & recovery | ✅ Active |

---

## Threat Model

### Assets to Protect

1. **Sensor Data**: Solar inverter measurements
2. **Firmware**: ESP32 application code
3. **Cryptographic Keys**: HMAC keys, RSA keys
4. **Device Identity**: Device credentials
5. **Command Authority**: Remote control access

### Threat Actors

| Actor | Capability | Motivation |
|:------|:-----------|:-----------|
| **Script Kiddie** | Low | Curiosity, vandalism |
| **Malicious Insider** | Medium | Data theft, sabotage |
| **Competitor** | High | IP theft, market intelligence |
| **Nation State** | Very High | Infrastructure disruption |

### Attack Vectors

```
Internet
   │
   ├─── Man-in-the-Middle ──────► Mitigated by HTTPS/TLS
   │
   ├─── Replay Attack ──────────► Mitigated by Nonce
   │
   ├─── Data Forgery ────────────► Mitigated by HMAC
   │
   ├─── Malicious Firmware ──────► Mitigated by RSA signatures
   │
   └─── DoS Attack ───────────────► Mitigated by rate limiting
```

---

## Upload Security

### Complete Security Flow

```
ESP32:
  ┌──────────────────────────────────────┐
  │ 1. Load nonce from NVS               │
  │    nonce = 10001                     │
  └────────────┬─────────────────────────┘
               │
  ┌────────────▼─────────────────────────┐
  │ 2. Compress data                     │
  │    payload = compress(sensor_data)   │
  └────────────┬─────────────────────────┘
               │
  ┌────────────▼─────────────────────────┐
  │ 3. (Optional) Encrypt payload        │
  │    encrypted = AES(payload, key, iv) │
  └────────────┬─────────────────────────┘
               │
  ┌────────────▼─────────────────────────┐
  │ 4. Base64 encode                     │
  │    b64_payload = base64(encrypted)   │
  └────────────┬─────────────────────────┘
               │
  ┌────────────▼─────────────────────────┐
  │ 5. Calculate HMAC                    │
  │    mac = HMAC-SHA256(key,            │
  │                      nonce+payload)  │
  └────────────┬─────────────────────────┘
               │
  ┌────────────▼─────────────────────────┐
  │ 6. Create JSON packet                │
  │    {nonce, payload, mac, ...}        │
  └────────────┬─────────────────────────┘
               │
  ┌────────────▼─────────────────────────┐
  │ 7. HTTPS POST to server              │
  └──────────────────────────────────────┘

Server:
  ┌──────────────────────────────────────┐
  │ 8. Receive packet                    │
  └────────────┬─────────────────────────┘
               │
  ┌────────────▼─────────────────────────┐
  │ 9. Check nonce > last_nonce          │
  │    if not: reject as replay attack   │
  └────────────┬─────────────────────────┘
               │
  ┌────────────▼─────────────────────────┐
  │ 10. Recalculate HMAC                 │
  │     expected_mac = HMAC-SHA256(...)  │
  └────────────┬─────────────────────────┘
               │
  ┌────────────▼─────────────────────────┐
  │ 11. Compare MACs                     │
  │     if mac != expected: reject       │
  └────────────┬─────────────────────────┘
               │
  ┌────────────▼─────────────────────────┐
  │ 12. Base64 decode payload            │
  └────────────┬─────────────────────────┘
               │
  ┌────────────▼─────────────────────────┐
  │ 13. (Optional) Decrypt               │
  │     payload = AES_decrypt(...)       │
  └────────────┬─────────────────────────┘
               │
  ┌────────────▼─────────────────────────┐
  │ 14. Decompress data                  │
  └────────────┬─────────────────────────┘
               │
  ┌────────────▼─────────────────────────┐
  │ 15. Store in database                │
  └────────────┬─────────────────────────┘
               │
  ┌────────────▼─────────────────────────┐
  │ 16. Update last_nonce                │
  └──────────────────────────────────────┘
```

### Nonce Management

**ESP32 Implementation**:

```cpp
// nonce.h
#define NVS_NAMESPACE "ecowatt"
#define NVS_NONCE_KEY "nonce"

uint32_t load_nonce() {
    nvs_handle_t handle;
    uint32_t nonce = 0;
    
    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle));
    nvs_get_u32(handle, NVS_NONCE_KEY, &nonce);
    nvs_close(handle);
    
    return nonce;
}

void save_nonce(uint32_t nonce) {
    nvs_handle_t handle;
    
    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle));
    ESP_ERROR_CHECK(nvs_set_u32(handle, NVS_NONCE_KEY, nonce));
    ESP_ERROR_CHECK(nvs_commit(handle));
    nvs_close(handle);
}

uint32_t generate_next_nonce() {
    uint32_t nonce = load_nonce();
    nonce++;
    save_nonce(nonce);
    return nonce;
}
```

**Server Implementation**:

```python
# security_handler.py
def validate_nonce(device_id, nonce):
    """Validate nonce to prevent replay attacks"""
    last_nonce = get_last_nonce_from_db(device_id)
    
    if last_nonce is None:
        # First communication from device
        update_nonce_in_db(device_id, nonce)
        return True
    
    if nonce <= last_nonce:
        log_security_event(device_id, "REPLAY_ATTACK", nonce)
        raise SecurityException("Replay attack detected")
    
    # Nonce is valid, update database
    update_nonce_in_db(device_id, nonce)
    return True
```

### HMAC-SHA256 Authentication

**Key Derivation**:

```python
# generate_keys.py
import secrets
import base64

def generate_hmac_key():
    """Generate 256-bit random key"""
    key_bytes = secrets.token_bytes(32)  # 256 bits
    key_hex = key_bytes.hex()
    key_b64 = base64.b64encode(key_bytes).decode()
    
    return {
        'bytes': key_bytes,
        'hex': key_hex,
        'base64': key_b64
    }
```

**ESP32 HMAC Calculation**:

```cpp
// security.cpp
#include "mbedtls/md.h"

void calculate_hmac_sha256(
    const uint8_t* key, size_t key_len,
    const uint8_t* data, size_t data_len,
    uint8_t* output
) {
    mbedtls_md_context_t ctx;
    mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
    
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 1);
    mbedtls_md_hmac_starts(&ctx, key, key_len);
    mbedtls_md_hmac_update(&ctx, data, data_len);
    mbedtls_md_hmac_finish(&ctx, output);
    mbedtls_md_free(&ctx);
}

// Usage
uint8_t hmac[32];
calculate_hmac_sha256(
    hmac_key, 32,
    message, message_len,
    hmac
);
```

**Server HMAC Validation**:

```python
# security_handler.py
import hmac
import hashlib

def validate_hmac(key, nonce, payload, received_mac):
    """Validate HMAC-SHA256 signature"""
    # Construct message (nonce + payload)
    message = str(nonce).encode() + payload.encode()
    
    # Calculate expected HMAC
    expected_mac = hmac.new(
        key.encode(),
        message,
        hashlib.sha256
    ).hexdigest()
    
    # Constant-time comparison to prevent timing attacks
    return hmac.compare_digest(expected_mac, received_mac)
```

### AES-128-CBC Encryption

**ESP32 Encryption**:

```cpp
// security.cpp
#include "mbedtls/aes.h"

int aes_encrypt_cbc(
    const uint8_t* key,      // 16 bytes
    const uint8_t* iv,       // 16 bytes
    const uint8_t* input,
    size_t input_len,
    uint8_t* output
) {
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    
    // Set encryption key
    mbedtls_aes_setkey_enc(&aes, key, 128);
    
    // Encrypt in CBC mode
    uint8_t iv_copy[16];
    memcpy(iv_copy, iv, 16);  // IV is modified during encryption
    
    int ret = mbedtls_aes_crypt_cbc(
        &aes,
        MBEDTLS_AES_ENCRYPT,
        input_len,
        iv_copy,
        input,
        output
    );
    
    mbedtls_aes_free(&aes);
    return ret;
}
```

**Server Decryption**:

```python
# security_handler.py
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
from cryptography.hazmat.backends import default_backend

def aes_decrypt_cbc(key, iv, ciphertext):
    """Decrypt data using AES-128-CBC"""
    cipher = Cipher(
        algorithms.AES(key),
        modes.CBC(iv),
        backend=default_backend()
    )
    
    decryptor = cipher.decryptor()
    plaintext = decryptor.update(ciphertext) + decryptor.finalize()
    
    # Remove PKCS7 padding
    padding_len = plaintext[-1]
    return plaintext[:-padding_len]
```

---

## FOTA Security

### Complete FOTA Security Flow

```
Server:
  ┌───────────────────────────────────────┐
  │ 1. Receive firmware.bin               │
  └────────────┬──────────────────────────┘
               │
  ┌────────────▼──────────────────────────┐
  │ 2. Calculate SHA-256 hash             │
  │    hash = SHA256(firmware.bin)        │
  └────────────┬──────────────────────────┘
               │
  ┌────────────▼──────────────────────────┐
  │ 3. Sign hash with RSA private key     │
  │    signature = RSA_sign(hash)         │
  └────────────┬──────────────────────────┘
               │
  ┌────────────▼──────────────────────────┐
  │ 4. Generate random IV                 │
  │    iv = random_bytes(16)              │
  └────────────┬──────────────────────────┘
               │
  ┌────────────▼──────────────────────────┐
  │ 5. Encrypt firmware with AES-256-CBC  │
  │    encrypted = AES256(firmware, iv)   │
  └────────────┬──────────────────────────┘
               │
  ┌────────────▼──────────────────────────┐
  │ 6. Split into 2KB chunks              │
  └────────────┬──────────────────────────┘
               │
  ┌────────────▼──────────────────────────┐
  │ 7. Create manifest JSON               │
  │    {version, hash, signature, iv}     │
  └───────────────────────────────────────┘

ESP32:
  ┌───────────────────────────────────────┐
  │ 8. Poll manifest endpoint             │
  └────────────┬──────────────────────────┘
               │
  ┌────────────▼──────────────────────────┐
  │ 9. Compare version with current       │
  │    if new: proceed with update        │
  └────────────┬──────────────────────────┘
               │
  ┌────────────▼──────────────────────────┐
  │ 10. Download chunks sequentially      │
  │     for each 2KB chunk:               │
  │       - Download                      │
  │       - Decrypt                       │
  │       - Write to flash partition      │
  └────────────┬──────────────────────────┘
               │
  ┌────────────▼──────────────────────────┐
  │ 11. Calculate SHA-256 of decrypted FW │
  │     calculated_hash = SHA256(...)     │
  └────────────┬──────────────────────────┘
               │
  ┌────────────▼──────────────────────────┐
  │ 12. Compare with manifest hash        │
  │     if not match: rollback & abort    │
  └────────────┬──────────────────────────┘
               │
  ┌────────────▼──────────────────────────┐
  │ 13. Verify RSA signature              │
  │     valid = RSA_verify(hash, sig, pk) │
  │     if not valid: rollback & abort    │
  └────────────┬──────────────────────────┘
               │
  ┌────────────▼──────────────────────────┐
  │ 14. Mark partition as bootable        │
  └────────────┬──────────────────────────┘
               │
  ┌────────────▼──────────────────────────┐
  │ 15. Reboot into new firmware          │
  └────────────┬──────────────────────────┘
               │
  ┌────────────▼──────────────────────────┐
  │ 16. Verify boot success               │
  │     if fail: auto-rollback            │
  └───────────────────────────────────────┘
```

### RSA Key Generation

```bash
# Generate RSA-2048 private key
openssl genrsa -out private_key.pem 2048

# Extract public key
openssl rsa -in private_key.pem -pubout -out public_key.pem

# Convert public key to C header format
python scripts/generate_keys.py
```

**Generated keys.h**:

```cpp
// keys.h
#ifndef KEYS_H
#define KEYS_H

// RSA-2048 Public Key (for firmware verification)
const char* RSA_PUBLIC_KEY = 
"-----BEGIN PUBLIC KEY-----\n"
"MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA...\n"
"-----END PUBLIC KEY-----";

// HMAC-SHA256 Key (for data authentication)
const uint8_t HMAC_KEY[32] = {
    0x5a, 0x7b, 0x3c, 0x9d, 0x2e, 0x4f, 0x1a, 0x6b,
    // ... 32 bytes total
};

#endif
```

### Firmware Signing

**Server-side**:

```python
# prepare_firmware.py
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import rsa, padding
from cryptography.hazmat.backends import default_backend

def sign_firmware(firmware_path, private_key_path):
    # Read firmware
    with open(firmware_path, 'rb') as f:
        firmware_data = f.read()
    
    # Calculate SHA-256 hash
    digest = hashes.Hash(hashes.SHA256(), backend=default_backend())
    digest.update(firmware_data)
    firmware_hash = digest.finalize()
    
    # Load private key
    with open(private_key_path, 'rb') as f:
        private_key = serialization.load_pem_private_key(
            f.read(),
            password=None,
            backend=default_backend()
        )
    
    # Sign hash
    signature = private_key.sign(
        firmware_hash,
        padding.PSS(
            mgf=padding.MGF1(hashes.SHA256()),
            salt_length=padding.PSS.MAX_LENGTH
        ),
        hashes.SHA256()
    )
    
    return firmware_hash.hex(), signature.hex()
```

### Signature Verification

**ESP32-side**:

```cpp
// OTAManager.cpp
#include "mbedtls/rsa.h"
#include "mbedtls/sha256.h"

bool verify_firmware_signature(
    const uint8_t* firmware_hash,
    const uint8_t* signature,
    size_t signature_len
) {
    mbedtls_rsa_context rsa;
    mbedtls_rsa_init(&rsa, MBEDTLS_RSA_PKCS_V21, MBEDTLS_MD_SHA256);
    
    // Load public key
    int ret = load_rsa_public_key(&rsa, RSA_PUBLIC_KEY);
    if (ret != 0) {
        return false;
    }
    
    // Verify signature
    ret = mbedtls_rsa_rsassa_pss_verify(
        &rsa,
        NULL, NULL,
        MBEDTLS_RSA_PUBLIC,
        MBEDTLS_MD_SHA256,
        32,  // Hash length
        firmware_hash,
        signature
    );
    
    mbedtls_rsa_free(&rsa);
    return (ret == 0);
}
```

---

## Key Management

### Key Storage

| Key Type | Location | Protection |
|:---------|:---------|:-----------|
| **HMAC Key** | ESP32 firmware | Embedded in code, flash encryption |
| **RSA Private Key** | Server filesystem | File permissions (600) |
| **RSA Public Key** | ESP32 firmware | Embedded in code |
| **AES Encryption Key** | Server config | Environment variable |

### Key Rotation

**HMAC Key Rotation**:
1. Generate new key
2. Distribute to all devices via secure channel
3. Update server configuration
4. Switch over at coordinated time

**RSA Key Rotation**:
1. Generate new RSA key pair
2. Rebuild firmware with new public key
3. Deploy firmware update (signed with old key)
4. After update, new firmware uses new key

---

## Attack Mitigations

### Replay Attack

**Mitigation**: Monotonically increasing nonce

**Test**:
```python
# Test replay attack
packet = capture_valid_packet()
response1 = send_packet(packet)  # Success
response2 = send_packet(packet)  # Should fail with replay error
```

### Man-in-the-Middle (MITM)

**Mitigation**: HTTPS/TLS with certificate validation

**ESP32 Configuration**:
```cpp
// Enable certificate validation
esp_http_client_config_t config = {
    .url = "https://server.example.com",
    .cert_pem = server_root_ca_pem_start,
    .skip_cert_common_name_check = false
};
```

### Data Forgery

**Mitigation**: HMAC-SHA256 signature

**Test**:
```python
# Test data forgery
packet = {
    'nonce': 10001,
    'payload': 'forged_data',
    'mac': 'invalid_mac'
}
response = send_packet(packet)  # Should fail HMAC validation
```

### Malicious Firmware

**Mitigation**: RSA-2048 signature verification

**Test**:
```cpp
// Attempt to install unsigned firmware
bool success = install_firmware("malicious.bin");
// Should fail signature verification and rollback
```

---

## Security Best Practices

### Development

1. **Never Hardcode Keys**: Use environment variables or secure storage
2. **Rotate Keys Regularly**: Minimum every 90 days
3. **Use Strong Random**: `secrets` module in Python, hardware RNG on ESP32
4. **Validate All Inputs**: Never trust user input
5. **Log Security Events**: Audit trail for forensics

### Deployment

1. **Enable Flash Encryption**: Protect firmware in ESP32 flash
2. **Enable Secure Boot**: Verify bootloader signature
3. **Use HTTPS**: Always encrypt transport layer
4. **Restrict Access**: Firewall rules, VPN access
5. **Monitor Logs**: Detect anomalies

### Operational

1. **Regular Updates**: Apply security patches promptly
2. **Backup Keys**: Secure, encrypted backups
3. **Incident Response**: Documented procedure
4. **Security Audits**: Regular third-party reviews
5. **Penetration Testing**: Annual pen tests

---

[← Back to Main README](../README.md)
