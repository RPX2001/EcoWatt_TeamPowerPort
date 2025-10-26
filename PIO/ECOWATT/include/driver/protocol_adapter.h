

#ifndef PROTOCOL_ADAPTER_H
#define PROTOCOL_ADAPTER_H

#include "hal/esp_arduino.h"

#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <string.h>

#include "delay.h"
#include "debug.h"

class ProtocolAdapter 
{
  public:
    ProtocolAdapter();
    bool writeRegister(const char* frameHex, char* outFrameHex, size_t outSize); // send write and return extracted frame
    bool readRegister(const char* frameHex, char* outFrameHex, size_t outSize);  // send read and return extracted frame  
    bool parseResponse(const char* responseJson, char* outFrameHex, size_t outSize); // parse JSON response
    
    
    //setters
    void setApiKey(const char* newApiKey);

    // Getters
    const char* getApiKey();

  private:
    // keep  default WiFi & API key private
    const char* ssid;
    const char* password;
    char apiKey[128];

    const char* writeURL = "http://20.15.114.131:8080/api/inverter/write";
    const char* readURL  = "http://20.15.114.131:8080/api/inverter/read";

    // retry & timeout
    const int maxRetries = 3;
    const int httpTimeout = 5000; // ms

    bool sendRequest(const char* url, const char* frameHex, char* outResponseJson, size_t outSize);
    void printErrorCode(int code);
    bool isFrameValid(const char* frameHex);
    bool isFrameCorrupted(const char* frameHex);
    uint16_t calculateModbusCRC(const uint8_t* data, int length);
};

#endif