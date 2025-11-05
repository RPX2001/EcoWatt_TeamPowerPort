#include "peripheral/acquisition.h"
#include "peripheral/logger.h"
#include "application/fault_recovery.h" // Milestone 5: Fault Recovery
#include "application/data_uploader.h"  // For getDeviceID()
#include <functional>  // For std::bind to reduce stack usage
#include <time.h>

// Helper to get current Unix timestamp (same as data_uploader)
static unsigned long getCurrentTimestamp() {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        time_t now = mktime(&timeinfo);
        return (unsigned long)now;
    }
    time_t now = time(nullptr);
    if (now > 1000000000) { // Sanity check
        return (unsigned long)now;
    }
    return millis() / 1000; // Last resort
}

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
    LOG_ERROR(LOG_TAG_MODBUS, "Failed to build write frame");
    return false;
  }

  LOG_DEBUG(LOG_TAG_MODBUS, "Sending write frame: %s", frame);

  char responseFrame[128];

  bool okReq;

  if (WiFi.status() != WL_CONNECTED)
  {
    LOG_WARN(LOG_TAG_MODBUS, "WiFi not connected");
    okReq = false;
  }
  else
  {
    okReq = adapter.writeRegister(frame, responseFrame, sizeof(responseFrame));
  }

  if (!okReq) 
  {
    LOG_ERROR(LOG_TAG_MODBUS, "Write request failed after retries");
    return false;
  }

  if (strcmp(responseFrame, frame) == 0) 
  {
    LOG_SUCCESS(LOG_TAG_MODBUS, "Power set to %u successfully", powerValue);
    return true;
  } 
  else 
  {
    LOG_ERROR(LOG_TAG_MODBUS, "Failed to set power, response mismatch");
    LOG_DEBUG(LOG_TAG_MODBUS, "Raw response frame: %s", responseFrame);
    return false;
  }
}


/**
 * @fn static bool retryModbusRead(const char* frame, char* responseFrame, size_t responseSize, uint8_t expectedByteCount)
 * 
 * @brief Helper function to retry Modbus read and validate response
 */
static bool retryModbusRead(const char* frame, char* responseFrame, size_t responseSize, uint8_t expectedByteCount) {
  static char retryResponse[256]; // Static to reduce stack usage
  
  bool retryOk = adapter.readRegister(frame, retryResponse, sizeof(retryResponse));
  
  if (!retryOk) return false;
  
  // Check if retry response is fault-free
  FaultType retryFault = detectFault(retryResponse, expectedByteCount, sizeof(retryResponse));
  
  if (retryFault == FaultType::NONE) {
    // Success! Copy response to output buffer
    strncpy(responseFrame, retryResponse, responseSize);
    responseFrame[responseSize - 1] = '\0';
    return true;
  }
  
  return false; // Still faulty
}

/**
 * @fn DecodedValues readRequest(const RegID* regs, size_t regCount)
 * 
 * @brief Read specified registers from the inverter with fault detection and recovery (Milestone 5).
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
    LOG_ERROR(LOG_TAG_MODBUS, "Failed to build read frame");
    result.values[0] = 0xFFFF; // Indicate error
    result.count = 1;
    return result;
  }

  // Calculate expected byte count for validation
  uint8_t expectedByteCount = count * 2; // Each register is 2 bytes

  // send request
  char responseFrame[256];
  
  bool ok;

  if (WiFi.status() != WL_CONNECTED)
  {
    LOG_WARN(LOG_TAG_MODBUS, "WiFi not connected");
    ok =  false;
  }
  else
  {
    ok = adapter.readRegister(frame, responseFrame, sizeof(responseFrame));
  }

  //if response is error return error values to main code
  if (!ok) 
  {
    LOG_ERROR(LOG_TAG_MODBUS, "Read request failed after retries");
    
    // MILESTONE 5: Report timeout fault
    FaultRecoveryEvent event;
    const char* deviceId = DataUploader::getDeviceID();
    strncpy(event.device_id, deviceId ? deviceId : "ESP32_UNKNOWN", sizeof(event.device_id));
    event.device_id[sizeof(event.device_id) - 1] = '\0';
    event.timestamp = getCurrentTimestamp(); // Use proper Unix timestamp
    event.fault_type = FaultType::TIMEOUT;
    event.recovery_action = RecoveryAction::RETRY_READ;
    event.success = false;
    event.retry_count = 0;
    snprintf(event.details, sizeof(event.details), "Modbus read timeout, WiFi or network error");
    
    sendRecoveryEvent(event);
    
    result.values[0] = 0xFFFF; // Indicate error
    result.count = 1;
    return result;
  }
  
  const char* response_frame = responseFrame;
  
  // MILESTONE 5: Detect faults in response
  FaultType fault = detectFault(response_frame, expectedByteCount, sizeof(responseFrame));
  
  if (fault != FaultType::NONE) {
    LOG_WARN(LOG_TAG_FAULT, "Fault detected: %s", getFaultTypeName(fault));
    
    // Execute recovery with retries (use std::bind to avoid lambda overhead)
    uint8_t retryCount = 0;
    bool recoverySuccess = false;
    
    // Use function pointer with std::bind to reduce stack usage
    std::function<bool()> retryFunc = std::bind(retryModbusRead, frame, responseFrame, sizeof(responseFrame), expectedByteCount);
    
    recoverySuccess = executeRecovery(fault, retryFunc, retryCount);
    
    // Report recovery event
    FaultRecoveryEvent event;
    const char* deviceId = DataUploader::getDeviceID();
    strncpy(event.device_id, deviceId ? deviceId : "ESP32_UNKNOWN", sizeof(event.device_id));
    event.device_id[sizeof(event.device_id) - 1] = '\0';
    event.timestamp = getCurrentTimestamp(); // Use proper Unix timestamp
    event.fault_type = fault;
    event.recovery_action = RecoveryAction::RETRY_READ;
    event.success = recoverySuccess;
    event.retry_count = retryCount;
    
    if (recoverySuccess) {
      snprintf(event.details, sizeof(event.details), 
               "%s detected and recovered after %u retries", 
               getFaultTypeName(fault), retryCount);
    } else {
      snprintf(event.details, sizeof(event.details), 
               "%s detected, recovery FAILED after %u retries", 
               getFaultTypeName(fault), retryCount);
    }
    
    sendRecoveryEvent(event);
    
    if (!recoverySuccess) {
      // Recovery failed - return error
      result.values[0] = 0xFFFF;
      result.count = 1;
      return result;
    }
    
    // Recovery succeeded - continue with corrected response
    response_frame = responseFrame;
  }
  
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