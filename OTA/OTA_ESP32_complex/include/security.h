/**
 * @file security.h
 * @brief Security utilities for ESP32 OTA system
 * 
 * This file provides cryptographic functions for secure OTA updates including:
 * - SHA256 hashing for firmware integrity verification
 * - HMAC-SHA256 for firmware authentication
 * - Secure random number generation
 * - Certificate validation helpers
 */

#ifndef SECURITY_H
#define SECURITY_H

#include <Arduino.h>
#include <WiFiClientSecure.h>

/**
 * @brief Calculate SHA256 hash of data
 * @param data Pointer to data to hash
 * @param length Length of data in bytes
 * @param hash Buffer to store resulting hash (32 bytes)
 * @return true if successful, false otherwise
 */
bool calculateSHA256(const uint8_t* data, size_t length, uint8_t* hash);

/**
 * @brief Calculate SHA256 hash of a stream (for large files)
 * @param stream Stream to read data from
 * @param length Total length of data to hash
 * @param hash Buffer to store resulting hash (32 bytes)
 * @return true if successful, false otherwise
 */
bool calculateSHA256Stream(Stream* stream, size_t length, uint8_t* hash);

/**
 * @brief Verify HMAC-SHA256 signature
 * @param data Pointer to data to verify
 * @param dataLength Length of data in bytes
 * @param key HMAC key
 * @param keyLength Length of HMAC key in bytes
 * @param expectedHmac Expected HMAC value (32 bytes)
 * @return true if HMAC matches, false otherwise
 */
bool verifyHMAC(const uint8_t* data, size_t dataLength, 
               const uint8_t* key, size_t keyLength, 
               const uint8_t* expectedHmac);

/**
 * @brief Convert hex string to byte array
 * @param hexString Hex string (e.g., "deadbeef")
 * @param bytes Buffer to store bytes
 * @param maxBytes Maximum number of bytes to convert
 * @return Number of bytes converted, -1 on error
 */
int hexStringToBytes(const char* hexString, uint8_t* bytes, size_t maxBytes);

/**
 * @brief Convert byte array to hex string
 * @param bytes Byte array to convert
 * @param length Number of bytes to convert
 * @param hexString Buffer to store hex string (must be at least length*2+1 bytes)
 */
void bytesToHexString(const uint8_t* bytes, size_t length, char* hexString);

/**
 * @brief Setup secure WiFi client with certificate validation
 * @param client WiFiClientSecure instance to configure
 * @param rootCACert Root CA certificate for server validation
 * @return true if setup successful, false otherwise
 */
bool setupSecureClient(WiFiClientSecure& client, const char* rootCACert);

/**
 * @brief Generate device-unique identifier
 * @param deviceId Buffer to store device ID string
 * @param maxLength Maximum length of device ID buffer
 */
void generateDeviceID(char* deviceId, size_t maxLength);

/**
 * @brief Secure memory comparison (constant time)
 * @param a First buffer
 * @param b Second buffer
 * @param length Length to compare
 * @return 0 if equal, non-zero if different
 */
int secureMemcmp(const void* a, const void* b, size_t length);

/**
 * @brief Secure memory clear (prevents compiler optimization)
 * @param ptr Pointer to memory to clear
 * @param length Number of bytes to clear
 */
void secureMemclear(void* ptr, size_t length);

#endif // SECURITY_H