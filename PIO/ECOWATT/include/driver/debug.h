/**
 * @file debug.h
 * @brief Debugging utilities for the ESP32 environment
 * 
 * @version 1.0
 * @date 2024-10-01
 * 
 * @par Revision history
 * - 2024-10-01: Initial version
 */

#ifndef DEBUG_H
#define DEBUG_H

#include "hal/esp_arduino.h"
#include <stdarg.h>

#define DEBUG_BUFFER_SIZE 256

class Debug 
{
  public:
    Debug();
    void init();
    void log(const char *format, ...);
};

// Single shared instance (defined in debug.cpp)
extern Debug debug;

#endif