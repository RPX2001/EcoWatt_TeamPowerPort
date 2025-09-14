/**
 * @file timers_util.h
 * @brief Utility functions for managing periodic timers using ESP-IDF.
 *
 * @author Yasith
 * @version 1.0
 * @date 2024-10-01
 *
 * @par Revision history
 * - 1.0 (2024-10-01, Yasith): Original file.
 */

#ifndef TIMER_H
#define TIMER_H

#include "esp_timer.h"
#include <stdio.h>

#include "delay.h"
#include "debug_out.h"

#define wait(ms) delay_wait(ms)

/**
 * @defgroup Timer_Utils Timer Utilities
 * @brief Functions for creating and managing periodic timers.
 * @{
 */

/**
 * @fn create_periodic_timer(const char* name, esp_timer_cb_t callback, int64_t period_us)
 * @brief Creates and starts a periodic timer.
 * 
 * @param name The name of the timer.
 * @param callback The callback function to be called on timer expiration.
 * @param period_us The timer period in microseconds.
 * 
 * @return esp_timer_handle_t Returns the handle to the created timer, or NULL on failure.
 */
esp_timer_handle_t create_periodic_timer(const char* name, esp_timer_cb_t callback, int64_t period_us);

/** @} */ // end of Timer_Utils

#endif