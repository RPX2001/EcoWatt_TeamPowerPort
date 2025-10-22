/**
 * @file config_manager.h
 * @brief Configuration management and change detection
 * 
 * This module handles dynamic configuration updates from the server
 * including register selection, polling frequency, and upload frequency.
 * 
 * @author Team PowerPort
 * @date 2025-10-22
 */

#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include "peripheral/acquisition.h"

/**
 * @struct SystemConfig
 * @brief Holds current system configuration
 */
struct SystemConfig {
    const RegID* registers;     ///< Active register selection
    size_t registerCount;       ///< Number of registers
    uint64_t pollFrequency;     ///< Polling frequency in microseconds
    uint64_t uploadFrequency;   ///< Upload frequency in microseconds
};

/**
 * @class ConfigManager
 * @brief Manages dynamic system configuration
 * 
 * This class provides a singleton-style interface for checking and
 * applying configuration changes from the cloud server.
 */
class ConfigManager {
public:
    /**
     * @brief Initialize the configuration manager
     * 
     * @param changesURL URL endpoint for checking configuration changes
     * @param deviceID Unique device identifier
     */
    static void init(const char* changesURL, const char* deviceID);

    /**
     * @brief Check for configuration changes from server
     * 
     * @param registersChanged Output: true if register selection changed
     * @param pollChanged Output: true if polling frequency changed
     * @param uploadChanged Output: true if upload frequency changed
     */
    static void checkForChanges(bool* registersChanged, bool* pollChanged, bool* uploadChanged);

    /**
     * @brief Apply pending register selection changes
     * 
     * @param newSelection Output: pointer to new register array
     * @param newCount Output: number of registers in new selection
     * @return true if changes were applied
     */
    static bool applyRegisterChanges(const RegID** newSelection, size_t* newCount);

    /**
     * @brief Apply pending polling frequency changes
     * 
     * @param newFreq Output: new polling frequency in microseconds
     * @return true if changes were applied
     */
    static bool applyPollFrequencyChange(uint64_t* newFreq);

    /**
     * @brief Apply pending upload frequency changes
     * 
     * @param newFreq Output: new upload frequency in microseconds
     * @return true if changes were applied
     */
    static bool applyUploadFrequencyChange(uint64_t* newFreq);

    /**
     * @brief Get current system configuration
     * 
     * @return Reference to current config
     */
    static const SystemConfig& getCurrentConfig();

    /**
     * @brief Update the current configuration
     * 
     * Used after applying changes to keep config in sync
     * 
     * @param newRegs New register selection
     * @param newRegCount Number of new registers
     * @param newPollFreq New polling frequency
     * @param newUploadFreq New upload frequency
     */
    static void updateCurrentConfig(const RegID* newRegs, size_t newRegCount,
                                    uint64_t newPollFreq, uint64_t newUploadFreq);

    /**
     * @brief Print current configuration to serial
     */
    static void printCurrentConfig();

private:
    static char changesURL[256];
    static char deviceID[64];
    static SystemConfig currentConfig;

    // Prevent instantiation
    ConfigManager() = delete;
    ~ConfigManager() = delete;
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;
};

#endif // CONFIG_MANAGER_H
