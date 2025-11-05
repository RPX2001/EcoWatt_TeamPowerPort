#ifndef CREDENTIALS_H
#define CREDENTIALS_H

/**
 * @file credentials.h
 * @brief Production credentials for WiFi, server, and device identity
 * 
 * NOTE: This file contains PRODUCTION configuration.
 * For test-specific configuration, see config/test_config.h
 * 
 * @warning Update these values before deployment!
 */

#warning Check WiFi and server credentials in credentials.h before compiling!

// ============================================
// PRODUCTION WIFI CREDENTIALS
// ============================================
#define WIFI_SSID "Galaxy A32B46A"
#define WIFI_PASSWORD "aubz5724"

// ============================================
// PRODUCTION FLASK SERVER
// ============================================
#define FLASK_SERVER_URL "http://192.168.31.249:5001"

// ============================================
// PRODUCTION DEVICE IDENTIFICATION
// ============================================
// UNIFORM across all modules - single source of truth
#define DEVICE_ID "ESP32_001"
#define DEVICE_NAME "EcoWatt Smart Monitor"

// ============================================
// URL HELPER MACROS
// ============================================

// Base URL builder
#define MAKE_FLASK_URL(endpoint) FLASK_SERVER_URL endpoint

// Device-specific Flask URLs (used by production modules)
#define FLASK_COMMAND_POLL_URL(device_id) FLASK_SERVER_URL "/commands/" device_id "/poll"
#define FLASK_COMMAND_RESULT_URL(device_id) FLASK_SERVER_URL "/commands/" device_id "/result"
#define FLASK_CONFIG_CHECK_URL(device_id) FLASK_SERVER_URL "/config/" device_id "/check"
#define FLASK_AGGREGATED_URL(device_id) FLASK_SERVER_URL "/aggregated/" device_id

#endif // CREDENTIALS_H