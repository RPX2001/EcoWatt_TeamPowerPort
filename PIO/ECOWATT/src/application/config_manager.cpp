/**
 * @file config_manager.cpp
 * @brief Implementation of configuration management
 * 
 * @author Team PowerPort
 * @date 2025-10-22
 */

#include "application/config_manager.h"
#include "application/task_manager.h"
#include "peripheral/print.h"
#include "application/nvs.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <time.h>

// Helper function to get current Unix timestamp in seconds
static unsigned long getCurrentTimestamp() {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        time_t now = mktime(&timeinfo);
        return (unsigned long)now;
    }
    return millis() / 1000;  // Fallback to seconds since boot
}

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

    // Create WiFiClient with extended connection timeout
    WiFiClient client;
    client.setTimeout(15000);  // 15 second connection timeout in MILLISECONDS
    
    HTTPClient http;
    http.begin(client, changesURL);  // Use our WiFiClient with custom timeout
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(15000);  // 15 seconds HTTP timeout (in milliseconds)

    // M4 Format: Use GET request (device_id is in URL)
    int httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
        // Get full response as String (simpler and more reliable)
        String responseStr = http.getString();
        
        // Truncate for logging if too long
        if (responseStr.length() > 200) {
            print("[ConfigManager] Response: %s...\n", responseStr.substring(0, 200).c_str());
        } else {
            print("[ConfigManager] Response: %s\n", responseStr.c_str());
        }

        // Use DynamicJsonDocument to avoid stack overflow (response includes large available_registers array)
        DynamicJsonDocument responsedoc(4096);  // Allocate on heap instead of stack
        DeserializationError error = deserializeJson(responsedoc, responseStr);
        
        if (!error) {
            // Check if there's a pending config (Milestone 4 format)
            bool isPending = responsedoc["is_pending"] | false;
            
            if (isPending) {
                print("[ConfigManager] ⚡ PENDING CONFIG DETECTED - Applying changes...\n");
                
                // Extract PENDING config from pending_config field (not config field)
                // The 'config' field contains current running config (for dashboard)
                // The 'pending_config' field contains what ESP32 should apply
                JsonObject configWrapper = responsedoc["pending_config"].as<JsonObject>();
                
                // Check if config_update exists (Milestone 4 format)
                JsonObject config;
                if (configWrapper.containsKey("config_update")) {
                    config = configWrapper["config_update"].as<JsonObject>();
                } else {
                    // Fallback: use pending_config directly (old format)
                    config = configWrapper;
                }
                
                if (!config.isNull()) {
                    // Track what changed
                    bool anyChanges = false;
                    
                    // 1. Check sampling_interval (poll frequency)
                    if (config.containsKey("sampling_interval")) {
                        uint32_t sampling_interval = config["sampling_interval"];  // in seconds
                        uint64_t new_poll_freq = sampling_interval * 1000000ULL;  // convert to microseconds
                        
                        if (new_poll_freq != currentConfig.pollFrequency) {
                            nvs::changePollFreq(new_poll_freq);
                            *pollChanged = true;
                            anyChanges = true;
                            print("[ConfigManager] ✓ Poll frequency will update to %u s (%llu μs)\n", 
                                  sampling_interval, new_poll_freq);
                        }
                    }
                    
                    // 2. Check upload_interval (in seconds)
                    if (config.containsKey("upload_interval")) {
                        uint32_t upload_interval = config["upload_interval"].as<uint32_t>();
                        uint64_t new_upload_freq = (uint64_t)upload_interval * 1000000ULL;  // Convert to microseconds
                        
                        if (new_upload_freq != currentConfig.uploadFrequency) {
                            nvs::changeUploadFreq(new_upload_freq);
                            *uploadChanged = true;
                            anyChanges = true;
                            print("[ConfigManager] ✓ Upload frequency will update to %u s (%llu μs)\n", 
                                  upload_interval, new_upload_freq);
                        }
                    }
                    
                    // 3. Check config_poll_interval (in seconds)
                    if (config.containsKey("config_poll_interval")) {
                        uint32_t config_interval = config["config_poll_interval"].as<uint32_t>();
                        uint64_t new_config_freq = (uint64_t)config_interval * 1000000ULL;  // Convert to microseconds
                        
                        if (nvs::changeConfigFreq(new_config_freq)) {
                            // Update TaskManager's config frequency using public function
                            TaskManager::updateConfigFrequency(config_interval * 1000);  // milliseconds
                            anyChanges = true;
                            print("[ConfigManager] ✓ Config poll frequency updated to %u s\n", config_interval);
                        }
                    }
                    
                    // 4. Check command_poll_interval (in seconds)
                    if (config.containsKey("command_poll_interval")) {
                        uint32_t command_interval = config["command_poll_interval"].as<uint32_t>();
                        uint64_t new_command_freq = (uint64_t)command_interval * 1000000ULL;  // Convert to microseconds
                        
                        if (nvs::changeCommandFreq(new_command_freq)) {
                            // Update TaskManager's command frequency using public function
                            TaskManager::updateCommandFrequency(command_interval * 1000);  // milliseconds
                            anyChanges = true;
                            print("[ConfigManager] ✓ Command poll frequency updated to %u s\n", command_interval);
                        }
                    }
                    
                    // 5. Check firmware_check_interval (in seconds)
                    if (config.containsKey("firmware_check_interval")) {
                        uint32_t ota_interval = config["firmware_check_interval"].as<uint32_t>();
                        uint64_t new_ota_freq = (uint64_t)ota_interval * 1000000ULL;  // Convert to microseconds
                        
                        if (nvs::changeOtaFreq(new_ota_freq)) {
                            // Update TaskManager's OTA frequency using public function
                            TaskManager::updateOtaFrequency(ota_interval * 1000);  // milliseconds
                            anyChanges = true;
                            print("[ConfigManager] ✓ Firmware check frequency updated to %u s\n", ota_interval);
                        }
                    }
                    
                    // 6. Check registers array (Milestone 4 format uses register NAMES not mask)
                    if (config.containsKey("registers")) {
                        JsonArray registers = config["registers"].as<JsonArray>();
                        
                        if (registers.size() > 0) {
                            // Convert register names to register IDs and build mask
                            uint16_t regsMask = 0;
                            size_t regsCount = 0;
                            
                            print("[ConfigManager] Processing %d registers:\n", registers.size());
                            
                            for (JsonVariant regName : registers) {
                                const char* name = regName.as<const char*>();
                                
                                // Find matching register ID
                                for (size_t i = 0; i < REGISTER_COUNT; i++) {
                                    if (strcmp(REGISTER_MAP[i].name, name) == 0) {
                                        regsMask |= (1 << i);
                                        regsCount++;
                                        print("  - %s (ID: %zu)\n", name, i);
                                        break;
                                    }
                                }
                            }
                            
                            if (regsCount > 0) {
                                bool saved = nvs::saveReadRegs(regsMask, regsCount);
                                if (saved) {
                                    *registersChanged = true;
                                    anyChanges = true;
                                    print("[ConfigManager] ✓ %zu registers will update in next cycle\n", regsCount);
                                } else {
                                    print("[ConfigManager] ❌ Failed to save register changes to NVS\n");
                                }
                            }
                        }
                    }
                    
                    if (anyChanges) {
                        print("[ConfigManager] ✅ Configuration changes applied successfully\n");
                        
                        // Send acknowledgment to Flask server to mark config as "acknowledged"
                        sendConfigAcknowledgment("applied", "Configuration updated successfully");
                    } else {
                        print("[ConfigManager] ℹ️ No actual changes (config same as current)\n");
                    }
                } else {
                    print("[ConfigManager] ⚠️ Config object not found in response\n");
                }
            } else {
                print("[ConfigManager] No pending configuration changes\n");
            }

            http.end();
        } else {
            http.end();
            print("[ConfigManager] ⚠️ JSON parse error: %s (response size: %d bytes)\n", 
                  error.c_str(), responseStr.length());
            print("[ConfigManager] Response preview: %s\n", responseStr.substring(0, 500).c_str());
        }
    } else {
        print("[ConfigManager] HTTP GET failed with error code: %d\n", httpResponseCode);
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
    
    // Send acknowledgment to Flask server
    sendConfigAcknowledgment("applied", "Register selection updated successfully");
    
    return true;
}

bool ConfigManager::applyPollFrequencyChange(uint64_t* newFreq) {
    *newFreq = nvs::getPollFreq();
    currentConfig.pollFrequency = *newFreq;
    
    print("[ConfigManager] Applied poll frequency change: %llu μs\n", *newFreq);
    
    // Send acknowledgment to Flask server
    sendConfigAcknowledgment("applied", "Poll frequency updated successfully");
    
    return true;
}

bool ConfigManager::applyUploadFrequencyChange(uint64_t* newFreq) {
    *newFreq = nvs::getUploadFreq();
    currentConfig.uploadFrequency = *newFreq;
    
    print("[ConfigManager] Applied upload frequency change: %llu μs\n", *newFreq);
    
    // Send acknowledgment to Flask server
    sendConfigAcknowledgment("applied", "Upload frequency updated successfully");
    
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

void ConfigManager::sendConfigAcknowledgment(const char* status, const char* message) {
    /**
     * Send configuration acknowledgment to Flask server
     * POST /config/{device_id}/acknowledge
     * Body: {"status": "applied"|"failed", "timestamp": 123456, "error_msg": "..."}
     */
    
    if (WiFi.status() != WL_CONNECTED) {
        print("[ConfigManager] WiFi not connected. Cannot send acknowledgment.\n");
        return;
    }
    
    // Construct acknowledgment URL
    char ackURL[512];
    snprintf(ackURL, sizeof(ackURL), "%s/acknowledge", changesURL);
    
    // Create WiFiClient with extended connection timeout
    WiFiClient client;
    client.setTimeout(15000);  // 15 second connection timeout in MILLISECONDS
    
    HTTPClient http;
    http.begin(client, ackURL);  // Use our WiFiClient with custom timeout
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(15000);  // 15 seconds HTTP timeout (in milliseconds)
    
    // Create JSON payload
    StaticJsonDocument<256> doc;
    doc["status"] = status;
    doc["timestamp"] = getCurrentTimestamp();  // Unix timestamp in seconds
    if (message != nullptr && strlen(message) > 0) {
        doc["error_msg"] = message;
    }
    
    String payload;
    serializeJson(doc, payload);
    
    print("[ConfigManager] Sending acknowledgment: %s\n", payload.c_str());
    
    int httpCode = http.POST(payload);
    
    if (httpCode > 0) {
        if (httpCode == 200) {
            print("[ConfigManager] ✅ Acknowledgment sent successfully\n");
        } else {
            print("[ConfigManager] ⚠️ Acknowledgment sent but received code: %d\n", httpCode);
        }
    } else {
        print("[ConfigManager] ❌ Failed to send acknowledgment: %d\n", httpCode);
    }
    
    http.end();
}

void ConfigManager::sendCurrentConfig() {
    /**
     * Send current running configuration to Flask server
     * This allows the frontend to display what ESP32 is actually running
     * 
     * POST /config/{device_id}/current
     * Body: {
     *   "sampling_interval": 2,
     *   "upload_interval": 15,
     *   "registers": ["Vac1", "Iac1", "Pac"],
     *   "timestamp": 1234567890
     * }
     */
    
    if (WiFi.status() != WL_CONNECTED) {
        print("[ConfigManager] WiFi not connected. Cannot send current config.\n");
        return;
    }
    
    // Construct URL for current config
    char configURL[512];
    snprintf(configURL, sizeof(configURL), "%s/current", changesURL);
    
    // Create WiFiClient
    WiFiClient client;
    client.setTimeout(15000);
    
    HTTPClient http;
    http.begin(client, configURL);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(15000);
    
    // Build JSON payload with current configuration
    StaticJsonDocument<1024> doc;
    
    // Convert microseconds to seconds for ALL 5 intervals
    doc["sampling_interval"] = (uint32_t)(currentConfig.pollFrequency / 1000000);
    doc["upload_interval"] = (uint32_t)(currentConfig.uploadFrequency / 1000000);
    doc["config_poll_interval"] = (uint32_t)(TaskManager::getConfigFrequency() / 1000);  // Convert ms to seconds
    doc["command_poll_interval"] = (uint32_t)(TaskManager::getCommandFrequency() / 1000);  // Convert ms to seconds
    doc["firmware_check_interval"] = (uint32_t)(TaskManager::getOtaFrequency() / 1000);  // Convert ms to seconds
    doc["compression_enabled"] = true;  // Always enabled in our implementation
    doc["timestamp"] = getCurrentTimestamp();
    
    // Add register names
    JsonArray registers = doc.createNestedArray("registers");
    for (size_t i = 0; i < currentConfig.registerCount && i < REGISTER_COUNT; i++) {
        registers.add(REGISTER_MAP[currentConfig.registers[i]].name);
    }
    
    String payload;
    serializeJson(doc, payload);
    
    print("[ConfigManager] Sending current config: %s\n", payload.c_str());
    
    int httpCode = http.POST(payload);
    
    if (httpCode > 0) {
        if (httpCode == 200 || httpCode == 201) {
            print("[ConfigManager] ✅ Current config sent successfully\n");
        } else {
            String response = http.getString();
            print("[ConfigManager] ⚠️ Config sent but received code: %d, response: %s\n", 
                  httpCode, response.c_str());
        }
    } else {
        print("[ConfigManager] ❌ Failed to send current config: %d\n", httpCode);
    }
    
    http.end();
}

