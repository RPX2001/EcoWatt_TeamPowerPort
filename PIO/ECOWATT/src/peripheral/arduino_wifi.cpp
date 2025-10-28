#include "peripheral/arduino_wifi.h"

Arduino_Wifi::Arduino_Wifi() {
  // Initialize with credentials from credentials.h
  ssid = WIFI_SSID;
  password = WIFI_PASSWORD;
  
  debug.log("WiFi credentials loaded:\n");
  debug.log("  SSID: %s\n", ssid);
};


/**
 * @fn void Arduino_Wifi::begin()
 * 
 * @brief Initialize WiFi connection with stored SSID and password.
 */
void Arduino_Wifi::begin() 
{
  // Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  
  debug.log("Connecting to WiFi SSID: %s\n", ssid);
  WiFi.begin(ssid, password);

  int attempts = 0;
  const int maxAttempts = 40;  // 20 seconds timeout (40 * 500ms)
  
  while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) 
  {
    wait.ms(500);
    debug.log(".");
    attempts++;
    
    if (attempts % 10 == 0) {
      debug.log(" [%d/%d]\n", attempts, maxAttempts);
    }
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    debug.log("\n✓ WiFi Connected!\n");
    debug.log("  IP Address: %s\n", WiFi.localIP().toString().c_str());
    debug.log("  Signal Strength: %d dBm\n", WiFi.RSSI());
  } else {
    debug.log("\n✗ WiFi Connection Failed!\n");
    debug.log("  Status Code: %d\n", WiFi.status());
    debug.log("  Please check:\n");
    debug.log("    1. SSID: %s\n", ssid);
    debug.log("    2. Password is correct\n");
    debug.log("    3. WiFi network is available\n");
  }
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

/**
 * @fn bool Arduino_Wifi::isConnected()
 * 
 * @brief Check if WiFi is currently connected.
 * 
 * @return true if connected, false otherwise.
 */
bool Arduino_Wifi::isConnected()
{
  return WiFi.status() == WL_CONNECTED;
}

/**
 * @fn void Arduino_Wifi::reconnect()
 * 
 * @brief Attempt to reconnect to WiFi if disconnected.
 */
void Arduino_Wifi::reconnect()
{
  if (!isConnected()) {
    debug.log("WiFi disconnected. Attempting to reconnect...\n");
    WiFi.disconnect();
    delay(100);
    begin();
  }
}
