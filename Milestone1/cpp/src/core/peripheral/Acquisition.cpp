/**
 * @file Acquisition.cpp
 * @brief Implementation of the AcquisitionScheduler class for inverter data acquisition.
 *
 * @details
 * Implements the AcquisitionScheduler methods, providing an interface for polling
 * samples from an InverterSIM instance. Designed for integration with system coordinators
 * or other scheduling logic to facilitate periodic or event-driven data acquisition.
 *
 * @author Yasith
 * @author Prabath
 * @version 1.0
 * @date 2025-08-18
 *
 * @par Revision history
 * - 1.0 (Yasith, 2025-08-18) Moved implementations to cpp file and split to layers.
 */


#include "Acquisition.h"

ProtocolAdapter adapter;
Debug debug;

/**
 * @brief AcquisitionScheduler::AcquisitionScheduler
 * Construct an AcquisitionScheduler bound to a specific inverter simulator.
 *
 * @param sim [in] Reference to an InverterSIM instance to acquire samples from.
 *
 * @note The referenced InverterSIM instance must outlive this scheduler.
 */
AcquisitionScheduler::AcquisitionScheduler(InverterSIM& sim) : sim_(sim) {}

/**
 * @brief AcquisitionScheduler::poll_once
 * Poll the inverter once to obtain a sample.
 *
 * @return std::pair<bool, Sample> 
 * - first: true if the sample was successfully obtained, false if acquisition failed.
 * - second: the acquired Sample if successful; otherwise default-initialized.
 *
 * @details Delegates the polling operation to the underlying InverterSIM instance.
 */
std::pair<bool, DecodedValues> AcquisitionScheduler::poll_once(const RegID* regs, size_t regCount) 
{ 
  adapter.setSSID("Raveenpsp");
  adapter.setPassword("raveen1234");
  adapter.setApiKey("NjhhZWIwNDU1ZDdmMzg3MzNiMTQ5YTFmOjY4YWViMDQ1NWQ3ZjM4NzMzYjE0OWExNQ==");
  
  if (adapter.begin())
  {
    debug.print("Inverter connected\n");
  }
  else 
  {
    debug.print("Inverter connection failed\n");
    //////
  }

  DecodedValues result;
  result.count = 0;

  // build read frame
  uint16_t startAddr, count;
  String frame = buildReadFrame(0x11, regs, regCount, startAddr, count);
  if (frame == "") 
  {
    debug.print("Failed to build read frame.");
    return result;
  }

  // send request
  printError(adapter.readRegister(frame, response));

  // decode response
  result = decodeReadResponse(response, startAddr, count, regs, regCount);

  return result;
}


/// ------------------------------------------------------------------------------------------------
//Set the output power 
bool AcquisitionScheduler::set(const RegID reg, uint16_t powerValue, String &response) 
{ 
  // Build write frame for register 8 (Pac)
  String frame = buildWriteFrame(0x11, 8, powerValue);

  // Call adapter to actually send (real implementation)
  printError(adapter.writeRegister(frame, response));  

  if (response == frame) 
  {
    return true;
  } 
  else 
  {
    return false;
  }
}

DecodedValues AcquisitionScheduler::decodeReadResponse(const String& frameHex,
                                 uint16_t startAddr,
                                 uint16_t count,
                                 const RegID* regs,
                                 size_t regCount) 
{
  DecodedValues result;
  result.count = 0;

  if (frameHex.length() < 10) return result;

  uint8_t func = strtol(frameHex.substring(2,4).c_str(), NULL, 16);
  if (func != 0x03) return result;

  uint8_t byteCount = strtol(frameHex.substring(4,6).c_str(), NULL, 16);
  if (byteCount != count * 2) return result;

  uint16_t allRegs[64];
  for (uint16_t i = 0; i < count; i++) 
  {
    uint16_t hi = strtol(frameHex.substring(6 + i*4, 8 + i*4).c_str(), NULL, 16);
    uint16_t lo = strtol(frameHex.substring(8 + i*4, 10 + i*4).c_str(), NULL, 16);
    allRegs[i] = (hi << 8) | lo;
  }

  for (size_t i = 0; i < regCount; i++) 
  {
    const RegisterDef* rd = findRegister(regs[i]);
    if (!rd) 
    {
      result.values[result.count++] = 0;
      continue;
    }
    result.values[result.count++] = allRegs[rd->addr - startAddr];
  }

  return result;
}





static uint16_t calculateCRC(const uint8_t* data, int length) 
{
  uint16_t crc = 0xFFFF;
  for (int i = 0; i < length; i++) 
  {
    crc ^= (data[i] & 0xFF);
    for (int j = 0; j < 8; j++) 
    {
      if (crc & 0x0001) 
      {
        crc >>= 1;
        crc ^= 0xA001;
      } 
      else 
      {
        crc >>= 1;
      }
    }
  }
  return crc & 0xFFFF;
}


static String toHex(const uint8_t* data, size_t len) 
{
  char buf[3];
  String out;
  for (size_t i = 0; i < len; i++) 
  {
    sprintf(buf, "%02X", data[i]);
    out += buf;
  }
  return out;
}

const RegisterDef* findRegister(RegID id) 
{
  for (size_t i = 0; i < REGISTER_COUNT; i++) 
  {
    if (REGISTER_MAP[i].id == id) return &REGISTER_MAP[i];
  }
  return nullptr;
}

// Read Request frame builder
String buildReadFrame(uint8_t slave, const RegID* regs, size_t regCount,
                      uint16_t& outStart, uint16_t& outCount) 
{
  // find min/max addresses
  uint16_t start = 0xFFFF, end = 0;
  for (size_t i = 0; i < regCount; i++) 
  {
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

//Write Request Frame builder
String buildWriteFrame(uint8_t slave, uint16_t regAddr, uint16_t value) 
{
  uint8_t frame[8];
  frame[0] = slave;
  frame[1] = 0x06; // write single register
  frame[2] = (regAddr >> 8) & 0xFF;
  frame[3] = regAddr & 0xFF;
  frame[4] = (value >> 8) & 0xFF;
  frame[5] = value & 0xFF;

  uint16_t crc = calculateCRC(frame, 6);
  frame[6] = crc & 0xFF;
  frame[7] = (crc >> 8) & 0xFF;

  return toHex(frame, 8);
}

void printError(int err) 
{
  debug.print("Error: ");
  if (err < 16)
  {
    switch (err) {
      case 0x01: debug.println("01 - Illegal Function\n"); break;
      case 0x02: debug.println("02 - Illegal Data Address\n"); break;
      case 0x03: debug.println("03 - Illegal Data Value\n"); break;
      case 0x04: debug.println("04 - Slave Device Failure\n"); break;
      case 0x05: debug.println("05 - Acknowledge (processing delayed)\n"); break;
      case 0x06: debug.println("06 - Slave Device Busy\n"); break;
      case 0x08: debug.println("08 - Memory Parity Error\n"); break;
      case 0x0A: debug.println("0A - Gateway Path Unavailable\n"); break;
      case 0x0B: debug.println("0B - Gateway Target Device Failed to Respond\n"); break;
      default:   debug.println("Unknown error code\n"); break;
    }
  }
  else
  {
    switch (err)
    {
      case 200: debug.print("Register set successful\n"); break;
      case 422: debug.print("JSON error\n"); break;
      case 504: debug.print("No response\n"); break;
      default:  debug.print("Undefined error\n"); break;
    }
  }
}