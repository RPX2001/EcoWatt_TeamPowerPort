#include "peripheral/arduino_wifi.h"

Arduino_Wifi::Arduino_Wifi() {};


/**
 * @fn void Arduino_Wifi::begin()
 * 
 * @brief Initialize WiFi connection with stored SSID and password.
 */
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

/**
 * @fn void Arduino_Wifi::setSSID(const char* newSSID)
 * 
 * @brief Set the WiFi SSID.
 * 
 * @param newSSID The new SSID to set.
 */
void Arduino_Wifi::setSSID(const char* newSSID) 
{ 
  ssid = newSSID; 
}


/**
 * @fn void Arduino_Wifi::setPassword(const char* newPassword)
 * 
 * @brief Set the WiFi password.
 * 
 * @param newPassword The new password to set.
 */
void Arduino_Wifi::setPassword(const char* newPassword) 
{ 
  password = newPassword; 
}


// Getters

/**
 * @fn const char* Arduino_Wifi::getSSID()
 * 
 * @brief Get the stored WiFi SSID.
 * 
 * @return The stored SSID.
 */
const char* Arduino_Wifi::getSSID() 
{ 
  return ssid; 
}


/**
 * @fn const char* Arduino_Wifi::getPassword()
 * 
 * @brief Get the stored WiFi password.
 * 
 * @return The stored password.
 */
const char* Arduino_Wifi::getPassword() 
{ 
  return password; 
}