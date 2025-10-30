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
#include <time.h>

// Include peripheral/print.h AFTER OTAManager.h
// Both define print() macro, but they both rely on debug.log() so it's fine
#include "peripheral/print.h"

// Initialize static members
bool SystemInitializer::initialized = false;

// External WiFi object (defined in main.cpp)
extern Arduino_Wifi Wifi;

bool SystemInitializer::initializeAll() {
    Serial.begin(115200);
    delay(1000);
    
    printBootSequence();
    
    // Step 1: Print system
    Serial.println("[BOOT] Step 1: Initializing print system...");
    print_init();
    Serial.println("[BOOT] Step 2: Print system initialized");
    print("Starting ECOWATT System Initialization\n");

    // Step 2: WiFi
    Serial.println("[BOOT] Step 3: Starting WiFi initialization...");
    if (!initWiFi()) {
        Serial.println("[BOOT] ERROR: WiFi initialization failed");
        return false;
    }
    Serial.println("[BOOT] Step 4: WiFi initialized successfully");

    // Step 3: Power Management
    Serial.println("[BOOT] Step 5: Initializing Power Management...");
    if (!initPowerManagement()) {
        Serial.println("[BOOT] ERROR: Power Management initialization failed");
        return false;
    }
    Serial.println("[BOOT] Step 6: Power Management initialized");

    // Step 4: Security Layer
    Serial.println("[BOOT] Step 7: Initializing Security Layer...");
    if (!initSecurity()) {
        Serial.println("[BOOT] ERROR: Security initialization failed");
        return false;
    }
    Serial.println("[BOOT] Step 8: Security Layer initialized");

    // Step 5: Fault Recovery (Milestone 5)
    Serial.println("[BOOT] Step 9: Initializing Fault Recovery Module...");
    initFaultRecovery();
    Serial.println("[BOOT] Step 10: Fault Recovery initialized");

    initialized = true;
    Serial.println("[BOOT] ✓ All core systems initialized successfully");
    
    return true;
}

bool SystemInitializer::initWiFi() {
    Wifi.begin();
    
    // Check if WiFi is actually connected
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("WiFi: CONNECTED ✓");
        Serial.printf("  IP: %s\n", WiFi.localIP().toString().c_str());
        
        // Initialize NTP time synchronization
        Serial.println("Initializing NTP time sync...");
        configTime(0, 0, "pool.ntp.org", "time.nist.gov");
        setenv("TZ", "UTC", 1);  // Set timezone to UTC
        tzset();
        
        // Wait for time to be set (up to 10 seconds)
        int retry = 0;
        const int retry_count = 10;
        struct tm timeinfo;
        
        // Temporarily undefine print macro to use Serial.print()
        #ifdef print
        #undef print
        #endif
        
        while (!getLocalTime(&timeinfo) && retry < retry_count) {
            Serial.print(".");
            delay(1000);
            retry++;
        }
        
        if (retry < retry_count) {
            Serial.println("NTP Time Sync: OK ✓");
            Serial.printf("  Current time: %04d-%02d-%02d %02d:%02d:%02d UTC\n",
                  timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                  timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        } else {
            Serial.println("NTP Time Sync: TIMEOUT (will use millis fallback)");
        }
        
        // Redefine print macro
        #define print(...) debug.log(__VA_ARGS__)
        
        return true;
    } else {
        Serial.println("WiFi: FAILED ✗");
        Serial.println("  System will continue but network features disabled");
        return false;  // Return false to indicate WiFi failure
    }
}

bool SystemInitializer::initPowerManagement() {
    PowerManagement::init();
    PeripheralPower::init();
    print("Power Management: OK\n");
    print("Peripheral Power Gating: OK\n");
    return true;
}

bool SystemInitializer::initSecurity() {
    print("Initializing Security Layer...\n");
    SecurityLayer::init();
    print("Security Layer: OK\n");
    return true;
}

bool SystemInitializer::initOTA(const char* serverURL, const char* deviceID, 
                                const char* version) {
    print("Initializing OTA Manager...\n");
    print("  Server: %s\n", serverURL);
    print("  Device: %s\n", deviceID);
    print("  Version: %s\n", version);
    
    // OTA Manager is created externally in main.cpp
    // This function just logs the initialization
    
    print("OTA Manager: OK\n");
    return true;
}

void SystemInitializer::printBootSequence() {
    Serial.println("\n\n===========================================");
    Serial.println("ESP32 BOOTING - EcoWatt System");
    Serial.println("Team: PowerPort");
    Serial.println("===========================================\n");
}

bool SystemInitializer::isInitialized() {
    return initialized;
}
