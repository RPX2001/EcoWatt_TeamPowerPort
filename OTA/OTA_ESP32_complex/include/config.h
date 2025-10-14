/**
 * @file config.h
 * @brief Configuration constants for Secure ESP32 OTA System
 * 
 * This file contains all configuration parameters for the secure OTA update system.
 * Modify these values according to your specific deployment requirements.
 * 
 * Security Note: In production, sensitive values should be stored in secure 
 * storage or retrieved from a secure provisioning system.
 */

#ifndef CONFIG_H
#define CONFIG_H

// ========== WiFi Configuration ==========
#define WIFI_SSID "YOUR_WIFI_SSID"                    // Replace with your WiFi network name
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"            // Replace with your WiFi password
#define WIFI_TIMEOUT_MS 30000                         // WiFi connection timeout in milliseconds
#define WIFI_RETRY_DELAY_MS 1000                      // Delay between WiFi connection attempts

// ========== OTA Server Configuration ==========
#define OTA_SERVER_HOST "192.168.1.100"               // Replace with your Flask server IP
#define OTA_SERVER_PORT 5000                          // Flask server port
#define OTA_SERVER_USE_HTTPS true                     // Use HTTPS for secure communication

// OTA Server Endpoints
#define MANIFEST_ENDPOINT "/firmware/manifest"
#define FIRMWARE_ENDPOINT "/firmware"
#define REPORT_ENDPOINT "/firmware/report"

// ========== Security Configuration ==========
// API Key for authenticating with OTA server
// In production, this should be securely provisioned or retrieved from secure storage
#define API_KEY "your-secure-api-key-here-change-this-in-production"

// HMAC key for firmware integrity verification (32 bytes)
// This must match the key used by the Flask server for signing firmware
#define HMAC_KEY "this-is-a-32-byte-hmac-key-change"

// Root CA certificate for HTTPS verification (self-signed for local development)
// In production, use a proper CA certificate or certificate pinning
extern const char* root_ca_cert;

// ========== OTA Update Configuration ==========
#define FIRMWARE_VERSION "1.0.0"                      // Current firmware version
#define CHECK_INTERVAL_MS 300000                      // Check for updates every 5 minutes
#define DOWNLOAD_CHUNK_SIZE 1024                      // Firmware download chunk size in bytes
#define MAX_DOWNLOAD_RETRIES 3                        // Maximum retry attempts for failed downloads
#define DOWNLOAD_TIMEOUT_MS 30000                     // Download timeout per chunk

// ========== System Configuration ==========
#define SERIAL_BAUD_RATE 115200                       // Serial communication baud rate
#define WATCHDOG_TIMEOUT_MS 60000                     // Watchdog timer timeout
#define STATUS_LED_PIN 2                              // Built-in LED pin for status indication
#define RESET_BUTTON_PIN 0                            // Boot button for factory reset

// ========== Debug Configuration ==========
#define DEBUG_ENABLED true                            // Enable debug output
#define SIMULATE_FAILURES false                       // Enable failure simulation for testing

// Debug levels
#define DEBUG_LEVEL_ERROR 1
#define DEBUG_LEVEL_WARN 2
#define DEBUG_LEVEL_INFO 3
#define DEBUG_LEVEL_DEBUG 4

#define DEBUG_LEVEL DEBUG_LEVEL_INFO                  // Current debug level

// ========== Memory and Performance ==========
#define OTA_BUFFER_SIZE 4096                          // OTA buffer size for memory-efficient updates
#define JSON_BUFFER_SIZE 1024                         // JSON parsing buffer size
#define HTTP_TIMEOUT_MS 15000                         // HTTP request timeout

// ========== Rollback Configuration ==========
#define ROLLBACK_TIMEOUT_MS 120000                    // Time to wait before rollback (2 minutes)
#define MAX_ROLLBACK_ATTEMPTS 2                       // Maximum automatic rollback attempts

// ========== Device Information ==========
#define DEVICE_ID_PREFIX "ESP32-OTA"                  // Device ID prefix
#define HARDWARE_VERSION "1.0"                        // Hardware version string

// ========== Validation Macros ==========
// Compile-time validation of configuration
#if DOWNLOAD_CHUNK_SIZE > OTA_BUFFER_SIZE
#error "DOWNLOAD_CHUNK_SIZE cannot be larger than OTA_BUFFER_SIZE"
#endif

#if CHECK_INTERVAL_MS < 60000
#warning "CHECK_INTERVAL_MS is less than 1 minute. Consider longer intervals for production."
#endif

#endif // CONFIG_H