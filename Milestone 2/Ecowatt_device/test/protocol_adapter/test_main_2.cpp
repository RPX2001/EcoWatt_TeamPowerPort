#include <Arduino.h>
#include "protocol_adapter.h"
#include "aquisition.h"

void setup() {
  Serial.begin(115200);
  // see the error handling in protocol_adapter.cpp
  String frame = "110300000002C69B"; // example read frame
    ProtocolAdapter adapter;
        adapter.setSSID("Raveenpsp");
        adapter.setPassword("raveen1234");
        adapter.setApiKey("NjhhZWIwNDU1ZDdmMzg3MzNiMTQ5YTFmOjY4YWViMDQ1NWQ3ZjM4NzMzYjE0OWExNQ==");
        adapter.begin();
        String response = adapter.writeRegister(frame);
        Serial.println("Read Response: " + response);
        //send write frame
        String write_frame = "1106000800100B54"; // example write frame 
        String write_response = adapter.readRegister(write_frame);
        Serial.println("Write Response: " + write_response);
}
void loop() {
  // nothing here
}