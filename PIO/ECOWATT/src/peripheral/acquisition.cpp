#include "peripheral/acquisition.h"
#include "application/fault_handler.h"
#include "application/fault_logger.h"

ProtocolAdapter adapter;

/**
 * @fn uint16_t calculateCRC(const uint8_t* data, int length)
 * 
 * @brief Calculate Modbus CRC16 checksum.
 * 
 * @param data Pointer to data bytes.
 * @param length Number of bytes in data.
 * 
 * @return Calculated CRC16 value.
 */
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


/**
 * @fn bool toHex(const uint8_t* data, size_t len, char* out, size_t outSize)
 * 
 * @brief Convert binary data to hexadecimal string.
 * 
 * @param data Pointer to binary data.
 * @param len Length of binary data in bytes.
 * @param out Output buffer to receive hex string.
 * @param outSize Size of the output buffer.
 * 
 * @return true if conversion successful, false if output buffer too small.
 */
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


/**
 * @fn const RegisterDef* findRegister(RegID id)
 * 
 * @brief Find register definition by RegID.
 * 
 * @param id The RegID to look up.
 * 
 * @return Pointer to RegisterDef if found, nullptr if not found.
 */
const RegisterDef* findRegister(RegID id) 
{
  for (size_t i = 0; i < REGISTER_COUNT; i++) 
  {
    if (REGISTER_MAP[i].id == id) return &REGISTER_MAP[i];
  }
  return nullptr;
}


/**
 * @fn bool buildReadFrame(uint8_t slave, const RegID* regs, size_t regCount, uint16_t& outStart, uint16_t& outCount, char* outHex, size_t outHexSize)
 * 
 * @brief Build a Modbus read frame for a given selection of registers.
 * 
 * @param slave Inverter address (usually 0x11).
 * @param regs Array of requested RegIDs.
 * @param regCount Number of registers requested.
 * @param outStart Output parameter to receive start address of contiguous block.
 * @param outCount Output parameter to receive number of registers in block.
 * @param outHex Output buffer to receive the frame as HEX C-string (null-terminated)
 * @param outHexSize Size of the output buffer.
 * 
 * @return true if frame built successfully, false if error.
 */
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


/**
 * @fn bool buildWriteFrame(uint8_t slave, uint16_t regAddr, uint16_t value, char* outHex, size_t outHexSize)
 * 
 * @brief Build a Modbus write frame to set a single register.
 * 
 * @param slave Inverter address (usually 0x11).
 * @param regAddr Register address to write.
 * @param value Value to write to the register.
 * @param outHex Output buffer to receive the frame as HEX C-string (null-terminated).
 * @param outHexSize Size of the output buffer.
 * 
 * @return true if frame built successfully, false if error.
 */
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


/**
 * @fn bool setPower(uint16_t powerValue)
 * 
 * @brief Set the power output.
 * 
 * @param powerValue Power value to set.
 * 
 * @return true if power set successfully, false if error.
 */
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


/**
 * @fn DecodedValues readRequest(const RegID* regs, size_t regCount)
 * 
 * @brief Read specified registers from the inverter.
 * 
 * @param regs Array of RegIDs to read.
 * @param regCount Number of registers to read.
 * 
 * @return DecodedValues struct containing the read values or error indication.
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

  // send request without internal retry handling
  char responseFrame[256];
  
  bool ok;

  if (WiFi.status() != WL_CONNECTED)
  {
    debug.log("WiFi not connected\n");
    ok =  false;
  }
  else
  {
    // Note: adapter.readRegister() will log "Attempt 1: Sending..." internally
    ok = adapter.readRegister(frame, responseFrame, sizeof(responseFrame));
    
    // ALWAYS validate the frame, even if adapter says ok
    // (adapter might return true even with error frames after protocol_adapter modification)
    if (strlen(responseFrame) > 0) {
      // Convert hex string to bytes for validation
      size_t hex_len = strlen(responseFrame);
      uint8_t response_bytes[128];
      size_t byte_count = hex_len / 2;
      
      for (size_t i = 0; i < byte_count; i++) {
        char byte_str[3] = {responseFrame[i*2], responseFrame[i*2+1], '\0'};
        response_bytes[i] = (uint8_t)strtol(byte_str, NULL, 16);
      }
      
      // Validate the response frame
      FrameValidation validation = FaultHandler::validateModbusFrame(
        response_bytes,
        byte_count,
        0x11,  // expected slave address
        0x03   // expected function code (read holding registers)
      );
      
      // If fault detected, attempt recovery
      if (validation.result != ValidationResult::VALID) {
        debug.log("\n");
        Serial.printf("  [ERROR] FAULT DETECTED: %s\n", validation.error_description.c_str());
        Serial.printf("  Frame: ");
        for (size_t i = 0; i < byte_count && i < 16; i++) {
          Serial.printf("%02X", response_bytes[i]);
        }
        Serial.printf("\n");
        Serial.printf("  Recoverable: %s\n", validation.recovered ? "YES" : "NO");
        
        if (validation.recovered) {
          // Retry with exponential backoff
          for (int retry = 0; retry < 3; retry++) {
            unsigned long delay_ms = FaultHandler::getRetryDelay(validation, retry);
            Serial.printf("  [INFO] Recovery attempt %d after %lu ms delay...\n", retry + 1, delay_ms);
            delay(delay_ms);
            
            ok = adapter.readRegister(frame, responseFrame, sizeof(responseFrame));
            
            if (strlen(responseFrame) > 0) {
              // Convert hex string to bytes for retry validation
              hex_len = strlen(responseFrame);
              byte_count = hex_len / 2;
              
              for (size_t i = 0; i < byte_count; i++) {
                char byte_str[3] = {responseFrame[i*2], responseFrame[i*2+1], '\0'};
                response_bytes[i] = (uint8_t)strtol(byte_str, NULL, 16);
              }
              
              // Validate retry response
              validation = FaultHandler::validateModbusFrame(
                response_bytes,
                byte_count,
                0x11,
                0x03
              );
              
              if (validation.result == ValidationResult::VALID) {
                Serial.printf("  [SUCCESS] ✓ Recovery successful!\n\n");
                ok = true;
                break;
              } else {
                Serial.printf("  [WARN] Retry %d still has error: %s\n", retry + 1, validation.error_description.c_str());
              }
            }
          }
          
          if (validation.result != ValidationResult::VALID) {
            Serial.printf("  [ERROR] ✗ Recovery failed after 3 retries\n\n");
            ok = false;
          }
        } else {
          Serial.printf("  [ERROR] ✗ Non-recoverable error, no retry attempted\n\n");
          ok = false;
        }
      }
    }
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

/**
 * @fn const uint16_t* returnValues(const DecodedValues& decoded)
 * 
 * @brief Return pointer to decoded register values array.
 * 
 * @param decoded DecodedValues struct containing the values.
 * 
 * @return Pointer to the array of decoded register values.
 */
const uint16_t* returnValues(const DecodedValues& decoded) 
{
  return decoded.values;
}


/**
 * @fn DecodedValues decodeReadResponse(const char* frameHex, uint16_t startAddr, uint16_t count, const RegID* regs, size_t regCount)
 * 
 * @brief Decode a Modbus read response frame.
 * 
 * @param frameHex Inverter response (HEX C-string).
 * @param startAddr Start of requested block.
 * @param count Number of registers read in block.
 * @param regs Array of requested RegIDs.
 * @param regCount Number of registers requested.
 * 
 * @return DecodedValues struct containing only the requested registers.
 */
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