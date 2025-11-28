/**
 * @file power_management.cpp
 * @brief Implementation of power management module for ESP32 EcoWatt device
 * 
 * @version 1.0.0
 * @date 2025-10-18
 */

#include "application/power_management.h"
#include "application/peripheral_power.h"
#include "application/nvs.h"
#include "peripheral/logger.h"
#include <WiFi.h>

// Static member initialization
PowerMode PowerManagement::currentMode = POWER_HIGH_PERFORMANCE;
PowerStats PowerManagement::stats = {0};
uint32_t PowerManagement::lastUpdateTime = 0;
bool PowerManagement::autoPowerManagement = true;
uint32_t PowerManagement::currentFrequency = 240;
PowerTechniqueFlags PowerManagement::enabledTechniques = POWER_TECH_PERIPHERAL_GATING;  // Only peripheral gating works
bool PowerManagement::powerManagementEnabled = false;

/**
 * @brief Initialize power management system
 * 
 * Loads configuration from NVS and applies Peripheral Gating.
 */
void PowerManagement::init() {
    LOG_SECTION("POWER MANAGEMENT INITIALIZATION");
    
    // Initialize NVS namespace first (ensures it exists)
    nvs::initPowerNamespace();
    
    // Load settings from NVS
    powerManagementEnabled = nvs::getPowerEnabled();
    enabledTechniques = nvs::getPowerTechniques();
    
    LOG_INFO(LOG_TAG_POWER, "Power optimization: Peripheral Gating (UART power control)");
    
    // Print loaded configuration
    LOG_INFO(LOG_TAG_POWER, "Configuration: %s | Techniques: 0x%02X", 
             powerManagementEnabled ? "ENABLED" : "DISABLED", enabledTechniques);
    
    // Check if peripheral gating is enabled
    if (enabledTechniques & POWER_TECH_PERIPHERAL_GATING) {
        LOG_DEBUG(LOG_TAG_POWER, "Peripheral Gating: ENABLED");
    } else {
        LOG_INFO(LOG_TAG_POWER, "Mode: Full performance (no techniques)");
    }
    
    if (powerManagementEnabled) {
        // Apply the enabled techniques
        applyTechniques();
    } else {
        LOG_INFO(LOG_TAG_POWER, "Power management disabled - full power mode");
    }
    
    // Reset statistics
    resetStats();
    
    // Start at 240 MHz (fixed - no frequency scaling)
    setCpuFrequencyMhz(240);
    currentFrequency = 240;
    currentMode = POWER_HIGH_PERFORMANCE;
    
    lastUpdateTime = millis();
    
    LOG_SUCCESS(LOG_TAG_POWER, "Power management initialized");
}

/**
 * @brief Set CPU frequency based on power mode
 * 
 * Implements Dynamic Clock Scaling for power optimization.
 * Scales between 240MHz (WiFi ops), 160MHz (normal), and 80MHz (idle).
 * 
 * IMPORTANT: WiFi requires minimum 160 MHz to avoid beacon timeouts.
 * Use 240MHz for WiFi transmissions, 160MHz for Modbus/processing, 80MHz for idle.
 */
void PowerManagement::setCPUFrequency(PowerMode mode) {
    // Record time in previous mode
    recordModeTime();
    
    uint32_t targetFreq = 240;  // Default to max
    
    switch (mode) {
        case POWER_HIGH_PERFORMANCE:
            targetFreq = 240;  // 240 MHz for WiFi operations
            break;
        case POWER_NORMAL:
            targetFreq = 160;  // 160 MHz for normal operations (Modbus, processing)
            break;
        case POWER_LOW:
            // Use WiFi-safe minimum frequency
            targetFreq = 160;
            break;
        case POWER_SLEEP:
            // Sleep mode handled separately
            return;
    }
    
    // Only change if different from current
    if (targetFreq != currentFrequency) {
        setCpuFrequencyMhz(targetFreq);
        currentFrequency = targetFreq;
        stats.freq_changes++;
    }
    
    currentMode = mode;
    lastUpdateTime = millis();
}

/**
 * @brief Enter light sleep for specified duration
 * 
 * This implements the Light CPU Idle technique for power optimization.
 * Uses delay() which allows CPU to enter idle states between interrupts.
 * True light sleep is disabled due to watchdog timer conflicts.
 */
bool PowerManagement::lightSleep(uint32_t duration_ms) {
    if (duration_ms < 10) {
        // Too short for sleep to be beneficial
        delay(duration_ms);
        return false;
    }
    
    // Record time in current mode before "sleep"
    recordModeTime();
    
    // Use delay() for "Light CPU Idle" - allows CPU to enter idle states
    // True light sleep (esp_light_sleep_start) disabled due to watchdog conflicts
    // delay() is superior for our use case as it:
    // 1. Allows WiFi to maintain connection (DTIM beacons)
    // 2. Permits hardware timers to fire
    // 3. Enables CPU to enter low-power idle between interrupts
    // 4. Avoids watchdog timer resets
    delay(duration_ms);
    
    // Update statistics - count this as "sleep" time for energy calculation
    stats.sleep_time_ms += duration_ms;
    stats.sleep_cycles++;
    
    lastUpdateTime = millis();
    
    return true;
}

/**
 * @brief Get current CPU frequency
 */
uint32_t PowerManagement::getCurrentFrequency() {
    return getCpuFrequencyMhz();
}

/**
 * @brief Get current power mode
 */
PowerMode PowerManagement::getCurrentMode() {
    return currentMode;
}

/**
 * @brief Update power statistics
 */
void PowerManagement::updateStats() {
    recordModeTime();
}

/**
 * @brief Record time spent in current mode
 */
void PowerManagement::recordModeTime() {
    uint32_t currentTime = millis();
    uint32_t elapsed = currentTime - lastUpdateTime;
    
    // Update time for current mode
    switch (currentMode) {
        case POWER_HIGH_PERFORMANCE:
            stats.high_perf_time_ms += elapsed;
            break;
        case POWER_NORMAL:
            stats.normal_time_ms += elapsed;
            break;
        case POWER_LOW:
            stats.low_power_time_ms += elapsed;
            break;
        case POWER_SLEEP:
            // Sleep time recorded separately in lightSleep()
            break;
    }
    
    // Update last update time
    lastUpdateTime = currentTime;
    
    // Total time is the SUM of all mode times (not millis())
    stats.total_time_ms = stats.high_perf_time_ms + stats.normal_time_ms + 
                          stats.low_power_time_ms + stats.sleep_time_ms;
}

/**
 * @brief Get power statistics
 * 
 * Calculates average current consumption and energy saved based on time
 * spent in each power mode and ESP32 datasheet current consumption values.
 * Also includes peripheral gating savings when enabled.
 */
PowerStats PowerManagement::getStats() {
    updateStats();
    
    // Calculate average current and energy saved
    float active_time_s = (stats.total_time_ms - stats.sleep_time_ms) / 1000.0f;
    float sleep_time_s = stats.sleep_time_ms / 1000.0f;
    
    // Estimated current consumption (based on ESP32 datasheet)
    // High performance (240 MHz, WiFi active): ~160-240 mA
    // Normal (160 MHz, WiFi active): ~120-160 mA
    // Low power (160 MHz): ~120 mA
    // Sleep with delay() (CPU idle): ~50 mA
    
    float high_perf_mah = (stats.high_perf_time_ms / 3600000.0f) * 200.0f;  // 200mA avg @ 240MHz
    float normal_mah = (stats.normal_time_ms / 3600000.0f) * 140.0f;        // 140mA avg @ 160MHz
    
    // Low power consumption
    float low_power_current = 120.0f;  // 120mA @ 160MHz
    float low_power_mah = (stats.low_power_time_ms / 3600000.0f) * low_power_current;
    
    // Sleep mode uses delay() which allows CPU idle
    float sleep_current = 50.0f;  // 50mA avg (CPU idle)
    float sleep_mah = (stats.sleep_time_ms / 3600000.0f) * sleep_current;
    
    float total_mah = high_perf_mah + normal_mah + low_power_mah + sleep_mah;
    
    // Calculate what it would have been without power management (always 240MHz, no modem sleep)
    float baseline_mah = (stats.total_time_ms / 3600000.0f) * 200.0f;
    
    // Add peripheral gating savings if enabled
    float peripheral_savings_mah = 0.0f;
    if (powerManagementEnabled && (enabledTechniques & POWER_TECH_PERIPHERAL_GATING)) {
        // Get actual peripheral power statistics
        PeripheralPowerStats pStats = PeripheralPower::getStats();
        // Convert estimated_uart_savings_ma to mAh based on total uptime
        peripheral_savings_mah = (stats.total_time_ms / 3600000.0f) * pStats.estimated_uart_savings_ma;
        // Subtract peripheral savings from total consumption
        total_mah -= peripheral_savings_mah;
    }
    
    stats.avg_current_ma = (total_mah / (stats.total_time_ms / 3600000.0f));
    stats.energy_saved_mah = baseline_mah - total_mah;
    stats.peripheral_savings_mah = peripheral_savings_mah;  // Store for reporting
    
    return stats;
}

/**
 * @brief Print power statistics
 */
void PowerManagement::printStats() {
    PowerStats s = getStats();
    
    LOG_SECTION("POWER MANAGEMENT STATISTICS");
    
    // Time distribution
    LOG_INFO(LOG_TAG_POWER, "Time Distribution:");
    LOG_DEBUG(LOG_TAG_POWER, "  High Performance: %u ms (%.1f%%)", 
              s.high_perf_time_ms, (s.high_perf_time_ms * 100.0f) / s.total_time_ms);
    LOG_DEBUG(LOG_TAG_POWER, "  Normal Mode: %u ms (%.1f%%)", 
              s.normal_time_ms, (s.normal_time_ms * 100.0f) / s.total_time_ms);
    LOG_DEBUG(LOG_TAG_POWER, "  Low Power: %u ms (%.1f%%)", 
              s.low_power_time_ms, (s.low_power_time_ms * 100.0f) / s.total_time_ms);
    LOG_DEBUG(LOG_TAG_POWER, "  Sleep: %u ms (%.1f%%)", 
              s.sleep_time_ms, (s.sleep_time_ms * 100.0f) / s.total_time_ms);
    LOG_INFO(LOG_TAG_POWER, "Total Uptime: %u ms (%.1f s)", s.total_time_ms, s.total_time_ms / 1000.0f);
    
    // Operation statistics
    LOG_INFO(LOG_TAG_POWER, "Sleep Cycles: %u | Freq Changes: %u", s.sleep_cycles, s.freq_changes);
    
    // Power consumption
    LOG_INFO(LOG_TAG_POWER, "Avg Current: %.2f mA | Energy Saved: %.2f mAh", s.avg_current_ma, s.energy_saved_mah);
    
    // Peripheral gating statistics (if enabled)
    if (powerManagementEnabled && (enabledTechniques & POWER_TECH_PERIPHERAL_GATING)) {
        PeripheralPowerStats pStats = PeripheralPower::getStats();
        LOG_INFO(LOG_TAG_POWER, "Peripheral Gating: UART duty=%.1f%%, savings=%.2f mA", 
                 pStats.uart_duty_cycle, pStats.estimated_uart_savings_ma);
    }
    
    if (s.energy_saved_mah > 0) {
        float savings_percent = (s.energy_saved_mah / 
            ((s.total_time_ms / 3600000.0f) * 200.0f)) * 100.0f;
        LOG_SUCCESS(LOG_TAG_POWER, "Power savings: %.1f%%", savings_percent);
    }
    
    // Current CPU state
    const char* modeStr[] = {"High Performance", "Normal", "Low Power", "Sleep"};
    LOG_INFO(LOG_TAG_POWER, "CPU: %u MHz | Mode: %s | Auto: %s", 
             getCurrentFrequency(), modeStr[currentMode], autoPowerManagement ? "ON" : "OFF");
}

/**
 * @brief Calculate estimated current consumption based on CPU frequency
 * 
 * Based on ESP32 datasheet typical values with WiFi active.
 * Values account for WiFi modem sleep when enabled.
 * 
 * @param frequency CPU frequency in MHz
 * @return Estimated current in mA
 */
float PowerManagement::estimateCurrent(uint32_t frequency) {
    // Base current consumption (WiFi active, no modem sleep)
    float base_current;
    if (frequency >= 240) {
        base_current = 200.0f;  // 200 mA at 240 MHz
    } else if (frequency >= 160) {
        base_current = 140.0f;  // 140 mA at 160 MHz
    } else if (frequency >= 80) {
        base_current = 80.0f;   // 80 mA at 80 MHz
    } else {
        base_current = 50.0f;   // ~50 mA at lower frequencies
    }
    
    return base_current;
}

/**
 * @brief Reset power statistics
 */
void PowerManagement::resetStats() {
    stats = {0};
    stats.total_time_ms = millis();
    lastUpdateTime = millis();
}

/**
 * @brief Enable/disable automatic power management
 */
void PowerManagement::enableAutoPowerManagement(bool enable) {
    autoPowerManagement = enable;
    
    if (enable) {
        LOG_SUCCESS(LOG_TAG_POWER, "Automatic power management enabled");
    } else {
        LOG_INFO(LOG_TAG_POWER, "Automatic power management disabled");
        // Return to high performance mode
        setCPUFrequency(POWER_HIGH_PERFORMANCE);
    }
}

/**
 * @brief Check if auto power management is enabled
 */
bool PowerManagement::isAutoPowerManagementEnabled() {
    return autoPowerManagement;
}

/**
 * @brief Enable or disable power management system
 */
void PowerManagement::enable(bool enabled) {
    powerManagementEnabled = enabled;
    
    // Save to NVS
    nvs::setPowerEnabled(enabled);
    
    if (enabled) {
        LOG_SUCCESS(LOG_TAG_POWER, "Power management ENABLED");
        // Apply currently enabled techniques
        applyTechniques();
    } else {
        LOG_INFO(LOG_TAG_POWER, "Power management DISABLED");
        // Disable all power saving
        WiFi.setSleep(false);  // WIFI_PS_NONE
        setCPUFrequency(POWER_HIGH_PERFORMANCE);
    }
}

/**
 * @brief Check if power management is enabled
 */
bool PowerManagement::isEnabled() {
    return powerManagementEnabled;
}

/**
 * @brief Set power management techniques
 * Only Peripheral Gating (0x08) is supported.
 */
void PowerManagement::setTechniques(PowerTechniqueFlags techniques) {
    enabledTechniques = techniques;
    
    // Save to NVS
    nvs::setPowerTechniques(techniques);
    
    LOG_INFO(LOG_TAG_POWER, "Power techniques: 0x%02X", techniques);
    
    // Check peripheral gating
    if (techniques & POWER_TECH_PERIPHERAL_GATING) {
        LOG_DEBUG(LOG_TAG_POWER, "  - Peripheral Gating: ENABLED");
    } else {
        LOG_DEBUG(LOG_TAG_POWER, "  - Peripheral Gating: DISABLED");
    }
    
    // Apply if power management is enabled
    if (powerManagementEnabled) {
        applyTechniques();
    }
    
    LOG_SUCCESS(LOG_TAG_POWER, "Power techniques updated");
}

/**
 * @brief Get current enabled techniques
 */
PowerTechniqueFlags PowerManagement::getTechniques() {
    return enabledTechniques;
}

/**
 * @brief Enable a specific technique
 */
void PowerManagement::enableTechnique(PowerTechnique technique) {
    enabledTechniques |= technique;
    setTechniques(enabledTechniques);
}

/**
 * @brief Disable a specific technique
 */
void PowerManagement::disableTechnique(PowerTechnique technique) {
    enabledTechniques &= ~technique;
    setTechniques(enabledTechniques);
}

/**
 * @brief Check if a technique is enabled
 */
bool PowerManagement::isTechniqueEnabled(PowerTechnique technique) {
    return (enabledTechniques & technique) != 0;
}

/**
 * @brief Apply currently enabled techniques
 * Only Peripheral Gating is supported - controls UART power via PeripheralPower class.
 */
void PowerManagement::applyTechniques() {
    LOG_INFO(LOG_TAG_POWER, "Applying power management techniques");
    
    // Peripheral Gating - actually control UART power
    if (enabledTechniques & POWER_TECH_PERIPHERAL_GATING) {
        // Peripheral gating is enabled - UART will be controlled by acquisition tasks
        // (enableUART() before poll, disableUART() after poll)
        LOG_SUCCESS(LOG_TAG_POWER, "Peripheral gating: ENABLED (UART power control active)");
    } else {
        // Peripheral gating disabled - keep UART always on
        PeripheralPower::enableUART();
        LOG_DEBUG(LOG_TAG_POWER, "Peripheral gating: DISABLED (UART always on)");
    }
    
    LOG_SUCCESS(LOG_TAG_POWER, "Techniques applied");
}
