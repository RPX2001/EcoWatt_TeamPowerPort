#ifndef SECURITY_H
#define SECURITY_H

#include <Arduino.h>
#include <vector>
#include <mbedtls/md.h>
#include <mbedtls/aes.h>

/**
 * @brief Security layer for data transmission
 * 
 * Implements:
 * - HMAC-SHA256 for authentication and integrity
 * - Optional AES-128-CBC encryption
 * - Anti-replay protection with nonce management
 * - Persistent nonce storage in NVS
 */
class SecurityLayer {
public:
    /**
     * @brief Initialize the security layer
     * 
     * Loads the last nonce from NVS and sets up keys
     */
    static void init();
    
    /**
     * @brief Secure a JSON payload with HMAC and optional encryption
     * 
     * @param jsonPayload The JSON string to secure
     * @param securedPayload Output buffer for the secured JSON (with nonce, payload, mac)
     * @param maxSize Maximum size of the output buffer
     * @return true if successful, false otherwise
     */
    static bool securePayload(const char* jsonPayload, char* securedPayload, size_t maxSize);
    
    /**
     * @brief Get the current nonce value
     * 
     * @return Current nonce
     */
    static uint32_t getCurrentNonce();
    
    /**
     * @brief Manually set the nonce (for testing or recovery)
     * 
     * @param nonce New nonce value
     */
    static void setNonce(uint32_t nonce);

private:
    // Pre-shared key for HMAC (32 bytes for SHA256)
    static const uint8_t PSK_HMAC[32];
    
    // Pre-shared key for AES encryption (16 bytes for AES-128)
    static const uint8_t PSK_AES[16];
    
    // Initialization vector for AES-CBC (16 bytes)
    static const uint8_t AES_IV[16];
    
    // Current nonce counter
    static uint32_t currentNonce;
    
    // Flag to enable/disable encryption (set to false for mock encryption)
    static const bool ENABLE_ENCRYPTION = false;
    
    /**
     * @brief Calculate HMAC-SHA256
     * 
     * @param data Input data
     * @param dataLen Length of input data
     * @param hmac Output buffer for HMAC (32 bytes)
     */
    static void calculateHMAC(const uint8_t* data, size_t dataLen, uint8_t* hmac);
    
    /**
     * @brief Encrypt data using AES-128-CBC
     * 
     * @param plaintext Input plaintext
     * @param plaintextLen Length of plaintext
     * @param ciphertext Output buffer for ciphertext
     * @param ciphertextLen Output length of ciphertext
     * @return true if successful
     */
    static bool encryptAES(const uint8_t* plaintext, size_t plaintextLen, 
                          uint8_t* ciphertext, size_t* ciphertextLen);
    
    /**
     * @brief Increment nonce and save to NVS
     */
    static void incrementAndSaveNonce();
    
    /**
     * @brief Load nonce from NVS
     */
    static void loadNonce();
    
    /**
     * @brief Save nonce to NVS
     */
    static void saveNonce();
    
    /**
     * @brief Convert binary data to hex string
     */
    static void binToHex(const uint8_t* bin, size_t binLen, char* hex, size_t hexSize);
};

#endif // SECURITY_H
