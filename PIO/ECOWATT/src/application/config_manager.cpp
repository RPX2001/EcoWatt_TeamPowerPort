/**
 * @file config_manager.cpp
 * @brief Implementation of configuration management
 * 
 * @author Team PowerPort
 * @date 2025-10-22
 */

#include "application/config_manager.h"
#include "peripheral/logger.h"
#include "application/task_manager.h"
#include "application/power_management.h"
#include "peripheral/logger.h"
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
    
    LOG_SUCCESS(LOG_TAG_CONFIG, "Initialized");
    LOG_INFO(LOG_TAG_CONFIG, "Changes URL: %s", changesURL);
    LOG_INFO(LOG_TAG_CONFIG, "Device: %s", deviceID);
    printCurrentConfig();
}

void ConfigManager::checkForChanges(bool* registersChanged, bool* pollChanged, 
                                    bool* uploadChanged) {
    LOG_INFO(LOG_TAG_CONFIG, "Checking for changes from cloud...");
    
    if (WiFi.status() != WL_CONNECTED) {
        LOG_WARN(LOG_TAG_CONFIG, "WiFi not connected. Cannot check changes.");
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
        
        // Only log full response in DEBUG mode if needed for troubleshooting
        // Removed constant response logging to reduce verbosity

        // Use DynamicJsonDocument to avoid stack overflow (response includes large available_registers array)
        DynamicJsonDocument responsedoc(4096);  // Allocate on heap instead of stack
        DeserializationError error = deserializeJson(responsedoc, responseStr);
        
        if (!error) {
            // Check if there's a pending config (Milestone 4 format)
            bool isPending = responsedoc["is_pending"] | false;
            
            if (isPending) {
                LOG_SECTION("PENDING CONFIG DETECTED - Applying changes");
                
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
                            // Update TaskManager static variable (in milliseconds)
                            TaskManager::updatePollFrequency(sampling_interval * 1000);
                            // Update currentConfig to track the change
                            currentConfig.pollFrequency = new_poll_freq;
                            *pollChanged = true;
                            anyChanges = true;
                            LOG_INFO(LOG_TAG_CONFIG, "Poll frequency will update to %u s (%llu μs)", 
                                  sampling_interval, new_poll_freq);
                        }
                    }
                    
                    // 2. Check upload_interval (in seconds)
                    if (config.containsKey("upload_interval")) {
                        uint32_t upload_interval = config["upload_interval"].as<uint32_t>();
                        uint64_t new_upload_freq = (uint64_t)upload_interval * 1000000ULL;  // Convert to microseconds
                        
                        if (new_upload_freq != currentConfig.uploadFrequency) {
                            nvs::changeUploadFreq(new_upload_freq);
                            // Update TaskManager static variable (in milliseconds)
                            TaskManager::updateUploadFrequency(upload_interval * 1000);
                            // Update currentConfig to track the change
                            currentConfig.uploadFrequency = new_upload_freq;
                            *uploadChanged = true;
                            anyChanges = true;
                            LOG_INFO(LOG_TAG_CONFIG, "Upload frequency will update to %u s (%llu μs)", 
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
                            LOG_SUCCESS(LOG_TAG_CONFIG, "Config poll frequency updated to %u s", config_interval);
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
                            LOG_SUCCESS(LOG_TAG_CONFIG, "Command poll frequency updated to %u s", command_interval);
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
                            LOG_SUCCESS(LOG_TAG_CONFIG, "Firmware check frequency updated to %u s", ota_interval);
                        }
                    }
                    
                    // 6. Check registers array (Milestone 4 format uses register NAMES not mask)
                    if (config.containsKey("registers")) {
                        JsonArray registers = config["registers"].as<JsonArray>();
                        
                        if (registers.size() > 0) {
                            // Convert register names to register IDs and build mask
                            uint16_t regsMask = 0;
                            size_t regsCount = 0;
                            
                            LOG_INFO(LOG_TAG_CONFIG, "Processing %d registers:", registers.size());
                            
                            for (JsonVariant regName : registers) {
                                const char* name = regName.as<const char*>();
                                
                                // Find matching register ID
                                for (size_t i = 0; i < REGISTER_COUNT; i++) {
                                    if (strcmp(REGISTER_MAP[i].name, name) == 0) {
                                        regsMask |= (1 << i);
                                        regsCount++;
                                        LOG_INFO(LOG_TAG_CONFIG, "  - %s (ID: %zu)", name, i);
                                        break;
                                    }
                                }
                            }
                            
                            if (regsCount > 0) {
                                bool saved = nvs::saveReadRegs(regsMask, regsCount);
                                if (saved) {
                                    *registersChanged = true;
                                    anyChanges = true;
                                    LOG_SUCCESS(LOG_TAG_CONFIG, "%zu registers will update in next cycle", regsCount);
                                } else {
                                    LOG_ERROR(LOG_TAG_CONFIG, "Failed to save register changes to NVS");
                                }
                            }
                        }
                    }
                    
                    // 7. Check power_management block
                    if (config.containsKey("power_management")) {
                        JsonObject powerConfig = config["power_management"].as<JsonObject>();
                        
                        if (!powerConfig.isNull()) {
                            LOG_INFO(LOG_TAG_CONFIG, "Processing power management configuration:");
                            
                            // Check if power management is enabled
                            if (powerConfig.containsKey("enabled")) {
                                bool enabled = powerConfig["enabled"].as<bool>();
                                bool currentEnabled = nvs::getPowerEnabled();
                                
                                if (enabled != currentEnabled) {
                                    nvs::setPowerEnabled(enabled);
                                    anyChanges = true;
                                    LOG_INFO(LOG_TAG_CONFIG, "  - Power Management: %s", 
                                          enabled ? "ENABLED" : "DISABLED");
                                    
                                    // Apply the enable/disable state
                                    PowerManagement::enable(enabled);
                                } else {
                                    LOG_DEBUG(LOG_TAG_CONFIG, "  - Power Management: %s (unchanged)", 
                                          enabled ? "ENABLED" : "DISABLED");
                                }
                            }
                            
                            // Check techniques bitmask
                            if (powerConfig.containsKey("techniques")) {
                                uint8_t techniques = powerConfig["techniques"].as<uint8_t>();
                                uint8_t currentTechniques = nvs::getPowerTechniques();
                                
                                if (techniques != currentTechniques) {
                                    nvs::setPowerTechniques(techniques);
                                    PowerManagement::setTechniques(techniques);
                                    anyChanges = true;
                                    
                                    LOG_INFO(LOG_TAG_CONFIG, "  - Active Techniques (0x%02X):", techniques);
                                    if (techniques & POWER_TECH_WIFI_MODEM_SLEEP) {
                                        LOG_INFO(LOG_TAG_CONFIG, "      • WiFi Modem Sleep [ACTIVE]");
                                    }
                                    if (techniques & POWER_TECH_CPU_FREQ_SCALING) {
                                        LOG_INFO(LOG_TAG_CONFIG, "      • CPU Frequency Scaling [FUTURE]");
                                    }
                                    if (techniques & POWER_TECH_LIGHT_SLEEP) {
                                        LOG_INFO(LOG_TAG_CONFIG, "      • Light Sleep [FUTURE]");
                                    }
                                    if (techniques & POWER_TECH_PERIPHERAL_GATING) {
                                        LOG_INFO(LOG_TAG_CONFIG, "      • Peripheral Gating [FUTURE]");
                                    }
                                    if (techniques == 0x00) {
                                        LOG_INFO(LOG_TAG_CONFIG, "      • None (full performance mode)");
                                    }
                                } else {
                                    LOG_DEBUG(LOG_TAG_CONFIG, "  - Active Techniques: 0x%02X (unchanged)", techniques);
                                }
                            }
                            
                            LOG_SUCCESS(LOG_TAG_CONFIG, "Power management config processed");
                        }
                    }
                    
                    // Check energy poll interval (in seconds from frontend) - TOP LEVEL CONFIG
                    if (config.containsKey("energy_poll_interval")) {
                        uint32_t interval_sec = config["energy_poll_interval"].as<uint32_t>();
                        // Convert seconds to microseconds for NVS storage
                        uint64_t freq_us = static_cast<uint64_t>(interval_sec) * 1000000ULL;
                        uint64_t currentFreq = nvs::getEnergyPollFreq();  // Returns microseconds
                        
                        if (freq_us != currentFreq) {
                            nvs::setEnergyPollFreq(freq_us);  // Expects microseconds
                            // Update TaskManager's power report frequency (expects milliseconds)
                            uint64_t freq_ms = static_cast<uint64_t>(interval_sec) * 1000ULL;
                            TaskManager::updatePowerReportFrequency(freq_ms);
                            anyChanges = true;
                            LOG_SUCCESS(LOG_TAG_CONFIG, "Energy report interval updated to %u s (%llu μs)", 
                                  interval_sec, freq_us);
                        }
                    }
                    
                    if (anyChanges) {
                        LOG_DIVIDER();
                        LOG_SUCCESS(LOG_TAG_CONFIG, "Configuration changes applied successfully");
                        LOG_DIVIDER();
                        
                        // Send acknowledgment to Flask server to mark config as "acknowledged"
                        sendConfigAcknowledgment("applied", "Configuration updated successfully");
                    } else {
                        LOG_INFO(LOG_TAG_CONFIG, "No actual changes (config same as current)");
                    }
                } else {
                    LOG_WARN(LOG_TAG_CONFIG, "Config object not found in response");
                }
            } else {
                LOG_INFO(LOG_TAG_CONFIG, "No pending configuration changes");
            }

            http.end();
        } else {
            http.end();
            LOG_ERROR(LOG_TAG_CONFIG, "JSON parse error: %s (response size: %d bytes)", 
                  error.c_str(), responseStr.length());
            LOG_DEBUG(LOG_TAG_CONFIG, "Response preview: %s", responseStr.substring(0, 500).c_str());
        }
    } else {
        LOG_ERROR(LOG_TAG_CONFIG, "HTTP GET failed with error code: %d", httpResponseCode);
        http.end();
    }
}

bool ConfigManager::applyRegisterChanges(const RegID** newSelection, size_t* newCount) {
    *newSelection = nvs::getReadRegs();
    *newCount = nvs::getReadRegCount();
    
    // Update current config
    currentConfig.registers = *newSelection;
    currentConfig.registerCount = *newCount;
    
    LOG_INFO(LOG_TAG_CONFIG, "Applied register changes: %zu registers", *newCount);
    
    // Send acknowledgment to Flask server
    sendConfigAcknowledgment("applied", "Register selection updated successfully");
    
    return true;
}

bool ConfigManager::applyPollFrequencyChange(uint64_t* newFreq) {
    *newFreq = nvs::getPollFreq();
    currentConfig.pollFrequency = *newFreq;
    
    LOG_INFO(LOG_TAG_CONFIG, "Applied poll frequency change: %llu μs", *newFreq);
    
    // Send acknowledgment to Flask server
    sendConfigAcknowledgment("applied", "Poll frequency updated successfully");
    
    return true;
}

bool ConfigManager::applyUploadFrequencyChange(uint64_t* newFreq) {
    *newFreq = nvs::getUploadFreq();
    currentConfig.uploadFrequency = *newFreq;
    
    LOG_INFO(LOG_TAG_CONFIG, "Applied upload frequency change: %llu μs", *newFreq);
    
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
    
    LOG_INFO(LOG_TAG_CONFIG, "Configuration updated");
}

void ConfigManager::printCurrentConfig() {
    LOG_SECTION("CURRENT CONFIGURATION");
    LOG_INFO(LOG_TAG_CONFIG, "Register Count:    %zu", currentConfig.registerCount);
    LOG_INFO(LOG_TAG_CONFIG, "Registers:");
    
    for (size_t i = 0; i < currentConfig.registerCount && i < REGISTER_COUNT; i++) {
        // Registers listed individually below
    }
    // Newline not needed with LOG_INFO
    
    LOG_INFO(LOG_TAG_CONFIG, "Poll Frequency:    %llu μs (%.2f s)", 
          currentConfig.pollFrequency, currentConfig.pollFrequency / 1000000.0);
    LOG_INFO(LOG_TAG_CONFIG, "Upload Frequency:  %llu μs (%.2f s)", 
          currentConfig.uploadFrequency, currentConfig.uploadFrequency / 1000000.0);
    
    // Print power management configuration
    bool powerEnabled = nvs::getPowerEnabled();
    uint8_t techniques = nvs::getPowerTechniques();
    uint64_t energyPollFreq = nvs::getEnergyPollFreq();
    
    LOG_INFO(LOG_TAG_CONFIG, "Power Management:  %s", powerEnabled ? "ENABLED" : "DISABLED");
    LOG_INFO(LOG_TAG_CONFIG, "Techniques:        0x%02X", techniques);
    // Techniques logged above
    // Techniques logged above
    // Techniques logged above
    // Techniques logged above
    // Newline not needed with LOG_INFO
    LOG_INFO(LOG_TAG_CONFIG, "Energy Poll:       %llu μs (%.2f s)", 
          energyPollFreq, energyPollFreq / 1000000.0);
    
    LOG_INFO(LOG_TAG_CONFIG, "===========================================");
}

void ConfigManager::sendConfigAcknowledgment(const char* status, const char* message) {
    /**
     * Send configuration acknowledgment to Flask server
     * POST /config/{device_id}/acknowledge
     * Body: {"status": "applied"|"failed", "timestamp": 123456, "error_msg": "..."}
     */
    
    if (WiFi.status() != WL_CONNECTED) {
        LOG_WARN(LOG_TAG_CONFIG, "WiFi not connected. Cannot send acknowledgment.");
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
    StaticJsonDocument<512> doc;  // Increased from 256 to 512 for power_management
    doc["status"] = status;
    doc["timestamp"] = getCurrentTimestamp();  // Unix timestamp in seconds
    if (message != nullptr && strlen(message) > 0) {
        doc["error_msg"] = message;
    }
    
    // Include current power management configuration in acknowledgment
    JsonObject powerMgmt = doc.createNestedObject("power_management");
    powerMgmt["enabled"] = nvs::getPowerEnabled();
    powerMgmt["techniques"] = nvs::getPowerTechniques();
    // Send frequency in milliseconds (energy_poll_freq not energy_poll_interval)
    uint64_t freq_ms = nvs::getEnergyPollFreq();
    powerMgmt["energy_poll_freq"] = static_cast<uint32_t>(freq_ms);
    
    String payload;
    serializeJsonPretty(doc, payload);
    
    LOG_DEBUG(LOG_TAG_CONFIG, "Sending acknowledgment:");
    // Print each line of the JSON with proper indentation
    int startPos = 0;
    int endPos = payload.indexOf('\n');
    while (endPos != -1) {
        LOG_DEBUG(LOG_TAG_CONFIG, "  %s", payload.substring(startPos, endPos).c_str());
        startPos = endPos + 1;
        endPos = payload.indexOf('\n', startPos);
    }
    if (startPos < payload.length()) {
        LOG_DEBUG(LOG_TAG_CONFIG, "  %s", payload.substring(startPos).c_str());
    }
    
    // Re-serialize compact for actual transmission
    payload = "";
    serializeJson(doc, payload);
    
    int httpCode = http.POST(payload);
    
    if (httpCode > 0) {
        if (httpCode == 200) {
            LOG_SUCCESS(LOG_TAG_CONFIG, "Acknowledgment sent successfully");
        } else {
            LOG_WARN(LOG_TAG_CONFIG, "Acknowledgment sent but received code: %d", httpCode);
        }
    } else {
        LOG_ERROR(LOG_TAG_CONFIG, "Failed to send acknowledgment: %d", httpCode);
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
        LOG_WARN(LOG_TAG_CONFIG, "WiFi not connected. Cannot send current config.");
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
    
    // Add energy poll interval (convert microseconds to seconds)
    uint64_t energyPollUs = nvs::getEnergyPollFreq();  // Returns microseconds
    doc["energy_poll_interval"] = (uint32_t)(energyPollUs / 1000000);  // Convert μs to seconds
    
    // Add power management configuration
    JsonObject powerMgmt = doc.createNestedObject("power_management");
    powerMgmt["enabled"] = nvs::getPowerEnabled();
    powerMgmt["techniques"] = nvs::getPowerTechniques();

    doc["compression_enabled"] = true;  // Always enabled in our implementation
    doc["timestamp"] = getCurrentTimestamp();
    
    // Add register names
    JsonArray registers = doc.createNestedArray("registers");
    for (size_t i = 0; i < currentConfig.registerCount && i < REGISTER_COUNT; i++) {
        registers.add(REGISTER_MAP[currentConfig.registers[i]].name);
    }
    
    String payload;
    serializeJsonPretty(doc, payload);
    
    LOG_INFO(LOG_TAG_CONFIG, "Sending current config:");
    // Print each line of the JSON with proper indentation
    int startPos = 0;
    int endPos = payload.indexOf('\n');
    while (endPos != -1) {
        LOG_INFO(LOG_TAG_CONFIG, "  %s", payload.substring(startPos, endPos).c_str());
        startPos = endPos + 1;
        endPos = payload.indexOf('\n', startPos);
    }
    if (startPos < payload.length()) {
        LOG_INFO(LOG_TAG_CONFIG, "  %s", payload.substring(startPos).c_str());
    }
    
    // Re-serialize compact for actual transmission
    payload = "";
    serializeJson(doc, payload);
    
    int httpCode = http.POST(payload);
    
    if (httpCode > 0) {
        if (httpCode == 200 || httpCode == 201) {
            LOG_SUCCESS(LOG_TAG_CONFIG, "Current config sent successfully");
        } else {
            String response = http.getString();
            LOG_WARN(LOG_TAG_CONFIG, "Config sent but received code: %d, response: %s", 
                  httpCode, response.c_str());
        }
    } else {
        LOG_ERROR(LOG_TAG_CONFIG, "Failed to send current config: %d", httpCode);
    }
    
    http.end();
}

