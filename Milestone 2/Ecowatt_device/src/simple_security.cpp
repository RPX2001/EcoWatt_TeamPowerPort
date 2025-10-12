#include "simple_security.h"
#include <mbedtls/base64.h>

// Static constants
const char* SimpleSecurity::PREFS_NAMESPACE = "security";
const char* SimpleSecurity::PREFS_PSK_KEY = "psk";
const char* SimpleSecurity::PREFS_NONCE_KEY = "nonce";
const char* SimpleSecurity::PREFS_LAST_NONCE_KEY = "last_nonce";

SimpleSecurity::SimpleSecurity() 
    : initialized(false), currentNonce(0), lastValidNonce(0) {
    memset(psk, 0, PSK_LENGTH);
}

bool SimpleSecurity::begin(const char* pskHex) {
    if (!prefs.begin(PREFS_NAMESPACE, false)) {
        Serial.println("[Security] Failed to initialize Preferences");
        return false;
    }
    
    // Load or set PSK
    if (pskHex != nullptr) {
        if (!setPSK(pskHex)) {
            Serial.println("[Security] Failed to set PSK");
            prefs.end();
            return false;
        }
        savePSKToStorage(pskHex);
    } else {
        if (!loadPSKFromStorage()) {
            Serial.println("[Security] No PSK provided and none in storage");
            prefs.end();
            return false;
        }
    }
    
    // Load nonce state
    currentNonce = prefs.getUInt(PREFS_NONCE_KEY, 1);
    lastValidNonce = prefs.getUInt(PREFS_LAST_NONCE_KEY, 0);
    
    // Ensure nonce advancement after reboot
    if (currentNonce <= lastValidNonce) {
        currentNonce = lastValidNonce + 1;
        persistNonce();
    }
    
    initialized = true;
    Serial.println("[Security] Security layer initialized successfully");
    Serial.printf("[Security] Current nonce: %u, Last valid: %u\n", currentNonce, lastValidNonce);
    
    return true;
}

void SimpleSecurity::end() {
    if (initialized) {
        persistNonce();
        prefs.end();
        initialized = false;
        Serial.println("[Security] Security layer stopped");
    }
}

bool SimpleSecurity::setPSK(const char* pskHex) {
    if (!pskHex || strlen(pskHex) != PSK_LENGTH * 2) {
        Serial.println("[Security] Invalid PSK format - must be 64 hex characters");
        return false;
    }
    
    return hexToBytes(pskHex, psk, PSK_LENGTH);
}

bool SimpleSecurity::loadPSKFromStorage() {
    size_t expectedLen = PSK_LENGTH * 2 + 1; // Hex string + null terminator
    char* storedPSK = new char[expectedLen];
    
    size_t actualLen = prefs.getString(PREFS_PSK_KEY, storedPSK, expectedLen);
    
    bool success = false;
    if (actualLen == PSK_LENGTH * 2) {
        success = hexToBytes(storedPSK, psk, PSK_LENGTH);
    }
    
    delete[] storedPSK;
    return success;
}

bool SimpleSecurity::savePSKToStorage(const char* pskHex) {
    if (!pskHex || strlen(pskHex) != PSK_LENGTH * 2) {
        return false;
    }
    
    return prefs.putString(PREFS_PSK_KEY, pskHex) == PSK_LENGTH * 2;
}

String SimpleSecurity::secureMessage(const String& jsonPayload, bool useEncryption) {
    if (!initialized) {
        Serial.println("[Security] Error: Security layer not initialized");
        return "";
    }
    
    uint32_t nonce = getNextNonce();
    
    // Create secured message structure
    JsonDocument securedDoc;
    securedDoc["nonce"] = nonce;
    
    String payload = jsonPayload;
    if (useEncryption) {
        // Apply mock encryption (base64 + flag)
        payload = mockEncrypt(jsonPayload);
        securedDoc["encrypted"] = true;
    } else {
        securedDoc["encrypted"] = false;
    }
    
    securedDoc["payload"] = payload;
    
    // Create string for HMAC calculation (nonce + payload)
    String dataForHMAC = String(nonce) + ":" + payload;
    
    // Calculate HMAC
    uint8_t hmac[HMAC_LENGTH];
    if (!calculateHMAC(dataForHMAC, hmac)) {
        Serial.println("[Security] Failed to calculate HMAC");
        return "";
    }
    
    securedDoc["mac"] = bytesToHex(hmac, HMAC_LENGTH);
    
    // Serialize to string
    String result;
    serializeJson(securedDoc, result);
    
    Serial.printf("[Security] Secured message with nonce %u\n", nonce);
    
    return result;
}

String SimpleSecurity::unsecureMessage(const String& securedMessage) {
    if (!initialized) {
        Serial.println("[Security] Error: Security layer not initialized");
        return "";
    }
    
    // Parse secured message
    JsonDocument securedDoc;
    DeserializationError error = deserializeJson(securedDoc, securedMessage);
    
    if (error) {
        Serial.printf("[Security] JSON parse error: %s\n", error.c_str());
        return "";
    }
    
    // Validate structure
    if (securedDoc["nonce"].isNull() || 
        securedDoc["payload"].isNull() || 
        securedDoc["mac"].isNull()) {
        Serial.println("[Security] Invalid secured message structure");
        return "";
    }
    
    uint32_t nonce = securedDoc["nonce"];
    String payload = securedDoc["payload"];
    String receivedMAC = securedDoc["mac"];
    bool isEncrypted = securedDoc["encrypted"] | false;
    
    // Validate nonce (anti-replay)
    if (!validateNonce(nonce)) {
        Serial.printf("[Security] Invalid nonce %u (replay attack?)\n", nonce);
        return "";
    }
    
    // Verify HMAC
    String dataForHMAC = String(nonce) + ":" + payload;
    uint8_t calculatedHMAC[HMAC_LENGTH];
    
    if (!calculateHMAC(dataForHMAC, calculatedHMAC)) {
        Serial.println("[Security] Failed to calculate HMAC for verification");
        return "";
    }
    
    String calculatedMACHex = bytesToHex(calculatedHMAC, HMAC_LENGTH);
    
    if (receivedMAC != calculatedMACHex) {
        Serial.println("[Security] HMAC verification failed - message tampered!");
        return "";
    }
    
    // Update last valid nonce
    lastValidNonce = nonce;
    persistNonce();
    
    String result = payload;
    if (isEncrypted) {
        result = mockDecrypt(payload);
    }
    
    Serial.printf("[Security] Successfully verified message with nonce %u\n", nonce);
    
    return result;
}

uint32_t SimpleSecurity::getNextNonce() {
    return currentNonce++;
}

bool SimpleSecurity::validateNonce(uint32_t receivedNonce) {
    // Sequential nonce validation - must be greater than last valid
    // Allow some tolerance for out-of-order delivery (within window of 10)
    const uint32_t NONCE_WINDOW = 10;
    
    if (receivedNonce <= lastValidNonce) {
        return false; // Replay attack or old message
    }
    
    if (receivedNonce > lastValidNonce + NONCE_WINDOW) {
        Serial.printf("[Security] Warning: Large nonce gap %u -> %u\n", 
                     lastValidNonce, receivedNonce);
    }
    
    return true;
}

void SimpleSecurity::persistNonce() {
    prefs.putUInt(PREFS_NONCE_KEY, currentNonce);
    prefs.putUInt(PREFS_LAST_NONCE_KEY, lastValidNonce);
}

bool SimpleSecurity::calculateHMAC(const String& data, uint8_t* hmac) {
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    
    const mbedtls_md_info_t* md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    
    if (mbedtls_md_setup(&ctx, md_info, 1) != 0) {
        mbedtls_md_free(&ctx);
        return false;
    }
    
    if (mbedtls_md_hmac_starts(&ctx, psk, PSK_LENGTH) != 0) {
        mbedtls_md_free(&ctx);
        return false;
    }
    
    if (mbedtls_md_hmac_update(&ctx, (const unsigned char*)data.c_str(), data.length()) != 0) {
        mbedtls_md_free(&ctx);
        return false;
    }
    
    if (mbedtls_md_hmac_finish(&ctx, hmac) != 0) {
        mbedtls_md_free(&ctx);
        return false;
    }
    
    mbedtls_md_free(&ctx);
    return true;
}

String SimpleSecurity::bytesToHex(const uint8_t* bytes, size_t length) {
    String result;
    result.reserve(length * 2);
    
    for (size_t i = 0; i < length; i++) {
        char hex[3];
        sprintf(hex, "%02x", bytes[i]);
        result += hex;
    }
    
    return result;
}

bool SimpleSecurity::hexToBytes(const char* hex, uint8_t* bytes, size_t length) {
    if (strlen(hex) != length * 2) {
        return false;
    }
    
    for (size_t i = 0; i < length; i++) {
        char hexByte[3] = {hex[i * 2], hex[i * 2 + 1], '\0'};
        bytes[i] = strtol(hexByte, nullptr, 16);
    }
    
    return true;
}

String SimpleSecurity::base64Encode(const String& input) {
    if (input.length() == 0) return "";
    
    size_t outputLen = ((input.length() + 2) / 3) * 4 + 1;
    char* output = new char[outputLen];
    
    size_t actualLen;
    int ret = mbedtls_base64_encode(
        (unsigned char*)output, outputLen, &actualLen,
        (const unsigned char*)input.c_str(), input.length()
    );
    
    String result;
    if (ret == 0) {
        output[actualLen] = '\0';
        result = String(output);
    }
    
    delete[] output;
    return result;
}

String SimpleSecurity::base64Decode(const String& input) {
    if (input.length() == 0) return "";
    
    size_t outputLen = (input.length() * 3) / 4 + 1;
    char* output = new char[outputLen];
    
    size_t actualLen;
    int ret = mbedtls_base64_decode(
        (unsigned char*)output, outputLen, &actualLen,
        (const unsigned char*)input.c_str(), input.length()
    );
    
    String result;
    if (ret == 0) {
        output[actualLen] = '\0';
        result = String(output);
    }
    
    delete[] output;
    return result;
}

String SimpleSecurity::mockEncrypt(const String& input) {
    // Simple mock encryption: base64 encode with character shifting
    String encoded = base64Encode(input);
    
    // Simple character shifting for mock encryption
    String result = "";
    for (size_t i = 0; i < encoded.length(); i++) {
        char c = encoded[i];
        if (c >= 'A' && c <= 'Z') {
            c = ((c - 'A' + 3) % 26) + 'A';
        } else if (c >= 'a' && c <= 'z') {
            c = ((c - 'a' + 3) % 26) + 'a';
        } else if (c >= '0' && c <= '9') {
            c = ((c - '0' + 3) % 10) + '0';
        }
        result += c;
    }
    
    return result;
}

String SimpleSecurity::mockDecrypt(const String& input) {
    // Reverse mock encryption: character shifting back then base64 decode
    String shifted = "";
    for (size_t i = 0; i < input.length(); i++) {
        char c = input[i];
        if (c >= 'A' && c <= 'Z') {
            c = ((c - 'A' - 3 + 26) % 26) + 'A';
        } else if (c >= 'a' && c <= 'z') {
            c = ((c - 'a' - 3 + 26) % 26) + 'a';
        } else if (c >= '0' && c <= '9') {
            c = ((c - '0' - 3 + 10) % 10) + '0';
        }
        shifted += c;
    }
    
    return base64Decode(shifted);
}