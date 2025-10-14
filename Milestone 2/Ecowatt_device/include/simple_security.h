#ifndef SIMPLE_SECURITY_H
#define SIMPLE_SECURITY_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <Preferences.h>
#include <mbedtls/md.h>

/**
 * Lightweight Security Layer for NodeMCU/ESP32
 * 
 * Features according to requirements:
 * - HMAC-SHA256 authentication with pre-shared key
 * - Optional simplified encryption (base64 + mock encryption flag)
 * - Sequential nonce-based anti-replay protection
 * - Persistent nonce storage using Preferences
 * - Secured payload format: {"nonce": N, "payload": "...", "mac": "..."}
 */

class SimpleSecurity {
public:
    // Constructor
    SimpleSecurity();
    
    // Initialization
    bool begin(const char* pskHex = nullptr);
    void end();
    
    // Pre-shared key management
    bool setPSK(const char* pskHex);
    bool loadPSKFromStorage();
    bool savePSKToStorage(const char* pskHex);
    
    // Main security functions
    String secureMessage(const String& jsonPayload, bool useEncryption = false);
    String unsecureMessage(const String& securedMessage);
    
    // Utility functions
    uint32_t getCurrentNonce() const { return currentNonce; }
    bool isInitialized() const { return initialized; }
    
private:
    // Constants
    static const char* PREFS_NAMESPACE;
    static const char* PREFS_PSK_KEY;
    static const char* PREFS_NONCE_KEY;
    static const char* PREFS_LAST_NONCE_KEY;
    static const size_t PSK_LENGTH = 32; // 256 bits
    static const size_t HMAC_LENGTH = 32; // SHA256 output
    
    // State
    bool initialized;
    uint8_t psk[PSK_LENGTH];
    uint32_t currentNonce;
    uint32_t lastValidNonce;
    Preferences prefs;
    
    // Internal methods
    uint32_t getNextNonce();
    bool validateNonce(uint32_t receivedNonce);
    void persistNonce();
    
    // Cryptographic functions
    bool calculateHMAC(const String& data, uint8_t* hmac);
    String bytesToHex(const uint8_t* bytes, size_t length);
    bool hexToBytes(const char* hex, uint8_t* bytes, size_t length);
    
    // Base64 encoding/decoding using mbedTLS
    String base64Encode(const String& input);
    String base64Decode(const String& input);
    
    // Mock encryption (simplified for demonstration)
    String mockEncrypt(const String& input);
    String mockDecrypt(const String& input);
};

#endif // SIMPLE_SECURITY_H