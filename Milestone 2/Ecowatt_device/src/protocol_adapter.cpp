#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "protocol_adapter.h"

ProtocolAdapter::ProtocolAdapter() {}

//initialization wifi connection
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

//write to register in server
String ProtocolAdapter::writeRegister(String frame) {
  String response = sendRequest(writeURL, frame);
  parseResponse(response);
  return response;
}

//read from register in server
String ProtocolAdapter::readRegister(String frame) {
  String response = sendRequest(readURL, frame);
  parseResponse(response);
  return response;
}

//send HTTP POST request to server
String ProtocolAdapter::sendRequest(String url, String frame) {
  //check WiFi connection status
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected");
    begin();
  }

  //making payload and header structure
  HTTPClient http;
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("accept", "*/*");
  http.addHeader("Authorization", apiKey);

  String payload = "{\"frame\": \"" + frame + "\"}";
  Serial.println("Sending: " + payload);

  //send request and get response
  int httpResponseCode = http.POST(payload);
  String response = "";

  if (httpResponseCode > 0) {
    response = http.getString();
    Serial.println("Response code: " + String(httpResponseCode));
    Serial.println("Raw response: " + response);
  } else {
    Serial.println("Error in request: " + String(httpResponseCode));
  }

  http.end();
  return response;
}

void ProtocolAdapter::parseResponse(String response) {
  if (response == "") {
    Serial.println("Empty response!");
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
  if (frame == "") {
    Serial.println("Invalid or empty frame in response.");
    return;
  }

  Serial.println("Received frame: " + frame);

  if (frame.length() >= 4) {
    int funcCode = strtol(frame.substring(2,4).c_str(), NULL, 16);

    if (funcCode & 0x80) {
      int errorCode = strtol(frame.substring(4,6).c_str(), NULL, 16);
      Serial.print("Modbus Exception: ");
      printErrorCode(errorCode);
    } else {
      Serial.println("Valid Modbus frame.");
    }
  }
}

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
