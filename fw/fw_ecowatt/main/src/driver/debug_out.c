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

#include "debug_out.h"

// Print formatted debug message
void print(const char* format, ...) 
{
    char message[MAX_PRINT_MSG_LENGTH];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);  
    
    va_end(args);
    esp32_print(message); 
}