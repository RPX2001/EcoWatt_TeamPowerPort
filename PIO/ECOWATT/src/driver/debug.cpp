#include "driver/debug.h"

// Define the single shared instance declared in the header
Debug debug;

Debug::Debug() {}

/**
 * @fn void Debug::init()
 * 
 * @brief Initialize the debug serial interface.
 */
void Debug::init()
{
    Serial.begin(115200);
}


/**
 * @fn void Debug::log(const char *format, ...)
 * 
 * @brief Log a formatted debug message to the serial console.
 * 
 * @param format printf-style format string.
 * @param ... Additional arguments for formatting.
 */
void Debug::log(const char *format, ...) 
{
    char buffer[DEBUG_BUFFER_SIZE];

    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    Serial.print(buffer);
}