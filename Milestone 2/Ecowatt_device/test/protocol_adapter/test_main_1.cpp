#include <Arduino.h>
#include "protocol_adapter.h"
#include "aquisition.h"
/*
  Protocol Adapter for EcoWatt Device
  Handles communication with the server via HTTP and JSON
  Implements Modbus RTU frame construction and parsing
  Connects to WiFi network
    Sends read and write requests
    Retries on failure
    Parses JSON responses

*/

void setup() {
  Serial.begin(115200);
  // send frame
  String frame = "110300000002C69B"; // example read frame
  ProtocolAdapter adapter;
    adapter.setSSID("Raveenpsp");
    adapter.setPassword("raveen1234");
    adapter.setApiKey("NjhhZWIwNDU1ZDdmMzg3MzNiMTQ5YTFmOjY4YWViMDQ1NWQ3ZjM4NzMzYjE0OWExNQ==");
    adapter.begin();
    String response = adapter.readRegister(frame);
    Serial.println("Read Response: " + response);

    //send write frame
    String write_frame = "1106000800100B54"; // example write frame
    String write_response = adapter.writeRegister(write_frame);
    Serial.println("Write Response: " + write_response);
}

void loop() {
  // nothing here
}