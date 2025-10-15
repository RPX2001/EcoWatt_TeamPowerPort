# EcoWatt FOTA (Firmware Over-The-Air) System - Complete Documentation

## Table of Contents
1. [Introduction](#introduction)
2. [Theoretical Background](#theoretical-background)
3. [System Architecture](#system-architecture)
4. [Security Implementation](#security-implementation)
5. [Complete Pipeline](#complete-pipeline)
6. [Code Implementation](#code-implementation)
7. [Testing Guide](#testing-guide)
8. [Troubleshooting](#troubleshooting)
9. [Best Practices](#best-practices)

---

## 1. Introduction

### What is FOTA?
**Firmware Over-The-Air (FOTA)** is a technology that enables wireless delivery of firmware updates to IoT devices without physical access. It's critical for:
- Bug fixes in deployed devices
- Security patches
- Feature enhancements
- Remote device management

### Why Secure FOTA?
Unsecured FOTA systems are vulnerable to:
- **Man-in-the-Middle attacks**: Attackers intercept and modify firmware
- **Replay attacks**: Old vulnerable firmware is reinstalled
- **Unauthorized updates**: Malicious firmware installed
- **Firmware theft**: Proprietary code stolen during transmission

---

## 2. Theoretical Background

### 2.1 Cryptographic Foundations

#### 2.1.1 Symmetric Encryption (AES-256-CBC)
**Purpose**: Protect firmware confidentiality during transmission

**How it works**:
```
Plain Firmware → AES Encryption → Encrypted Firmware
                  (with key)
```

**Key Concepts**:
- **AES-256**: Advanced Encryption Standard with 256-bit key (very secure)
- **CBC Mode**: Cipher Block Chaining - each block depends on previous block
- **IV (Initialization Vector)**: Random 16-byte value ensures different ciphertext even for same plaintext
- **Block Size**: 16 bytes (128 bits) - firmware must be padded to multiple of 16

**Mathematical Operation**:
```
Ciphertext[i] = AES_Encrypt(Plaintext[i] ⊕ Ciphertext[i-1], Key)
Where Ciphertext[0] = IV
```

**Why CBC over ECB?**
- ECB encrypts each block independently → patterns visible
- CBC chains blocks together → randomizes output

#### 2.1.2 Asymmetric Encryption (RSA-2048)
**Purpose**: Digital signatures to verify firmware authenticity

**How it works**:
```
Server has: Private Key (secret)
ESP32 has: Public Key (shared)

Signing:   Hash → Sign with Private Key → Signature
Verifying: Signature → Decrypt with Public Key → Verify matches Hash
```

**Key Concepts**:
- **RSA-2048**: Public-key cryptography with 2048-bit key
- **Private Key**: Only server has this, used to sign firmware
- **Public Key**: ESP32 has this, used to verify signature
- **Mathematical Basis**: Based on difficulty of factoring large primes

**Why RSA?**
- Signatures can't be forged without private key
- Anyone with public key can verify authenticity
- Widely supported and tested

#### 2.1.3 Hash Functions (SHA-256)
**Purpose**: Create unique "fingerprint" of firmware

**Properties**:
1. **Deterministic**: Same input → same output
2. **Fixed Size**: Always 256 bits (32 bytes) regardless of input size
3. **One-Way**: Can't reverse hash to get original data
4. **Collision-Resistant**: Nearly impossible to find two inputs with same hash
5. **Avalanche Effect**: Tiny change in input → completely different hash

**Example**:
```
Firmware (1 MB) → SHA-256 → 0006dec671e04be987ae43fb13bb58c4...
Changed 1 bit   → SHA-256 → f8a91b3c2e7d4f5a6b8c9d0e1f2a3b4c...
                            (completely different!)
```

#### 2.1.4 HMAC (Hash-based Message Authentication Code)
**Purpose**: Authenticate individual chunks and prevent replay attacks

**How it works**:
```
HMAC = Hash(Key || Hash(Key || Message))
```

**Key Concepts**:
- **Pre-Shared Key (PSK)**: Secret key shared between server and ESP32
- **Message**: Chunk data + chunk number
- **Output**: 256-bit authentication tag

**Why HMAC?**
- Proves chunk came from legitimate server (has the PSK)
- Includes chunk number → prevents replay/reordering attacks
- Faster than RSA for per-chunk authentication

### 2.2 PKCS Standards

#### 2.2.1 PKCS#7 Padding
**Purpose**: Pad data to AES block size (16 bytes)

**Algorithm**:
```python
padding_needed = 16 - (data_length % 16)
if padding_needed == 0:
    padding_needed = 16  # Always add at least 1 byte padding
    
padded_data = data + bytes([padding_needed] * padding_needed)
```

**Example**:
```
Data: [A, B, C, D, E]  (5 bytes)
Need: 16 - 5 = 11 bytes padding
Result: [A, B, C, D, E, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B]
```

**Removal**:
```python
last_byte = data[-1]
# Verify all padding bytes match
if all(data[-last_byte:] == last_byte):
    unpadded_data = data[:-last_byte]
```

#### 2.2.2 PKCS#1 v1.5 Padding (RSA Signatures)
**Purpose**: Format data for RSA signing

**Structure**:
```
[0x00] [0x01] [0xFF...0xFF] [0x00] [DigestInfo]
  ^      ^         ^          ^         ^
  |      |         |          |         |
header  type    padding   separator  ASN.1 + hash
```

**DigestInfo for SHA-256**:
```
30 31 30 0d 06 09 60 86 48 01 65 03 04 02 01 05 00 04 20 [32-byte hash]
^                                                      ^    ^
|                                                      |    |
ASN.1 structure identifier                       SHA-256   hash
```

**Why this format?**
- **0x00 0x01**: Header identifies padding scheme
- **0xFF padding**: Makes forgery computationally infeasible
- **0x00 separator**: Marks end of padding
- **DigestInfo**: Identifies hash algorithm (prevents algorithm substitution attacks)

---

## 3. System Architecture

### 3.1 Component Overview

```
┌─────────────────────────────────────────────────────────────┐
│                     FOTA System Architecture                 │
└─────────────────────────────────────────────────────────────┘

┌──────────────────┐         HTTP/JSON        ┌──────────────┐
│                  │◄─────────────────────────►│              │
│   ESP32 Device   │     Chunk Requests        │ Flask Server │
│   (Client)       │     Chunk Delivery        │  (Backend)   │
│                  │                            │              │
└──────────────────┘                            └──────────────┘
        │                                              │
        │ Decrypts & Verifies                          │ Encrypts & Signs
        ▼                                              ▼
┌──────────────────┐                          ┌──────────────┐
│  OTA Partition   │                          │  Firmware    │
│   (Flash)        │                          │  Directory   │
└──────────────────┘                          └──────────────┘
```

### 3.2 Key Components

#### Server Side (Python Flask)
1. **FirmwareManager** (`firmware_manager.py`)
   - Loads cryptographic keys
   - Prepares firmware for distribution
   - Handles encryption, signing, chunking

2. **Flask Server** (`flask_server_hivemq.py`)
   - REST API endpoints
   - Serves firmware manifests
   - Delivers encrypted chunks

3. **Keys Directory** (`keys/`)
   - `server_private_key.pem`: RSA private key for signing
   - `server_public_key.pem`: RSA public key (shared with ESP32)
   - `aes_firmware_key.bin`: AES-256 key (32 bytes)
   - `keys.h`: C header with keys for ESP32

#### Client Side (ESP32 C++)
1. **OTAManager** (`OTAManager.h/cpp`)
   - Orchestrates OTA update process
   - Downloads and decrypts chunks
   - Verifies signatures
   - Writes to OTA partition

2. **Keys** (`keys.h/cpp`)
   - RSA public key for signature verification
   - AES key for decryption
   - HMAC PSK for chunk authentication

3. **Arduino Update Library**
   - Low-level OTA partition management
   - Flash writing
   - Boot partition switching

### 3.3 Data Flow Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    Data Flow Pipeline                        │
└─────────────────────────────────────────────────────────────┘

SERVER SIDE:
┌──────────────┐
│ firmware.bin │  (Original binary from PlatformIO build)
└──────┬───────┘
       │
       ▼
┌──────────────────────────────────────┐
│ 1. Calculate SHA-256 Hash            │
│    Hash = SHA256(firmware)           │
└──────┬───────────────────────────────┘
       │
       ▼
┌──────────────────────────────────────┐
│ 2. Sign Hash with RSA Private Key    │
│    Signature = RSA_Sign(Hash)        │
└──────┬───────────────────────────────┘
       │
       ▼
┌──────────────────────────────────────┐
│ 3. Encrypt with AES-256-CBC          │
│    Encrypted = AES(firmware, IV)     │
└──────┬───────────────────────────────┘
       │
       ▼
┌──────────────────────────────────────┐
│ 4. Split into 1KB Chunks             │
│    Chunks = Split(Encrypted, 1024)   │
└──────┬───────────────────────────────┘
       │
       ▼
┌──────────────────────────────────────┐
│ 5. Calculate HMAC for Each Chunk     │
│    HMAC[i] = HMAC(Chunk[i] + i)      │
└──────┬───────────────────────────────┘
       │
       ▼
┌──────────────────────────────────────┐
│ 6. Create Manifest JSON              │
│    - Version, Size, Hash             │
│    - Signature, IV, Total Chunks     │
└──────┬───────────────────────────────┘
       │
       ▼
┌──────────────────────────────────────┐
│ Files Ready for Distribution:        │
│ - firmware_1.0.4_encrypted.bin       │
│ - firmware_1.0.4_manifest.json       │
└──────────────────────────────────────┘

ESP32 SIDE:
┌──────────────────────────────────────┐
│ 1. Request Manifest                  │
│    GET /ota/manifest?version=1.0.4   │
└──────┬───────────────────────────────┘
       │
       ▼
┌──────────────────────────────────────┐
│ 2. Parse Manifest                    │
│    - Extract: size, hash, sig, IV    │
│    - Initialize AES context with IV  │
└──────┬───────────────────────────────┘
       │
       ▼
┌──────────────────────────────────────┐
│ 3. Download Chunks (Loop 0-990)      │
│    POST /ota/chunk {chunk_number: i} │
└──────┬───────────────────────────────┘
       │
       ▼
┌──────────────────────────────────────┐
│ 4. For Each Chunk:                   │
│    a. Verify HMAC                    │
│    b. Decrypt with AES-CBC           │
│    c. Remove padding (last chunk)    │
│    d. Write to OTA partition         │
└──────┬───────────────────────────────┘
       │
       ▼
┌──────────────────────────────────────┐
│ 5. After All Chunks Downloaded:      │
│    - Read firmware from OTA partition│
│    - Calculate SHA-256 hash          │
│    - Verify RSA signature            │
└──────┬───────────────────────────────┘
       │
       ▼
┌──────────────────────────────────────┐
│ 6. If Verified: Reboot               │
│    - Set boot partition              │
│    - ESP.restart()                   │
└──────────────────────────────────────┘
       │
       ▼
┌──────────────────────────────────────┐
│ 7. Boot into New Firmware            │
│    - Version 1.0.4 running!          │
└──────────────────────────────────────┘
```

---

## 4. Security Implementation

### 4.1 Three-Layer Security Model

```
┌─────────────────────────────────────────────────────────────┐
│                     Security Layers                          │
└─────────────────────────────────────────────────────────────┘

Layer 1: CHUNK AUTHENTICATION (HMAC-SHA256)
┌────────────────────────────────────────────────────────┐
│ • Verifies each chunk came from legitimate server      │
│ • Prevents tampering with individual chunks            │
│ • Prevents replay attacks (chunk number included)      │
│ • Fast verification (symmetric crypto)                 │
│ • PSK: 16ecd7da91ce8db9c0de79be4da968e9...             │
└────────────────────────────────────────────────────────┘

Layer 2: FIRMWARE CONFIDENTIALITY (AES-256-CBC)
┌────────────────────────────────────────────────────────┐
│ • Encrypts entire firmware                             │
│ • Protects intellectual property                       │
│ • Prevents firmware analysis during transit            │
│ • 256-bit key: Very secure (2^256 possible keys)       │
│ • Random IV: Different ciphertext each time            │
└────────────────────────────────────────────────────────┘

Layer 3: FIRMWARE AUTHENTICITY (RSA-2048 Signature)
┌────────────────────────────────────────────────────────┐
│ • Proves firmware came from legitimate source          │
│ • Verifies firmware integrity (not modified)           │
│ • Can't be forged without private key                  │
│ • Verified AFTER all chunks downloaded                 │
│ • 2048-bit key: Industry standard for IoT              │
└────────────────────────────────────────────────────────┘
```

### 4.2 Attack Resistance

| Attack Type | Defense Mechanism | How It Works |
|-------------|-------------------|--------------|
| **Man-in-the-Middle** | RSA Signature + AES Encryption | Attacker can't decrypt firmware (no AES key) or create valid signature (no RSA private key) |
| **Replay Attack** | HMAC with chunk number | Each chunk HMAC includes chunk number; old chunks rejected |
| **Firmware Tampering** | SHA-256 Hash + RSA Signature | Any modification changes hash; signature won't verify |
| **Downgrade Attack** | Version checking | ESP32 compares version numbers; rejects older versions |
| **Firmware Theft** | AES-256-CBC Encryption | Transmitted firmware is encrypted; useless without key |
| **Chunk Reordering** | Sequential download + chunk numbers | Chunks downloaded in order; HMAC includes chunk number |

### 4.3 Key Management

#### Server Keys (Secret - Never Share)
```python
# Private RSA Key (2048-bit)
Location: flask/keys/server_private_key.pem
Purpose: Sign firmware hashes
Format: PEM encoded
Security: Keep offline, backup securely

# AES Key (256-bit)
Location: flask/keys/aes_firmware_key.bin
Purpose: Encrypt firmware
Format: Binary (32 bytes)
Security: Generated randomly, never transmitted

# HMAC PSK (512-bit hex string)
Location: flask/keys/keys.h
Purpose: Authenticate chunks
Format: Hex string (64 characters)
Security: Shared secret between server and ESP32
```

#### ESP32 Keys (Embedded in Firmware)
```cpp
// Public RSA Key (2048-bit)
Location: PIO/ECOWATT/src/application/keys.cpp
Purpose: Verify firmware signatures
Format: PEM string in C code
Security: Public key, safe to embed

// AES Key (256-bit) - Same as server
Location: PIO/ECOWATT/src/application/keys.cpp
Purpose: Decrypt firmware
Format: Byte array
Security: Embedded in firmware, extract-resistant

// HMAC PSK - Same as server
Location: PIO/ECOWATT/src/application/keys.cpp
Purpose: Verify chunk HMACs
Format: Hex string
Security: Embedded in firmware
```

**Key Generation Commands**:
```bash
# Generate RSA key pair
openssl genrsa -out server_private_key.pem 2048
openssl rsa -in server_private_key.pem -pubout -out server_public_key.pem

# Generate AES key
openssl rand -out aes_firmware_key.bin 32

# Generate HMAC PSK
openssl rand -hex 32  # 64 hex characters
```

---

## 5. Complete Pipeline

### 5.1 Firmware Preparation (Server Side)

```python
# Location: flask/firmware_manager.py
class FirmwareManager:
    def prepare_firmware(self, bin_file_path: str, version: str):
        """
        Complete firmware preparation pipeline
        """
        
        # Step 1: Read firmware binary
        with open(bin_file_path, 'rb') as f:
            firmware_data = f.read()
        original_size = len(firmware_data)
        
        # Step 2: Calculate SHA-256 hash
        sha256_hash = hashlib.sha256(firmware_data).hexdigest()
        # Result: 0006dec671e04be987ae43fb13bb58c42850223af089359c63dbea515b0f9289
        
        # Step 3: Sign hash with RSA private key
        hash_bytes = bytes.fromhex(sha256_hash)
        signature = self.private_key.sign(
            hash_bytes,
            padding.PKCS1v15(),
            Prehashed(hashes.SHA256())  # CRITICAL: Use Prehashed()!
        )
        signature_b64 = base64.b64encode(signature).decode('utf-8')
        
        # Step 4: Encrypt firmware with AES-256-CBC
        iv = os.urandom(16)  # Random IV
        
        # Pad to 16-byte blocks
        padder = crypto_padding.PKCS7(128).padder()
        padded_data = padder.update(firmware_data) + padder.finalize()
        
        # Encrypt
        cipher = Cipher(algorithms.AES(self.aes_key), modes.CBC(iv))
        encryptor = cipher.encryptor()
        encrypted_data = encryptor.update(padded_data) + encryptor.finalize()
        
        # Step 5: Split into chunks
        chunk_size = 1024
        total_chunks = (len(encrypted_data) + chunk_size - 1) // chunk_size
        
        # Step 6: Save encrypted firmware
        with open(f"firmware_{version}_encrypted.bin", 'wb') as f:
            f.write(encrypted_data)
        
        # Step 7: Create manifest
        manifest = {
            "version": version,
            "original_size": original_size,
            "encrypted_size": len(encrypted_data),
            "sha256_hash": sha256_hash,
            "signature": signature_b64,
            "iv": base64.b64encode(iv).decode('utf-8'),
            "chunk_size": chunk_size,
            "total_chunks": total_chunks
        }
        
        # Save manifest
        with open(f"firmware_{version}_manifest.json", 'w') as f:
            json.dump(manifest, f, indent=2)
        
        return manifest
```

**Command to prepare firmware**:
```bash
cd flask
python3 prepare_firmware.py
```

### 5.2 Firmware Distribution (Server Side)

```python
# Location: flask/flask_server_hivemq.py

@app.route('/ota/manifest', methods=['GET'])
def get_manifest():
    """
    Return firmware manifest for requested version
    """
    version = request.args.get('version')
    manifest = firmware_manager.get_manifest(version)
    return jsonify(manifest)

@app.route('/ota/chunk', methods=['POST'])
def get_chunk():
    """
    Return encrypted firmware chunk with HMAC
    """
    data = request.json
    chunk_number = data['chunk_number']
    version = data['version']
    
    # Read chunk from encrypted firmware
    chunk_data = firmware_manager.get_chunk(version, chunk_number)
    
    # Calculate HMAC for this chunk
    hmac_value = hmac.new(
        HMAC_PSK.encode(),
        chunk_data + str(chunk_number).encode(),
        hashlib.sha256
    ).hexdigest()
    
    return jsonify({
        "chunk": {
            "chunk_number": chunk_number,
            "data": base64.b64encode(chunk_data).decode('utf-8'),
            "hmac": hmac_value,
            "size": len(chunk_data),
            "total_chunks": manifest['total_chunks']
        }
    })
```

**Start server**:
```bash
cd flask
source venv/bin/activate
python3 flask_server_hivemq.py
```

### 5.3 Firmware Update (ESP32 Side)

```cpp
// Location: PIO/ECOWATT/src/application/OTAManager.cpp

bool OTAManager::downloadAndApplyFirmware() {
    // Step 1: Request manifest
    if (!requestManifest()) {
        return false;
    }
    
    // Step 2: Initialize AES decryption context
    mbedtls_aes_init(&aes_ctx);
    mbedtls_aes_setkey_dec(&aes_ctx, AES_FIRMWARE_KEY, 256);
    memcpy(aes_iv, manifest.iv, 16);  // Copy IV from manifest
    
    // Step 3: Begin OTA update
    if (!Update.begin(manifest.original_size)) {
        Serial.printf("ERROR: Update.begin() failed\n");
        return false;
    }
    
    // Step 4: Download and decrypt all chunks
    for (uint16_t chunk = 0; chunk < manifest.total_chunks; chunk++) {
        // Download chunk from server
        if (!downloadChunk(chunk)) {
            return false;
        }
        
        // Verify HMAC
        if (!verifyChunkHMAC(encryptedChunk, chunkSize, chunk, expectedHMAC)) {
            Serial.println("HMAC verification failed!");
            return false;
        }
        
        // Decrypt chunk (streaming AES-CBC)
        uint8_t decrypted[1024];
        size_t decLen;
        if (!decryptChunk(encryptedChunk, chunkSize, decrypted, &decLen, chunk)) {
            return false;
        }
        
        // Write to OTA partition
        if (Update.write(decrypted, decLen) != decLen) {
            Serial.println("Write failed!");
            return false;
        }
        
        progress.bytes_downloaded += decLen;
    }
    
    // Step 5: Finalize OTA
    if (!Update.end()) {
        Serial.printf("Update.end() failed: %s\n", Update.errorString());
        return false;
    }
    
    // Step 6: Verify signature
    if (!verifySignature(manifest.signature)) {
        Serial.println("Signature verification failed!");
        // Rollback will happen automatically on reboot
        return false;
    }
    
    // Step 7: Reboot
    Serial.println("=== OTA UPDATE SUCCESSFUL ===");
    Serial.printf("Version: %s → %s\n", currentVersion.c_str(), manifest.version.c_str());
    delay(2000);
    ESP.restart();
    
    return true;
}
```

### 5.4 Critical Implementation Details

#### Streaming AES-CBC Decryption
```cpp
bool OTAManager::decryptChunk(const uint8_t* encrypted, size_t encLen, 
                              uint8_t* decrypted, size_t* decLen, 
                              uint16_t chunkNumber) {
    // CRITICAL: Don't reset IV! It's updated by mbedtls_aes_crypt_cbc()
    // The IV state is maintained across chunks for proper CBC operation
    
    uint8_t temp_iv[16];
    memcpy(temp_iv, aes_iv, 16);  // Use current IV state
    
    // Decrypt (modifies temp_iv to become next IV)
    int result = mbedtls_aes_crypt_cbc(
        &aes_ctx,
        MBEDTLS_AES_DECRYPT,
        encLen,
        temp_iv,  // Will be updated to last ciphertext block
        encrypted,
        decrypted
    );
    
    if (result != 0) {
        return false;
    }
    
    // Update IV for next chunk
    memcpy(aes_iv, temp_iv, 16);
    
    *decLen = encLen;
    
    // Remove PKCS7 padding ONLY on last chunk
    if (chunkNumber == (manifest.total_chunks - 1)) {
        uint8_t paddingLen = decrypted[encLen - 1];
        
        // Validate padding
        if (paddingLen > 0 && paddingLen <= 16 && paddingLen <= encLen) {
            bool validPadding = true;
            for (int i = encLen - paddingLen; i < encLen; i++) {
                if (decrypted[i] != paddingLen) {
                    validPadding = false;
                    break;
                }
            }
            
            if (validPadding) {
                *decLen = encLen - paddingLen;
            }
        }
    }
    
    return true;
}
```

#### RSA Signature Verification
```cpp
bool OTAManager::verifyRSASignature(const uint8_t* hash, const uint8_t* signature) {
    // Parse public key
    mbedtls_pk_context pk;
    mbedtls_pk_init(&pk);
    
    int result = mbedtls_pk_parse_public_key(
        &pk, 
        (const uint8_t*)SERVER_PUBLIC_KEY, 
        strlen(SERVER_PUBLIC_KEY) + 1
    );
    
    if (result != 0) {
        Serial.printf("ERROR: Failed to parse public key: %d\n", result);
        mbedtls_pk_free(&pk);
        return false;
    }
    
    // Verify signature
    // mbedtls_pk_verify automatically:
    // 1. Decrypts signature with public key
    // 2. Extracts PKCS#1 v1.5 padding
    // 3. Extracts DigestInfo (ASN.1 + hash algorithm ID)
    // 4. Compares extracted hash with provided hash
    result = mbedtls_pk_verify(
        &pk,
        MBEDTLS_MD_SHA256,  // Hash algorithm
        hash,               // Expected hash (32 bytes)
        32,                 // Hash length
        signature,          // Signature to verify (256 bytes)
        256                 // Signature length
    );
    
    mbedtls_pk_free(&pk);
    
    if (result != 0) {
        Serial.printf("ERROR: RSA signature verification failed: %d\n", result);
        return false;
    }
    
    Serial.println("✓ RSA signature verification successful");
    return true;
}
```

---

## 6. Code Implementation

### 6.1 Key Files and Their Purpose

#### Server Side
```
flask/
├── firmware_manager.py      # Core firmware preparation logic
├── flask_server_hivemq.py   # REST API for OTA
├── prepare_firmware.py      # CLI tool to prepare firmware
├── keys/
│   ├── server_private_key.pem   # RSA private key (SECRET!)
│   ├── server_public_key.pem    # RSA public key
│   ├── aes_firmware_key.bin     # AES-256 key (SECRET!)
│   └── keys.h                   # C header with keys
└── firmware/
    ├── firmware_v1.0.4.bin          # Original firmware
    ├── firmware_1.0.4_encrypted.bin # Encrypted firmware
    └── firmware_1.0.4_manifest.json # Manifest
```

#### ESP32 Side
```
PIO/ECOWATT/
├── include/application/
│   ├── OTAManager.h         # OTA manager interface
│   └── keys.h               # Key declarations (extern)
├── src/application/
│   ├── OTAManager.cpp       # OTA implementation
│   └── keys.cpp             # Key definitions
└── src/main.cpp             # Main application
```

### 6.2 REST API Endpoints

| Endpoint | Method | Purpose | Request | Response |
|----------|--------|---------|---------|----------|
| `/ota/manifest` | GET | Get firmware metadata | `?version=1.0.4` | Manifest JSON |
| `/ota/chunk` | POST | Get encrypted chunk | `{version, chunk_number}` | Chunk data + HMAC |
| `/health` | GET | Server health check | - | `{"status": "ok"}` |

### 6.3 Data Structures

#### Manifest Structure (JSON)
```json
{
  "version": "1.0.4",
  "original_size": 1013920,
  "encrypted_size": 1013936,
  "sha256_hash": "0006dec671e04be987ae43fb13bb58c42850223af089359c63dbea515b0f9289",
  "signature": "bi9vuc6xMwKetZyLMa6bcBUJo2OXQVIOboTnkXB/zp4aP...",
  "iv": "k3F2d9s7fK1mP4qR6tU8vY2z...",
  "chunk_size": 1024,
  "total_chunks": 991,
  "timestamp": "2025-10-15T10:30:00"
}
```

#### OTAManager State Machine
```cpp
enum OTAState {
    OTA_IDLE,           // No update in progress
    OTA_CHECKING,       // Checking for updates
    OTA_DOWNLOADING,    // Downloading chunks
    OTA_VERIFYING,      // Verifying signature
    OTA_COMPLETED,      // Update successful
    OTA_ERROR           // Error occurred
};
```

---

## 7. Testing Guide

### 7.1 Prerequisites

#### Hardware
- ESP32 development board
- USB cable for programming/serial monitor
- WiFi network

#### Software
```bash
# Python environment
cd flask
python3 -m venv venv
source venv/bin/activate
pip install flask cryptography

# PlatformIO
# Install VS Code + PlatformIO extension
```

### 7.2 Step-by-Step Testing Procedure

#### Test 1: Key Generation
```bash
cd flask/keys

# 1. Generate RSA key pair (if not exists)
openssl genrsa -out server_private_key.pem 2048
openssl rsa -in server_private_key.pem -pubout -out server_public_key.pem

# 2. Generate AES key
openssl rand -out aes_firmware_key.bin 32

# 3. Generate HMAC PSK
openssl rand -hex 32 > hmac_psk.txt

# 4. Verify keys
ls -lh
# Should see:
# - server_private_key.pem (1.7KB)
# - server_public_key.pem (451 bytes)
# - aes_firmware_key.bin (32 bytes)
# - hmac_psk.txt (64 hex characters)

# 5. Extract public key and create keys.h
cat > keys.h << 'EOF'
#ifndef OTA_KEYS_H
#define OTA_KEYS_H

extern const char* SERVER_PUBLIC_KEY;
extern const uint8_t AES_FIRMWARE_KEY[32];
extern const char* HMAC_PSK;

#define RSA_KEY_SIZE 2048
#define AES_KEY_SIZE 256
#define HMAC_PSK_SIZE 64

#endif
EOF

# 6. Copy public key to ESP32 project
cp server_public_key.pem ../../PIO/ECOWATT/include/application/
```

**Expected Result**: ✅ All keys generated and in place

#### Test 2: Firmware Preparation
```bash
cd flask

# 1. Build ESP32 firmware
cd ../PIO/ECOWATT
pio run

# 2. Copy firmware binary
cp .pio/build/esp32dev/firmware.bin ../../flask/firmware/firmware_v1.0.4.bin

# 3. Prepare firmware for OTA
cd ../../flask
python3 prepare_firmware.py

# Expected output:
# ============================================================
# Preparing firmware: firmware_v1.0.4.bin
# Version: 1.0.4
# ============================================================
# 
# 📖 Reading firmware binary...
# 📏 Original size: 1,013,920 bytes
# 🔍 Calculating SHA-256 hash...
# ✅ Hash: 0006dec671e04be9...
# ✍️  Signing with RSA-2048...
# 🔐 Signing hash: 0006dec671e04be987ae43fb13bb58c42850223af089359c63dbea515b0f9289
# ✅ Signature generated (256 bytes)
# 🔐 Encrypting with AES-256-CBC...
# ✅ Encrypted size: 1,013,936 bytes
# 📦 Chunking: 991 chunks of 1024 bytes
# ✅ Firmware preparation complete!

# 4. Verify files created
ls -lh firmware/
# Should see:
# - firmware_v1.0.4.bin (original)
# - firmware_1.0.4_encrypted.bin (encrypted)
# - firmware_1.0.4_manifest.json (metadata)
```

**Expected Result**: ✅ Firmware encrypted and manifest created

#### Test 3: Server Startup
```bash
cd flask
source venv/bin/activate
python3 flask_server_hivemq.py

# Expected output:
# ✅ FirmwareManager initialized
#    📁 Firmware directory: /path/to/flask/firmware
#    🔐 Keys loaded
# 
#  * Running on http://0.0.0.0:5001

# Test health endpoint (in another terminal)
curl http://localhost:5001/health
# Expected: {"status": "ok"}

# Test manifest endpoint
curl "http://localhost:5001/ota/manifest?version=1.0.4" | jq
# Expected: Full manifest JSON
```

**Expected Result**: ✅ Server running and responding

#### Test 4: Network Configuration
```bash
# 1. Find your computer's IP address
ifconfig | grep "inet " | grep -v 127.0.0.1

# Example output:
# inet 10.78.228.2 netmask 0xffffff00

# 2. Update ESP32 code with server IP
# Edit: PIO/ECOWATT/src/application/OTAManager.cpp
# Change: serverURL = "http://10.78.228.2:5001"

# 3. Ensure ESP32 and computer on same WiFi network
```

**Expected Result**: ✅ ESP32 can reach server

#### Test 5: ESP32 Firmware Upload (Version 1.0.3)
```bash
cd PIO/ECOWATT

# 1. Update version in code to 1.0.3
# Edit: src/main.cpp
# Change: #define FIRMWARE_VERSION "1.0.3"

# 2. Build and upload
pio run --target upload

# 3. Monitor serial output
pio device monitor

# Expected output:
# Starting ECOWATT
# Connected to WiFi
# Initializing OTA Manager...
# Current Version: 1.0.3
# OTA Manager initialized successfully
```

**Expected Result**: ✅ ESP32 running version 1.0.3

#### Test 6: OTA Update Test
```bash
# 1. Ensure Flask server is running (from Test 3)

# 2. Watch ESP32 serial monitor
pio device monitor

# 3. Wait for automatic OTA check (or trigger manually)
# OTA checks happen every 6 hours by default
# For testing, you can modify the interval in OTAManager.cpp:
# #define OTA_CHECK_INTERVAL_MS (60 * 1000)  // 1 minute

# Expected serial output:
# 
# Checking for OTA updates...
# POST /ota/manifest
# Response (200): {"version":"1.0.4"...}
# New version available: 1.0.4 (current: 1.0.3)
# Starting OTA update...
# 
# Downloading firmware chunks...
# POST /ota/chunk {"chunk_number":0}
# Chunk 0 decrypted successfully (1024 bytes)
# Progress: [0%]  1/ 991 chunks
# 
# ... (downloading chunks) ...
# 
# Progress: [100%]  991/ 991 chunks | 1013920 bytes | 4.0 KB/s
# 
# *** DOWNLOAD COMPLETED ***
# Total time: 246 seconds
# Total bytes written: 1013920
# 
# Starting firmware verification...
# ESP32 calculated hash: 0006dec671e04be9...
# Server expected hash:  0006dec671e04be9...
# ✓ RSA signature verification successful
# ✓ Firmware signature verified
# 
# === OTA UPDATE SUCCESSFUL ===
# Version: 1.0.3 → 1.0.4
# Rebooting to apply update...
# 
# [ESP32 reboots]
# 
# Starting ECOWATT
# Current Version: 1.0.4
# ✅ OTA UPDATE COMPLETE!
```

**Expected Result**: ✅ ESP32 successfully updated to version 1.0.4

#### Test 7: Signature Verification Test
```bash
cd flask

# Test signature verification
python3 << 'EOF'
from cryptography.hazmat.primitives import serialization, hashes
from cryptography.hazmat.primitives.asymmetric import padding
from cryptography.hazmat.primitives.asymmetric.utils import Prehashed
import json
import base64

# Load keys
with open('keys/server_private_key.pem', 'rb') as f:
    private_key = serialization.load_pem_private_key(f.read(), password=None)

public_key = private_key.public_key()

# Load manifest
with open('firmware/firmware_1.0.4_manifest.json', 'r') as f:
    manifest = json.load(f)

hash_bytes = bytes.fromhex(manifest['sha256_hash'])
signature = base64.b64decode(manifest['signature'])

# Verify signature
try:
    public_key.verify(
        signature,
        hash_bytes,
        padding.PKCS1v15(),
        Prehashed(hashes.SHA256())
    )
    print("✅ Signature verification: PASSED")
except Exception as e:
    print(f"❌ Signature verification: FAILED - {e}")
EOF
```

**Expected Result**: ✅ Signature verification: PASSED

#### Test 8: Rollback Test
```bash
# 1. Intentionally corrupt the signature in manifest
cd flask/firmware
cp firmware_1.0.4_manifest.json firmware_1.0.4_manifest.json.backup

# Edit manifest: change one character in signature
sed -i '' 's/bi9vuc6xMw/XX9vuc6xMw/' firmware_1.0.4_manifest.json

# 2. Restart Flask server
# 3. Trigger OTA update on ESP32

# Expected ESP32 output:
# Downloading firmware chunks...
# ... (all chunks download successfully) ...
# Starting firmware verification...
# ERROR: RSA signature verification failed: -17280
# ERROR: Firmware signature verification failed!
# OTA State: ERROR
# [ESP32 stays on version 1.0.3 - rollback successful]

# 4. Restore manifest
mv firmware_1.0.4_manifest.json.backup firmware_1.0.4_manifest.json
```

**Expected Result**: ✅ ESP32 rejects corrupted firmware and stays on old version

#### Test 9: Performance Benchmarking
```bash
# Monitor during OTA update

# Metrics to record:
# - Download speed (KB/s)
# - Total time (seconds)
# - Memory usage (heap free)
# - CPU usage (if available)

# From ESP32 serial output:
# Total time: 246 seconds
# Average speed: 4.0 KB/s
# Free Heap: 235756 bytes

# Calculate:
# Throughput = 1,013,920 bytes / 246 seconds = 4,121 bytes/sec
# Per chunk: 1024 bytes average
# Chunk time: 246 / 991 = 0.248 seconds per chunk
```

**Expected Result**: ✅ Update completes in 4-5 minutes

#### Test 10: Error Recovery Test
```bash
# Test scenario: Network interruption during download

# 1. Start OTA update
# 2. After ~50% downloaded, disconnect WiFi
# 3. Observe ESP32 behavior

# Expected output:
# Chunk 495 decrypted successfully
# POST /ota/chunk {"chunk_number":496}
# HTTP request failed for chunk 496
# ERROR: Failed to download chunk 496
# OTA State: ERROR
# 
# [ESP32 stays on version 1.0.3]

# 4. Reconnect WiFi
# 5. Wait for next OTA check
# 6. Update should retry and succeed
```

**Expected Result**: ✅ ESP32 handles errors gracefully, retries on next check

### 7.3 Debugging Tools

#### Serial Monitor Commands
```bash
# PlatformIO serial monitor
pio device monitor

# With baud rate
pio device monitor --baud 115200

# With filters
pio device monitor --filter direct
```

#### Network Debugging
```bash
# Monitor Flask server logs
# All HTTP requests are logged automatically

# Capture network traffic (optional)
sudo tcpdump -i en0 -w ota_capture.pcap host 10.78.228.2 and port 5001

# Analyze with Wireshark
wireshark ota_capture.pcap
```

#### Signature Debugging Script
```bash
cd flask

cat > debug_signature.py << 'EOF'
#!/usr/bin/env python3
from cryptography.hazmat.primitives import serialization, hashes
from cryptography.hazmat.primitives.asymmetric import padding
import base64
import json

with open('keys/server_private_key.pem', 'rb') as f:
    private_key = serialization.load_pem_private_key(f.read(), password=None)

public_key = private_key.public_key()

with open('firmware/firmware_1.0.4_manifest.json', 'r') as f:
    manifest = json.load(f)

hash_hex = manifest['sha256_hash']
hash_bytes = bytes.fromhex(hash_hex)
signature = base64.b64decode(manifest['signature'])

print(f"Hash: {hash_hex}")
print(f"Signature (first 16): {signature[:16].hex()}")

# Manually decrypt signature to see what's inside
public_numbers = public_key.public_numbers()
sig_int = int.from_bytes(signature, byteorder='big')
decrypted_int = pow(sig_int, public_numbers.e, public_numbers.n)
decrypted_bytes = decrypted_int.to_bytes(256, byteorder='big')

print(f"\nDecrypted signature structure:")
print(f"Header: {decrypted_bytes[:2].hex()}")  # Should be 0001
print(f"Padding ends at: {decrypted_bytes.index(0x00, 2)}")
digest_info_start = decrypted_bytes.index(0x00, 2) + 1
digest_info = decrypted_bytes[digest_info_start:]
print(f"DigestInfo: {digest_info.hex()}")

# Extract hash from DigestInfo
sha256_prefix = bytes.fromhex("3031300d060960864801650304020105000420")
if digest_info.startswith(sha256_prefix):
    extracted_hash = digest_info[len(sha256_prefix):]
    print(f"\nExtracted hash: {extracted_hash.hex()}")
    print(f"Expected hash:  {hash_hex}")
    print(f"Match: {extracted_hash.hex() == hash_hex}")
EOF

chmod +x debug_signature.py
python3 debug_signature.py
```

---

## 8. Troubleshooting

### 8.1 Common Issues and Solutions

#### Issue 1: "ERROR: RSA signature verification failed: -17280"

**Symptoms**:
```
✓ All chunks downloaded
✓ Hash matches
❌ RSA signature verification failed
```

**Causes**:
1. Signature was created by hashing the hash again (missing `Prehashed()`)
2. Public key on ESP32 doesn't match private key on server
3. Signature corruption during transmission

**Solutions**:
```python
# In firmware_manager.py, ensure:
signature = self.private_key.sign(
    hash_bytes,
    padding.PKCS1v15(),
    Prehashed(hashes.SHA256())  # ← MUST have Prehashed()!
)

# Verify keys match:
openssl rsa -in server_private_key.pem -pubout -outform PEM
# Compare with SERVER_PUBLIC_KEY in keys.cpp
```

#### Issue 2: "ERROR: Update.begin() failed"

**Symptoms**:
```
Starting OTA update...
ERROR: Update.begin() failed
```

**Causes**:
1. Not enough space in OTA partition
2. Wrong partition size in `Update.begin()`
3. Partition table doesn't have OTA partitions

**Solutions**:
```cpp
// Use original_size, NOT encrypted_size
Update.begin(manifest.original_size)  // ✅ Correct

// Check partition table
cd PIO/ECOWATT
pio run --target erase
pio run --target upload

// Verify OTA partitions exist
# In ESP32 serial monitor after boot:
const esp_partition_t* partition = esp_ota_get_next_update_partition(NULL);
Serial.printf("OTA partition: %s\n", partition->label);
```

#### Issue 3: "Size mismatch after download"

**Symptoms**:
```
Progress: [100%] 991/991 chunks | 1013936 bytes
Expected: 1013920 bytes
WARNING: Size mismatch
```

**Causes**:
1. PKCS7 padding not removed from last chunk
2. Writing encrypted size instead of decrypted size

**Solutions**:
```cpp
// In decryptChunk(), ensure padding removal on LAST chunk only:
bool isLastChunk = (chunkNumber == (manifest.total_chunks - 1));

if (isLastChunk && encLen > 0) {
    uint8_t paddingLen = decrypted[encLen - 1];
    // Validate and remove padding
    *decLen = encLen - paddingLen;
}
```

#### Issue 4: "HMAC verification failed"

**Symptoms**:
```
Chunk 42 downloaded
ERROR: HMAC mismatch for chunk 42
```

**Causes**:
1. HMAC_PSK different between server and ESP32
2. Chunk data corrupted during transmission
3. Chunk number not included in HMAC calculation

**Solutions**:
```python
# Server side - ensure chunk number included:
hmac_value = hmac.new(
    HMAC_PSK.encode(),
    chunk_data + str(chunk_number).encode(),  # ← Include chunk number
    hashlib.sha256
).hexdigest()

# ESP32 side - must match:
String chunkNumStr = String(chunkNum);
mbedtls_md_hmac_update(&ctx, (const unsigned char*)chunkNumStr.c_str(), 
                       chunkNumStr.length());
```

#### Issue 5: "Decryption produces garbage"

**Symptoms**:
```
Chunk 0 decrypted: ���������...
Hash mismatch after all chunks
```

**Causes**:
1. IV not preserved between chunks (CBC mode requires continuous IV)
2. Wrong AES key
3. Chunks decrypted out of order

**Solutions**:
```cpp
// CRITICAL: Don't reset IV between chunks!
// Store IV as class member: uint8_t aes_iv[16];

// Initialize ONCE from manifest:
memcpy(aes_iv, manifest.iv, 16);

// In decryptChunk():
uint8_t temp_iv[16];
memcpy(temp_iv, aes_iv, 16);  // Copy current state

mbedtls_aes_crypt_cbc(&aes_ctx, MBEDTLS_AES_DECRYPT, encLen, 
                      temp_iv, encrypted, decrypted);

memcpy(aes_iv, temp_iv, 16);  // Update state for next chunk
```

#### Issue 6: "ESP32 crashes during OTA"

**Symptoms**:
```
Chunk 234 decrypted...
Guru Meditation Error: Core 1 panic'ed (LoadProhibited)
```

**Causes**:
1. Out of memory (heap exhausted)
2. Stack overflow
3. Buffer overrun

**Solutions**:
```cpp
// Monitor memory:
Serial.printf("Free Heap: %u bytes\n", ESP.getFreeHeap());

// Reduce buffer sizes:
static const size_t DECRYPT_BUFFER_SIZE = 2048;  // Was 4096

// Use dynamic allocation carefully:
uint8_t* buffer = (uint8_t*)malloc(size);
if (!buffer) {
    Serial.println("Out of memory!");
    return false;
}
// ... use buffer ...
free(buffer);
```

### 8.2 Diagnostic Checklist

Before requesting help, verify:

- [ ] Keys generated correctly (RSA 2048-bit, AES 256-bit)
- [ ] Public key on ESP32 matches private key on server
- [ ] Firmware prepared with `prepare_firmware.py`
- [ ] Encrypted firmware and manifest files exist
- [ ] Flask server running and accessible
- [ ] ESP32 on same network as server
- [ ] Server URL correct in ESP32 code
- [ ] Version numbers correct (old < new)
- [ ] Enough flash space for OTA partition
- [ ] Serial monitor shows detailed logs
- [ ] No firewall blocking port 5001

### 8.3 Debug Logging Levels

```cpp
// In OTAManager.cpp, add verbose logging:

#define OTA_DEBUG_LEVEL 2
// 0 = Errors only
// 1 = + Warnings
// 2 = + Info
// 3 = + Debug (very verbose)

#if OTA_DEBUG_LEVEL >= 3
    Serial.printf("[DEBUG] IV state: ");
    for (int i = 0; i < 16; i++) {
        Serial.printf("%02x", aes_iv[i]);
    }
    Serial.println();
#endif
```

---

## 9. Best Practices

### 9.1 Security Best Practices

1. **Key Management**
   - ✅ Generate keys with sufficient entropy (use `openssl rand`)
   - ✅ Store private keys offline, never commit to git
   - ✅ Use hardware security modules (HSM) for production
   - ✅ Rotate keys periodically (every 1-2 years)
   - ❌ Never hardcode keys in plaintext
   - ❌ Never share private keys

2. **Signature Verification**
   - ✅ Always verify signature AFTER firmware fully downloaded
   - ✅ Use `Prehashed()` when signing already-hashed data
   - ✅ Verify hash algorithm matches (SHA-256)
   - ❌ Don't skip signature verification "for testing"

3. **Version Management**
   - ✅ Use semantic versioning (MAJOR.MINOR.PATCH)
   - ✅ Only allow upgrades (reject downgrades)
   - ✅ Log all OTA attempts (success/failure)
   - ❌ Don't reuse version numbers

4. **Network Security**
   - ✅ Use HTTPS in production (TLS 1.2+)
   - ✅ Validate server certificates
   - ✅ Implement rate limiting on server
   - ❌ Don't expose Flask server directly to internet

### 9.2 Performance Best Practices

1. **Chunk Size Optimization**
   ```
   Smaller chunks (512B):  + Less memory, + Resume easier, - More overhead
   Larger chunks (4KB):    + Faster transfer, - More memory, - Resume harder
   Recommended: 1KB        ✅ Good balance
   ```

2. **Memory Management**
   ```cpp
   // Free memory before OTA
   // Close unnecessary connections
   // Clear large buffers
   
   // Monitor during OTA
   if (ESP.getFreeHeap() < 50000) {
       Serial.println("WARNING: Low memory!");
   }
   ```

3. **Network Optimization**
   ```cpp
   // Enable HTTP keep-alive
   httpClient.setReuse(true);
   
   // Set reasonable timeouts
   httpClient.setTimeout(10000);  // 10 seconds
   
   // Retry failed chunks
   int retries = 3;
   while (retries-- > 0 && !downloadChunk(chunkNum)) {
       delay(1000);
   }
   ```

### 9.3 Production Deployment

#### Pre-Deployment Checklist
- [ ] Test OTA with 10+ devices
- [ ] Test with slow network (3G speeds)
- [ ] Test with intermittent connectivity
- [ ] Test rollback scenarios
- [ ] Implement A/B partition scheme
- [ ] Set up monitoring/alerting
- [ ] Document rollback procedures
- [ ] Train support team

#### Staged Rollout Strategy
```
Phase 1: 1% of devices (canary)
         Wait 24 hours, monitor for issues
         
Phase 2: 10% of devices
         Wait 48 hours, monitor metrics
         
Phase 3: 50% of devices
         Wait 24 hours
         
Phase 4: 100% rollout
```

#### Monitoring Metrics
- Update success rate
- Average download time
- Signature verification failures
- Rollback rate
- Memory usage during OTA
- Network errors

---

## 10. Advanced Topics

### 10.1 Delta Updates
Instead of sending full firmware, send only changed bytes:
```python
# Calculate binary diff
old_firmware = read_firmware("v1.0.3.bin")
new_firmware = read_firmware("v1.0.4.bin")
delta = bsdiff(old_firmware, new_firmware)

# Delta is typically 10-30% of full size
# ESP32 applies patch: old_firmware + delta = new_firmware
```

### 10.2 Compression
Compress firmware before encryption:
```python
import zlib

compressed = zlib.compress(firmware_data, level=9)
# Typical compression: 40-60% size reduction
encrypted = aes_encrypt(compressed)
```

ESP32 decompresses after decryption:
```cpp
// Decompress with miniz or similar
size_t decompressed_size;
mz_uncompress(decompressed, &decompressed_size, compressed, compressed_size);
```

### 10.3 Secure Boot
Ensure only signed firmware can boot:
```
1. Enable secure boot in ESP32 bootloader
2. Sign firmware with secure boot key
3. ESP32 verifies signature during boot
4. Prevents malicious firmware execution
```

### 10.4 Fleet Management
For managing thousands of devices:
- Implement device grouping (by region, type, etc.)
- Staged rollouts with canary deployments
- Rollback entire fleets if issues detected
- Analytics dashboard for update status

---

## 11. Appendix

### A. Glossary

| Term | Definition |
|------|------------|
| **FOTA** | Firmware Over-The-Air: Remote firmware updates |
| **OTA Partition** | Flash memory section for storing new firmware |
| **Manifest** | JSON file with firmware metadata (hash, size, signature) |
| **Chunk** | Small piece of firmware (1KB) transmitted separately |
| **IV** | Initialization Vector: Random data for CBC mode |
| **HMAC** | Hash-based Message Authentication Code |
| **PSK** | Pre-Shared Key: Secret shared between client and server |
| **DigestInfo** | ASN.1 structure identifying hash algorithm |
| **Prehashed** | Indicates data is already hashed (don't hash again) |

### B. Useful Commands

```bash
# Check ESP32 partition table
esptool.py --port /dev/ttyUSB0 read_flash 0x8000 0xC00 partition_table.bin
python3 -m esptool.py partition_table partition_table.bin

# Monitor ESP32 flash usage
pio run --target size

# Erase ESP32 flash completely
pio run --target erase

# Generate test firmware with specific size
dd if=/dev/urandom of=test_firmware.bin bs=1024 count=1024

# Base64 encode/decode
echo "data" | base64
echo "ZGF0YQo=" | base64 -d

# Calculate SHA-256 hash
sha256sum firmware.bin
openssl dgst -sha256 firmware.bin

# Verify RSA key pair
openssl rsa -in server_private_key.pem -pubout | diff - server_public_key.pem
```

### C. References

- ESP32 OTA Documentation: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/ota.html
- mbedtls Documentation: https://mbed-tls.readthedocs.io/
- Python Cryptography: https://cryptography.io/
- PKCS Standards: https://www.rfc-editor.org/rfc/rfc2315
- AES-CBC Mode: NIST SP 800-38A

### D. Version History

| Version | Date | Changes |
|---------|------|---------|
| 1.0.0 | Initial | Basic OTA implementation |
| 1.0.1 | +1 week | Added HMAC chunk authentication |
| 1.0.2 | +2 weeks | Fixed CBC IV handling |
| 1.0.3 | +3 weeks | Added signature verification |
| 1.0.4 | +4 weeks | Fixed Prehashed() bug, production ready |

---

## Conclusion

This FOTA system provides **enterprise-grade security** for ESP32 firmware updates with:
- ✅ Three-layer security (HMAC + AES + RSA)
- ✅ Robust error handling and rollback
- ✅ Chunked delivery for reliability
- ✅ Comprehensive testing procedures
- ✅ Production-ready implementation

**Key Takeaway**: The most critical lesson learned was the importance of using `Prehashed()` when signing already-hashed data with Python's cryptography library. Without it, the library hashes the data again, producing an invalid signature.

---

**Document Version**: 1.0  
**Last Updated**: October 15, 2025  
**Author**: EcoWatt Team PowerPort  
**License**: MIT
