#include "debug.h"

void print(const char message[MAX_PRINT_MSG_LENGTH]) {
    esp_print(message);
}