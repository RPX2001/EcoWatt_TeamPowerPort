#include <Arduino.h>
#include "aquisition.h"
#include "protocol_adapter.h"
#include "ringbuffer.h"
/*
  Acquisition Module
  Handles Modbus RTU frame construction and parsing
  Uses ProtocolAdapter to send 

choose registers to poll
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

// Global RingBuffer instance
RingBuffer<String, 20> ringBuffer;  // Buffer for 20 strings

void setup() {
  Serial.begin(115200);
  
}

void loop() {

  const RegID selection[] = {REG_VAC1, REG_IAC1, REG_IPV1, REG_PAC};

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

  // Create array of all register values as a single string entry
  String registerArray = "[";
  for (size_t i = 0; i < values.count; i++) {
    registerArray += String(values.values[i]);
    if (i < values.count - 1) {
      registerArray += ",";  // Add comma between values
    }
  }
  registerArray += "]";
  
  // Add the complete array as one entry to ring buffer
  ringBuffer.push(registerArray);
  Serial.println("Added to buffer: " + registerArray);

// test draining every 10 samples
if (ringBuffer.size() >= 10) {
  auto logs = ringBuffer.drain_all();
  Serial.println("=== Batch Drain ===");
  for (auto& s : logs) {
    Serial.println(s);
  }
}

  delay(2000);  // Wait 1 second before next iteration
}
