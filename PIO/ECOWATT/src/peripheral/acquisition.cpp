#include "peripheral/acquisition.h"

ProtocolAdapter adapter;

// CRC calculator
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


//Convert in to hexdecimal value
static bool toHex(const uint8_t* data, size_t len, char* out, size_t outSize) 
{
  if (outSize < (len * 2 + 1)) return false;
  for (size_t i = 0; i < len; i++) 
  {
    sprintf(out + (i * 2), "%02X", data[i]);
  }
  out[len * 2] = '\0';
  return true;
}

// ---- Register Lookup ----
const RegisterDef* findRegister(RegID id) 
{
  for (size_t i = 0; i < REGISTER_COUNT; i++) 
  {
    if (REGISTER_MAP[i].id == id) return &REGISTER_MAP[i];
  }
  return nullptr;
}


// Read Request frame builder
bool buildReadFrame(uint8_t slave, const RegID* regs, size_t regCount, uint16_t& outStart, uint16_t& outCount, char* outHex, size_t outHexSize) 
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

  return toHex(frame, 8, outHex, outHexSize);
}


//Write Request Frame builder
bool buildWriteFrame(uint8_t slave, uint16_t regAddr, uint16_t value, char* outHex, size_t outHexSize) 
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

  return toHex(frame, 8, outHex, outHexSize);
}


//Set the output power 
bool setPower(uint16_t powerValue) 
{
  char frame[32];
  if (!buildWriteFrame(0x11, 8, powerValue, frame, sizeof(frame))) 
  {
    debug.log("Failed to build write frame\n");
    return false;
  }

  debug.log("Sending write frame: %s\n", frame);

  char responseFrame[128];

  bool okReq;

  if (WiFi.status() != WL_CONNECTED)
  {
    debug.log("WiFi not connected\n");
    okReq = false;
  }
  else
  {
    okReq = adapter.writeRegister(frame, responseFrame, sizeof(responseFrame));
  }

  if (!okReq) 
  {
    debug.log("Write request failed after retries.\n");
    return false;
  }

  if (strcmp(responseFrame, frame) == 0) 
  {
    debug.log("Power set to %u successfully\n", powerValue);
    return true;
  } 
  else 
  {
    debug.log("Failed to set power, response mismatch\n");
    debug.log("Raw response frame: %s\n", responseFrame);
    return false;
  }
}

/*


read request frame function to get the registers from user from main code and make relevant read request frame
and send to protocol adapter


*/
DecodedValues readRequest(const RegID* regs, size_t regCount) 
{
  adapter.setApiKey("NjhhZWIwNDU1ZDdmMzg3MzNiMTQ5YTFmOjY4YWViMDQ1NWQ3ZjM4NzMzYjE0OWExNQ==");

  DecodedValues result;
  result.count = 0;

  // build read frame
  uint16_t startAddr, count;
  char frame[64];
  if (!buildReadFrame(0x11, regs, regCount, startAddr, count, frame, sizeof(frame))) 
  {
    debug.log("Failed to build read frame.\n");
    result.values[0] = 0xFFFF; // Indicate error
    result.count = 1;
    return result;
  }

  // send request
  char responseFrame[256];
  
  bool ok;

  if (WiFi.status() != WL_CONNECTED)
  {
    debug.log("WiFi not connected\n");
    ok =  false;
  }
  else
  {
    ok = adapter.readRegister(frame, responseFrame, sizeof(responseFrame));
  }

  //if response is error return error values to main code
  if (!ok) 
  {
    debug.log("Read request failed after retries.\n");
    result.values[0] = 0xFFFF; // Indicate error
    result.count = 1;
    return result;
  }
  const char* response_frame = responseFrame;
  // decode response
  result = decodeReadResponse(response_frame, startAddr, count, regs, regCount);

  return result;
}




//Return decoded register values as an array to main code
const uint16_t* returnValues(const DecodedValues& decoded) 
{
  return decoded.values;
}



// Response Frame Decoder 
DecodedValues decodeReadResponse(const char* frameHex, uint16_t startAddr, uint16_t count, const RegID* regs, size_t regCount) 
{
  DecodedValues result;
  result.count = 0;

  if (!frameHex) return result;
  size_t len = strlen(frameHex);
  if (len < 10) return result;

  char buf[3]; buf[2] = '\0';
  memcpy(buf, frameHex + 2, 2);
  uint8_t func = (uint8_t)strtol(buf, NULL, 16);
  if (func != 0x03) return result;

  memcpy(buf, frameHex + 4, 2);
  uint8_t byteCount = (uint8_t)strtol(buf, NULL, 16);
  if (byteCount != count * 2) return result;

  uint16_t allRegs[64];
  for (uint16_t i = 0; i < count; i++) 
  {
    memcpy(buf, frameHex + 6 + i*4, 2);
    uint16_t hi = (uint16_t)strtol(buf, NULL, 16);
    memcpy(buf, frameHex + 8 + i*4, 2);
    uint16_t lo = (uint16_t)strtol(buf, NULL, 16);
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
