/**
 * @file peripheral_power.h
 * @brief Peripheral power gating control for ESP32 EcoWatt device
 * 
 * Implements power gating for peripherals to reduce idle current consumption:
 * - UART (Modbus): Disabled when not actively polling (~9.5 mA savings)
 * - Future: I2C, SPI, ADC gating support
 * 
 * @version 1.0.0
 * @date 2025-10-18
 */

#ifndef PERIPHERAL_POWER_H
#define PERIPHERAL_POWER_H

#include <Arduino.h>
#include <HardwareSerial.h>

// Enable debug logging for peripheral gating (uncomment to see enable/disable events)
#define PERIPHERAL_POWER_DEBUG

/**
 * @brief Peripheral power gating statistics
 */
struct PeripheralPowerStats {
    uint32_t uart_enable_count;      // Number of times UART was enabled
    uint32_t uart_disable_count;     // Number of times UART was disabled
    uint32_t uart_active_time_ms;    // Time UART was active
    uint32_t uart_idle_time_ms;      // Time UART was idle (powered off)
    
    float uart_duty_cycle;           // Percentage of time UART is active
    float estimated_uart_savings_ma; // Estimated current savings from UART gating
    
    uint32_t last_enable_time;       // Timestamp of last UART enable
    bool uart_currently_enabled;     // Current UART state
};

/**
 * @brief Peripheral Power Management Class
 */
class PeripheralPower {
public:
    /**
     * @brief Initialize peripheral power management
     */
    static void init();
    
    /**
     * @brief Enable UART peripheral for Modbus communication
     * @param baud Baud rate (default 9600 for Modbus)
     */
    static void enableUART(uint32_t baud = 9600);
    
    /**
     * @brief Disable UART peripheral to save power
     */
    static void disableUART();
    
    /**
     * @brief Check if UART is currently enabled
     * @return true if UART is active, false if disabled
     */
    static bool isUARTEnabled();
    
    /**
     * @brief Get peripheral power statistics
     * @return PeripheralPowerStats structure
     */
    static PeripheralPowerStats getStats();
    
    /**
     * @brief Print peripheral power statistics
     */
    static void printStats();
    
    /**
     * @brief Reset peripheral power statistics
     */
    static void resetStats();
    
    /**
     * @brief Update statistics (call periodically)
     */
    static void updateStats();

private:
    static PeripheralPowerStats stats;
    static HardwareSerial* modbusSerial;
    static const int MODBUS_RX_PIN = 16;  // GPIO16 for RX
    static const int MODBUS_TX_PIN = 17;  // GPIO17 for TX
    
    /**
     * @brief Record UART state change time
     */
    static void recordStateChange(bool enabling);
};

// Convenience macros for peripheral power management
#define PERIPHERAL_UART_ON()   PeripheralPower::enableUART()
#define PERIPHERAL_UART_OFF()  PeripheralPower::disableUART()

#endif // PERIPHERAL_POWER_H
