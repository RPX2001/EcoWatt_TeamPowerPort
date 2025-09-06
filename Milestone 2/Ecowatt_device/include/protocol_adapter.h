#ifndef PROTOCOL_ADAPTER_H
#define PROTOCOL_ADAPTER_H

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

class ProtocolAdapter {
  public:
    ProtocolAdapter();
    void begin();  // connect WiFi
    String writeRegister(String frame); //write to register in server
    String readRegister(String frame);  //read from register in server  
    void parseResponse(String response); //parse JSON response from server
    
    
    //setters
    void setSSID(const char* newSSID);
    void setPassword(const char* newPassword);
    void setApiKey(String newApiKey);

    // Getters
    String getSSID();
    String getPassword();
    String getApiKey();

  private:
    // keep  default WiFi & API key private
    const char* ssid;
    const char* password;
    String apiKey;

    String writeURL = "http://20.15.114.131:8080/api/inverter/write";
    String readURL  = "http://20.15.114.131:8080/api/inverter/read";

    // retry & timeout
    const int maxRetries = 3;
    const int httpTimeout = 5000; // ms

    String sendRequest(String url, String frame);
    void printErrorCode(int code);
    bool isFrameValid(String frame);
};

#endif
