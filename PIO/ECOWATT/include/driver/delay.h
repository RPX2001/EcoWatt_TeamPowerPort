/**
 * @file delay.h
 * @brief Wrapper for delay functions in ESP32 environment
 * 
 * @version 1.0
 * @date 2024-10-01
 * 
 * @par Revision history
 * - 2024-10-01: Initial version
 */

#ifndef DELAY_H
#define DELAY_H

#include "hal/esp_arduino.h"

// Lightweight wrapper around Arduino delay()
class Delay {
  public:
    void ms(unsigned long ms);
};

// Single shared instance (defined in delay.cpp)
extern Delay wait;

#endif