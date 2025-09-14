/**
 * @file debug.c
 * @brief Printing debug msgs through UART.
 *
 * @author Yasith
 * @version 1.0
 * @date 09-09-2025
 *
 * @par Revision history
 * - 1.0 (Yasith, 2025-09-09) Original file.
 */

#ifndef DEBUG_H
#define DEBUG_H

#include <stdarg.h>

#include "esp_log.h"
#include "esp32.h"

/**
 * @defgroup Debug Debug Utilities
 * @brief Functions for printing debug messages.
 *
 * Provides formatted printing functionality, mainly for debugging purposes.
 * @{
 */

/**
 * @fn void print(const char* format, ...);
 * @brief Prints a formatted debug message.
 * 
 * @param format A format string (like printf).
 * @param ... Additional arguments as required by the format string.
 */
void print(const char* format, ...);

/** @} */ // end of Debug

#endif