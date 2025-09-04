#include <Arduino.h>
#include "aquisition.h"
#include "protocol_adapter.h"

ProtocolAdapter adapter;

void setup() {
  // adapter.setSSID("Raveenpsp");
  // adapter.setPassword("raveen1234");
  // adapter.setApiKey("NjhhZWIwNDU1ZDdmMzg3MzNiMTQ5YTFmOjY4YWViMDQ1NWQ3ZjM4NzMzYjE0OWExNQ==");
  // adapter.begin();
  
  // // Example write
  // adapter.writeRegister("1106000800100B54");

  // // Example read
  // adapter.readRegister("110300000002C69B");
}

void loop() {
  // Your main program logic
  // choose registers to poll
  const RegID selection[] = {REG_VAC1, REG_IAC1, REG_IPV1, REG_IPV2};

  // poll inverter
  DecodedValues values = readRequest(selection, 4);

  // print results
  for (size_t i = 0; i < values.count; i++) {
    const RegisterDef* rd = findRegister(selection[i]);
    Serial.printf("%s = %u\n", rd->name, values.values[i]);
  }

  delay(5000);
}
