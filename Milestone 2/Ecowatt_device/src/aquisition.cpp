#include <Arduino.h>
#include "protocol_adapter.h"
#include "aquisition.h"

ProtocolAdapter adapter;

// ---- CRC16 Modbus ----
static uint16_t calculateCRC(const uint8_t* data, int length) {
  uint16_t crc = 0xFFFF;
  for (int i = 0; i < length; i++) {
    crc ^= (data[i] & 0xFF);
    for (int j = 0; j < 8; j++) {
      if (crc & 0x0001) {
        crc >>= 1;
        crc ^= 0xA001;
      } else {
        crc >>= 1;
      }
    }
  }
  return crc & 0xFFFF;
}

static String toHex(const uint8_t* data, size_t len) {
  char buf[3];
  String out;
  for (size_t i = 0; i < len; i++) {
    sprintf(buf, "%02X", data[i]);
    out += buf;
  }
  return out;
}

// ---- Register Lookup ----
const RegisterDef* findRegister(RegID id) {
  for (size_t i = 0; i < REGISTER_COUNT; i++) {
    if (REGISTER_MAP[i].id == id) return &REGISTER_MAP[i];
  }
  return nullptr;
}

// ---- Frame Builder ----
String buildReadFrame(uint8_t slave, const RegID* regs, size_t regCount,
                      uint16_t& outStart, uint16_t& outCount) {
  // find min/max addresses
  uint16_t start = 0xFFFF, end = 0;
  for (size_t i = 0; i < regCount; i++) {
    const RegisterDef* rd = findRegister(regs[i]);
    if (!rd) continue;
    if (rd->addr < start) start = rd->addr;
    if (rd->addr > end)   end   = rd->addr;
  }
  outStart = start;
  outCount = (end - start) + 1;

  // Build Modbus frame
  uint8_t frame[8];
  frame[0] = slave;
  frame[1] = 0x03; // read holding regs
  frame[2] = (start >> 8) & 0xFF;
  frame[3] = start & 0xFF;
  frame[4] = (outCount >> 8) & 0xFF;
  frame[5] = outCount & 0xFF;

  uint16_t crc = calculateCRC(frame, 6);
  frame[6] = crc & 0xFF;
  frame[7] = (crc >> 8) & 0xFF;

  return toHex(frame, 8);
}

/*
read request frame function to get the registers from user from main code and 
make relevant read request frame and send to protocol adapter
*/
DecodedValues readRequest(const RegID* regs, size_t regCount) {
  adapter.setSSID("Raveenpsp");
  adapter.setPassword("raveen1234");
  adapter.setApiKey("NjhhZWIwNDU1ZDdmMzg3MzNiMTQ5YTFmOjY4YWViMDQ1NWQ3ZjM4NzMzYjE0OWExNQ==");
  adapter.begin();

  DecodedValues result;
  result.count = 0;

  // build read frame
  uint16_t startAddr, count;
  String frame = buildReadFrame(0x11, regs, regCount, startAddr, count);
  if (frame == "") {
    Serial.println("Failed to build read frame.");
    return result;
  }

  // send request
  String response = adapter.readRegister(frame);

  // decode response
  result = decodeReadResponse(response, startAddr, count, regs, regCount);

  return result;
}


const uint16_t* returnValues(const DecodedValues& decoded) {
  return decoded.values;
}


// ---- Frame Decoder ----
DecodedValues decodeReadResponse(const String& frameHex,
                                 uint16_t startAddr,
                                 uint16_t count,
                                 const RegID* regs,
                                 size_t regCount) {
  DecodedValues result;
  result.count = 0;

  if (frameHex.length() < 10) return result;

  uint8_t func = strtol(frameHex.substring(2,4).c_str(), NULL, 16);
  if (func != 0x03) return result;

  uint8_t byteCount = strtol(frameHex.substring(4,6).c_str(), NULL, 16);
  if (byteCount != count * 2) return result;

  uint16_t allRegs[64];
  for (uint16_t i = 0; i < count; i++) {
    uint16_t hi = strtol(frameHex.substring(6 + i*4, 8 + i*4).c_str(), NULL, 16);
    uint16_t lo = strtol(frameHex.substring(8 + i*4, 10 + i*4).c_str(), NULL, 16);
    allRegs[i] = (hi << 8) | lo;
  }

  for (size_t i = 0; i < regCount; i++) {
    const RegisterDef* rd = findRegister(regs[i]);
    if (!rd) {
      result.values[result.count++] = 0;
      continue;
    }
    result.values[result.count++] = allRegs[rd->addr - startAddr];
  }

  return result;
}