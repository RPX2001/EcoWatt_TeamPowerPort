/**
 * @file security.cpp
 * @brief Implementation of security layer for EcoWatt data communication
 */

#include "application/security.h"
#include "peripheral/print.h"

// Global instance
SecurityManager securityManager;

/**
 * @brief Initialize security manager with PSK
 */
bool SecurityManager::initialize(const uint8_t* psk_in, size_t psk_length) {
    if (psk_in == nullptr || psk_length == 0 || psk_length > HMAC_KEY_SIZE) {
        print("ERROR: Invalid PSK parameters\n");
        return false;
    }
    
    // Copy PSK
    memcpy(psk, psk_in, psk_length);
    psk_len = psk_length;
    
    // Pad with zeros if needed
    if (psk_len < HMAC_KEY_SIZE) {
        memset(psk + psk_len, 0, HMAC_KEY_SIZE - psk_len);
        psk_len = HMAC_KEY_SIZE;
    }
    
    // Initialize NVS for sequence number storage
    if (!prefs.begin("security", false)) {
        print("ERROR: Failed to open security NVS namespace\n");
        return false;
    }
    
    // Load sequence number from NVS
    sequence_number = loadSequenceFromNVS();
    print("Security Manager initialized - Sequence number: %u\n", sequence_number);
    
    initialized = true;
    return true;
}

/**
 * @brief Compute HMAC-SHA256 for payload
 */
bool SecurityManager::computeHMAC(const uint8_t* data, size_t data_len, 
                                  uint32_t sequence_number, uint8_t* hmac_out) {
    if (!initialized) {
        print("ERROR: Security manager not initialized\n");
        return false;
    }
    
    if (data == nullptr || hmac_out == nullptr) {
        print("ERROR: Invalid HMAC parameters\n");
        return false;
    }
    
    // Create message: data + sequence_number
    // We'll hash: data || sequence_number (concatenated)
    size_t total_len = data_len + sizeof(uint32_t);
    uint8_t* message = (uint8_t*)malloc(total_len);
    
    if (message == nullptr) {
        print("ERROR: Failed to allocate memory for HMAC message\n");
        return false;
    }
    
    // Copy data
    memcpy(message, data, data_len);
    
    // Append sequence number (big-endian)
    message[data_len]     = (sequence_number >> 24) & 0xFF;
    message[data_len + 1] = (sequence_number >> 16) & 0xFF;
    message[data_len + 2] = (sequence_number >> 8) & 0xFF;
    message[data_len + 3] = sequence_number & 0xFF;
    
    // Compute HMAC using mbedtls
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    
    const mbedtls_md_info_t* md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    
    int ret = mbedtls_md_setup(&ctx, md_info, 1); // 1 = HMAC mode
    if (ret != 0) {
        print("ERROR: mbedtls_md_setup failed: %d\n", ret);
        free(message);
        mbedtls_md_free(&ctx);
        return false;
    }
    
    ret = mbedtls_md_hmac_starts(&ctx, psk, psk_len);
    if (ret != 0) {
        print("ERROR: mbedtls_md_hmac_starts failed: %d\n", ret);
        free(message);
        mbedtls_md_free(&ctx);
        return false;
    }
    
    ret = mbedtls_md_hmac_update(&ctx, message, total_len);
    if (ret != 0) {
        print("ERROR: mbedtls_md_hmac_update failed: %d\n", ret);
        free(message);
        mbedtls_md_free(&ctx);
        return false;
    }
    
    ret = mbedtls_md_hmac_finish(&ctx, hmac_out);
    if (ret != 0) {
        print("ERROR: mbedtls_md_hmac_finish failed: %d\n", ret);
        free(message);
        mbedtls_md_free(&ctx);
        return false;
    }
    
    // Cleanup
    mbedtls_md_free(&ctx);
    free(message);
    
    return true;
}

/**
 * @brief Get current sequence number and increment it
 */
uint32_t SecurityManager::getAndIncrementSequence() {
    uint32_t current = sequence_number;
    sequence_number++;
    
    // Save to NVS every 10 increments to reduce wear
    if (sequence_number % 10 == 0) {
        saveSequenceToNVS(sequence_number);
    }
    
    return current;
}

/**
 * @brief Get current sequence number without incrementing
 */
uint32_t SecurityManager::getCurrentSequence() {
    return sequence_number;
}

/**
 * @brief Reset sequence number (for testing only)
 */
void SecurityManager::resetSequence() {
    sequence_number = 0;
    saveSequenceToNVS(0);
    print("WARNING: Sequence number reset to 0\n");
}

/**
 * @brief Convert binary HMAC to hex string
 */
void SecurityManager::hmacToHex(const uint8_t* hmac, char* hex_out) {
    for (int i = 0; i < HMAC_OUTPUT_SIZE; i++) {
        sprintf(hex_out + (i * 2), "%02x", hmac[i]);
    }
    hex_out[HMAC_OUTPUT_SIZE * 2] = '\0';
}

/**
 * @brief Load sequence number from NVS
 */
uint32_t SecurityManager::loadSequenceFromNVS() {
    uint32_t seq = prefs.getUInt(SEQUENCE_NVS_KEY, 0);
    print("Loaded sequence number from NVS: %u\n", seq);
    return seq;
}

/**
 * @brief Save sequence number to NVS
 */
void SecurityManager::saveSequenceToNVS(uint32_t seq) {
    prefs.putUInt(SEQUENCE_NVS_KEY, seq);
}
