#ifndef ESP32_H
#define ESP32_H

#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <string.h>

#define MAX_PRINT_MSG_LENGTH 256

void esp32_wait(uint32_t milliseconds);
void esp32_print(const char message[MAX_PRINT_MSG_LENGTH]);

#endif