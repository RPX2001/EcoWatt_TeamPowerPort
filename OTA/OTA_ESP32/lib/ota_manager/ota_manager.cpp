#include "ota_manager.h"
#include <mbedtls/md.h>
#include <mbedtls/sha256.h>

// Constructor
OTAManager::OTAManager(const char* ssid, const char* password, const char* serverURL, const String& currentVersion) 
    : _ssid(ssid), _password(password), _serverURL(serverURL), _currentVersion(currentVersion) {
    _manifestURL = String(serverURL) + "/manifest";
    _firmwareURL = String(serverURL) + "/firmware/latest.bin";
}

// Initialize OTA Manager
void OTAManager::begin() {
    Serial.println("Initializing OTA Manager...");
    Serial.println("Server URL: " + String(_serverURL));
    Serial.println("Current Version: " + _currentVersion);
    
    if (!connectToWiFi()) {
        Serial.println("WiFi connection failed! OTA updates disabled.");
        return;
    }
    
    Serial.println("OTA Manager initialized successfully!");
}

// Connect to WiFi
bool OTAManager::connectToWiFi() {
    if (WiFi.status() == WL_CONNECTED) {
        return true;
    }
    
    Serial.print("Connecting to WiFi: ");
    Serial.println(_ssid);
    
    WiFi.begin(_ssid, _password);
    
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

// Check WiFi connection status
bool OTAManager::isWiFiConnected() {
    return WiFi.status() == WL_CONNECTED;
}

// Get manifest from server
bool OTAManager::getManifest(String& version, String& sha256, String& url, size_t& size) {
    HTTPClient http;
    
    Serial.println("Requesting manifest from: " + _manifestURL);
    
    if (!http.begin(_manifestURL)) {
        Serial.println("Failed to begin HTTP connection");
        return false;
    }
    
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
        
        version = doc["version"].as<String>();
        sha256 = doc["sha256"].as<String>();
        url = doc["url"].as<String>();
        size = doc["size"];
        
        http.end();
        return true;
    } else {
        Serial.printf("Manifest request failed, error: %d\n", httpCode);
        if (httpCode > 0) {
            Serial.println("Response: " + http.getString());
        }
        http.end();
        return false;
    }
}

// Compare version strings (simple comparison)
int OTAManager::compareVersions(const String& current, const String& server) {
    if (current == server) return 0;  // Same version
    if (current < server) return -1;  // Current is older
    return 1;  // Current is newer
}

// Perform OTA update with streaming validation
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
    const size_t bufferSize = 4096; // Use 4KB chunks
    uint8_t buffer[bufferSize];
    size_t totalRead = 0;
    size_t bytesToRead;
    
    Serial.println("Downloading and validating firmware...");
    
    while (totalRead < contentLength) {
        // Calculate how many bytes to read this iteration
        bytesToRead = min((size_t)(contentLength - totalRead), bufferSize);
        
        // Read chunk from stream
        int bytesRead = client->readBytes(buffer, bytesToRead);
        if (bytesRead <= 0) {
            Serial.println("Failed to read firmware data");
            Update.abort();
            mbedtls_md_free(&ctx);
            return;
        }
        
        // Update SHA256 hash with this chunk
        mbedtls_md_update(&ctx, buffer, bytesRead);
        
        // Write chunk to flash
        size_t written = Update.write(buffer, bytesRead);
        if (written != bytesRead) {
            Serial.printf("Write error: expected %d, wrote %d\n", bytesRead, written);
            Update.abort();
            mbedtls_md_free(&ctx);
            return;
        }
        
        totalRead += bytesRead;
        
        // Show progress every 64KB
        if (totalRead % 65536 == 0 || totalRead == contentLength) {
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
    
    // Convert hash to hex string
    String calculatedHash = "";
    for (size_t i = 0; i < hashLength; i++) {
        char str[3];
        sprintf(str, "%02x", hash[i]);
        calculatedHash += str;
    }
    
    Serial.println("Calculated SHA256: " + calculatedHash);
    
    // Verify hash
    if (calculatedHash != expectedHash) {
        Serial.println("SHA256 mismatch! Firmware corrupted or tampered with.");
        Serial.println("Expected: " + expectedHash);
        Serial.println("Got:      " + calculatedHash);
        Update.abort();
        return;
    }
    
    Serial.println("SHA256 verification passed!");
    
    // Finalize the update
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

// Check for firmware updates
void OTAManager::checkForUpdate() {
    if (!isWiFiConnected()) {
        Serial.println("WiFi not connected. Attempting to reconnect...");
        if (!connectToWiFi()) {
            Serial.println("WiFi reconnection failed. Skipping update check.");
            return;
        }
    }
    
    Serial.println("Checking for firmware updates...");
    
    // Step 1: Get manifest
    String serverVersion, sha256Hash, firmwareUrl;
    size_t firmwareSize;
    
    if (!getManifest(serverVersion, sha256Hash, firmwareUrl, firmwareSize)) {
        Serial.println("Failed to get manifest. Skipping update.");
        return;
    }
    
    Serial.println("Current version: " + _currentVersion);
    Serial.println("Server version: " + serverVersion);
    
    // Step 2: Compare versions
    int versionComparison = compareVersions(_currentVersion, serverVersion);
    if (versionComparison >= 0) {
        Serial.println("Firmware is up to date. No update needed.");
        return;
    }
    
    Serial.println("New firmware available! Starting download...");
    
    // Step 3: Download firmware with validation
    HTTPClient http;
    
    if (!http.begin(_firmwareURL)) {
        Serial.println("Failed to begin firmware download connection");
        return;
    }
    
    http.addHeader("User-Agent", "ESP32-OTA-Client");
    
    int httpCode = http.GET();
    
    if (httpCode == HTTP_CODE_OK) {
        int contentLength = http.getSize();
        Serial.printf("Firmware file size: %d bytes\n", contentLength);
        
        // Verify size matches manifest
        if (contentLength != (int)firmwareSize) {
            Serial.printf("Size mismatch! Expected: %d, Got: %d\n", firmwareSize, contentLength);
            http.end();
            return;
        }
        
        if (contentLength > 0) {
            performOTAUpdate(http, contentLength, sha256Hash);
        } else {
            Serial.println("Invalid firmware file size");
        }
    } else {
        Serial.printf("HTTP request failed, error: %d\n", httpCode);
        if (httpCode > 0) {
            Serial.println("Response: " + http.getString());
        }
    }
    
    http.end();
}