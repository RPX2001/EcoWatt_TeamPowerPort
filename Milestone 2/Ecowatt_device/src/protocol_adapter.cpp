#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Arduino.h>
#include "protocol_adapter.h"

ProtocolAdapter::ProtocolAdapter() {}

void ProtocolAdapter::begin() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);

  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" Connected!");
}

String ProtocolAdapter::writeRegister(String frame) {
  String response = sendRequest(writeURL, frame);
  parseResponse(response);
  return response;
}

String ProtocolAdapter::readRegister(String frame) {
  String response = sendRequest(readURL, frame);
  parseResponse(response);
  return response;
}

String ProtocolAdapter::make_Payload(String frame){
  return "{\"frame\": \"" + frame + "\"}";
}

//  Robust Send with Retry 
String ProtocolAdapter::sendRequest(String url, String frame) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected");
    return "";
  }

  HTTPClient http;
  String response = "";
  int attempt = 0;
  int backoffDelay = 500; // start with 500ms


  //transmit and retry logic with exponential backoff time..
  while (attempt < maxRetries) {
    attempt++;
    http.begin(url);
    http.setTimeout(httpTimeout);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("accept", "*/*");
    http.addHeader("Authorization", apiKey);

    //String payload = "{\"frame\": \"" + frame + "\"}";
    String payload = make_Payload(frame);
    Serial.printf("Attempt %d: Sending %s\n", attempt, payload.c_str());

    int httpResponseCode = http.POST(payload);

    if (httpResponseCode > 0) {
      response = http.getString();
      Serial.printf("Response code: %d\n", httpResponseCode);
      Serial.println("Raw response: " + response);

      if (response != "") {
        http.end();
        return response; // success
      } else {
        Serial.println("Empty response, retrying...");
      }
    } else {
      Serial.printf("Request failed (code %d), retrying...\n", httpResponseCode);
    }

    http.end();

    // exponential backoff
    Serial.printf("Waiting %d ms before retry...\n", backoffDelay);
    delay(backoffDelay);
    backoffDelay *= 2; // double the delay
  }

  Serial.println("Failed after max retries.");
  return "";
}

//  Parse & Error Handling 
void ProtocolAdapter::parseResponse(String response) {
  if (response == "") {
    Serial.println("No response.");
    return;
  }

  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, response);

  if (error) {
    Serial.print("JSON parse failed: ");
    Serial.println(error.c_str());
    return;
  }

  String frame = doc["frame"];
  // if (!isFrameValid(frame)) {
  //   Serial.println("Malformed or empty frame.");
  //   return;
  // }

  Serial.println("Received frame: " + frame);

  // Modbus function code check
  int funcCode = strtol(frame.substring(2,4).c_str(), NULL, 16);
  if (funcCode & 0x80) {
    int errorCode = strtol(frame.substring(4,6).c_str(), NULL, 16);
    Serial.print("Modbus Exception: ");
    printErrorCode(errorCode);
  } else {
    Serial.println("Valid Modbus frame.");
  }
}

//  Frame Validation 
bool ProtocolAdapter::isFrameValid(String frame) {
  // minimum valid Modbus RTU frame = 6 hex chars (3 bytes)
  if (frame.length() < 6) return false;

  // ensure only hex chars
  for (int i = 0; i < frame.length(); i++) {
    char c = frame.charAt(i);
    if (!isxdigit(c)) return false;
  }
  return true;
}

//  Error Printer 
void ProtocolAdapter::printErrorCode(int code) {
  switch (code) {
    case 0x01: Serial.println("01 - Illegal Function"); break;
    case 0x02: Serial.println("02 - Illegal Data Address"); break;
    case 0x03: Serial.println("03 - Illegal Data Value"); break;
    case 0x04: Serial.println("04 - Slave Device Failure"); break;
    case 0x05: Serial.println("05 - Acknowledge (processing delayed)"); break;
    case 0x06: Serial.println("06 - Slave Device Busy"); break;
    case 0x08: Serial.println("08 - Memory Parity Error"); break;
    case 0x0A: Serial.println("0A - Gateway Path Unavailable"); break;
    case 0x0B: Serial.println("0B - Gateway Target Device Failed to Respond"); break;
    default:   Serial.println("Unknown error code"); break;
  }
}

// Setters 
void ProtocolAdapter::setSSID(const char* newSSID) { 
  ssid = newSSID; 
}
void ProtocolAdapter::setPassword(const char* newPassword) { 
  password = newPassword; 
}
void ProtocolAdapter::setApiKey(String newApiKey) { 
  apiKey = newApiKey; 
}

//  Getters 
String ProtocolAdapter::getSSID() { 
  return String(ssid); 
}
String ProtocolAdapter::getPassword() { 
  return String(password); 
}
String ProtocolAdapter::getApiKey() { 
  return apiKey; 
}
