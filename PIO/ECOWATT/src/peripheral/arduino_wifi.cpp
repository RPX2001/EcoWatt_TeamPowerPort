#include "peripheral/arduino_wifi.h"
#include "peripheral/logger.h"

Arduino_Wifi::Arduino_Wifi() {
  // Initialize with credentials from credentials.h
  ssid = WIFI_SSID;
  password = WIFI_PASSWORD;
  
  LOG_DEBUG(LOG_TAG_WIFI, "WiFi credentials loaded");
  LOG_DEBUG(LOG_TAG_WIFI, "  SSID: %s", ssid);
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
  
  LOG_INFO(LOG_TAG_WIFI, "Connecting to WiFi SSID: %s", ssid);
  WiFi.begin(ssid, password);

  int attempts = 0;
  const int maxAttempts = 40;  // 20 seconds timeout (40 * 500ms)
  
  while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) 
  {
    wait.ms(500);
    Serial.print(".");
    attempts++;
    
    if (attempts % 10 == 0) {
      Serial.printf(" [%d/%d]\n", attempts, maxAttempts);
    }
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    LOG_SUCCESS(LOG_TAG_WIFI, "WiFi Connected");
    LOG_INFO(LOG_TAG_WIFI, "  IP Address: %s", WiFi.localIP().toString().c_str());
    LOG_INFO(LOG_TAG_WIFI, "  Signal Strength: %d dBm", WiFi.RSSI());
  } else {
    LOG_ERROR(LOG_TAG_WIFI, "WiFi Connection Failed");
    LOG_ERROR(LOG_TAG_WIFI, "  Status Code: %d", WiFi.status());
    LOG_WARN(LOG_TAG_WIFI, "  Please check:");
    LOG_WARN(LOG_TAG_WIFI, "    1. SSID: %s", ssid);
    LOG_WARN(LOG_TAG_WIFI, "    2. Password is correct");
    LOG_WARN(LOG_TAG_WIFI, "    3. WiFi network is available");
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
    LOG_WARN(LOG_TAG_WIFI, "WiFi disconnected. Attempting to reconnect...");
    WiFi.disconnect();
    delay(100);
    begin();
  }
}
