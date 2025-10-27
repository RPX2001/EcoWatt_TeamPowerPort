/**
 * @file test_config.h
 * @brief Centralized configuration for WiFi, servers, and API endpoints
 * 
 * This file contains all configurable parameters used across the codebase:
 * - WiFi credentials
 * - Flask server endpoints
 * - Inverter simulator API configuration
 * - Device identifiers
 * 
 * Update this file to change configuration for all tests and source code.
 * 
 * @author Team PowerPort
 * @date 2025-01-22
 */

#ifndef TEST_CONFIG_H
#define TEST_CONFIG_H

// ============================================================================
// WIFI CONFIGURATION
// ============================================================================
#define WIFI_SSID "Galaxy A32B46A"
#define WIFI_PASSWORD "aubz5724"
#define WIFI_TIMEOUT_MS 20000

// ============================================================================
// FLASK SERVER CONFIGURATION
// ============================================================================
#define FLASK_SERVER_IP "192.168.194.249"
#define FLASK_SERVER_PORT 5001
#define FLASK_BASE_URL "http://192.168.194.249:5001"

// Flask API Endpoints
#define FLASK_HEALTH_ENDPOINT "/health"
#define FLASK_AGGREGATED_DATA_ENDPOINT "/aggregated"
#define FLASK_COMMANDS_ENDPOINT "/commands"
#define FLASK_CONFIG_ENDPOINT "/config"
#define FLASK_OTA_ENDPOINT "/ota"
#define FLASK_DIAGNOSTICS_ENDPOINT "/diagnostics"
#define FLASK_SECURITY_ENDPOINT "/security"

// ============================================================================
// INVERTER SIMULATOR API CONFIGURATION (EN4440 API Documentation)
// ============================================================================
#define INVERTER_API_BASE_URL "http://20.15.114.131:8080"
#define INVERTER_API_READ_ENDPOINT "/api/inverter/read"
#define INVERTER_API_WRITE_ENDPOINT "/api/inverter/write"
#define INVERTER_API_ERROR_ENDPOINT "/api/inverter/error"
#define INVERTER_API_KEY "NjhhZWIwNDU1ZDdmMzg3MzNiMTQ5YTFmOjY4YWViMDQ1NWQ3ZjM4NzMzYjE0OWExNQ=="

// Modbus Configuration
#define MODBUS_SLAVE_ADDRESS 0x11  // 17 decimal
#define MODBUS_FUNC_READ 0x03      // Read Holding Registers
#define MODBUS_FUNC_WRITE 0x06     // Write Single Register

// ============================================================================
// DEVICE CONFIGURATION
// ============================================================================
#define DEFAULT_DEVICE_ID "ESP32_EcoWatt_001"
#define TEST_DEVICE_ID_M3 "TEST_ESP32_M3_INTEGRATION"
#define TEST_DEVICE_ID_M4_INTEGRATION "ESP32_EcoWatt_Smart"  // M4 integration test device
#define TEST_DEVICE_ID_M4_CONFIG "TEST_CONFIG_ESP32"
#define TEST_DEVICE_ID_M4_CMD "TEST_CMD_ESP32"
#define TEST_DEVICE_ID_M4_SECURITY "TEST_SECURITY_ESP32"
#define TEST_DEVICE_ID_M4_OTA "TEST_OTA_ESP32"

// ============================================================================
// HELPER MACROS FOR BUILDING URLS
// ============================================================================
#define MAKE_FLASK_URL(endpoint) FLASK_BASE_URL endpoint
#define MAKE_INVERTER_URL(endpoint) INVERTER_API_BASE_URL endpoint

// Device-specific Flask URLs
#define FLASK_COMMAND_POLL_URL(device_id) FLASK_BASE_URL "/commands/" device_id "/poll"
#define FLASK_COMMAND_RESULT_URL(device_id) FLASK_BASE_URL "/commands/" device_id "/result"
#define FLASK_CONFIG_CHECK_URL(device_id) FLASK_BASE_URL "/config/" device_id "/check"
#define FLASK_AGGREGATED_URL(device_id) FLASK_BASE_URL "/aggregated/" device_id

// ============================================================================
// TEST CONFIGURATION
// ============================================================================
#define M3_TEST_SAMPLES 60  // Number of samples for M3 tests
#define M4_TEST_TIMEOUT_MS 10000
#define MAX_RETRY_ATTEMPTS 3

// ============================================================================
// TIMING CONFIGURATION
// ============================================================================
#define DEFAULT_POLL_FREQUENCY_US 5000000    // 5 seconds
#define DEFAULT_UPLOAD_FREQUENCY_US 15000000 // 15 seconds
#define MIN_POLL_FREQUENCY_US 1000000        // 1 second
#define MAX_POLL_FREQUENCY_US 3600000000     // 1 hour

#endif // TEST_CONFIG_H
