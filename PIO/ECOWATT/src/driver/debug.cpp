#include "driver/debug.h"

// Define the single shared instance declared in the header
Debug debug;

Debug::Debug() {}

void Debug::init()
{
    Serial.begin(115200);
}

void Debug::log(const char *format, ...) 
{
    char buffer[DEBUG_BUFFER_SIZE];

    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    Serial.print(buffer);
}