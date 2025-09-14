/**
 * @file esp32.c
 * @brief Porting to ESP32.
 *
 * @author Yasith
 * @version 1.0
 * @date 09-09-2025
 *
 * @par Revision history
 * - 1.0 (Yasith, 2025-09-09) Original file.
 */

#include "esp32.h"

static const char* TAG = "ECW";

// Delay execution for specified milliseconds
void esp32_wait(uint32_t milliseconds) {
     vTaskDelay(pdMS_TO_TICKS(milliseconds));
}

// Print message to ESP32 log
void esp32_print(const char message[MAX_PRINT_MSG_LENGTH]) {
    ESP_LOGI(TAG, "%s", message);
}