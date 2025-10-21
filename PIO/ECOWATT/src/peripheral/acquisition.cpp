#include "peripheral/acquisition.h"
#include "application/fault_recovery.h"

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
 * @brief Set the power output with fault recovery.
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
  uint8_t retryCount = 0;
  bool success = false;

  while (retryCount <= FaultRecovery::MAX_TIMEOUT_RETRIES && !success)
  {
    // Check WiFi connectivity
    if (WiFi.status() != WL_CONNECTED)
    {
      debug.log("WiFi not connected\n");
      FaultRecovery::logFault(FAULT_TIMEOUT, "WiFi disconnected during write request",
                              0, 0x11, 0x06, 8);
      return false;
    }

    bool okReq = adapter.writeRegister(frame, responseFrame, sizeof(responseFrame));

    if (!okReq) 
    {
      // Log timeout and retry
      if (FaultRecovery::handleTimeout(8, retryCount)) {
        retryCount++;
        continue;
      } else {
        debug.log("Write request failed after %d retries.\n", retryCount);
        return false;
      }
    }

    // Validate response
    if (strcmp(responseFrame, frame) == 0) 
    {
      success = true;
      
      if (retryCount > 0) {
        FaultRecovery::markRecovered();
        debug.log("✓ Write recovered after %d retries\n", retryCount);
      }
      
      debug.log("Power set to %u successfully\n", powerValue);
      return true;
    } 
    else 
    {
      debug.log("Failed to set power, response mismatch\n");
      debug.log("Expected: %s\n", frame);
      debug.log("Received: %s\n", responseFrame);
      
      FaultRecovery::logFault(FAULT_CORRUPT_RESPONSE, 
                              "Write response mismatch",
                              0, 0x11, 0x06, 8);
      
      if (retryCount < FaultRecovery::MAX_TIMEOUT_RETRIES) {
        retryCount++;
        delay(FaultRecovery::getRetryDelay(retryCount));
        continue;
      } else {
        return false;
      }
    }
  }

  return false;
}


/**
 * @fn DecodedValues readRequest(const RegID* regs, size_t regCount)
 * 
 * @brief Read specified registers from the inverter with fault recovery.
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

  // Retry loop with fault recovery
  uint8_t retryCount = 0;
  bool success = false;
  char responseFrame[256];
  
  while (retryCount <= FaultRecovery::MAX_TIMEOUT_RETRIES && !success) 
  {
    // Check WiFi connectivity
    if (WiFi.status() != WL_CONNECTED)
    {
      debug.log("WiFi not connected\n");
      FaultRecovery::logFault(FAULT_TIMEOUT, "WiFi disconnected during read request", 
                              0, 0x11, 0x03, startAddr);
      result.values[0] = 0xFFFF;
      result.count = 1;
      return result;
    }

    // Send Modbus request
    unsigned long requestStartTime = millis();
    bool ok = adapter.readRegister(frame, responseFrame, sizeof(responseFrame));
    unsigned long requestDuration = millis() - requestStartTime;
    
    // Check for excessive delay (>2 seconds)
    if (requestDuration > 2000) {
      FaultRecovery::handleDelay(1000, requestDuration);
    }

    if (!ok) 
    {
      // Log timeout and determine if should retry
      if (FaultRecovery::handleTimeout(startAddr, retryCount)) {
        retryCount++;
        continue;  // Retry
      } else {
        // Max retries exceeded
        debug.log("Read request failed after %d retries.\n", retryCount);
        result.values[0] = 0xFFFF;
        result.count = 1;
        return result;
      }
    }

    // Convert hex string to bytes for validation
    size_t frameLength = strlen(responseFrame) / 2;
    uint8_t frameBytes[128];
    
    if (frameLength > sizeof(frameBytes)) {
      FaultRecovery::logFault(FAULT_BUFFER_OVERFLOW, 
                              "Response frame too large", 
                              0, 0x11, 0x03, startAddr);
      result.values[0] = 0xFFFF;
      result.count = 1;
      return result;
    }

    // Convert hex string to bytes
    for (size_t i = 0; i < frameLength; i++) {
      char buf[3] = {responseFrame[i*2], responseFrame[i*2+1], '\0'};
      frameBytes[i] = (uint8_t)strtol(buf, NULL, 16);
    }

    // Check for malformed frame
    if (FaultRecovery::isMalformedFrame(frameBytes, frameLength)) {
      FaultRecovery::logFault(FAULT_MALFORMED_FRAME, 
                              "Invalid frame structure detected",
                              0, 0x11, 0x03, startAddr);
      
      if (retryCount < FaultRecovery::MAX_TIMEOUT_RETRIES) {
        retryCount++;
        delay(FaultRecovery::getRetryDelay(retryCount));
        continue;
      } else {
        result.values[0] = 0xFFFF;
        result.count = 1;
        return result;
      }
    }

    // Check for Modbus exception
    uint8_t exceptionCode = 0;
    if (FaultRecovery::isModbusException(frameBytes, frameLength, &exceptionCode)) {
      bool shouldRetry = FaultRecovery::handleModbusException(exceptionCode, 0x11, 0x03, startAddr);
      
      if (shouldRetry && retryCount < FaultRecovery::MAX_EXCEPTION_RETRIES) {
        retryCount++;
        continue;
      } else {
        result.values[0] = 0xFFFF;
        result.count = 1;
        return result;
      }
    }

    // Validate CRC
    if (!FaultRecovery::validateCRC(frameBytes, frameLength)) {
      if (FaultRecovery::handleCRCError(frameBytes, frameLength, retryCount)) {
        retryCount++;
        continue;
      } else {
        result.values[0] = 0xFFFF;
        result.count = 1;
        return result;
      }
    }

    // If we got here, response is valid
    success = true;
    
    // Mark recovery if there were previous retries
    if (retryCount > 0) {
      FaultRecovery::markRecovered();
      debug.log("✓ Successfully recovered after %d retries\n", retryCount);
    }
  }

  // Decode valid response
  const char* response_frame = responseFrame;
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