#include "driver/delay.h"

// Define the single shared instance declared in the header
Delay wait;

void Delay::ms(unsigned long ms)
{
  ::delay(ms);
}