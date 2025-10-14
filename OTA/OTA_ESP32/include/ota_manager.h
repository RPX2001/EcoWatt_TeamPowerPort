#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include <ArduinoJson.h>

class OTAManager {
private:
    const char* _ssid;
    const char* _password;
    const char* _serverURL;
    String _currentVersion;
    String _manifestURL;
    String _firmwareURL;
    
    // Private methods
    bool connectToWiFi();
    bool getManifest(String& version, String& sha256, String& url, size_t& size);
    void performOTAUpdate(HTTPClient& http, int contentLength, const String& expectedHash);
    int compareVersions(const String& current, const String& server);
    
public:
    // Constructor
    OTAManager(const char* ssid, const char* password, const char* serverURL, const String& currentVersion);
    
    // Public methods
    void begin();
    void checkForUpdate();
    bool isWiFiConnected();
};

#endif // OTA_MANAGER_H