/**
 * @file system_initializer.cpp
 * @brief Implementation of system initialization
 * 
 * @author Team PowerPort
 * @date 2025-10-22
 */

#include "application/system_initializer.h"
#include "peripheral/arduino_wifi.h"
#include "application/power_management.h"
#include "application/peripheral_power.h"
#include "application/security.h"
#include "application/OTAManager.h"
#include "application/fault_recovery.h" // Milestone 5
#include "peripheral/logger.h"
#include <time.h>

// Initialize static members
bool SystemInitializer::initialized = false;

// External WiFi object (defined in main.cpp)
extern Arduino_Wifi Wifi;

bool SystemInitializer::initializeAll() {
    // Note: WiFi and Logger already initialized in main.cpp setup()
    
    printBootSequence();
    
    // Step 1: Sync NTP time (WiFi already connected from main.cpp)
    LOG_INFO(LOG_TAG_BOOT, "Syncing NTP time...");
    syncNTPTime();
    LOG_SUCCESS(LOG_TAG_BOOT, "WiFi and NTP initialized");

    // Step 2: Power Management
    LOG_INFO(LOG_TAG_BOOT, "Initializing Power Management...");
    if (!initPowerManagement()) {
        LOG_ERROR(LOG_TAG_BOOT, "Power Management initialization failed");
        return false;
    }
    LOG_SUCCESS(LOG_TAG_BOOT, "Power Management initialized");

    // Step 3: Security Layer
    LOG_INFO(LOG_TAG_BOOT, "Initializing Security Layer...");
    if (!initSecurity()) {
        LOG_ERROR(LOG_TAG_BOOT, "Security initialization failed");
        return false;
    }
    LOG_SUCCESS(LOG_TAG_BOOT, "Security Layer initialized");

    // Step 4: Fault Recovery (Milestone 5)
    LOG_INFO(LOG_TAG_BOOT, "Initializing Fault Recovery...");
    initFaultRecovery();
    LOG_SUCCESS(LOG_TAG_BOOT, "Fault Recovery initialized");

    initialized = true;
    LOG_SECTION("SYSTEM INITIALIZATION COMPLETE");
    LOG_SUCCESS(LOG_TAG_BOOT, "All core systems ready");
    
    return true;
}

// Separate function for NTP sync (WiFi already connected)
bool SystemInitializer::syncNTPTime() {
    if (WiFi.status() != WL_CONNECTED) {
        LOG_WARN(LOG_TAG_WIFI, "WiFi not connected - skipping NTP sync");
        return false;
    }
    
    // Initialize NTP time synchronization with Sri Lankan timezone (UTC+5:30)
    configTime(19800, 0, "pool.ntp.org", "time.nist.gov");  // 19800 = 5.5 hours * 3600 seconds
    setenv("TZ", "IST-5:30", 1);  // Set timezone to Sri Lankan Time (UTC+5:30)
    tzset();
    
    // Wait for time to be set (up to 10 seconds)
    int retry = 0;
    const int retry_count = 10;
    struct tm timeinfo;
    
    while (!getLocalTime(&timeinfo) && retry < retry_count) {
        delay(1000);
        retry++;
    }
    
    if (retry < retry_count) {
        LOG_SUCCESS(LOG_TAG_WIFI, "NTP time synchronized");
        char timeStr[64];
        strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
        LOG_INFO(LOG_TAG_WIFI, "Time: %s Sri Lankan Time (UTC+5:30)", timeStr);
        return true;
    } else {
        LOG_WARN(LOG_TAG_WIFI, "NTP sync timeout - using millis() fallback");
        return false;
    }
}

bool SystemInitializer::initPowerManagement() {
    PowerManagement::init();
    PeripheralPower::init();
    LOG_SUCCESS(LOG_TAG_POWER, "Power management initialized");
    LOG_SUCCESS(LOG_TAG_POWER, "Peripheral power gating enabled");
    return true;
}

bool SystemInitializer::initSecurity() {
    SecurityLayer::init();
    LOG_SUCCESS(LOG_TAG_SECURITY, "Security layer initialized");
    return true;
}

bool SystemInitializer::initOTA(const char* serverURL, const char* deviceID, 
                                const char* version) {
    LOG_INFO(LOG_TAG_FOTA, "Initializing OTA Manager");
    LOG_DEBUG(LOG_TAG_FOTA, "Server: %s", serverURL);
    LOG_DEBUG(LOG_TAG_FOTA, "Device: %s", deviceID);
    LOG_DEBUG(LOG_TAG_FOTA, "Version: %s", version);
    
    // OTA Manager is created externally in main.cpp
    // This function just logs the initialization
    
    LOG_SUCCESS(LOG_TAG_FOTA, "OTA Manager ready");
    return true;
}

void SystemInitializer::printBootSequence() {
    LOG_INFO(LOG_TAG_BOOT, "");
    LOG_INFO(LOG_TAG_BOOT, "╔════════════════════════════════════════════════════════════╗");
    LOG_INFO(LOG_TAG_BOOT, "║              ESP32 EcoWatt System Boot                     ║");
    LOG_INFO(LOG_TAG_BOOT, "║                  Team PowerPort                            ║");
    LOG_INFO(LOG_TAG_BOOT, "╚════════════════════════════════════════════════════════════╝");
    LOG_INFO(LOG_TAG_BOOT, "");
}

bool SystemInitializer::isInitialized() {
    return initialized;
}
