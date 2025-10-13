#include "driver/delay.h"

// Define the single shared instance declared in the header
Delay wait;

/**
 * @fn void Delay::ms(unsigned long ms)
 * 
 * @brief Delay execution for a specified number of milliseconds.
 * 
 * @param ms Number of milliseconds to delay.
 */
void Delay::ms(unsigned long ms)
{
  ::delay(ms);
}