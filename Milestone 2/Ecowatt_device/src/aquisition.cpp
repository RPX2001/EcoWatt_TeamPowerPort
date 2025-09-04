#include <Arduino.h>
#include "protocol_adapter.h"
#include "aquisition.h"

// ---------------- CRC16 (Modbus, low byte first) ----------------
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

// ---------------- Register Lookup ----------------
const RegisterDef* findRegister(RegID id) {
  for (size_t i = 0; i < REGISTER_COUNT; i++) {
    if (REGISTER_MAP[i].id == id) return &REGISTER_MAP[i];  //now looking at id of register but can changee that to name
  }
  return nullptr;
}

// ---------------- Frame Builders ----------------
// Build Read Holding Registers frame
String buildReadFrame(uint8_t slave, uint16_t startAddr, uint16_t count) {
  uint8_t frame[8];
  frame[0] = slave;
  frame[1] = 0x03; // Function: read holding regs
  frame[2] = (startAddr >> 8) & 0xFF;
  frame[3] = startAddr & 0xFF;
  frame[4] = (count >> 8) & 0xFF;
  frame[5] = count & 0xFF;
  uint16_t crc = calculateCRC(frame, 6);
  frame[6] = crc & 0xFF;        // Low byte
  frame[7] = (crc >> 8) & 0xFF; // High byte
  return toHex(frame, 8);
}

// Build Write Single Register frame
String buildWriteFrame(uint8_t slave, uint16_t addr, uint16_t val) {
  uint8_t frame[8];
  frame[0] = slave;
  frame[1] = 0x06; // Function: write single reg
  frame[2] = (addr >> 8) & 0xFF;
  frame[3] = addr & 0xFF;
  frame[4] = (val >> 8) & 0xFF;
  frame[5] = val & 0xFF;
  uint16_t crc = calculateCRC(frame, 6);
  frame[6] = crc & 0xFF;
  frame[7] = (crc >> 8) & 0xFF;
  return toHex(frame, 8);
}
