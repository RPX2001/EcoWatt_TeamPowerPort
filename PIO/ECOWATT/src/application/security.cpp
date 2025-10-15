#include "application/security.h"
#include "application/nvs.h"
#include "peripheral/print.h"
#include <Preferences.h>
#include <base64.h>
#include <ArduinoJson.h>

// Pre-shared keys (256-bit for HMAC, 128-bit for AES)
// In production, these should be securely provisioned
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

uint32_t SecurityLayer::currentNonce = 0;

void SecurityLayer::init() {
    loadNonce();
    print("Security Layer initialized with nonce: %u\n", currentNonce);
}

void SecurityLayer::loadNonce() {
    Preferences preferences;
    preferences.begin("security", false);
    currentNonce = preferences.getUInt("nonce", 10000); // Start from 10000 if not found
    preferences.end();
}

void SecurityLayer::saveNonce() {
    Preferences preferences;
    preferences.begin("security", false);
    preferences.putUInt("nonce", currentNonce);
    preferences.end();
}

void SecurityLayer::incrementAndSaveNonce() {
    currentNonce++;
    saveNonce();
}

uint32_t SecurityLayer::getCurrentNonce() {
    return currentNonce;
}

void SecurityLayer::setNonce(uint32_t nonce) {
    currentNonce = nonce;
    saveNonce();
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

bool SecurityLayer::encryptAES(const uint8_t* plaintext, size_t plaintextLen, 
                               uint8_t* ciphertext, size_t* ciphertextLen) {
    // Calculate padded length (AES block size is 16 bytes)
    size_t paddedLen = ((plaintextLen + 15) / 16) * 16;
    
    // Create padded plaintext (PKCS7 padding)
    uint8_t* paddedPlaintext = new uint8_t[paddedLen];
    memcpy(paddedPlaintext, plaintext, plaintextLen);
    
    // Apply PKCS7 padding
    uint8_t paddingValue = paddedLen - plaintextLen;
    for (size_t i = plaintextLen; i < paddedLen; i++) {
        paddedPlaintext[i] = paddingValue;
    }
    
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    
    // Copy IV (it will be modified during encryption)
    uint8_t iv[16];
    memcpy(iv, AES_IV, 16);
    
    int ret = mbedtls_aes_setkey_enc(&aes, PSK_AES, 128);
    if (ret != 0) {
        print("AES key setup failed: %d\n", ret);
        delete[] paddedPlaintext;
        mbedtls_aes_free(&aes);
        return false;
    }
    
    ret = mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, paddedLen, 
                                iv, paddedPlaintext, ciphertext);
    
    delete[] paddedPlaintext;
    mbedtls_aes_free(&aes);
    
    if (ret != 0) {
        print("AES encryption failed: %d\n", ret);
        return false;
    }
    
    *ciphertextLen = paddedLen;
    return true;
}

void SecurityLayer::binToHex(const uint8_t* bin, size_t binLen, char* hex, size_t hexSize) {
    if (hexSize < (binLen * 2 + 1)) {
        print("Hex buffer too small\n");
        return;
    }
    
    for (size_t i = 0; i < binLen; i++) {
        sprintf(hex + (i * 2), "%02x", bin[i]);
    }
    hex[binLen * 2] = '\0';
}

bool SecurityLayer::securePayload(const char* jsonPayload, char* securedPayload, size_t maxSize) {
    // Increment nonce for anti-replay protection
    incrementAndSaveNonce();
    
    // Create the data to be signed: nonce + payload
    size_t payloadLen = strlen(jsonPayload);
    size_t dataToSignLen = 4 + payloadLen; // 4 bytes for uint32_t nonce
    uint8_t* dataToSign = new uint8_t[dataToSignLen];
    
    // Pack nonce (big-endian)
    dataToSign[0] = (currentNonce >> 24) & 0xFF;
    dataToSign[1] = (currentNonce >> 16) & 0xFF;
    dataToSign[2] = (currentNonce >> 8) & 0xFF;
    dataToSign[3] = currentNonce & 0xFF;
    
    // Copy payload
    memcpy(dataToSign + 4, jsonPayload, payloadLen);
    
    // Calculate HMAC-SHA256
    uint8_t hmac[32];
    calculateHMAC(dataToSign, dataToSignLen, hmac);
    
    delete[] dataToSign;
    
    // Convert HMAC to hex string
    char hmacHex[65]; // 32 bytes * 2 + null terminator
    binToHex(hmac, 32, hmacHex, sizeof(hmacHex));
    
    // Prepare payload (either encrypted or base64-encoded plaintext)
    String encodedPayload;
    
    if (ENABLE_ENCRYPTION) {
        // Real encryption
        uint8_t ciphertext[2048];
        size_t ciphertextLen;
        
        if (!encryptAES((const uint8_t*)jsonPayload, payloadLen, ciphertext, &ciphertextLen)) {
            print("Encryption failed\n");
            return false;
        }
        
        // Base64 encode the ciphertext
        encodedPayload = base64::encode(ciphertext, ciphertextLen);
    } else {
        // Mock encryption: just base64 encode the plaintext
        encodedPayload = base64::encode((const uint8_t*)jsonPayload, payloadLen);
    }
    
    // Create secured JSON structure
    DynamicJsonDocument doc(8192);
    doc["nonce"] = currentNonce;
    doc["payload"] = encodedPayload;
    doc["mac"] = hmacHex;
    doc["encrypted"] = ENABLE_ENCRYPTION;
    
    size_t jsonLen = serializeJson(doc, securedPayload, maxSize);
    
    if (jsonLen == 0 || jsonLen >= maxSize) {
        print("Secured payload serialization failed or buffer too small\n");
        return false;
    }
    
    print("Payload secured with nonce %u, HMAC authentication\n", currentNonce);
    return true;
}
