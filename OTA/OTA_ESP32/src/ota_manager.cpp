#include "ota_manager.h"

OTAManager::OTAManager(const char* ssid, const char* password, const char* serverURL, const String& version) 
    : ssid(ssid),
      password(password),
      baseURL(serverURL),
      manifestURL(baseURL + "/manifest"),
      firmwareURL(baseURL + "/firmware/latest.bin"),
      currentVersion(version) {}

void OTAManager::begin() {
    Serial.println("OTA Manager initialized");
    Serial.println("Version: " + currentVersion);
    connectToWiFi();
}

bool OTAManager::connectToWiFi() {
    Serial.print("Connecting to WiFi: ");
    Serial.println(ssid);
    
    WiFi.begin(ssid, password);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(1000);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println();
        Serial.println("WiFi connected!");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
        return true;
    } else {
        Serial.println();
        Serial.println("Failed to connect to WiFi.");
        return false;
    }
}

int OTAManager::compareVersions(String current, String server) {
    if (current == server) return 0;  // Same version
    if (current < server) return -1;  // Current is older
    return 1;  // Current is newer
}

bool OTAManager::getManifest(FirmwareManifest& manifest) {
    HTTPClient http;
    http.begin(manifestURL);
    http.addHeader("User-Agent", "ESP32-OTA-Client");
    
    int httpCode = http.GET();
    
    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        Serial.println("Manifest received:");
        Serial.println(payload);
        
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, payload);
        
        if (error) {
            Serial.print("JSON parsing failed: ");
            Serial.println(error.c_str());
            http.end();
            return false;
        }
        
        manifest.version = doc["version"].as<String>();
        manifest.sha256 = doc["sha256"].as<String>();
        manifest.url = doc["url"].as<String>();
        manifest.size = doc["size"];
        
        http.end();
        return true;
    } else {
        Serial.printf("Manifest request failed, error: %d\n", httpCode);
        http.end();
        return false;
    }
}

void OTAManager::performOTAUpdate(HTTPClient& http, int contentLength, const String& expectedHash) {
    Serial.println("Starting OTA update with streaming validation...");
    Serial.println("Expected SHA256: " + expectedHash);
    
    WiFiClient* client = http.getStreamPtr();
    
    // Initialize SHA256 context for streaming calculation
    mbedtls_md_context_t ctx;
    mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
    
    mbedtls_md_init(&ctx);
    if (mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 0) != 0) {
        Serial.println("Failed to setup SHA256 context");
        return;
    }
    mbedtls_md_starts(&ctx);
    
    // Begin OTA update
    if (!Update.begin(contentLength)) {
        Serial.println("Cannot start update");
        Update.printError(Serial);
        mbedtls_md_free(&ctx);
        return;
    }
    
    // Stream firmware data in chunks
    const size_t bufferSize = 4096;
    uint8_t buffer[bufferSize];
    size_t totalRead = 0;
    size_t bytesToRead;
    
    Serial.println("Downloading and validating firmware...");
    
    while (totalRead < contentLength) {
        bytesToRead = min((size_t)(contentLength - totalRead), bufferSize);
        
        int bytesRead = client->readBytes(buffer, bytesToRead);
        if (bytesRead <= 0) {
            Serial.println("Failed to read firmware data");
            Update.abort();
            mbedtls_md_free(&ctx);
            return;
        }
        
        mbedtls_md_update(&ctx, buffer, bytesRead);
        
        size_t written = Update.write(buffer, bytesRead);
        if (written != bytesRead) {
            Serial.printf("Write error: expected %d, wrote %d\n", bytesRead, written);
            Update.abort();
            mbedtls_md_free(&ctx);
            return;
        }
        
        totalRead += bytesRead;
        
        if (totalRead % 32768 == 0 || totalRead == contentLength) {
            Serial.printf("Progress: %d/%d bytes (%.1f%%)\n", 
                         totalRead, contentLength, 
                         (float)totalRead * 100.0 / contentLength);
        }
    }
    
    // Finalize SHA256 calculation
    const size_t hashLength = 32;
    unsigned char hash[hashLength];
    mbedtls_md_finish(&ctx, hash);
    mbedtls_md_free(&ctx);
    
    String calculatedHash = "";
    for (size_t i = 0; i < hashLength; i++) {
        char str[3];
        sprintf(str, "%02x", hash[i]);
        calculatedHash += str;
    }
    
    Serial.println("Calculated SHA256: " + calculatedHash);
    
    if (calculatedHash != expectedHash) {
        Serial.println("SHA256 mismatch! Firmware corrupted or tampered with.");
        Serial.println("Expected: " + expectedHash);
        Serial.println("Got:      " + calculatedHash);
        Update.abort();
        return;
    }
    
    Serial.println("SHA256 verification passed!");
    
    if (Update.end()) {
        Serial.println("OTA update completed successfully!");
        if (Update.isFinished()) {
            Serial.println("Rebooting with new firmware...");
            delay(1000);
            ESP.restart();
        } else {
            Serial.println("Update not finished? Something went wrong!");
        }
    } else {
        Serial.println("Error occurred during update finalization.");
        Serial.println("Error #: " + String(Update.getError()));
        Update.printError(Serial);
    }
}

void OTAManager::checkForUpdate() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not connected. Skipping update check.");
        return;
    }
    
    Serial.println("Checking for firmware updates...");
    
    FirmwareManifest manifest;
    if (!getManifest(manifest)) {
        Serial.println("Failed to get manifest. Skipping update.");
        return;
    }
    
    Serial.println("Current version: " + currentVersion);
    Serial.println("Server version: " + manifest.version);
    
    int versionComparison = compareVersions(currentVersion, manifest.version);
    if (versionComparison >= 0) {
        Serial.println("Firmware is up to date. No update needed.");
        return;
    }
    
    Serial.println("New firmware available! Starting download...");
    
    HTTPClient http;
    http.begin(firmwareURL);
    http.addHeader("User-Agent", "ESP32-OTA-Client");
    
    int httpCode = http.GET();
    
    if (httpCode == HTTP_CODE_OK) {
        int contentLength = http.getSize();
        Serial.printf("Firmware file size: %d bytes\n", contentLength);
        
        if (contentLength != manifest.size) {
            Serial.printf("Size mismatch! Expected: %d, Got: %d\n", manifest.size, contentLength);
            http.end();
            return;
        }
        
        if (contentLength > 0) {
            performOTAUpdate(http, contentLength, manifest.sha256);
        } else {
            Serial.println("Invalid firmware file size");
        }
    } else {
        Serial.printf("HTTP request failed, error: %d\n", httpCode);
    }
    
    http.end();
}
