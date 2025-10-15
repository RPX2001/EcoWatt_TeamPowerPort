# FOTA Quick Reference Guide

## 🚀 Quick Start (5 Minutes)

### Prerequisites
```bash
# Install Python dependencies
cd flask
python3 -m venv venv
source venv/bin/activate
pip install flask cryptography

# Install PlatformIO (if not already)
# VS Code → Extensions → PlatformIO IDE
```

### Generate Keys (One-Time Setup)
```bash
cd flask/keys

# 1. RSA key pair
openssl genrsa -out server_private_key.pem 2048
openssl rsa -in server_private_key.pem -pubout -out server_public_key.pem

# 2. AES key
openssl rand -out aes_firmware_key.bin 32

# 3. HMAC PSK
openssl rand -hex 32
# Copy output to keys.h as HMAC_PSK
```

### Prepare Firmware
```bash
cd flask

# 1. Build ESP32 firmware
cd ../PIO/ECOWATT
pio run

# 2. Copy binary
cp .pio/build/esp32dev/firmware.bin ../../flask/firmware/firmware_v1.0.4.bin

# 3. Encrypt & sign
cd ../../flask
python3 prepare_firmware.py
```

### Start Server
```bash
cd flask
source venv/bin/activate
python3 flask_server_hivemq.py
```

### Upload to ESP32
```bash
cd PIO/ECOWATT

# Ensure version is older (e.g., 1.0.3)
# Edit src/main.cpp: #define FIRMWARE_VERSION "1.0.3"

pio run --target upload
pio device monitor
```

### Trigger Update
Wait for automatic check (6 hours) or modify interval:
```cpp
// In OTAManager.cpp
#define OTA_CHECK_INTERVAL_MS (60 * 1000)  // 1 minute for testing
```

---

## 📋 Common Commands

### Server Side
```bash
# Prepare firmware
python3 prepare_firmware.py

# Start server
python3 flask_server_hivemq.py

# Test endpoints
curl http://localhost:5001/health
curl "http://localhost:5001/ota/manifest?version=1.0.4"

# Verify signature
python3 verify_signature.py
```

### ESP32 Side
```bash
# Build
pio run

# Upload
pio run --target upload

# Monitor
pio device monitor

# Clean build
pio run --target clean

# Erase flash
pio run --target erase
```

---

## 🔍 Debugging Checklist

When OTA fails, check in order:

1. **Network Connectivity**
   ```bash
   # Ping server from ESP32's network
   ping 10.78.228.2
   ```

2. **Server Running**
   ```bash
   curl http://10.78.228.2:5001/health
   ```

3. **Firmware Files Exist**
   ```bash
   ls -lh flask/firmware/
   # Should see: firmware_1.0.4_encrypted.bin and manifest.json
   ```

4. **Keys Match**
   ```bash
   # Extract public from private
   openssl rsa -in flask/keys/server_private_key.pem -pubout
   
   # Compare with ESP32's SERVER_PUBLIC_KEY in keys.cpp
   ```

5. **Signature Valid**
   ```bash
   cd flask
   python3 verify_signature.py
   # Should print: ✅ Signature verification: PASSED
   ```

6. **ESP32 Logs**
   ```bash
   pio device monitor
   # Look for error messages
   ```

---

## 🐛 Common Errors & Fixes

### Error: "RSA signature verification failed: -17280"
```python
# In firmware_manager.py, ensure Prehashed() is used:
signature = self.private_key.sign(
    hash_bytes,
    padding.PKCS1v15(),
    Prehashed(hashes.SHA256())  # ← MUST be here!
)
```

### Error: "Update.begin() failed"
```cpp
// Use original_size, NOT encrypted_size:
Update.begin(manifest.original_size)  // ✅
Update.begin(manifest.encrypted_size) // ❌
```

### Error: "Size mismatch"
```cpp
// Remove padding ONLY on last chunk:
bool isLastChunk = (chunkNumber == (manifest.total_chunks - 1));
if (isLastChunk) {
    // Remove PKCS7 padding
}
```

### Error: "HMAC verification failed"
```python
# Server must include chunk number:
hmac_value = hmac.new(
    HMAC_PSK.encode(),
    chunk_data + str(chunk_number).encode(),  # ← Include this
    hashlib.sha256
).hexdigest()
```

### Error: "Decryption produces garbage"
```cpp
// Don't reset IV between chunks!
// Store as class member: uint8_t aes_iv[16];
// Initialize once from manifest
// Let mbedtls_aes_crypt_cbc update it
```

---

## 📊 Expected Output

### Successful OTA Update
```
Checking for OTA updates...
New version available: 1.0.4 (current: 1.0.3)
Starting OTA update...

Downloading firmware chunks...
Progress: [100%]  991/ 991 chunks | 1013920 bytes | 4.0 KB/s

*** DOWNLOAD COMPLETED ***
Total time: 246 seconds

Starting firmware verification...
ESP32 calculated hash: 0006dec671e04be9...
Server expected hash:  0006dec671e04be9...
✓ RSA signature verification successful
✓ Firmware signature verified

=== OTA UPDATE SUCCESSFUL ===
Version: 1.0.3 → 1.0.4
Rebooting to apply update...

[Reboot]

Starting ECOWATT
Current Version: 1.0.4
```

---

## 🔐 Security Layers

| Layer | Technology | Purpose | Protects Against |
|-------|-----------|---------|------------------|
| 1 | HMAC-SHA256 | Chunk authentication | Tampering, Replay |
| 2 | AES-256-CBC | Firmware encryption | Eavesdropping, Theft |
| 3 | RSA-2048 | Digital signature | Forgery, Modification |

---

## 📁 File Structure

```
Project/
├── flask/
│   ├── firmware_manager.py          # Core logic
│   ├── flask_server_hivemq.py       # REST API
│   ├── prepare_firmware.py          # CLI tool
│   ├── verify_signature.py          # Test script
│   ├── keys/
│   │   ├── server_private_key.pem   # SECRET!
│   │   ├── server_public_key.pem
│   │   ├── aes_firmware_key.bin     # SECRET!
│   │   └── keys.h
│   └── firmware/
│       ├── firmware_v1.0.4.bin
│       ├── firmware_1.0.4_encrypted.bin
│       └── firmware_1.0.4_manifest.json
├── PIO/ECOWATT/
│   ├── include/application/
│   │   ├── OTAManager.h
│   │   └── keys.h
│   ├── src/application/
│   │   ├── OTAManager.cpp
│   │   └── keys.cpp
│   └── src/main.cpp
└── FOTA_DOCUMENTATION.md            # Full docs
```

---

## 🔢 Key Metrics

| Metric | Value |
|--------|-------|
| Firmware Size | ~1 MB |
| Chunk Size | 1 KB |
| Total Chunks | ~991 |
| Download Time | 4-5 minutes |
| Download Speed | 3-4 KB/s |
| Memory Usage | ~240 KB free heap |
| Security Level | Military-grade (AES-256 + RSA-2048) |

---

## 🎯 Testing Scenarios

### Scenario 1: Normal Update
1. ESP32 on v1.0.3
2. Server has v1.0.4
3. OTA check triggers
4. ✅ Update succeeds
5. ESP32 reboots to v1.0.4

### Scenario 2: Corrupted Signature
1. Modify manifest signature
2. OTA downloads all chunks
3. Signature verification fails
4. ✅ ESP32 stays on old version

### Scenario 3: Network Interruption
1. Start OTA update
2. Disconnect WiFi at 50%
3. Download fails
4. ✅ ESP32 stays on old version
5. Reconnect WiFi
6. Next check retries and succeeds

### Scenario 4: Downgrade Attempt
1. ESP32 on v1.0.4
2. Server has v1.0.3
3. OTA check compares versions
4. ✅ Update rejected (downgrade)

---

## 💡 Pro Tips

1. **Testing**: Set OTA check interval to 1 minute during development
2. **Logging**: Enable verbose logging for debugging
3. **Memory**: Monitor heap usage during OTA
4. **Backups**: Always backup working firmware before OTA
5. **Versioning**: Use semantic versioning (MAJOR.MINOR.PATCH)
6. **Rollback**: Test rollback scenarios extensively
7. **Keys**: Never commit private keys to git
8. **Production**: Use HTTPS in production

---

## 📞 Quick Help

### Issue: Can't connect to server
- Check IP address: `ifconfig | grep inet`
- Verify firewall: `sudo ufw status`
- Test with curl: `curl http://IP:5001/health`

### Issue: Signature fails
- Run: `python3 verify_signature.py`
- Check for `Prehashed()` in code
- Verify keys match

### Issue: Out of memory
- Reduce chunk size to 512 bytes
- Close unused connections
- Clear large buffers before OTA

### Issue: Update too slow
- Increase chunk size to 2048 bytes
- Enable HTTP keep-alive
- Check network quality

---

## 📚 Resources

- Full Documentation: `FOTA_DOCUMENTATION.md`
- ESP32 OTA Guide: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/ota.html
- mbedtls API: https://mbed-tls.readthedocs.io/
- Python Cryptography: https://cryptography.io/

---

**Last Updated**: October 15, 2025  
**Version**: 1.0
