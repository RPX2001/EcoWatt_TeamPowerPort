#include "Debug.h"


void Debug::begin()
{
    Serial.begin(115200);
}

void Debug::print(const char* message)
{
    Serial.print(message);
}