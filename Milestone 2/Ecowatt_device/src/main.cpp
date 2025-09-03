#include <Arduino.h>

#include "protocol_adapter.h"

ProtocolAdapter adapter;

void setup() {
  adapter.setSSID("Raveenpsp");
  adapter.setPassword("raveen1234");
  adapter.setApiKey("NjhhZWIwNDU1ZDdmMzg3MzNiMTQ5YTFmOjY4YWViMDQ1NWQ3ZjM4NzMzYjE0OWExNQ==");
  adapter.begin();

  // Example write
  adapter.writeRegister("1106000800100B54");

  // Example read
  adapter.readRegister("110300000002C69B");
}

void loop() {
  // Your main program logic
}
