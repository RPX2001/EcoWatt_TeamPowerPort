#include <Arduino.h>
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

// Global RingBuffer instance for compressed data
RingBuffer<CompressedData, 20> ringBuffer;  // Buffer for 20 compressed data entries

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

  // Compress register data using delta compression
  String compressedData = DataCompression::compressRegisterData(values.values, values.count, true);
  
  // Calculate compression stats
  size_t originalSize = values.count * sizeof(uint16_t);
  size_t compressedSize = compressedData.length();
  
  // Create compressed data entry
  CompressedData entry(compressedData, true, values.count);
  
  // Add compressed data to ring buffer
  ringBuffer.push(entry);
  
  Serial.println("Original values: [" + String(values.values[0]) + "," + String(values.values[1]) + "," + String(values.values[2]) + "," + String(values.values[3]) + "]");
  Serial.println("Compressed: " + compressedData);
  DataCompression::printCompressionStats("Delta", originalSize, compressedSize);

// // test draining every 5 samples
// if (ringBuffer.size() >= 5) {
//   auto logs = ringBuffer.drain_all();
//   Serial.println("=== Batch Drain & Decompress ===");
//   for (auto& entry : logs) {
//     // Decompress and display
//     auto decompressed = DataCompression::decompressRegisterData(entry.data, entry.isDelta);
//     Serial.print("Timestamp: " + String(entry.timestamp) + " | Original count: " + String(entry.originalCount) + " | Data: [");
//     for (size_t i = 0; i < decompressed.size(); i++) {
//       Serial.print(String(decompressed[i]));
//       if (i < decompressed.size() - 1) Serial.print(",");
//     }
//     Serial.println("]");
//   }
// }

  delay(2000);  // Wait 1 second before next iteration
}
