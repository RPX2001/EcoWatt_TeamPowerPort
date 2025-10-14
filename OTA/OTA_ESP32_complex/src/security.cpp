/**
 * @file security.cpp
 * @brief Security utilities implementation for ESP32 OTA system
 */

#include "security.h"
#include "config.h"
#include <mbedtls/md.h>
#include <mbedtls/sha256.h>
#include <esp_system.h>
#include <esp_random.h>

bool calculateSHA256(const uint8_t* data, size_t length, uint8_t* hash) {
    if (!data || !hash || length == 0) {
        return false;
    }
    
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    
    int ret = mbedtls_sha256_starts_ret(&ctx, 0); // 0 for SHA256 (not SHA224)
    if (ret != 0) {
        mbedtls_sha256_free(&ctx);
        return false;
    }
    
    ret = mbedtls_sha256_update_ret(&ctx, data, length);
    if (ret != 0) {
        mbedtls_sha256_free(&ctx);
        return false;
    }
    
    ret = mbedtls_sha256_finish_ret(&ctx, hash);
    mbedtls_sha256_free(&ctx);
    
    return ret == 0;
}

bool calculateSHA256Stream(Stream* stream, size_t length, uint8_t* hash) {
    if (!stream || !hash || length == 0) {
        return false;
    }
    
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    
    int ret = mbedtls_sha256_starts_ret(&ctx, 0);
    if (ret != 0) {
        mbedtls_sha256_free(&ctx);
        return false;
    }
    
    uint8_t buffer[512];
    size_t remaining = length;
    
    while (remaining > 0) {
        size_t toRead = min(remaining, sizeof(buffer));
        size_t bytesRead = stream->readBytes(buffer, toRead);
        
        if (bytesRead == 0) {
            mbedtls_sha256_free(&ctx);
            return false; // Stream ended prematurely
        }
        
        ret = mbedtls_sha256_update_ret(&ctx, buffer, bytesRead);
        if (ret != 0) {
            mbedtls_sha256_free(&ctx);
            return false;
        }
        
        remaining -= bytesRead;
    }
    
    ret = mbedtls_sha256_finish_ret(&ctx, hash);
    mbedtls_sha256_free(&ctx);
    
    return ret == 0;
}

bool verifyHMAC(const uint8_t* data, size_t dataLength, 
               const uint8_t* key, size_t keyLength, 
               const uint8_t* expectedHmac) {
    if (!data || !key || !expectedHmac || dataLength == 0 || keyLength == 0) {
        return false;
    }
    
    uint8_t calculatedHmac[32];
    const mbedtls_md_info_t* md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    
    if (!md_info) {
        return false;
    }
    
    int ret = mbedtls_md_hmac(md_info, key, keyLength, data, dataLength, calculatedHmac);
    if (ret != 0) {
        return false;
    }
    
    // Use constant-time comparison to prevent timing attacks
    return secureMemcmp(calculatedHmac, expectedHmac, 32) == 0;
}

int hexStringToBytes(const char* hexString, uint8_t* bytes, size_t maxBytes) {
    if (!hexString || !bytes || maxBytes == 0) {
        return -1;
    }
    
    size_t hexLen = strlen(hexString);
    if (hexLen % 2 != 0) {
        return -1; // Hex string must have even length
    }
    
    size_t numBytes = hexLen / 2;
    if (numBytes > maxBytes) {
        return -1; // Not enough space in output buffer
    }
    
    for (size_t i = 0; i < numBytes; i++) {
        char hexByte[3] = {hexString[i*2], hexString[i*2+1], '\0'};
        char* endPtr;
        long value = strtol(hexByte, &endPtr, 16);
        
        if (*endPtr != '\0' || value < 0 || value > 255) {
            return -1; // Invalid hex character
        }
        
        bytes[i] = (uint8_t)value;
    }
    
    return numBytes;
}

void bytesToHexString(const uint8_t* bytes, size_t length, char* hexString) {
    if (!bytes || !hexString || length == 0) {
        return;
    }
    
    for (size_t i = 0; i < length; i++) {
        sprintf(&hexString[i*2], "%02x", bytes[i]);
    }
    hexString[length * 2] = '\0';
}

bool setupSecureClient(WiFiClientSecure& client, const char* rootCACert) {
    if (!rootCACert) {
        return false;
    }
    
    // Set root CA certificate for server verification
    client.setCACert(rootCACert);
    
    // Enable certificate verification
    client.setInsecure(false);
    
    // Set timeout for secure connections
    client.setTimeout(HTTP_TIMEOUT_MS / 1000);
    
    return true;
}

void generateDeviceID(char* deviceId, size_t maxLength) {
    if (!deviceId || maxLength < 20) {
        return;
    }
    
    // Get ESP32 chip ID for unique identifier
    uint64_t chipId = ESP.getEfuseMac();
    
    snprintf(deviceId, maxLength, "%s-%04X%08X", 
             DEVICE_ID_PREFIX, 
             (uint16_t)(chipId >> 32), 
             (uint32_t)chipId);
}

int secureMemcmp(const void* a, const void* b, size_t length) {
    if (!a || !b) {
        return -1;
    }
    
    const uint8_t* pa = (const uint8_t*)a;
    const uint8_t* pb = (const uint8_t*)b;
    uint8_t result = 0;
    
    // Constant-time comparison to prevent timing attacks
    for (size_t i = 0; i < length; i++) {
        result |= pa[i] ^ pb[i];
    }
    
    return result;
}

void secureMemclear(void* ptr, size_t length) {
    if (!ptr || length == 0) {
        return;
    }
    
    volatile uint8_t* vptr = (volatile uint8_t*)ptr;
    for (size_t i = 0; i < length; i++) {
        vptr[i] = 0;
    }
}

// Root CA certificate for HTTPS validation (self-signed for development)
// In production, replace with proper CA certificate or use certificate pinning
const char* root_ca_cert = R"(-----BEGIN CERTIFICATE-----
MIIDXTCCAkWgAwIBAgIJAKL0UG+mRkSPMA0GCSqGSIb3DQEBCwUAMEUxCzAJBgNV
BAYTAkFVMRMwEQYDVQQIDApTb21lLVN0YXRlMSEwHwYDVQQKDBhJbnRlcm5ldCBX
aWRnaXRzIFB0eSBMdGQwHhcNMjMxMjAxMDAwMDAwWhcNMjQxMjAxMDAwMDAwWjBF
MQswCQYDVQQGEwJBVTETMBEGA1UECAwKU29tZS1TdGF0ZTEhMB8GA1UECgwYSW50
ZXJuZXQgV2lkZ2l0cyBQdHkgTHRkMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIB
CgKCAQEAyKp7QfCZlRyBf6TWSJowzl8KYJ2nP6nQ8WnQKdCNGQlsNqspzbzeyI9T
wPQ8vXjV8ZrLpZSRtNmImyGqko2ATgqerFfEARgyT8E4rf3QP7d9VmhLdqVQ7qBq
1Y2Y3aHlpYoUmS9LU8uqr6KQ5snjqOCQdynNvBdyTqJ1TmBzqGWqjTetJE3L5D+R
nSJqaYikcUFejWHOFgYzE8BYNJOzCJpJeTK7l1Y4m9o+xJQrmXcYizl8/7Nk6hQP
KaCKqFcNjwDUhzRl1m7gKuqcC6Xd9xvb8Y6pxns2nqB2QnYm2Wbdx2d2c3v+2QU+
dLNJklA4YG7xoSqjqcvd4j8V1ZZWmwIDAQABo1AwTjAdBgNVHQ4EFgQUQn2WBd+Z
+++Kqjl+bTljnpN7gaEwHwYDVHSMEGDAWgBRCfZYF35nP765qKX5tOWOek3uBoTA
fBgNVHSMEGDAWgBRCfZYF35n47656qKX5tOWOek3uBoTAMBQGA1UdEQQNMAuCCWxv
Y2FsaG9zdDANBgkqhkiG9w0BAQsFAAOCAQEAkOcF1jvRXVV/8qYzCsGxzg5z+9G+
a4LYz0x+GQ5xO+FvYn7nQ2YNm1mVn3gZvPpPn4hRHmOXvV1Yz+cL8K+Xw7dYjGOx
NQY6qXGEiP5QKXp1h+V0xL4vlZX7DjJQ+P2Lk7D8wPw4DlJPHhL7nFV0SJz5HzFO
jYG4YvU+X6mJ2G1xY5YvC2U+uF1YvQXj7YnV4L2Q1R5WgKUZ3F8JaXzVpVlPFJ4e
bPvKpF0+H6xRZwJwE4Z0t5dGK2uV+JDw8jJwJ1X3vF2v9+oGJv4Ov4bRQ8j1dXnN
j0FdT8cQSQQGSF2QhQQ4Xz6Kz9UMBT+XvJlT9k+bv1yT9F4jY+jWYQbqOg==
-----END CERTIFICATE-----)";