#include "esp32.h"

// Wrap delay in a function
void esp32_wait(uint32_t milliseconds) {
     vTaskDelay(pdMS_TO_TICKS(milliseconds));
}

static const char* TAG = "ECW";

void esp32_print(const char message[MAX_PRINT_MSG_LENGTH]) {
    ESP_LOGI(TAG, "%s", message);
}