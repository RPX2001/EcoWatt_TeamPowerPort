
#include <ArduinoJson.h>
#include <Arduino.h>
#include "driver/protocol_adapter.h"
#include "peripheral/logger.h"
#include "application/fault_recovery.h"
#include "application/data_uploader.h"
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

ProtocolAdapter::ProtocolAdapter() {}

/**
 * @fn bool ProtocolAdapter::writeRegister(const char* frameHex, char* outFrameHex, size_t outSize)
 * 
 * @brief Write a register value to the device.
 * 
 * @param frameHex Hexadecimal frame data to send.
 * @param outFrameHex Buffer to store the response frame.
 * @param outSize Size of the response buffer.
 * @return true if successful, false otherwise.
 */
bool ProtocolAdapter::writeRegister(const char* frameHex, char* outFrameHex, size_t outSize)
{
  char responseJson[256];
  bool state = sendRequest(writeURL, frameHex, responseJson, sizeof(responseJson));
  if (!state) return false;

  state = parseResponse(responseJson, outFrameHex, outSize);
  
  // One-time retry for corrupted/failed packets
  if (!state)
  {
    LOG_WARN(LOG_TAG_MODBUS, "Write operation failed. Attempting ONE retry...");
    state = sendRequest(writeURL, frameHex, responseJson, sizeof(responseJson));
    if (state)
    {
      state = parseResponse(responseJson, outFrameHex, outSize);
    }
    
    if (!state)
    {
      LOG_ERROR(LOG_TAG_MODBUS, "Write retry failed. Packet DROPPED.");
    }
    else
    {
      LOG_SUCCESS(LOG_TAG_MODBUS, "Write operation successful on retry");
    }
  }
  
  return state;
}


/** 
 * @fn bool ProtocolAdapter::readRegister(const char* frameHex, char* outFrameHex, size_t outSize)
 * 
 * @brief Read a register value from the device.
 * 
 * @param frameHex Hexadecimal frame data to send.
 * @param outFrameHex Buffer to store the response frame.
 * @param outSize Size of the response buffer.
 * @return true if successful, false otherwise.
 */
bool ProtocolAdapter::readRegister(const char* frameHex, char* outFrameHex, size_t outSize)
{
  char responseJson[1024];
  bool state = sendRequest(readURL, frameHex, responseJson, sizeof(responseJson));
  if (!state) return false;

  state = parseResponse(responseJson, outFrameHex, outSize);
  
  // One-time retry for corrupted/failed packets
  if (!state)
  {
    LOG_WARN(LOG_TAG_MODBUS, "Read operation failed. Attempting ONE retry...");
    
    // MILESTONE 5: Send recovery event for fault detection
    FaultRecoveryEvent event;
    const char* deviceId = DataUploader::getDeviceID();
    strncpy(event.device_id, deviceId ? deviceId : "ESP32_UNKNOWN", sizeof(event.device_id));
    event.device_id[sizeof(event.device_id) - 1] = '\0';
    event.timestamp = getCurrentTimestamp(); // Use proper Unix timestamp
    event.fault_type = FaultType::MODBUS_EXCEPTION; // Most likely cause of parseResponse failure
    event.recovery_action = RecoveryAction::RETRY_READ;
    event.retry_count = 1;
    
    state = sendRequest(readURL, frameHex, responseJson, sizeof(responseJson));
    if (state)
    {
      state = parseResponse(responseJson, outFrameHex, outSize);
    }
    
    if (!state)
    {
      LOG_ERROR(LOG_TAG_MODBUS, "Read retry failed. Packet DROPPED.");
      event.success = false;
      snprintf(event.details, sizeof(event.details), "Modbus read failed after 1 retry");
    }
    else
    {
      LOG_SUCCESS(LOG_TAG_MODBUS, "Read operation successful on retry");
      event.success = true;
      snprintf(event.details, sizeof(event.details), "Modbus read recovered after 1 retry");
    }
    
    // Send recovery event to Flask
    sendRecoveryEvent(event);
  }
  
  return state;
}

//  Robust Send with Retry
/**
 * @fn bool ProtocolAdapter::sendRequest(const char* url, const char* frameHex, char* outResponseJson, size_t outSize)
 * 
 * @brief Send a JSON request to the specified URL with retry logic.
 * 
 * @param url The URL to send the request to.
 * @param frameHex The hexadecimal frame data to include in the request.
 * @param outResponseJson Buffer to store the JSON response.
 * @param outSize Size of the response buffer.
 * @return true if the request was successful, false otherwise.
 */
bool ProtocolAdapter::sendRequest(const char* url, const char* frameHex, char* outResponseJson, size_t outSize)
{
  HTTPClient http;
  int attempt = 0;
  int backoffDelay = 500; // ms

  while (attempt < maxRetries)
  {
    attempt++;
    if (!http.begin(url)) 
    {
      LOG_ERROR(LOG_TAG_MODBUS, "HTTP begin failed");
      return false;
    }
    http.setTimeout(httpTimeout);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("accept", "*/*");
    http.addHeader("Authorization", apiKey);

    char payload[256];
    snprintf(payload, sizeof(payload), "{\"frame\": \"%s\"}", frameHex);
    LOG_DEBUG(LOG_TAG_MODBUS, "Attempt %d: Sending frame: %s", attempt, frameHex);

    // Use String for HTTP POST, but only for the call
    String payloadStr(payload);
    int httpResponseCode = http.POST((uint8_t*)payloadStr.c_str(), payloadStr.length());
    bool ok = false;
    if (httpResponseCode > 0) 
    {
      //debug.log("Response code: %d\n", httpResponseCode);
      String response = http.getString();
      //debug.log("HTTP response: %s\n", response.c_str());
      // Convert String response to char array
      if (outResponseJson && outSize > 0) {
        size_t copyLen = (response.length() < (outSize-1)) ? response.length() : (outSize-1);
        memcpy(outResponseJson, response.c_str(), copyLen);
        outResponseJson[copyLen] = '\0';
      }
      ok = (response.length() > 0);
      http.end();  // Always close connection after getting response
      if (!ok) 
      {
        LOG_WARN(LOG_TAG_MODBUS, "Empty response, retrying...");
      } 
      else 
      {
        return true;
      }
    } 
    else 
    {
      LOG_WARN(LOG_TAG_MODBUS, "Request failed (code %d), retrying...", httpResponseCode);
      http.end();  // Always close connection even on failure
    }
    LOG_DEBUG(LOG_TAG_MODBUS, "Waiting %d ms before retry...", backoffDelay);
    wait.ms(backoffDelay);
    backoffDelay *= 2;
  }

  LOG_ERROR(LOG_TAG_MODBUS, "Failed after max retries.");
  return false;
}

//  Parse & Error Handling
/**
 * @fn bool ProtocolAdapter::parseResponse(const char* response, char* outFrameHex, size_t outSize)
 * 
 * @brief Parse the JSON response from the device.
 * 
 * @param response The JSON response string.
 * @param outFrameHex Buffer to store the extracted frame data.
 * @param outSize Size of the output buffer.
 * @return true if parsing was successful, false otherwise.
 */
bool ProtocolAdapter::parseResponse(const char* response, char* outFrameHex, size_t outSize) 
{

  size_t resp_len = response ? strlen(response) : 0;
  //debug.log("Raw HTTP response: %s\n", response ? response : "(null)");
  //debug.log("HTTP response length: %u\n", (unsigned)resp_len);
  //debug.log("HTTP response hex: ");
  for (size_t i = 0; i < resp_len && i < 32; ++i) {
    //debug.log("%02X ", (unsigned char)response[i]);
  }
  // Removed empty newline
  if (!response || response[0] == '\0') 
  {
    LOG_ERROR(LOG_TAG_MODBUS, "No response.");
    return false;
  }

  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, response);
  if (err) {
    LOG_ERROR(LOG_TAG_MODBUS, "JSON parse failed: Malformed response from inverter");
    return false;
  }
  const char* frame = doc["frame"] | "";
  size_t len = strlen(frame);
  if (len + 1 > outSize) {
    LOG_ERROR(LOG_TAG_MODBUS, "Frame too large for buffer");
    return false;
  }
  
  // CHECK FOR CORRUPTION
  if (isFrameCorrupted(frame)) 
  {
    LOG_ERROR(LOG_TAG_MODBUS, "PACKET CORRUPTED - Frame integrity check failed");
    LOG_ERROR(LOG_TAG_MODBUS, "   Corrupted frame: %s", frame);
    return false;  // Will trigger retry in caller
  }

  memcpy(outFrameHex, frame, len + 1);

  // Use enhanced validation with CRC checking
  ParseResult validationResult = validateModbusFrame(frame);
  
  switch (validationResult) {
    case PARSE_OK:
      LOG_DEBUG(LOG_TAG_MODBUS, "Valid Modbus frame (CRC verified).");
      return true;
      
    case PARSE_CRC_ERROR:
      LOG_ERROR(LOG_TAG_MODBUS, "Frame validation failed: CRC error");
      return false;
      
    case PARSE_MALFORMED:
      LOG_ERROR(LOG_TAG_MODBUS, "Frame validation failed: Malformed frame");
      return false;
      
    case PARSE_TRUNCATED:
      LOG_ERROR(LOG_TAG_MODBUS, "Frame validation failed: Truncated frame");
      return false;
      
    case PARSE_EXCEPTION:
      // Extract error code from exception frame
      if (len >= 6) {
        char buf[3]; buf[2] = '\0';
        memcpy(buf, frame + 4, 2);
        int errorCode = (int)strtol(buf, NULL, 16);
        LOG_ERROR(LOG_TAG_MODBUS, "Modbus Exception");
        printErrorCode(errorCode);
      }
      return false;
      
    default:
      LOG_ERROR(LOG_TAG_MODBUS, "Frame validation failed: Unknown error");
      return false;
  }
}

//  CRC Calculation (Modbus CRC16)
/**
 * @fn uint16_t ProtocolAdapter::calculateModbusCRC(const uint8_t* data, int length)
 * 
 * @brief Calculate Modbus CRC16 checksum.
 * 
 * @param data Pointer to data bytes.
 * @param length Number of bytes in data.
 * 
 * @return Calculated CRC16 value.
 */
uint16_t ProtocolAdapter::calculateModbusCRC(const uint8_t* data, int length) 
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

//  Frame Validation
/** 
 * @fn bool ProtocolAdapter::isFrameValid(const char* frame)
 * 
 * @brief Check if the given frame is valid.
 * 
 * @param frame The frame to check.
 * 
 * @return true if valid, false otherwise.
 */
bool ProtocolAdapter::isFrameValid(const char* frame) 
{
  if (!frame) return false;
  size_t len = strlen(frame);
  if (len < 6) return false;
  for (size_t i = 0; i < len; i++) 
  {
    char c = frame[i];
    if (!isxdigit(c)) return false;
  }
  return true;
}

//  Frame Corruption Detection
/**
 * @fn bool ProtocolAdapter::isFrameCorrupted(const char* frameHex)
 * 
 * @brief Detect if a Modbus frame is corrupted.
 * 
 * Checks for:
 * - Invalid hex characters
 * - Incorrect frame length
 * - CRC mismatch
 * 
 * @param frameHex The frame to check (hex string).
 * 
 * @return true if corrupted, false if valid.
 */
bool ProtocolAdapter::isFrameCorrupted(const char* frameHex) 
{
  if (!frameHex || frameHex[0] == '\0') 
  {
    LOG_ERROR(LOG_TAG_MODBUS, "Corruption detected: NULL or empty frame");
    return true;
  }

  size_t len = strlen(frameHex);
  
  // Check 1: Minimum length (slave + func + CRC = 8 hex chars minimum)
  if (len < 8) 
  {
    LOG_ERROR(LOG_TAG_MODBUS, "Corruption detected: Frame too short (%u bytes, min 8)", (unsigned)len);
    return true;
  }

  // Check 2: Must be even length (each byte = 2 hex chars)
  if (len % 2 != 0) 
  {
    LOG_ERROR(LOG_TAG_MODBUS, "Corruption detected: Odd frame length (%u chars)", (unsigned)len);
    return true;
  }

  // Check 3: Validate hex characters
  for (size_t i = 0; i < len; i++) 
  {
    if (!isxdigit(frameHex[i])) 
    {
      LOG_ERROR(LOG_TAG_MODBUS, "Corruption detected: Invalid hex char '%c' at position %u", frameHex[i], (unsigned)i);
      return true;
    }
  }

  // Check 4: Convert hex string to bytes
  size_t byteLen = len / 2;
  uint8_t frameBytes[256];
  if (byteLen > sizeof(frameBytes)) 
  {
    LOG_ERROR(LOG_TAG_MODBUS, "Corruption detected: Frame too large (%u bytes)", (unsigned)byteLen);
    return true;
  }

  for (size_t i = 0; i < byteLen; i++) 
  {
    char buf[3];
    buf[0] = frameHex[i * 2];
    buf[1] = frameHex[i * 2 + 1];
    buf[2] = '\0';
    frameBytes[i] = (uint8_t)strtol(buf, NULL, 16);
  }

  // Check 5: Validate Modbus function code (should be 0x03, 0x06, 0x10, etc.)
  uint8_t funcCode = frameBytes[1];
  if (funcCode == 0x00 || funcCode == 0xFF) 
  {
    LOG_ERROR(LOG_TAG_MODBUS, "Corruption detected: Invalid function code 0x%02X", funcCode);
    return true;
  }

  // Check 6: CRC validation (last 2 bytes are CRC in Modbus)
  if (byteLen >= 4) 
  {
    uint16_t receivedCRC = frameBytes[byteLen - 2] | (frameBytes[byteLen - 1] << 8);
    uint16_t calculatedCRC = calculateModbusCRC(frameBytes, byteLen - 2);
    
    if (receivedCRC != calculatedCRC) 
    {
      LOG_ERROR(LOG_TAG_MODBUS, "Corruption detected: CRC mismatch");
      LOG_ERROR(LOG_TAG_MODBUS, "  Expected CRC: 0x%04X", calculatedCRC);
      LOG_ERROR(LOG_TAG_MODBUS, "  Received CRC: 0x%04X", receivedCRC);
      LOG_ERROR(LOG_TAG_MODBUS, "  Frame: %s", frameHex);
      return true;
    }
  }

  // All checks passed - frame is valid
  return false;
}

//  Error Printer 
/**
 * @fn void ProtocolAdapter::printErrorCode(int code)
 * 
 * @brief Print a human-readable Modbus error message.
 * 
 * @param code The Modbus error code.
 */
void ProtocolAdapter::printErrorCode(int code) 
{
  switch (code) 
  {
    case 0x01: LOG_ERROR(LOG_TAG_MODBUS, "01 - Illegal Function"); break;
    case 0x02: LOG_ERROR(LOG_TAG_MODBUS, "02 - Illegal Data Address"); break;
    case 0x03: LOG_ERROR(LOG_TAG_MODBUS, "03 - Illegal Data Value"); break;
    case 0x04: LOG_ERROR(LOG_TAG_MODBUS, "04 - Slave Device Failure"); break;
    case 0x05: LOG_WARN(LOG_TAG_MODBUS, "05 - Acknowledge (processing delayed)"); break;
    case 0x06: LOG_WARN(LOG_TAG_MODBUS, "06 - Slave Device Busy"); break;
    case 0x08: LOG_ERROR(LOG_TAG_MODBUS, "08 - Memory Parity Error"); break;
    case 0x0A: LOG_ERROR(LOG_TAG_MODBUS, "0A - Gateway Path Unavailable"); break;
    case 0x0B: LOG_ERROR(LOG_TAG_MODBUS, "0B - Gateway Target Device Failed to Respond"); break;
    default:   LOG_ERROR(LOG_TAG_MODBUS, "Unknown error code"); break;
  }
}


/**
 * @fn ParseResult ProtocolAdapter::validateModbusFrame(const char* frameHex)
 * 
 * @brief Validate Modbus frame with detailed CRC and structure checking.
 * 
 * @param frameHex Hexadecimal frame string to validate.
 * @return ParseResult indicating specific validation result.
 */
ParseResult ProtocolAdapter::validateModbusFrame(const char* frameHex) 
{
  if (!frameHex) {
    LOG_ERROR(LOG_TAG_MODBUS, "Frame validation: NULL frame");
    return PARSE_MALFORMED;
  }
  
  size_t len = strlen(frameHex);
  
  // Check minimum length (at least: SlaveID(2) + Function(2) + CRC(4) = 8 hex chars)
  if (len < 8) {
    LOG_ERROR(LOG_TAG_MODBUS, "Frame validation: Too short (%zu bytes)", len);
    return PARSE_TRUNCATED;
  }
  
  // Check for valid hex characters
  for (size_t i = 0; i < len; i++) {
    char c = frameHex[i];
    if (!isxdigit(c)) {
      LOG_ERROR(LOG_TAG_MODBUS, "Frame validation: Invalid character at position %zu", i);
      return PARSE_MALFORMED;
    }
  }
  
  // Check if length is even (each byte = 2 hex chars)
  if (len % 2 != 0) {
    LOG_ERROR(LOG_TAG_MODBUS, "Frame validation: Odd length (%zu)", len);
    return PARSE_MALFORMED;
  }
  
  // Convert hex string to bytes for CRC calculation
  size_t byteLen = len / 2;
  uint8_t* bytes = new uint8_t[byteLen];
  
  for (size_t i = 0; i < byteLen; i++) {
    char byteStr[3] = {frameHex[i*2], frameHex[i*2+1], '\0'};
    bytes[i] = (uint8_t)strtol(byteStr, NULL, 16);
  }
  
  // Check for Modbus exception (function code & 0x80)
  if (byteLen >= 2 && (bytes[1] & 0x80)) {
    LOG_ERROR(LOG_TAG_MODBUS, "Frame validation: Modbus exception detected (function code: 0x%02X)", bytes[1]);
    delete[] bytes;
    return PARSE_EXCEPTION;
  }
  
  // Extract CRC from frame (last 2 bytes, little-endian)
  if (byteLen < 4) {
    LOG_ERROR(LOG_TAG_MODBUS, "Frame validation: Frame too short for CRC");
    delete[] bytes;
    return PARSE_TRUNCATED;
  }
  
  uint16_t receivedCRC = bytes[byteLen-2] | (bytes[byteLen-1] << 8);
  
  // Calculate CRC for frame data (excluding CRC bytes)
  uint16_t calculatedCRC = 0xFFFF;
  for (size_t i = 0; i < byteLen - 2; i++) {
    calculatedCRC ^= bytes[i];
    for (int j = 0; j < 8; j++) {
      if (calculatedCRC & 0x0001) {
        calculatedCRC >>= 1;
        calculatedCRC ^= 0xA001;
      } else {
        calculatedCRC >>= 1;
      }
    }
  }
  
  delete[] bytes;
  
  // Compare CRCs
  if (calculatedCRC != receivedCRC) {
    LOG_ERROR(LOG_TAG_MODBUS, "Frame validation: CRC mismatch (calculated: 0x%04X, received: 0x%04X)", 
              calculatedCRC, receivedCRC);
    return PARSE_CRC_ERROR;
  }
  
  LOG_SUCCESS(LOG_TAG_MODBUS, "Frame validation: OK (CRC: 0x%04X)", calculatedCRC);
  return PARSE_OK;
}


/**
 * @fn void ProtocolAdapter::setApiKey(const char* newApiKey)
 * 
 * @brief Set a new API key for authentication.
 * 
 * @param newApiKey The new API key string.
 */
void ProtocolAdapter::setApiKey(const char* newApiKey)
{
  if (!newApiKey) { apiKey[0] = '\0'; return; }
  strncpy(apiKey, newApiKey, sizeof(apiKey) - 1);
  apiKey[sizeof(apiKey) - 1] = '\0';
}

//  Getters 
/** 
 * @fn const char* ProtocolAdapter::getApiKey()
 * 
 * @brief Get the current API key.
 * 
 * @return The API key string.
 */
const char* ProtocolAdapter::getApiKey() 
{ 
  return apiKey; 
}