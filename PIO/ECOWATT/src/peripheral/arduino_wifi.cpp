#include "peripheral/arduino_wifi.h"

Arduino_Wifi::Arduino_Wifi() {};

void Arduino_Wifi::begin() 
{
  // Serial.begin(115200);
  WiFi.begin(ssid, password);

  debug.log("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) 
  {
    wait.ms(500);
    debug.log(".");
  }
  debug.log("\rConnected!\n");
}


// Setters 
void Arduino_Wifi::setSSID(const char* newSSID) 
{ 
  ssid = newSSID; 
}

void Arduino_Wifi::setPassword(const char* newPassword) 
{ 
  password = newPassword; 
}

const char* Arduino_Wifi::getSSID() { return ssid; }
const char* Arduino_Wifi::getPassword() { return password; }