#ifndef ARDUINO_WIFI_H
#define ARDUINO_WIFI_H

#include <WiFi.h>
#include "driver/debug.h"
#include "driver/delay.h"
#include "application/credentials.h"

class Arduino_Wifi
{
private:
    const char* ssid;
    const char* password;
public:
    Arduino_Wifi();
    void begin();
    
    // Status check
    bool isConnected();
    void reconnect();

    // Setters
    void setSSID(const char* newSSID);
    void setPassword(const char* newPassword);

    // Getters
    const char* getSSID();
    const char* getPassword();
};

#endif // ARDUINO_WIFI_H