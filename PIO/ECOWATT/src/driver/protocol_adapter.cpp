
#include <ArduinoJson.h>
#include <Arduino.h>
#include "driver/protocol_adapter.h"

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
    debug.log("Write operation failed. Attempting ONE retry...\n");
    state = sendRequest(writeURL, frameHex, responseJson, sizeof(responseJson));
    if (state)
    {
      state = parseResponse(responseJson, outFrameHex, outSize);
    }
    
    if (!state)
    {
      debug.log("❌ Write retry failed. Packet DROPPED.\n");
    }
    else
    {
      debug.log("✓ Write operation successful on retry\n");
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
    debug.log("Read operation failed. Attempting ONE retry...\n");
    state = sendRequest(readURL, frameHex, responseJson, sizeof(responseJson));
    if (state)
    {
      state = parseResponse(responseJson, outFrameHex, outSize);
    }
    
    if (!state)
    {
      debug.log("❌ Read retry failed. Packet DROPPED.\n");
    }
    else
    {
      debug.log("✓ Read operation successful on retry\n");
    }
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
      debug.log("HTTP begin failed\n");
      return false;
    }
    http.setTimeout(httpTimeout);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("accept", "*/*");
    http.addHeader("Authorization", apiKey);

    char payload[256];
    snprintf(payload, sizeof(payload), "{\"frame\": \"%s\"}", frameHex);
    debug.log("Attempt %d: Sending %s\n", attempt, payload);

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
        debug.log("Empty response, retrying...\n");
      } 
      else 
      {
        return true;
      }
    } 
    else 
    {
      debug.log("Request failed (code %d), retrying...\n", httpResponseCode);
      http.end();  // Always close connection even on failure
    }
    debug.log("Waiting %d ms before retry...\n", backoffDelay);
    wait.ms(backoffDelay);
    backoffDelay *= 2;
  }

  debug.log("Failed after max retries.\n");
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
  debug.log("\n");
  if (!response || response[0] == '\0') 
  {
    debug.log("No response.\n");
    return false;
  }

  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, response);
  if (err) {
    debug.log("JSON parse failed: Malformed response from inverter\n");
    return false;
  }
  const char* frame = doc["frame"] | "";
  size_t len = strlen(frame);
  if (len + 1 > outSize) {
    debug.log("Frame too large for buffer\n");
    return false;
  }
  
  // CHECK FOR CORRUPTION
  if (isFrameCorrupted(frame)) 
  {
    debug.log("PACKET CORRUPTED - Frame integrity check failed\n");
    debug.log("   Corrupted frame: %s\n", frame);
    return false;  // Will trigger retry in caller
  }

  memcpy(outFrameHex, frame, len + 1);
  debug.log("Received frame: %s\n", frame);

  // Modbus function code check
  if (len < 6) return false;
  char buf[3]; buf[2] = '\0';
  memcpy(buf, frame + 2, 2);
  int funcCode = (int)strtol(buf, NULL, 16);
  if (funcCode & 0x80) {
    memcpy(buf, frame + 4, 2);
    int errorCode = (int)strtol(buf, NULL, 16);
    debug.log("Modbus Exception: ");
    printErrorCode(errorCode);
    return false;
  } else {
    debug.log("Valid Modbus frame.\n");
    return true;
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
    debug.log("Corruption detected: NULL or empty frame\n");
    return true;
  }

  size_t len = strlen(frameHex);
  
  // Check 1: Minimum length (slave + func + CRC = 8 hex chars minimum)
  if (len < 8) 
  {
    debug.log("Corruption detected: Frame too short (%u bytes, min 8)\n", (unsigned)len);
    return true;
  }

  // Check 2: Must be even length (each byte = 2 hex chars)
  if (len % 2 != 0) 
  {
    debug.log("Corruption detected: Odd frame length (%u chars)\n", (unsigned)len);
    return true;
  }

  // Check 3: Validate hex characters
  for (size_t i = 0; i < len; i++) 
  {
    if (!isxdigit(frameHex[i])) 
    {
      debug.log("Corruption detected: Invalid hex char '%c' at position %u\n", frameHex[i], (unsigned)i);
      return true;
    }
  }

  // Check 4: Convert hex string to bytes
  size_t byteLen = len / 2;
  uint8_t frameBytes[256];
  if (byteLen > sizeof(frameBytes)) 
  {
    debug.log("Corruption detected: Frame too large (%u bytes)\n", (unsigned)byteLen);
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
    debug.log("Corruption detected: Invalid function code 0x%02X\n", funcCode);
    return true;
  }

  // Check 6: CRC validation (last 2 bytes are CRC in Modbus)
  if (byteLen >= 4) 
  {
    uint16_t receivedCRC = frameBytes[byteLen - 2] | (frameBytes[byteLen - 1] << 8);
    uint16_t calculatedCRC = calculateModbusCRC(frameBytes, byteLen - 2);
    
    if (receivedCRC != calculatedCRC) 
    {
      debug.log("Corruption detected: CRC mismatch!\n");
      debug.log("   Expected CRC: 0x%04X\n", calculatedCRC);
      debug.log("   Received CRC: 0x%04X\n", receivedCRC);
      debug.log("   Frame: %s\n", frameHex);
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
    case 0x01: debug.log("01 - Illegal Function\n"); break;
    case 0x02: debug.log("02 - Illegal Data Address\n"); break;
    case 0x03: debug.log("03 - Illegal Data Value\n"); break;
    case 0x04: debug.log("04 - Slave Device Failure\n"); break;
    case 0x05: debug.log("05 - Acknowledge (processing delayed)\n"); break;
    case 0x06: debug.log("06 - Slave Device Busy\n"); break;
    case 0x08: debug.log("08 - Memory Parity Error\n"); break;
    case 0x0A: debug.log("0A - Gateway Path Unavailable\n"); break;
    case 0x0B: debug.log("0B - Gateway Target Device Failed to Respond\n"); break;
    default:   debug.log("Unknown error code\n"); break;
  }
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