/**
 * @file system_initializer.h
 * @brief System initialization and setup orchestration
 * 
 * This module manages the boot sequence and initialization of all
 * system components in the correct order.
 * 
 * @author Team PowerPort
 * @date 2025-10-22
 */

#ifndef SYSTEM_INITIALIZER_H
#define SYSTEM_INITIALIZER_H

#include <Arduino.h>

/**
 * @class SystemInitializer
 * @brief Manages system initialization sequence
 * 
 * This class provides a singleton-style interface for initializing
 * all system components during boot.
 */
class SystemInitializer {
public:
    /**
     * @brief Initialize all system components
     * 
     * Performs complete system initialization including Serial, WiFi,
     * Power Management, Security, OTA, and all application modules.
     * 
     * @return true if all initialization successful
     */
    static bool initializeAll();

    /**
     * @brief Initialize WiFi connection
     * 
     * @return true if WiFi connected successfully
     */
    static bool initWiFi();

    /**
     * @brief Initialize Power Management
     * 
     * @return true if initialization successful
     */
    static bool initPowerManagement();

    /**
     * @brief Initialize Security Layer
     * 
     * @return true if initialization successful
     */
    static bool initSecurity();

    /**
     * @brief Initialize OTA Manager
     * 
     * @param serverURL Flask server URL
     * @param deviceID Device identifier
     * @param version Current firmware version
     * @return true if initialization successful
     */
    static bool initOTA(const char* serverURL, const char* deviceID, const char* version);

    /**
     * @brief Print boot sequence information
     */
    static void printBootSequence();

    /**
     * @brief Check if system is fully initialized
     * 
     * @return true if all components initialized
     */
    static bool isInitialized();

private:
    static bool initialized;

    // Prevent instantiation
    SystemInitializer() = delete;
    ~SystemInitializer() = delete;
    SystemInitializer(const SystemInitializer&) = delete;
    SystemInitializer& operator=(const SystemInitializer&) = delete;
};

#endif // SYSTEM_INITIALIZER_H
