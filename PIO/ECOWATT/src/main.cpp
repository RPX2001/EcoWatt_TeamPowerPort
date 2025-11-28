/**
 * @file main.cpp
 * @brief EcoWatt ESP32 Main Firmware - FreeRTOS Dual-Core Version
 * @version 1.3.3
 * 
 * 
 * @author Team PowerPort
 * @date 2025-11-28
 */

#include <Arduino.h>
#include "peripheral/logger.h"

// ArduinoJson requires clean 'print' identifier
#undef print
#include "peripheral/acquisition.h"

#include "application/system_initializer.h"
#include "application/system_config.h"  // Centralized configuration constants
#include "application/task_manager.h"
#include "application/data_uploader.h"
#include "application/command_executor.h"
#include "application/config_manager.h"
#include "application/security.h"
#include "application/OTAManager.h"
#include "application/nvs.h"
#include "application/credentials.h"
#include "peripheral/arduino_wifi.h"
#include "esp_task_wdt.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>

// ============================================
// Global Objects
// ============================================
OTAManager* otaManager = nullptr;
Arduino_Wifi Wifi;

#define FIRMWARE_VERSION "1.3.3"

// ============================================
// Helper Functions
// ============================================

/**
 * @brief Enhance compression dictionary with common sensor patterns
 */
void enhanceDictionaryForOptimalCompression() {
    // Dictionary enhancement happens automatically in compression module
    LOG_INFO(LOG_TAG_BOOT, "Compression dictionary ready");;
}

/**
 * @brief Register device with Flask server
 */
bool registerDeviceWithServer() {
    LOG_INFO(LOG_TAG_BOOT, "Registering device with server...");;
    
    if (WiFi.status() != WL_CONNECTED) {
        LOG_INFO(LOG_TAG_BOOT, "WiFi not connected. Cannot register device.");;
        return false;
    }
    
    // Create WiFiClient with extended timeout
    WiFiClient client;
    client.setTimeout(10000);  // 10 second connection timeout
    
    HTTPClient http;
    http.begin(client, FLASK_SERVER_URL "/devices");
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(10000);  // 10 seconds
    
    // Create registration JSON
    StaticJsonDocument<256> doc;
    doc["device_id"] = DEVICE_ID;
    doc["device_name"] = DEVICE_NAME;
    doc["firmware_version"] = FIRMWARE_VERSION;
    doc["location"] = "Default Location";
    doc["description"] = "EcoWatt Energy Monitor";
    
    String payload;
    serializeJsonPretty(doc, payload);
    
    LOG_INFO(LOG_TAG_BOOT, "Sending registration:");
    // Print each line of the JSON with proper indentation
    int startPos = 0;
    int endPos = payload.indexOf('\n');
    while (endPos != -1) {
        LOG_INFO(LOG_TAG_BOOT, "  %s", payload.substring(startPos, endPos).c_str());
        startPos = endPos + 1;
        endPos = payload.indexOf('\n', startPos);
    }
    if (startPos < payload.length()) {
        LOG_INFO(LOG_TAG_BOOT, "  %s", payload.substring(startPos).c_str());
    }
    
    // Re-serialize compact for actual transmission
    payload = "";
    serializeJson(doc, payload);
    
    int httpCode = http.POST(payload);
    
    if (httpCode == 201) {
        LOG_SUCCESS(LOG_TAG_BOOT, "Device registered successfully");
        http.end();
        return true;
    } else if (httpCode == 200 || httpCode == 409) {
        // 200 = Updated existing device, 409 = Already registered
        LOG_SUCCESS(LOG_TAG_BOOT, "Device already registered and updated");
        http.end();
        return true;
    } else if (httpCode > 0) {
        String response = http.getString();
        
        // Try to parse and pretty-print if JSON
        StaticJsonDocument<512> responseDoc;
        DeserializationError err = deserializeJson(responseDoc, response);
        
        if (!err) {
            String prettyResponse;
            serializeJsonPretty(responseDoc, prettyResponse);
            LOG_WARN(LOG_TAG_BOOT, "Registration response (%d):", httpCode);
            int startPos = 0;
            int endPos = prettyResponse.indexOf('\n');
            while (endPos != -1) {
                LOG_WARN(LOG_TAG_BOOT, "  %s", prettyResponse.substring(startPos, endPos).c_str());
                startPos = endPos + 1;
                endPos = prettyResponse.indexOf('\n', startPos);
            }
            if (startPos < prettyResponse.length()) {
                LOG_WARN(LOG_TAG_BOOT, "  %s", prettyResponse.substring(startPos).c_str());
            }
        } else {
            LOG_WARN(LOG_TAG_BOOT, "Registration response (%d): %s", httpCode, response.c_str());
        }
        
        http.end();
        return false;
    } else {
        LOG_ERROR(LOG_TAG_BOOT, "Registration failed: %s", http.errorToString(httpCode).c_str());
        http.end();
        return false;
    }
}

// ============================================
// Setup Function
// ============================================
void setup() 
{
    Serial.begin(115200);
    delay(1000);
    
    // STEP 1: Initialize WiFi FIRST (before logger so timestamps work)
    Wifi.begin();
    
    // STEP 2: Initialize logger (will now use NTP time if WiFi connected)
    // Log level controlled by g_logLevel in logger.cpp - change there for DEBUG/INFO/etc.
    initLogger();  // Uses g_logLevel from logger.cpp
    
    // CRITICAL: Reconfigure task watchdog with longer timeout
    // Uses centralized HARDWARE_WATCHDOG_TIMEOUT_S from system_config.h
    esp_task_wdt_deinit();                              // Clean slate
    esp_task_wdt_init(HARDWARE_WATCHDOG_TIMEOUT_S, true); // Configurable timeout, panic enabled
    LOG_INFO(LOG_TAG_BOOT, "Task watchdog configured: %d seconds timeout\n", HARDWARE_WATCHDOG_TIMEOUT_S);
    
    LOG_INFO(LOG_TAG_BOOT, "");;
    LOG_INFO(LOG_TAG_BOOT, "╔══════════════════════════════════════════════════════════╗");;
    LOG_INFO(LOG_TAG_BOOT, "║  EcoWatt ESP32 FreeRTOS System v3.0 - Dual-Core Edition ║");;
    LOG_INFO(LOG_TAG_BOOT, "╚══════════════════════════════════════════════════════════╝");;
    LOG_INFO(LOG_TAG_BOOT, "");;
    
    // STEP 3: Initialize remaining system components
    LOG_INFO(LOG_TAG_BOOT, "Initializing system components...");
    SystemInitializer::initializeAll();
    
    // Initialize OTA Manager  
    LOG_INFO(LOG_TAG_BOOT, "Initializing OTA Manager...");
    otaManager = new OTAManager(
        FLASK_SERVER_URL,  // Already includes port, don't duplicate
        DEVICE_ID,
        FIRMWARE_VERSION
    );
    
    // Handle rollback if new firmware failed
    otaManager->handleRollback();
    
    // Run post-OTA diagnostics to verify system health
    LOG_INFO(LOG_TAG_BOOT, "Running post-boot diagnostics...");;
    bool diagnosticsPassed = otaManager->runDiagnostics();
    
    if (diagnosticsPassed) {
        LOG_INFO(LOG_TAG_BOOT, "✓ Diagnostics passed - firmware stable");;
        
        // Register device with server (auto-registration)
        LOG_INFO(LOG_TAG_BOOT, "Attempting device auto-registration...");;
        if (registerDeviceWithServer()) {
            LOG_INFO(LOG_TAG_BOOT, "✓ Device registration complete");;
        } else {
            LOG_INFO(LOG_TAG_BOOT, "⚠ Device registration failed (will retry later)");;
        }
        
        // Report OTA completion status to Flask server
        LOG_INFO(LOG_TAG_BOOT, "Reporting OTA status to server...");;
        if (otaManager->reportOTACompletionStatus()) {
            LOG_INFO(LOG_TAG_BOOT, "✓ OTA status reported successfully");;
        } else {
            LOG_INFO(LOG_TAG_BOOT, "⚠ Failed to report OTA status (will retry later)");;
        }
    } else {
        LOG_INFO(LOG_TAG_BOOT, "✗ Diagnostics failed - system may be unstable");;
    }
    
    // Get configuration from NVS (centralized configuration source)
    uint64_t pollFreq = nvs::getPollFreq();
    uint64_t uploadFreq = nvs::getUploadFreq();
    uint64_t configCheckFreq = nvs::getConfigFreq();     // From NVS with DEFAULT_CONFIG_FREQUENCY_US fallback
    uint64_t commandPollFreq = nvs::getCommandFreq();    // From NVS with DEFAULT_COMMAND_FREQUENCY_US fallback
    uint64_t otaCheckFreq = nvs::getOtaFreq();           // From NVS with DEFAULT_OTA_FREQUENCY_US fallback
    
    // Convert to milliseconds for TaskManager
    uint32_t pollFreqMs = pollFreq / 1000;
    uint32_t uploadFreqMs = uploadFreq / 1000;
    uint32_t configFreqMs = configCheckFreq / 1000;
    uint32_t commandFreqMs = commandPollFreq / 1000;
    uint32_t otaFreqMs = otaCheckFreq / 1000;
    
    LOG_INFO(LOG_TAG_BOOT, "Task frequencies configured from NVS:");;
    LOG_INFO(LOG_TAG_BOOT, "  - Sensor Poll:  %lu ms (configurable via NVS)", pollFreqMs);
    LOG_INFO(LOG_TAG_BOOT, "  - Upload:       %lu ms (configurable via NVS)", uploadFreqMs);
    LOG_INFO(LOG_TAG_BOOT, "  - Config Check: %lu ms (configurable via NVS)", configFreqMs);
    LOG_INFO(LOG_TAG_BOOT, "  - Command Poll: %lu ms (configurable via NVS)", commandFreqMs);
    LOG_INFO(LOG_TAG_BOOT, "  - OTA Check:    %lu ms (configurable via NVS)", otaFreqMs);

    uint64_t energyPollFreq = nvs::getEnergyPollFreq();
    uint32_t energyPollMs = energyPollFreq / 1000;
    LOG_INFO(LOG_TAG_BOOT, "  - Energy Poll:  %lu ms (%.1f s, configurable via NVS)\n", 
          energyPollMs, energyPollMs / 1000.0);
    
    // Initialize Data Uploader (/aggregated/<device_id>)
    DataUploader::init(FLASK_SERVER_URL "/aggregated/" DEVICE_ID, DEVICE_ID);
    
    // Initialize Command Executor (/commands/<device_id>/poll)
    CommandExecutor::init(
        FLASK_SERVER_URL "/commands/" DEVICE_ID "/poll",
        FLASK_SERVER_URL "/commands/" DEVICE_ID "/result",
        DEVICE_ID
    );
    
    // Initialize Config Manager (/config/<device_id>)
    ConfigManager::init(FLASK_SERVER_URL "/config/" DEVICE_ID, DEVICE_ID);
    
    // Send current config to server so frontend can display it
    LOG_INFO(LOG_TAG_BOOT, "Reporting current configuration to server...");;
    ConfigManager::sendCurrentConfig();
    
    // Enhance compression dictionary
    enhanceDictionaryForOptimalCompression();
    
    // ========================================
    // Initialize FreeRTOS Task Manager
    // ========================================
    LOG_INFO(LOG_TAG_BOOT, "Initializing FreeRTOS Task Manager...");;
    if (!TaskManager::init(pollFreqMs, uploadFreqMs, configFreqMs, commandFreqMs, otaFreqMs)) {
        LOG_INFO(LOG_TAG_BOOT, "ERROR: Failed to initialize TaskManager!");;
        LOG_INFO(LOG_TAG_BOOT, "System halted.");;
        while(1) {
            delay(1000);
        }
    }
    
    // Start all FreeRTOS tasks
    LOG_INFO(LOG_TAG_BOOT, "Starting FreeRTOS tasks on both cores...");;
    TaskManager::startAllTasks(otaManager);
    
    LOG_INFO(LOG_TAG_BOOT, "");;
    LOG_INFO(LOG_TAG_BOOT, "╔══════════════════════════════════════════════════════════╗");;
    LOG_INFO(LOG_TAG_BOOT, "║            FreeRTOS System Initialization Complete       ║");;
    LOG_INFO(LOG_TAG_BOOT, "║                                                          ║");;
    LOG_INFO(LOG_TAG_BOOT, "║  Core 0 (PRO_CPU):  Upload, Commands, Config, OTA       ║");;
    LOG_INFO(LOG_TAG_BOOT, "║  Core 1 (APP_CPU):  Sensors, Compression, Watchdog      ║");;
    LOG_INFO(LOG_TAG_BOOT, "║                                                          ║");;
    LOG_INFO(LOG_TAG_BOOT, "║  Real-time scheduling active with deadline guarantees   ║");;
    LOG_INFO(LOG_TAG_BOOT, "╚══════════════════════════════════════════════════════════╝");;
    LOG_INFO(LOG_TAG_BOOT, "");;
}

// ============================================
// Main Loop Function - EMPTY (FreeRTOS runs everything)
// ============================================
void loop()
{
    // FreeRTOS tasks handle all processing
    // Loop does nothing except yield to scheduler
    
    // Optional: Print health report every 10 minutes
    static unsigned long lastHealthPrint = 0;
    unsigned long now = millis();
    
    if (now - lastHealthPrint > 600000) {  // 10 minutes
        TaskManager::printSystemHealth();
        lastHealthPrint = now;
    }
    
    delay(1000);  // Sleep 1 second
}
