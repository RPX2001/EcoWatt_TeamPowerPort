/**
 * @file security.cpp
 * @brief Security Layer Implementation for EcoWatt Data Upload
 */

#include "application/security.h"

// Temporarily undefine print macro to avoid conflict with ArduinoJson
#ifdef print
#define PRINT_MACRO_BACKUP print
#undef print
#endif

#include <ArduinoJson.h>
#include <base64.h>
#include <mbedtls/md.h>
#include <mbedtls/aes.h>

// Restore print macro
#ifdef PRINT_MACRO_BACKUP
#define print PRINT_MACRO_BACKUP
#undef PRINT_MACRO_BACKUP
#endif

#include "peripheral/print.h"

// Initialize static members
uint32_t SecurityLayer::currentNonce = 10000;
Preferences SecurityLayer::preferences;

// Pre-Shared Keys - MUST match on server side!
const uint8_t SecurityLayer::PSK_HMAC[32] = {
    0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
    0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c,
    0x76, 0x2e, 0x71, 0x60, 0xf3, 0x8b, 0x4d, 0xa5,
    0x6a, 0x78, 0x4d, 0x90, 0x45, 0x19, 0x0c, 0xfe
};

const uint8_t SecurityLayer::PSK_AES[16] = {
    0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
    0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c
};

const uint8_t SecurityLayer::AES_IV[16] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f
};

void SecurityLayer::init() {
    print("Security Layer: Initializing...\n");
    loadNonce();
    print("Security Layer: Initialized with nonce = %u\n", currentNonce);
}

void SecurityLayer::loadNonce() {
    preferences.begin("security", false);
    currentNonce = preferences.getUInt("nonce", 10000);
    preferences.end();
}

uint32_t SecurityLayer::incrementAndSaveNonce() {
    currentNonce++;
    saveNonce(currentNonce);
    return currentNonce;
}

void SecurityLayer::saveNonce(uint32_t nonce) {
    preferences.begin("security", false);
    preferences.putUInt("nonce", nonce);
    preferences.end();
}

uint32_t SecurityLayer::getCurrentNonce() {
    return currentNonce;
}

void SecurityLayer::setNonce(uint32_t nonce) {
    currentNonce = nonce;
    saveNonce(nonce);
}

bool SecurityLayer::securePayload(const char* payload, char* securedPayload, size_t securedPayloadSize) {
    if (!payload || !securedPayload || securedPayloadSize == 0) {
        print("Security Layer: Invalid parameters\n");
        return false;
    }
    
    // Step 1: Increment nonce
    uint32_t nonce = incrementAndSaveNonce();
    
    // Step 2: Encode payload (Base64 for mock encryption, or AES + Base64 if enabled)
    size_t payloadLen = strlen(payload);
    String encodedPayload;
    
    if (ENABLE_ENCRYPTION) {
        // Real AES encryption (not implemented in mock mode)
        print("Security Layer: AES encryption not implemented in mock mode\n");
        return false;
    } else {
        // Mock encryption: Just Base64 encode
        encodedPayload = base64::encode((const uint8_t*)payload, payloadLen);
    }
    
    // Step 3: Calculate HMAC over (nonce + original payload)
    // Prepare data: nonce (4 bytes, big-endian) + payload
    size_t dataLen = 4 + payloadLen;
    uint8_t* dataToSign = new uint8_t[dataLen];
    
    // Convert nonce to big-endian
    dataToSign[0] = (nonce >> 24) & 0xFF;
    dataToSign[1] = (nonce >> 16) & 0xFF;
    dataToSign[2] = (nonce >> 8) & 0xFF;
    dataToSign[3] = nonce & 0xFF;
    
    // Append payload
    memcpy(dataToSign + 4, payload, payloadLen);
    
    // Calculate HMAC
    uint8_t hmac[32];
    calculateHMAC(dataToSign, dataLen, hmac);
    
    // Convert HMAC to hex string
    char hmacHex[65];
    bytesToHex(hmac, 32, hmacHex, sizeof(hmacHex));
    
    // Clean up
    delete[] dataToSign;
    
    // Step 4: Build secured JSON using dynamic allocation to avoid stack overflow
    DynamicJsonDocument* doc = new DynamicJsonDocument(8192);
    if (!doc) {
        print("Security Layer: Failed to allocate JSON document\n");
        return false;
    }
    
    (*doc)["nonce"] = nonce;
    (*doc)["payload"] = encodedPayload;
    (*doc)["mac"] = hmacHex;
    (*doc)["encrypted"] = ENABLE_ENCRYPTION;
    
    size_t jsonLen = serializeJson(*doc, securedPayload, securedPayloadSize);
    
    // Clean up document
    delete doc;
    
    if (jsonLen == 0) {
        print("Security Layer: Failed to serialize JSON\n");
        return false;
    }
    
    if (jsonLen >= securedPayloadSize - 1) {
        print("Security Layer: Buffer too small for secured payload (need %zu, have %zu)\n", 
              jsonLen + 1, securedPayloadSize);
        return false;
    }
    
    // Ensure null termination
    securedPayload[jsonLen] = '\0';
    
    print("Security Layer: Payload secured with nonce %u (size: %zu bytes)\n", nonce, jsonLen);
    return true;
}

void SecurityLayer::calculateHMAC(const uint8_t* data, size_t dataLen, uint8_t* hmac) {
    mbedtls_md_context_t ctx;
    mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
    
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 1); // 1 = HMAC mode
    mbedtls_md_hmac_starts(&ctx, PSK_HMAC, 32);
    mbedtls_md_hmac_update(&ctx, data, dataLen);
    mbedtls_md_hmac_finish(&ctx, hmac);
    mbedtls_md_free(&ctx);
}

bool SecurityLayer::encryptAES(const uint8_t* data, size_t dataLen, uint8_t* output, size_t& outputLen) {
    // Mock encryption mode - just copy data
    // In real mode, implement AES-128-CBC with PKCS7 padding
    if (!ENABLE_ENCRYPTION) {
        memcpy(output, data, dataLen);
        outputLen = dataLen;
        return true;
    }
    
    // Real AES encryption would go here
    // For now, return false if encryption is enabled
    return false;
}

void SecurityLayer::bytesToHex(const uint8_t* data, size_t dataLen, char* hexStr, size_t hexStrSize) {
    if (hexStrSize < (dataLen * 2 + 1)) {
        print("Security Layer: Hex buffer too small\n");
        return;
    }
    
    for (size_t i = 0; i < dataLen; i++) {
        sprintf(hexStr + (i * 2), "%02x", data[i]);
    }
    hexStr[dataLen * 2] = '\0';
}
