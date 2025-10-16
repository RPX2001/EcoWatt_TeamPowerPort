/**
 * @file security.h
 * @brief Security Layer for EcoWatt Data Upload
 * 
 * Provides HMAC-SHA256 authentication, optional AES-128-CBC encryption,
 * and anti-replay protection using sequential nonce.
 */

#ifndef SECURITY_H
#define SECURITY_H

#include <Arduino.h>
#include <Preferences.h>

/**
 * @class SecurityLayer
 * @brief Manages security operations for data uploads
 * 
 * Features:
 * - HMAC-SHA256 authentication and integrity verification
 * - Optional AES-128-CBC encryption (disabled by default for mock mode)
 * - Anti-replay protection with persistent nonce storage
 * - Base64 encoding for JSON transmission
 */
class SecurityLayer {
public:
    /**
     * @brief Initialize the security layer
     * 
     * Loads the nonce from NVS and initializes security subsystems.
     * Call this once during setup().
     */
    static void init();
    
    /**
     * @brief Secure a payload with HMAC and optional encryption
     * 
     * @param payload Original JSON payload to secure
     * @param securedPayload Output buffer for secured JSON
     * @param securedPayloadSize Size of the output buffer
     * @return true if successful, false if buffer too small or operation failed
     * 
     * Output format:
     * {
     *   "nonce": 10001,
     *   "payload": "base64_encoded_data",
     *   "mac": "hmac_sha256_hex_string",
     *   "encrypted": false
     * }
     */
    static bool securePayload(const char* payload, char* securedPayload, size_t securedPayloadSize);
    
    /**
     * @brief Get the current nonce value
     * @return Current nonce counter value
     */
    static uint32_t getCurrentNonce();
    
    /**
     * @brief Manually set the nonce value (for testing/debugging)
     * @param nonce New nonce value to set
     */
    static void setNonce(uint32_t nonce);

private:
    // Configuration
    static const bool ENABLE_ENCRYPTION = false;  // Mock encryption mode (Base64 only)
    
    // Pre-Shared Keys (PSK) - MUST match on server!
    static const uint8_t PSK_HMAC[32];  // 256-bit key for HMAC-SHA256
    static const uint8_t PSK_AES[16];   // 128-bit key for AES-128
    static const uint8_t AES_IV[16];    // Initialization vector for AES-CBC
    
    // Nonce management
    static uint32_t currentNonce;
    static Preferences preferences;
    
    /**
     * @brief Load nonce from NVS
     */
    static void loadNonce();
    
    /**
     * @brief Increment nonce and save to NVS
     * @return New nonce value
     */
    static uint32_t incrementAndSaveNonce();
    
    /**
     * @brief Save nonce to NVS
     * @param nonce Nonce value to save
     */
    static void saveNonce(uint32_t nonce);
    
    /**
     * @brief Calculate HMAC-SHA256
     * @param data Data to sign
     * @param dataLen Length of data
     * @param hmac Output buffer for 32-byte HMAC
     */
    static void calculateHMAC(const uint8_t* data, size_t dataLen, uint8_t* hmac);
    
    /**
     * @brief Encrypt data using AES-128-CBC (currently mock - just returns input)
     * @param data Data to encrypt
     * @param dataLen Length of data
     * @param output Output buffer for encrypted data
     * @param outputLen Output length (will be padded to 16-byte blocks)
     * @return true if successful
     */
    static bool encryptAES(const uint8_t* data, size_t dataLen, uint8_t* output, size_t& outputLen);
    
    /**
     * @brief Convert binary data to hex string
     * @param data Binary data
     * @param dataLen Length of data
     * @param hexStr Output hex string buffer
     * @param hexStrSize Size of hex string buffer
     */
    static void bytesToHex(const uint8_t* data, size_t dataLen, char* hexStr, size_t hexStrSize);
};

#endif // SECURITY_H
