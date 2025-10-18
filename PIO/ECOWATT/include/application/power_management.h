/**
 * @file power_management.h
 * @brief Power management module for ESP32 EcoWatt device
 * 
 * Implements power-saving mechanisms including:
 * - CPU frequency scaling (240MHz active, 80MHz idle)
 * - Light sleep between operations
 * - Power consumption monitoring
 * 
 * @version 1.0.0
 * @date 2025-10-18
 */

#ifndef POWER_MANAGEMENT_H
#define POWER_MANAGEMENT_H

#include <Arduino.h>
#include <esp_pm.h>
#include <esp_sleep.h>
#include <driver/rtc_io.h>

/**
 * @brief Power mode enumeration
 */
enum PowerMode {
    POWER_HIGH_PERFORMANCE = 0,  // 240 MHz - During critical operations
    POWER_NORMAL,                // 160 MHz - Normal operations
    POWER_LOW,                   // 160 MHz - Idle/waiting (WiFi-safe minimum)
    POWER_SLEEP                  // Light sleep mode
};

/**
 * @brief Power statistics structure
 */
struct PowerStats {
    uint32_t high_perf_time_ms;      // Time spent in high performance mode
    uint32_t normal_time_ms;         // Time spent in normal mode
    uint32_t low_power_time_ms;      // Time spent in low power mode
    uint32_t sleep_time_ms;          // Time spent in sleep mode
    uint32_t total_time_ms;          // Total uptime
    
    uint32_t sleep_cycles;           // Number of sleep cycles
    uint32_t freq_changes;           // Number of frequency changes
    
    float avg_current_ma;            // Average current consumption (estimated)
    float energy_saved_mah;          // Estimated energy saved
};

/**
 * @brief Power Management Class
 */
class PowerManagement {
public:
    /**
     * @brief Initialize power management system
     */
    static void init();
    
    /**
     * @brief Set CPU frequency based on power mode
     * @param mode Power mode to set
     */
    static void setCPUFrequency(PowerMode mode);
    
    /**
     * @brief Enter light sleep for specified duration
     * @param duration_ms Sleep duration in milliseconds
     * @return true if sleep was successful, false otherwise
     */
    static bool lightSleep(uint32_t duration_ms);
    
    /**
     * @brief Get current CPU frequency
     * @return Current CPU frequency in MHz
     */
    static uint32_t getCurrentFrequency();
    
    /**
     * @brief Get current power mode
     * @return Current power mode
     */
    static PowerMode getCurrentMode();
    
    /**
     * @brief Update power statistics
     */
    static void updateStats();
    
    /**
     * @brief Get power statistics
     * @return Power statistics structure
     */
    static PowerStats getStats();
    
    /**
     * @brief Print power statistics to serial
     */
    static void printStats();
    
    /**
     * @brief Calculate estimated current consumption
     * @param frequency CPU frequency in MHz
     * @return Estimated current in mA
     */
    static float estimateCurrent(uint32_t frequency);
    
    /**
     * @brief Reset power statistics
     */
    static void resetStats();
    
    /**
     * @brief Enable/disable automatic power management
     * @param enable true to enable, false to disable
     */
    static void enableAutoPowerManagement(bool enable);
    
    /**
     * @brief Check if auto power management is enabled
     * @return true if enabled, false otherwise
     */
    static bool isAutoPowerManagementEnabled();

private:
    static PowerMode currentMode;
    static PowerStats stats;
    static uint32_t lastUpdateTime;
    static bool autoPowerManagement;
    static uint32_t currentFrequency;
    
    /**
     * @brief Update time spent in current mode
     */
    static void recordModeTime();
};

// Convenience macros for power management
#define POWER_ENTER_HIGH_PERF()   PowerManagement::setCPUFrequency(POWER_HIGH_PERFORMANCE)
#define POWER_ENTER_NORMAL()      PowerManagement::setCPUFrequency(POWER_NORMAL)
#define POWER_ENTER_LOW()         PowerManagement::setCPUFrequency(POWER_LOW)
#define POWER_SLEEP(ms)           PowerManagement::lightSleep(ms)

#endif // POWER_MANAGEMENT_H
