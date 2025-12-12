/**
 * @file test_config.h
 * @brief Test-specific configuration (inherits production config from credentials.h)
 * 
 * This file contains TEST-ONLY configuration:
 * - Test device identifiers
 * - Inverter simulator API configuration
 * - Test-specific endpoints
 * 
 * Production configuration (WiFi, server, device ID) comes from:
 * - application/credentials.h (WiFi, Flask server, device ID)
 * - application/system_config.h (timing, buffers, thresholds)
 * 
 * @author Team PowerPort
 * @date 2025-01-22
 */

#ifndef TEST_CONFIG_H
#define TEST_CONFIG_H

// Import production credentials
#include "application/credentials.h"
#include "application/system_config.h"

// ============================================================================
// TEST DEVICE IDENTIFIERS (override production DEVICE_ID for tests)
// ============================================================================
#define TEST_DEVICE_ID_M3 "TEST_ESP32_M3_INTEGRATION"
#define TEST_DEVICE_ID_M4_INTEGRATION "ESP32_EcoWatt_Smart"  // M4 integration test device
#define TEST_DEVICE_ID_M4_CONFIG "TEST_CONFIG_ESP32"
#define TEST_DEVICE_ID_M4_CMD "TEST_CMD_ESP32"
#define TEST_DEVICE_ID_M4_SECURITY "TEST_SECURITY_ESP32"
#define TEST_DEVICE_ID_M4_OTA "TEST_OTA_ESP32"

// ============================================================================
// INVERTER SIMULATOR API CONFIGURATION (EN4440 API Documentation)
// ============================================================================
// NOTE: This is ONLY for integration tests - production uses real Modbus via acquisition.cpp

#define INVERTER_API_BASE_URL "http://20.15.114.131:8080"
#define INVERTER_API_READ_ENDPOINT "/api/inverter/read"
#define INVERTER_API_WRITE_ENDPOINT "/api/inverter/write"
#define INVERTER_API_ERROR_ENDPOINT "/api/inverter/error"
#define INVERTER_API_KEY "NjhhZWIwNDU1ZDdmMzg3MzNiMTQ5YTFmOjY4YWViMDQ1NWQ3ZjM4NzMzYjE0OWExNQ=="

// Test-specific URL builder (for simulator API)
#define MAKE_INVERTER_URL(endpoint) INVERTER_API_BASE_URL endpoint

// ============================================================================
// TEST CONFIGURATION
// ============================================================================
#define M3_TEST_SAMPLES 60  // Number of samples for M3 tests
#define M4_TEST_TIMEOUT_MS 10000
#define MAX_RETRY_ATTEMPTS 3
#define WIFI_TIMEOUT_MS 20000

// ============================================================================
// FLASK SERVER HELPERS (extracted from FLASK_SERVER_URL)
// ============================================================================
// For tests that need IP and PORT separately (like test_m4_integration)
// Note: Update these if FLASK_SERVER_URL in credentials.h changes
#define FLASK_SERVER_IP "10.116.241.2"
#define FLASK_SERVER_PORT 5001

#endif // TEST_CONFIG_H
