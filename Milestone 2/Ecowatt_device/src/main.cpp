#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "aquisition.h"
#include "protocol_adapter.h"
#include "ringbuffer.h"
#include "data_compression.h"
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

// WiFi and Cloud Configuration
const char* ssid = "HydroBK";
const char* password = "Hydrolink123";
const char* serverURL = "http://10.40.99.2:5001/process";

// Global RingBuffer instance for compressed data
RingBuffer<CompressedData, 20> ringBuffer;  // Buffer for 20 compressed data entries
unsigned long lastUpload = 0;
const unsigned long uploadInterval = 15000;  // Upload every 15 seconds

void setup() {
  Serial.begin(115200);
  
  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi...");
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("");
  Serial.print("Connected to WiFi. IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Flask server URL: ");
  Serial.println(serverURL);
}

void uploadRingBufferToCloud() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected. Cannot upload to cloud.");
    return;
  }

  if (ringBuffer.empty()) {
    Serial.println("Ring buffer is empty. Nothing to upload.");
    return;
  }

  HTTPClient http;
  http.begin(serverURL);
  http.addHeader("Content-Type", "application/json");
  
  // Create JSON payload in required format: {id: {}, n: {}, data: {}, compression: {}, registers: {}}
  JsonDocument doc;
  
  // Get all data from ring buffer
  auto allData = ringBuffer.drain_all();
  
  // Format: {id: {}, n: {}, data: {}, compression: {}, registers: {}}
  doc["id"] = "ESP32_EcoWatt_001";
  doc["n"] = allData.size();  // Number of entries
  
  // Add register header information
  JsonArray registersArray = doc["registers"].to<JsonArray>();
  registersArray.add("REG_VAC1");  // AC Voltage
  registersArray.add("REG_IAC1");  // AC Current  
  registersArray.add("REG_IPV1");  // PV Current
  registersArray.add("REG_PAC");   // AC Power
  
  // Create data array (compressed data)
  JsonArray dataArray = doc["data"].to<JsonArray>();
  JsonArray compressionArray = doc["compression"].to<JsonArray>();
  
  for (auto& entry : allData) {
    // Add compressed data directly to data array
    dataArray.add(entry.data);
    
    // Add compression metadata
    JsonObject compressionEntry = compressionArray.add<JsonObject>();
    compressionEntry["type"] = entry.compressionType;
    compressionEntry["timestamp"] = entry.timestamp;
    compressionEntry["original_count"] = entry.originalCount;
  }
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  Serial.println("=== Uploading Ring Buffer to Cloud ===");
  Serial.print("Entries to upload: ");
  Serial.println(allData.size());
  Serial.print("Payload size: ");
  Serial.println(jsonString.length());
  
  // Print the JSON packet being sent
  Serial.println("📤 JSON Packet to send:");
  Serial.println("---JSON START---");
  Serial.println(jsonString);
  Serial.println("---JSON END---");
  
  // Send POST request
  int httpResponseCode = http.POST(jsonString);
  
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    Serial.print("Server response: ");
    Serial.println(response);
    Serial.println("Successfully uploaded to cloud!");
  } else {
    Serial.print(" Upload failed. Error code: ");
    Serial.println(httpResponseCode);
    Serial.println("Check if Flask server is running at " + String(serverURL));
    
    // Put data back into ring buffer if upload failed
    for (auto& entry : allData) {
      ringBuffer.push(entry);
    }
  }
  
  http.end();
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

  // Use delta compression only
  String compressedData = DataCompression::compressRegisterData(values.values, values.count);
  
  // Calculate compression stats
  size_t originalSize = values.count * sizeof(uint16_t);
  size_t compressedSize = compressedData.length();
  
  // Create compressed data entry
  CompressedData entry(compressedData, values.count);
  
  // Add compressed data to ring buffer
  ringBuffer.push(entry);
  
  Serial.println("Original values: [" + String(values.values[0]) + "," + String(values.values[1]) + "," + String(values.values[2]) + "," + String(values.values[3]) + "]");
  Serial.println("Delta compressed: " + compressedData);
  DataCompression::printCompressionStats("Delta", originalSize, compressedSize);

  // Check if it's time to upload to cloud or ring buffer is getting full
  unsigned long currentTime = millis();
  bool timeToUpload = (currentTime - lastUpload) >= uploadInterval;
  bool bufferNearlyFull = ringBuffer.size() >= 15;  // Upload when 75% full
  
  Serial.println("Ring buffer size: " + String(ringBuffer.size()) + "/20 | Next upload in: " + String((uploadInterval - (currentTime - lastUpload))/1000) + "s");
  
  if (timeToUpload || bufferNearlyFull) {
    if (timeToUpload) Serial.println("🕒 15-second timer triggered upload");
    if (bufferNearlyFull) Serial.println("📦 Buffer nearly full triggered upload");
    uploadRingBufferToCloud();
    lastUpload = currentTime;
  }

  delay(2000);  // Wait 2 seconds before next iteration

}
