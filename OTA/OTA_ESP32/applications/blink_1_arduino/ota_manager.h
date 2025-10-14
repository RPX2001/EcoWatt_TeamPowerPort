#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include <ArduinoJson.h>
#include <mbedtls/md.h>

class OTAManager {
private:
    const char* ssid;
    const char* password;
    const char* manifestURL;
    const char* firmwareURL;
    String currentVersion;
    
    struct FirmwareManifest {
        String version;
        String sha256;
        String url;
        size_t size;
    };
    
    bool connectToWiFi();
    bool getManifest(FirmwareManifest& manifest);
    int compareVersions(String current, String server);
    void performOTAUpdate(HTTPClient& http, int contentLength, const String& expectedHash);
    
public:
    OTAManager(const char* ssid, const char* password, const char* serverURL, const String& version);
    void begin();
    void checkForUpdate();
};

#endif