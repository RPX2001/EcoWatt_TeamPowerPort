#pragma once

#include <string>
#include <cstring>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "time.h"

enum RegID : uint8_t {
  REG_VAC1,
  REG_IAC1,
  REG_FAC1,
  REG_VPV1,
  REG_VPV2,
  REG_IPV1,
  REG_IPV2,
  REG_TEMP,
  REG_POW,
  REG_PAC
};

struct RegisterDef {
  RegID id;
  uint16_t addr;       // Modbus register address
  const char* name;    // identification name
};

// Lookup table
static const RegisterDef REGISTER_MAP[] = {
  {REG_VAC1, 0, "Vac1"},
  {REG_IAC1, 1, "Iac1"},
  {REG_FAC1, 2, "Fac1"},
  {REG_VPV1, 3, "Vpv1"},
  {REG_VPV2, 4, "Vpv2"},
  {REG_IPV1, 5, "Ipv1"},
  {REG_IPV2, 6, "Ipv2"},
  {REG_TEMP, 7, "Temp"},
  {REG_POW,  8, "Pow"},
  {REG_PAC,  9, "Pac"}
};

// number of registers in the map
static const size_t REGISTER_COUNT = sizeof(REGISTER_MAP)/sizeof(REGISTER_MAP[0]);

class ProtocolAdapter {
  public:
    ProtocolAdapter();
    bool begin();  // connect WiFi
    int writeRegister(const String& frame, String& response); //write to register in server
    String readRegister(String frame);  //read from register in server  
    int parseResponse(String response); //parse JSON response from server

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
    bool isFrameValid(String frame);
};
