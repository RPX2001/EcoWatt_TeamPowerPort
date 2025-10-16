
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
  if (!state)
  {
    debug.log("Write operation failed. Then Retry\n");
    int retry = 1;
    while (retry <= 3 && !state)
    {
      debug.log("Retry attempt %d\n", retry);
      state = sendRequest(writeURL, frameHex, responseJson, sizeof(responseJson));
      if (state)
      {
        state = parseResponse(responseJson, outFrameHex, outSize);
      }
      if (!state)
      {
        retry++;
      }
      else
      {
        debug.log("Write operation successful on retry\n");
        break;
      }
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
  if (!state)
  {
    debug.log("Read operation failed. Then Retry\n");
    int retry = 1;
    while (retry <= 3 && !state)
    {
      debug.log("Retry attempt %d\n", retry);
      state = sendRequest(readURL, frameHex, responseJson, sizeof(responseJson));
      if (state)
      {
        state = parseResponse(responseJson, outFrameHex, outSize);
      }
      if (!state)
      {
        retry++;
      }
      else
      {
        debug.log("Read operation successful on retry\n");
        break;
      }
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
    debug.log("JSON parse failed\n");
    return false;
  }
  const char* frame = doc["frame"] | "";
  size_t len = strlen(frame);
  if (len + 1 > outSize) {
    debug.log("Frame too large for buffer\n");
    return false;
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