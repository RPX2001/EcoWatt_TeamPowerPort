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

// ============================================
// Setup Function
// ============================================
void setup() 
{
    Serial.begin(115200);
    delay(1000);
    print_init();
    
    // CRITICAL: Reconfigure task watchdog with longer timeout (30 seconds)
    // HTTP operations with retries can take 15+ seconds
    esp_task_wdt_deinit();                    // Clean slate
    esp_task_wdt_init(30, true);              // 30s timeout, panic enabled
    print("[Main] Task watchdog configured: 30s timeout\n");
    
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
        "ESP32_EcoWatt_Smart",
        FIRMWARE_VERSION
    );
    otaManager->handleRollback();
    
    // Get configuration from NVS
    uint64_t pollFreq = nvs::getPollFreq();
    uint64_t uploadFreq = nvs::getUploadFreq();
    uint64_t configCheckFreq = 5000000ULL;  // 5 seconds
    uint64_t otaCheckFreq = 60000000ULL;    // 1 minute
    
    // Override upload frequency for M2-M4 testing (15 seconds)
    print("[Main] Using 15-second upload cycle for M2-M4 testing\n");
    uploadFreq = 15000000ULL;  // 15 seconds in microseconds
    
    // Convert to milliseconds for TaskManager
    uint32_t pollFreqMs = pollFreq / 1000;
    uint32_t uploadFreqMs = uploadFreq / 1000;
    uint32_t configFreqMs = configCheckFreq / 1000;
    uint32_t otaFreqMs = otaCheckFreq / 1000;
    
    print("[Main] Task frequencies configured:\n");
    print("  - Sensor Poll:  %lu ms\n", pollFreqMs);
    print("  - Upload:       %lu ms\n", uploadFreqMs);
    print("  - Config Check: %lu ms\n", configFreqMs);
    print("  - OTA Check:    %lu ms\n", otaFreqMs);
    
    // Initialize Data Uploader (M4 format: /aggregated/<device_id>)
    DataUploader::init(FLASK_SERVER_URL "/aggregated/ESP32_001", "ESP32_001");
    
    // Initialize Command Executor (M4 format: /commands/<device_id>/poll)
    CommandExecutor::init(
        FLASK_SERVER_URL "/commands/ESP32_001/poll",
        FLASK_SERVER_URL "/commands/ESP32_001/result",
        "ESP32_001"
    );
    
    // Initialize Config Manager (M4 format: /config/<device_id>)
    ConfigManager::init(FLASK_SERVER_URL "/config/ESP32_001", "ESP32_001");
    
    // Enhance compression dictionary
    enhanceDictionaryForOptimalCompression();
    
    // ========================================
    // Initialize FreeRTOS Task Manager
    // ========================================
    print("\n[Main] Initializing FreeRTOS Task Manager...\n");
    if (!TaskManager::init(pollFreqMs, uploadFreqMs, configFreqMs, otaFreqMs)) {
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
