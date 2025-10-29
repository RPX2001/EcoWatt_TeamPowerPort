/**
 * @file main_freertos.cpp
 * @brief EcoWatt ESP32 Main Firmware - FreeRTOS Dual-Core Version
 * @version 3.0.0
 * 
 * @note Migrated to FreeRTOS dual-core architecture for guaranteed real-time deadlines
 * 
 * @author Team PowerPort
 * @date 2025-10-28
 */

#include <Arduino.h>
#include "peripheral/print.h"

// ArduinoJson requires clean 'print' identifier
#undef print
#include "peripheral/acquisition.h"
#define print(...) debug.log(__VA_ARGS__)

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

#define FIRMWARE_VERSION "1.0.4"

// ============================================
// Helper Functions
// ============================================

/**
 * @brief Enhance compression dictionary with common sensor patterns
 */
void enhanceDictionaryForOptimalCompression() {
    // Dictionary enhancement happens automatically in compression module
    print("[Main] Compression dictionary ready\n");
}

/**
 * @brief Register device with Flask server
 */
bool registerDeviceWithServer() {
    print("[Main] Registering device with server...\n");
    
    if (WiFi.status() != WL_CONNECTED) {
        print("[Main] WiFi not connected. Cannot register device.\n");
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
    serializeJson(doc, payload);
    
    print("[Main] Sending registration: %s\n", payload.c_str());
    
    int httpCode = http.POST(payload);
    
    if (httpCode == 201) {
        print("[Main] ✓ Device registered successfully\n");
        http.end();
        return true;
    } else if (httpCode == 409) {
        print("[Main] ✓ Device already registered\n");
        http.end();
        return true;
    } else if (httpCode > 0) {
        String response = http.getString();
        print("[Main] ⚠ Registration response (%d): %s\n", httpCode, response.c_str());
        http.end();
        return false;
    } else {
        print("[Main] ✗ Registration failed: %s\n", http.errorToString(httpCode).c_str());
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
    print_init();
    
    // CRITICAL: Reconfigure task watchdog with longer timeout
    // Uses centralized HARDWARE_WATCHDOG_TIMEOUT_S from system_config.h
    esp_task_wdt_deinit();                              // Clean slate
    esp_task_wdt_init(HARDWARE_WATCHDOG_TIMEOUT_S, true); // Configurable timeout, panic enabled
    print("[Main] Task watchdog configured: %d seconds timeout\n", HARDWARE_WATCHDOG_TIMEOUT_S);
    
    print("\n");
    print("╔══════════════════════════════════════════════════════════╗\n");
    print("║  EcoWatt ESP32 FreeRTOS System v3.0 - Dual-Core Edition ║\n");
    print("╚══════════════════════════════════════════════════════════╝\n");
    print("\n");
    
    // Initialize all system components
    print("[Main] Initializing system components...\n");
    SystemInitializer::initializeAll();
    
    // Initialize OTA Manager  
    print("[Main] Initializing OTA Manager...\n");
    otaManager = new OTAManager(
        FLASK_SERVER_URL ":5001",
        DEVICE_ID,
        FIRMWARE_VERSION
    );
    
    // Handle rollback if new firmware failed
    otaManager->handleRollback();
    
    // Run post-OTA diagnostics to verify system health
    print("[Main] Running post-boot diagnostics...\n");
    bool diagnosticsPassed = otaManager->runDiagnostics();
    
    if (diagnosticsPassed) {
        print("[Main] ✓ Diagnostics passed - firmware stable\n");
        
        // Register device with server (auto-registration)
        print("[Main] Attempting device auto-registration...\n");
        if (registerDeviceWithServer()) {
            print("[Main] ✓ Device registration complete\n");
        } else {
            print("[Main] ⚠ Device registration failed (will retry later)\n");
        }
        
        // Report OTA completion status to Flask server
        print("[Main] Reporting OTA status to server...\n");
        if (otaManager->reportOTACompletionStatus()) {
            print("[Main] ✓ OTA status reported successfully\n");
        } else {
            print("[Main] ⚠ Failed to report OTA status (will retry later)\n");
        }
    } else {
        print("[Main] ✗ Diagnostics failed - system may be unstable\n");
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
    
    print("[Main] Task frequencies configured from NVS:\n");
    print("  - Sensor Poll:  %lu ms (configurable via NVS)\n", pollFreqMs);
    print("  - Upload:       %lu ms (configurable via NVS)\n", uploadFreqMs);
    print("  - Config Check: %lu ms (configurable via NVS)\n", configFreqMs);
    print("  - Command Poll: %lu ms (configurable via NVS)\n", commandFreqMs);
    print("  - OTA Check:    %lu ms (configurable via NVS)\n", otaFreqMs);
    
    // Initialize Data Uploader (M4 format: /aggregated/<device_id>)
    DataUploader::init(FLASK_SERVER_URL "/aggregated/" DEVICE_ID, DEVICE_ID);
    
    // Initialize Command Executor (M4 format: /commands/<device_id>/poll)
    CommandExecutor::init(
        FLASK_SERVER_URL "/commands/" DEVICE_ID "/poll",
        FLASK_SERVER_URL "/commands/" DEVICE_ID "/result",
        DEVICE_ID
    );
    
    // Initialize Config Manager (M4 format: /config/<device_id>)
    ConfigManager::init(FLASK_SERVER_URL "/config/" DEVICE_ID, DEVICE_ID);
    
    // Send current config to server so frontend can display it
    print("[Main] Reporting current configuration to server...\n");
    ConfigManager::sendCurrentConfig();
    
    // Enhance compression dictionary
    enhanceDictionaryForOptimalCompression();
    
    // ========================================
    // Initialize FreeRTOS Task Manager
    // ========================================
    print("\n[Main] Initializing FreeRTOS Task Manager...\n");
    if (!TaskManager::init(pollFreqMs, uploadFreqMs, configFreqMs, commandFreqMs, otaFreqMs)) {
        print("[Main] ERROR: Failed to initialize TaskManager!\n");
        print("[Main] System halted.\n");
        while(1) {
            delay(1000);
        }
    }
    
    // Start all FreeRTOS tasks
    print("[Main] Starting FreeRTOS tasks on both cores...\n");
    TaskManager::startAllTasks(otaManager);
    
    print("\n");
    print("╔══════════════════════════════════════════════════════════╗\n");
    print("║            FreeRTOS System Initialization Complete       ║\n");
    print("║                                                          ║\n");
    print("║  Core 0 (PRO_CPU):  Upload, Commands, Config, OTA       ║\n");
    print("║  Core 1 (APP_CPU):  Sensors, Compression, Watchdog      ║\n");
    print("║                                                          ║\n");
    print("║  Real-time scheduling active with deadline guarantees   ║\n");
    print("╚══════════════════════════════════════════════════════════╝\n");
    print("\n");
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
