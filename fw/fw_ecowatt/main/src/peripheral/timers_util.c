/**
 * @file timers_util.c
 * @brief Utility functions for managing periodic timers using ESP-IDF.
 *
 * @author Yasith
 * @version 1.0
 * @date 2024-10-01
 *
 * @par Revision history
 * - 1.0 (2024-10-01, Yasith): Original file.
 */


#include "timers_util.h"

// Create and start a periodic timer
esp_timer_handle_t create_periodic_timer(const char* name, esp_timer_cb_t callback, int64_t period_us) 
{
    const esp_timer_create_args_t timer_args = 
    {
        .callback = callback,
        .name = name
    };

    esp_timer_handle_t timer;
    esp_err_t err = esp_timer_create(&timer_args, &timer);
    if (err != ESP_OK) 
    {
        print("Failed to create timer %s: %s\n", name, esp_err_to_name(err));
        return NULL;
    }

    err = esp_timer_start_periodic(timer, period_us);
    if (err != ESP_OK) 
    {
        print("Failed to start timer %s: %s\n", name, esp_err_to_name(err));
        return NULL;
    }

    return timer;
}
