#include <Arduino.h>

#include "protocol_adapter.h"

ProtocolAdapter adapter;

void setup() {
  adapter.begin();

  // Example write
  adapter.writeRegister("1106000800100B54");

  // Example read
  adapter.readRegister("110300000002C69B");
}

void loop() {
  // Your main program logic
}
