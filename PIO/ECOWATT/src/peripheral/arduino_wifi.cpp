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
 *        BLOCKS INDEFINITELY until WiFi is connected.
 *        System will not proceed without WiFi connection.
 */
void Arduino_Wifi::begin() 
{
  // Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  
  LOG_INFO(LOG_TAG_WIFI, "Connecting to WiFi SSID: %s", ssid);
  LOG_WARN(LOG_TAG_WIFI, "System will NOT proceed until WiFi is connected!");
  WiFi.begin(ssid, password);

  int attempts = 0;
  const int maxAttemptsPerCycle = 40;  // 20 seconds per cycle (40 * 500ms)
  int totalCycles = 0;
  
  while (WiFi.status() != WL_CONNECTED) 
  {
    wait.ms(500);
    Serial.print(".");
    attempts++;
    
    if (attempts % 10 == 0) {
      Serial.printf(" [%d attempts]\n", attempts);
    }
    
    // After each cycle (20 seconds), try reconnecting
    if (attempts >= maxAttemptsPerCycle) {
      totalCycles++;
      LOG_WARN(LOG_TAG_WIFI, "WiFi connection attempt cycle %d failed. Retrying...", totalCycles);
      LOG_WARN(LOG_TAG_WIFI, "  Please check:");
      LOG_WARN(LOG_TAG_WIFI, "    1. SSID: %s", ssid);
      LOG_WARN(LOG_TAG_WIFI, "    2. Password is correct");
      LOG_WARN(LOG_TAG_WIFI, "    3. WiFi network is available");
      
      // Reset WiFi and try again
      WiFi.disconnect();
      delay(1000);
      WiFi.begin(ssid, password);
      attempts = 0;
      
      // Add longer delay between retry cycles to prevent overwhelming
      if (totalCycles >= 3) {
        LOG_WARN(LOG_TAG_WIFI, "Multiple failures. Waiting 10 seconds before next attempt...");
        delay(10000);
      }
    }
  }
  
  // WiFi is now connected (we exit the while loop only when connected)
  LOG_SUCCESS(LOG_TAG_WIFI, "WiFi Connected after %d cycles", totalCycles);
  LOG_INFO(LOG_TAG_WIFI, "  IP Address: %s", WiFi.localIP().toString().c_str());
  LOG_INFO(LOG_TAG_WIFI, "  Signal Strength: %d dBm", WiFi.RSSI());
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
