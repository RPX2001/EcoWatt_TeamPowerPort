/**
 * @file esp32.h
 * @brief Porting to ESP32.
 *
 * @author Yasith
 * @version 1.0
 * @date 09-09-2025
 *
 * @par Revision history
 * - 1.0 (Yasith, 2025-09-09) Original file.
 */

#ifndef ESP32_H
#define ESP32_H

#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <string.h>

#define MAX_PRINT_MSG_LENGTH 256

/**
 * @defgroup ESP32_Utils ESP32 Utilities
 * @brief Helper functions for ESP32-specific operations.
 *
 * These functions provide common operations such as waiting
 * and logging messages on the ESP32 platform.
 * @{
 */

/**
 * @fn void esp32_wait(uint32_t milliseconds);
 * @brief Delays execution for a specified number of milliseconds on ESP32.
 * 
 * @param milliseconds The duration to wait in milliseconds (uint32_t). Must be a positive value;
 */
void esp32_wait(uint32_t milliseconds);

/**
 * @fn void esp32_print(const char message[MAX_PRINT_MSG_LENGTH]);
 * @brief Prints a message to the ESP32 log using INFO level.
 * 
 * @param message A character array containing the message to print. It should not exceed
 *                MAX_PRINT_MSG_LENGTH (assumed to be defined in "esp32.h" or
 *                elsewhere).
 */
void esp32_print(const char message[MAX_PRINT_MSG_LENGTH]);

/** @} */ // end of ESP32_Utils

#endif