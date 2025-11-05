/**
 * @file peripheral_power.cpp
 * @brief Implementation of peripheral power gating control
 * 
 * @version 1.0.0
 * @date 2025-10-18
 */

#include "application/peripheral_power.h"
#include "peripheral/logger.h"

// Static member initialization
PeripheralPowerStats PeripheralPower::stats = {0};
HardwareSerial* PeripheralPower::modbusSerial = &Serial2;

/**
 * @brief Initialize peripheral power management
 */
void PeripheralPower::init() {
    LOG_INFO(LOG_TAG_POWER, "Peripheral power gating initialization");
    
    // Reset statistics
    resetStats();
    
    // UART starts disabled (will be enabled when needed)
    stats.uart_currently_enabled = false;
    
    LOG_INFO(LOG_TAG_POWER, "UART power gating enabled - powered only during Modbus polls");
    LOG_SUCCESS(LOG_TAG_POWER, "Peripheral power management initialized");
}

/**
 * @brief Enable UART peripheral for Modbus communication
 */
void PeripheralPower::enableUART(uint32_t baud) {
    if (stats.uart_currently_enabled) {
        // Already enabled, do nothing
        return;
    }
    
    // Record state change time
    recordStateChange(true);
    
    // Initialize UART with specified baud rate
    modbusSerial->begin(baud, SERIAL_8N1, MODBUS_RX_PIN, MODBUS_TX_PIN);
    
    // Small delay to allow UART to stabilize
    delayMicroseconds(100);
    
    stats.uart_currently_enabled = true;
    stats.uart_enable_count++;
    
#ifdef PERIPHERAL_POWER_DEBUG
    LOG_DEBUG(LOG_TAG_POWER, "UART Enabled (count: %u)\n", stats.uart_enable_count);
#endif
}

/**
 * @brief Disable UART peripheral to save power
 */
void PeripheralPower::disableUART() {
    if (!stats.uart_currently_enabled) {
        // Already disabled, do nothing
        return;
    }
    
    // Record state change time
    recordStateChange(false);
    
    // Flush any pending data
    modbusSerial->flush();
    
    // End UART (powers down peripheral)
    modbusSerial->end();
    
    stats.uart_currently_enabled = false;
    stats.uart_disable_count++;
    
#ifdef PERIPHERAL_POWER_DEBUG
    LOG_DEBUG(LOG_TAG_POWER, "UART Disabled (count: %u, duty: %.2f%%)\n", 
                  stats.uart_disable_count, 
                  getStats().uart_duty_cycle);
#endif
}

/**
 * @brief Check if UART is currently enabled
 */
bool PeripheralPower::isUARTEnabled() {
    return stats.uart_currently_enabled;
}

/**
 * @brief Record UART state change time
 */
void PeripheralPower::recordStateChange(bool enabling) {
    uint32_t currentTime = millis();
    
    if (enabling) {
        // Switching from idle to active
        if (stats.last_enable_time > 0) {
            uint32_t idle_duration = currentTime - stats.last_enable_time;
            stats.uart_idle_time_ms += idle_duration;
        }
        stats.last_enable_time = currentTime;
    } else {
        // Switching from active to idle
        if (stats.last_enable_time > 0) {
            uint32_t active_duration = currentTime - stats.last_enable_time;
            stats.uart_active_time_ms += active_duration;
        }
        stats.last_enable_time = currentTime;
    }
}

/**
 * @brief Update statistics
 */
void PeripheralPower::updateStats() {
    // Update current state duration
    uint32_t currentTime = millis();
    
    if (stats.last_enable_time > 0) {
        uint32_t duration = currentTime - stats.last_enable_time;
        
        if (stats.uart_currently_enabled) {
            stats.uart_active_time_ms += duration;
        } else {
            stats.uart_idle_time_ms += duration;
        }
        
        stats.last_enable_time = currentTime;
    }
}

/**
 * @brief Get peripheral power statistics
 */
PeripheralPowerStats PeripheralPower::getStats() {
    updateStats();
    
    // Calculate duty cycle
    uint32_t total_time = stats.uart_active_time_ms + stats.uart_idle_time_ms;
    if (total_time > 0) {
        stats.uart_duty_cycle = (stats.uart_active_time_ms * 100.0f) / total_time;
    } else {
        stats.uart_duty_cycle = 0.0f;
    }
    
    // Calculate estimated savings
    // UART idle current: ~10 mA
    // Time saved: (100% - duty_cycle)
    // Savings = 10 mA × (idle_time / total_time)
    if (total_time > 0) {
        float idle_fraction = stats.uart_idle_time_ms / (float)total_time;
        stats.estimated_uart_savings_ma = 10.0f * idle_fraction;
    } else {
        stats.estimated_uart_savings_ma = 0.0f;
    }
    
    return stats;
}

/**
 * @brief Print peripheral power statistics
 */
void PeripheralPower::printStats() {
    PeripheralPowerStats s = getStats();
    
    LOG_INFO(LOG_TAG_STATS, "Peripheral Power Gating Statistics");
    
    // UART statistics
    LOG_INFO(LOG_TAG_STATS, "UART - Enable: %u, Disable: %u", s.uart_enable_count, s.uart_disable_count);
    LOG_INFO(LOG_TAG_STATS, "UART - Active: %u ms (%.1f s), Idle: %u ms (%.1f s)", 
             s.uart_active_time_ms, s.uart_active_time_ms / 1000.0f,
             s.uart_idle_time_ms, s.uart_idle_time_ms / 1000.0f);
    LOG_INFO(LOG_TAG_STATS, "UART - Duty Cycle: %.2f%%, State: %s", 
             s.uart_duty_cycle, s.uart_currently_enabled ? "ACTIVE" : "IDLE");
    
    // Power savings
    LOG_INFO(LOG_TAG_STATS, "Power Savings - Gating Efficiency: %.1f%%, Estimated: %.2f mA", 
             100.0f - s.uart_duty_cycle, s.estimated_uart_savings_ma);
    
    if (s.estimated_uart_savings_ma > 0) {
        float baseline_uart_current = 10.0f;
        float savings_percent = (s.estimated_uart_savings_ma / baseline_uart_current) * 100.0f;
        LOG_SUCCESS(LOG_TAG_STATS, "Peripheral gating saving %.1f%% power", savings_percent);
    } else {
        LOG_DEBUG(LOG_TAG_STATS, "No significant UART power savings yet");
    }
    
    // Overall system impact
    float baseline_system_current = 150.0f;
    float system_reduction_percent = (s.estimated_uart_savings_ma / baseline_system_current) * 100.0f;
    LOG_INFO(LOG_TAG_STATS, "System Impact - Reduction: %.2f mA (%.1f%%)", 
             s.estimated_uart_savings_ma, system_reduction_percent);
}

/**
 * @brief Reset peripheral power statistics
 */
void PeripheralPower::resetStats() {
    stats.uart_enable_count = 0;
    stats.uart_disable_count = 0;
    stats.uart_active_time_ms = 0;
    stats.uart_idle_time_ms = 0;
    stats.uart_duty_cycle = 0.0f;
    stats.estimated_uart_savings_ma = 0.0f;
    stats.last_enable_time = millis();
}
