/**
 * @file config_manager.cpp
 * @brief Implementation of configuration management
 * 
 * @author Team PowerPort
 * @date 2025-10-22
 */

#include "application/config_manager.h"
#include "peripheral/print.h"
#include "application/nvs.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>

// Initialize static members
char ConfigManager::changesURL[256] = "";
char ConfigManager::deviceID[64] = "ESP32_Unknown";
SystemConfig ConfigManager::currentConfig = {nullptr, 0, 0, 0};

void ConfigManager::init(const char* endpoint, const char* devID) {
    strncpy(changesURL, endpoint, sizeof(changesURL) - 1);
    changesURL[sizeof(changesURL) - 1] = '\0';
    
    strncpy(deviceID, devID, sizeof(deviceID) - 1);
    deviceID[sizeof(deviceID) - 1] = '\0';
    
    // Load current configuration from NVS
    currentConfig.registers = nvs::getReadRegs();
    currentConfig.registerCount = nvs::getReadRegCount();
    currentConfig.pollFrequency = nvs::getPollFreq();
    currentConfig.uploadFrequency = nvs::getUploadFreq();
    
    print("[ConfigManager] Initialized\n");
    print("[ConfigManager] Changes URL: %s\n", changesURL);
    print("[ConfigManager] Device: %s\n", deviceID);
    printCurrentConfig();
}

void ConfigManager::checkForChanges(bool* registersChanged, bool* pollChanged, 
                                    bool* uploadChanged) {
    print("[ConfigManager] Checking for changes from cloud...\n");
    
    if (WiFi.status() != WL_CONNECTED) {
        print("[ConfigManager] WiFi not connected. Cannot check changes.\n");
        return;
    }

    HTTPClient http;
    http.begin(changesURL);
    http.addHeader("Content-Type", "application/json");

    StaticJsonDocument<128> changesRequestBody;
    changesRequestBody["device_id"] = deviceID;
    changesRequestBody["timestamp"] = millis();

    char requestBody[128];
    serializeJson(changesRequestBody, requestBody, sizeof(requestBody));

    int httpResponseCode = http.POST((uint8_t*)requestBody, strlen(requestBody));

    if (httpResponseCode > 0) {
        // Get response into buffer
        WiFiClient* stream = http.getStreamPtr();
        static char responseBuffer[1024];
        int len = http.getSize();
        int index = 0;

        while (http.connected() && (len > 0 || len == -1)) {
            if (stream->available()) {
                char c = stream->read();
                if (index < (int)sizeof(responseBuffer) - 1)
                    responseBuffer[index++] = c;
                if (len > 0) len--;
            }
        }
        responseBuffer[index] = '\0';
        
        print("[ConfigManager] Response: %s\n", responseBuffer);

        StaticJsonDocument<1024> responsedoc;
        DeserializationError error = deserializeJson(responsedoc, responseBuffer);
        
        if (!error) {
            bool settingsChanged = responsedoc["Changed"] | false;
            
            if (settingsChanged) {
                // Check poll frequency change
                bool pollFreqChanged = responsedoc["pollFreqChanged"] | false;
                if (pollFreqChanged) {
                    uint64_t new_poll_timer = responsedoc["newPollTimer"] | 0;
                    nvs::changePollFreq(new_poll_timer * 1000000);
                    *pollChanged = true;
                    print("[ConfigManager] Poll timer will update to %llu μs\n", 
                          new_poll_timer * 1000000);
                }

                // Check upload frequency change
                bool uploadFreqChanged = responsedoc["uploadFreqChanged"] | false;
                if (uploadFreqChanged) {
                    uint64_t new_upload_timer = responsedoc["newUploadTimer"] | 0;
                    nvs::changeUploadFreq(new_upload_timer * 1000000);
                    *uploadChanged = true;
                    print("[ConfigManager] Upload timer will update to %llu μs\n", 
                          new_upload_timer * 1000000);
                }

                // Check register selection change
                bool regsChanged = responsedoc["regsChanged"] | false;
                if (regsChanged) {
                    size_t regsCount = responsedoc["regsCount"] | 0;

                    if (regsCount > 0 && responsedoc.containsKey("regs")) {
                        uint16_t regsMask = responsedoc["regs"] | 0;
                        print("[ConfigManager] Received regsMask: %u, regsCount: %zu\n", 
                              regsMask, regsCount);
                        
                        bool saved = nvs::saveReadRegs(regsMask, regsCount);
                        if (saved) {
                            *registersChanged = true;
                            print("[ConfigManager] %zu registers will update in next cycle\n", 
                                  regsCount);
                        } else {
                            print("[ConfigManager] Failed to save register changes to NVS\n");
                        }
                    }
                }
                
                print("[ConfigManager] Changes detected and queued\n");
            } else {
                print("[ConfigManager] No changes detected\n");
            }

            http.end();
        } else {
            http.end();
            print("[ConfigManager] Settings change parse error\n");
        }
    } else {
        print("[ConfigManager] HTTP POST failed with error code: %d\n", httpResponseCode);
        http.end();
    }
}

bool ConfigManager::applyRegisterChanges(const RegID** newSelection, size_t* newCount) {
    *newSelection = nvs::getReadRegs();
    *newCount = nvs::getReadRegCount();
    
    // Update current config
    currentConfig.registers = *newSelection;
    currentConfig.registerCount = *newCount;
    
    print("[ConfigManager] Applied register changes: %zu registers\n", *newCount);
    return true;
}

bool ConfigManager::applyPollFrequencyChange(uint64_t* newFreq) {
    *newFreq = nvs::getPollFreq();
    currentConfig.pollFrequency = *newFreq;
    
    print("[ConfigManager] Applied poll frequency change: %llu μs\n", *newFreq);
    return true;
}

bool ConfigManager::applyUploadFrequencyChange(uint64_t* newFreq) {
    *newFreq = nvs::getUploadFreq();
    currentConfig.uploadFrequency = *newFreq;
    
    print("[ConfigManager] Applied upload frequency change: %llu μs\n", *newFreq);
    return true;
}

const SystemConfig& ConfigManager::getCurrentConfig() {
    return currentConfig;
}

void ConfigManager::updateCurrentConfig(const RegID* newRegs, size_t newRegCount,
                                        uint64_t newPollFreq, uint64_t newUploadFreq) {
    currentConfig.registers = newRegs;
    currentConfig.registerCount = newRegCount;
    currentConfig.pollFrequency = newPollFreq;
    currentConfig.uploadFrequency = newUploadFreq;
    
    print("[ConfigManager] Configuration updated\n");
}

void ConfigManager::printCurrentConfig() {
    print("\n========== CURRENT CONFIGURATION ==========\n");
    print("  Register Count:    %zu\n", currentConfig.registerCount);
    print("  Registers:         ");
    
    for (size_t i = 0; i < currentConfig.registerCount && i < REGISTER_COUNT; i++) {
        print("%s ", REGISTER_MAP[currentConfig.registers[i]].name);
    }
    print("\n");
    
    print("  Poll Frequency:    %llu μs (%.2f s)\n", 
          currentConfig.pollFrequency, currentConfig.pollFrequency / 1000000.0);
    print("  Upload Frequency:  %llu μs (%.2f s)\n", 
          currentConfig.uploadFrequency, currentConfig.uploadFrequency / 1000000.0);
    print("===========================================\n\n");
}
