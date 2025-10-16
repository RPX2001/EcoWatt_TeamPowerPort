/**
 * @file security.h
 * @brief Security layer for EcoWatt data communication
 * 
 * Implements:
 * - HMAC-SHA256 authentication using Pre-Shared Key (PSK)
 * - Sequence number tracking for anti-replay protection
 * - Payload integrity verification
 * 
 * Security Model:
 * - PSK stored in NVS (shared between ESP32 and Flask server)
 * - Each message includes: data + sequence_number + HMAC
 * - Server validates HMAC and checks sequence number monotonicity
 * 
 * @date October 2025
 */

#ifndef SECURITY_H
#define SECURITY_H

#include <Arduino.h>
#include <mbedtls/md.h>
#include <mbedtls/sha256.h>
#include <Preferences.h>

// Security configuration
#define HMAC_KEY_SIZE 32        // 256-bit key
#define HMAC_OUTPUT_SIZE 32     // SHA-256 produces 32 bytes
#define SEQUENCE_NVS_KEY "seq_num"

/**
 * @class SecurityManager
 * @brief Manages security operations for data communication
 */
class SecurityManager {
public:
    /**
     * @brief Initialize security manager with PSK
     * @param psk Pre-shared key (32 bytes for HMAC-SHA256)
     * @param psk_length Length of PSK
     * @return true if initialization successful
     */
    bool initialize(const uint8_t* psk, size_t psk_length);
    
    /**
     * @brief Compute HMAC-SHA256 for payload
     * @param data Input data
     * @param data_len Length of input data
     * @param sequence_number Current sequence number
     * @param hmac_out Output buffer (must be at least 32 bytes)
     * @return true if HMAC computed successfully
     */
    bool computeHMAC(const uint8_t* data, size_t data_len, 
                     uint32_t sequence_number, uint8_t* hmac_out);
    
    /**
     * @brief Get current sequence number and increment it
     * @return Current sequence number before increment
     */
    uint32_t getAndIncrementSequence();
    
    /**
     * @brief Get current sequence number without incrementing
     * @return Current sequence number
     */
    uint32_t getCurrentSequence();
    
    /**
     * @brief Reset sequence number (for testing only)
     */
    void resetSequence();
    
    /**
     * @brief Convert binary HMAC to hex string for transmission
     * @param hmac Binary HMAC (32 bytes)
     * @param hex_out Output buffer (must be at least 65 bytes)
     */
    void hmacToHex(const uint8_t* hmac, char* hex_out);
    
    /**
     * @brief Get security status
     * @return true if security manager is initialized
     */
    bool isInitialized() const { return initialized; }

private:
    uint8_t psk[HMAC_KEY_SIZE];     // Pre-shared key
    size_t psk_len;                  // Length of PSK
    bool initialized;                // Initialization status
    Preferences prefs;               // NVS storage for sequence number
    uint32_t sequence_number;        // Current sequence number (cached)
    
    /**
     * @brief Load sequence number from NVS
     * @return Sequence number from NVS (0 if not found)
     */
    uint32_t loadSequenceFromNVS();
    
    /**
     * @brief Save sequence number to NVS
     * @param seq Sequence number to save
     */
    void saveSequenceToNVS(uint32_t seq);
};

// Global security manager instance
extern SecurityManager securityManager;

#endif // SECURITY_H
