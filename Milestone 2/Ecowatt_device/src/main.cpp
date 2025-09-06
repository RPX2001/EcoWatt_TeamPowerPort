#include <Arduino.h>
#include "aquisition.h"
#include "protocol_adapter.h"


void setup() {
  Serial.begin(115200);
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
  // choose registers to poll
/*
  {REG_VAC1, 0, "Vac1"},
  {REG_IAC1, 1, "Iac1"},
  {REG_FAC1, 2, "Fac1"},
  {REG_VPV1, 3, "Vpv1"},
  {REG_VPV2, 4, "Vpv2"},
  {REG_IPV1, 5, "Ipv1"},
  {REG_IPV2, 6, "Ipv2"},
  {REG_TEMP, 7, "Temp"},
  {REG_POW,  8, "Pow"},
  {REG_PAC,  9, "Pac"}

*/
  
  // Registers need to read from InverterSIM
  const RegID selection[] = {REG_VAC1, REG_IAC1, REG_IPV1, REG_IPV2};

  // poll inverter
  DecodedValues values = readRequest(selection, 4);

  // print results from the inverter sim
  Serial.println("Decoded Values:");
  for (size_t i = 0; i < values.count; i++) {
    Serial.printf("  [%d] = %u\n", i, values.values[i]);
  }


  //set power to 50W out
  bool ok = setPower(50); // set Pac = 50W
  if (ok) {
    Serial.println("Output power register updated!");
  }

}
