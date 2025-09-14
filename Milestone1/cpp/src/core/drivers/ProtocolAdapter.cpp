#include "ProtocolAdapter.h"


ProtocolAdapter::ProtocolAdapter() {}


bool ProtocolAdapter::begin() 
{
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    time.wait(500);
  }
  return true;
}

int ProtocolAdapter::writeRegister(const String& frame, String& response) 
{
  response = sendRequest(writeURL, frame);
  int err = parseResponse(response);
  return err;
}

String ProtocolAdapter::readRegister(const String& frame, String& response) 
{
  response = sendRequest(readURL, frame);
  int err = parseResponse(response);
  return err;
}

//  Parse & Error Handling 
int ProtocolAdapter::parseResponse(String response) 
{
  if (response == "") return 504;

  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, response);

  if (error) return 422;

  String frame = doc["frame"];
  // if (!isFrameValid(frame)) return 422;

  Serial.println("Received frame: " + frame);

  // Modbus function code check
  int funcCode = strtol(frame.substring(2,4).c_str(), NULL, 16);
  if (funcCode & 0x80)
  {
    int errorCode = strtol(frame.substring(4,6).c_str(), NULL, 16);
    return errorCode;
  } 
  else 
  {
    return 200;
  }
}

//  Frame Validation 
bool ProtocolAdapter::isFrameValid(String frame) {
  // minimum valid Modbus RTU frame = 6 hex chars (3 bytes)
  if (frame.length() < 6) return false;

  // ensure only hex chars
  for (int i = 0; i < frame.length(); i++) 
  {
    char c = frame.charAt(i);
    if (!isxdigit(c)) return false;
  }
  return true;
}

// Setters 
void ProtocolAdapter::setSSID(const char* newSSID) 
{ 
  ssid = newSSID; 
}

void ProtocolAdapter::setPassword(const char* newPassword) 
{ 
  password = newPassword; 
}

void ProtocolAdapter::setApiKey(String newApiKey) 
{ 
  apiKey = newApiKey; 
}

//  Getters 
String ProtocolAdapter::getSSID() 
{ 
  return String(ssid); 
}

String ProtocolAdapter::getPassword() 
{ 
  return String(password); 
}

String ProtocolAdapter::getApiKey() 
{ 
  return apiKey; 
}
