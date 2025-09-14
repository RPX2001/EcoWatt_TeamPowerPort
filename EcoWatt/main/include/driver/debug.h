#ifndef DEBUG_H
#define DEBUG_H

#include "esp_log.h"
#include "esp32.h"

void print(const char message[MAX_PRINT_MSG_LENGTH]);

#endif