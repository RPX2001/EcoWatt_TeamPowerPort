/**
 * @file command_executor.cpp
 * @brief Implementation of remote command execution
 * 
 * @author Team PowerPort
 * @date 2025-10-22
 */

#include "application/command_executor.h"
#include "peripheral/logger.h"
// Temporarily undef print macro before any ArduinoJson includes (via acquisition.h)
#ifdef print
#undef print
#endif
#include "peripheral/acquisition.h"
#include "application/power_management.h"
#include "application/peripheral_power.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>

// Initialize static members
char CommandExecutor::pollURL[256] = "";
char CommandExecutor::resultURL[256] = "";
char CommandExecutor::deviceID[64] = "ESP32_Unknown";
unsigned long CommandExecutor::commandsExecuted = 0;
unsigned long CommandExecutor::commandsSuccessful = 0;
unsigned long CommandExecutor::commandsFailed = 0;

void CommandExecutor::init(const char* pollEndpoint, const char* resultEndpoint, 
                           const char* devID) {
    strncpy(pollURL, pollEndpoint, sizeof(pollURL) - 1);
    pollURL[sizeof(pollURL) - 1] = '\0';
    
    strncpy(resultURL, resultEndpoint, sizeof(resultURL) - 1);
    resultURL[sizeof(resultURL) - 1] = '\0';
    
    strncpy(deviceID, devID, sizeof(deviceID) - 1);
    deviceID[sizeof(deviceID) - 1] = '\0';
    
    commandsExecuted = 0;
    commandsSuccessful = 0;
    commandsFailed = 0;
    
    LOG_INFO(LOG_TAG_COMMAND, "CommandExecutor initialized");
    LOG_DEBUG(LOG_TAG_COMMAND, "Poll URL: %s", pollURL);
    LOG_DEBUG(LOG_TAG_COMMAND, "Result URL: %s", resultURL);
    LOG_DEBUG(LOG_TAG_COMMAND, "Device ID: %s", deviceID);
}

void CommandExecutor::checkAndExecuteCommands() {
    if (WiFi.status() != WL_CONNECTED) {
        return; // Silently skip if WiFi not connected
    }

    // Feed watchdog before network operation
    yield();
    vTaskDelay(1); // Give other tasks a chance to run

    // Removed verbose logging to reduce serial output spam
    // Only print when commands are actually found and executed
    // PRINT_SECTION("COMMAND POLL");
    // PRINT_PROGRESS("Checking for pending commands...");

    // Create WiFiClient with REDUCED timeout (shorter than mutex hold time)
    WiFiClient client;
    client.setTimeout(3000);  // 3 second connection timeout (must be < 5s mutex timeout)
    
    HTTPClient http;
    http.begin(client, pollURL);  // Use our WiFiClient with custom timeout
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Connection", "close");  // CRITICAL: Force server to close connection (avoids indefinite read loops)
    http.setConnectTimeout(3000);  // 3 seconds connect timeout
    http.setTimeout(3000);         // 3 seconds read timeout
    http.setReuse(false);          // Don't keep TCP connection alive
    
    // Feed watchdog before HTTP request
    yield();

    // M4 Format: Use GET request (device_id is in URL)
    int httpResponseCode = http.GET();

    if (httpResponseCode == 200) {
        // Success - use getString() to avoid manual stream loop traps
        String payload = http.getString();
        
        StaticJsonDocument<512> responseDoc;
        DeserializationError error = deserializeJson(responseDoc, payload);

        if (!error && responseDoc.containsKey("commands") && responseDoc["count"].as<int>() > 0) {
            // Process first command from the array
            JsonArray commands = responseDoc["commands"];
            if (commands.size() > 0) {
                JsonObject commandObj = commands[0];
                const char* commandId = commandObj["command_id"] | "";
                
                // M4 FORMAT: Extract command object with action, target_register, value
                JsonObject m4Command = commandObj["command"];
                if (!m4Command.isNull()) {
                    const char* action = m4Command["action"] | "";
                    
                    LOG_DEBUG(LOG_TAG_COMMAND, "Received command: %s (ID: %s)", action, commandId);
                    
                    // Parse M4 parameters
                    const char* targetRegister = m4Command["target_register"] | "";
                    int value = m4Command["value"] | 0;
                    int regAddress = m4Command["register_address"] | -1;  // Flask adds this
                    
                    if (strlen(targetRegister) > 0) {
                        LOG_DEBUG(LOG_TAG_COMMAND, "Target Register: %s (address: %d), Value: %d", 
                                 targetRegister, regAddress, value);
                    }
                    
                    // Execute the command
                    bool success = executeCommand(commandId, action, m4Command);
                    
                    // Send M4-compliant result back to server
                    sendCommandResult(commandId, success);
                    
                    if (success) {
                        LOG_SUCCESS(LOG_TAG_COMMAND, "Command executed successfully");
                    } else {
                        LOG_ERROR(LOG_TAG_COMMAND, "Command execution failed");
                    }
                } else {
                    LOG_ERROR(LOG_TAG_COMMAND, "Invalid Command format - missing 'command' object");
                }
            }
        } else if (error) {
            LOG_ERROR(LOG_TAG_COMMAND, "Failed to parse JSON response: %s", error.c_str());
        } else {
            // No commands - this is normal, don't print anything
            // Silently skip to avoid log spam
        }

        http.end();
    } else if (httpResponseCode == -1) {
        // Connection timeout or failed - silently skip to avoid log spam
        // This is normal when server is slow or network is congested
        http.end();
    } else if (httpResponseCode < 0) {
        // Other HTTP client error (connection failed, etc)
        LOG_WARN(LOG_TAG_COMMAND, "Command poll failed - network error (code: %d)", httpResponseCode);
        http.end();
    } else {
        // HTTP error response (4xx, 5xx)
        LOG_ERROR(LOG_TAG_COMMAND, "HTTP GET failed - Error code: %d", httpResponseCode);
        http.end();
    }
}

bool CommandExecutor::executeCommand(const char* commandId, const char* action, 
                                     JsonObject& m4Command) {
    LOG_DEBUG(LOG_TAG_COMMAND, "Executing command action: %s", action);
    
    commandsExecuted++;
    bool success = false;
    
    // Route to appropriate handler based on M4 action
    if (strcmp(action, "write_register") == 0) {
        success = executeWriteRegisterCommand(m4Command);
    } else if (strcmp(action, "set_power") == 0) {
        success = executePowerCommand(m4Command);
    } else if (strcmp(action, "set_power_percentage") == 0) {
        success = executePowerPercentageCommand(m4Command);
    } else if (strcmp(action, "get_power_stats") == 0) {
        success = executeGetPowerStatsCommand();
    } else if (strcmp(action, "reset_power_stats") == 0) {
        success = executeResetPowerStatsCommand();
    } else if (strcmp(action, "get_peripheral_stats") == 0) {
        success = executeGetPeripheralStatsCommand();
    } else if (strcmp(action, "reset_peripheral_stats") == 0) {
        success = executeResetPeripheralStatsCommand();
    } else {
        LOG_DEBUG(LOG_TAG_COMMAND, " Unknown command action: %s", action);
        success = false;
    }
    
    if (success) {
        commandsSuccessful++;
    } else {
        commandsFailed++;
    }
    
    return success;
}

bool CommandExecutor::executePowerCommand(JsonObject& m4Command) {
    int powerValue = m4Command["value"] | 0;
    
    // Register 8 accepts PERCENTAGE (0-100), not absolute watts
    const int MAX_INVERTER_CAPACITY = 10000; // Watts
    int powerPercentage = (powerValue * 100) / MAX_INVERTER_CAPACITY;
    
    // Clamp to valid range
    if (powerPercentage < 0) powerPercentage = 0;
    if (powerPercentage > 100) powerPercentage = 100;
    
    LOG_DEBUG(LOG_TAG_COMMAND, " Setting power to %d W (%d%%)", powerValue, powerPercentage);
    
    bool result = setPower(powerPercentage);
    
    if (result) {
        LOG_DEBUG(LOG_TAG_COMMAND, " Power set successfully");
    } else {
        LOG_DEBUG(LOG_TAG_COMMAND, " Failed to set power");
    }
    
    return result;
}

bool CommandExecutor::executePowerPercentageCommand(JsonObject& m4Command) {
    int percentage = m4Command["value"] | 0;
    
    // Clamp to valid range
    if (percentage < 0) percentage = 0;
    if (percentage > 100) percentage = 100;
    
    LOG_DEBUG(LOG_TAG_COMMAND, " Setting power percentage to %d%%", percentage);
    
    bool result = setPower(percentage);
    
    if (result) {
        LOG_DEBUG(LOG_TAG_COMMAND, " Power percentage set successfully");
    } else {
        LOG_DEBUG(LOG_TAG_COMMAND, " Failed to set power percentage");
    }
    
    return result;
}

bool CommandExecutor::executeWriteRegisterCommand(JsonObject& m4Command) {
    // M4 FORMAT: Extract target_register and value
    const char* targetRegister = m4Command["target_register"] | "";
    int regAddress = m4Command["register_address"] | -1;  // Flask adds this
    int value = m4Command["value"] | 0;
    
    // If we don't have numeric address, try to extract from target_register
    if (regAddress < 0) {
        // Try to parse target_register as number
        regAddress = atoi(targetRegister);
        if (regAddress == 0 && strcmp(targetRegister, "0") != 0) {
            LOG_DEBUG(LOG_TAG_COMMAND, " Invalid register address: %s", targetRegister);
            return false;
        }
    }
    
    LOG_DEBUG(LOG_TAG_COMMAND, " Writing register %d (%s) with value %d", 
          regAddress, strlen(targetRegister) > 0 ? targetRegister : "unnamed", value);
    
    // Build Modbus write frame using existing acquisition function
    char writeFrame[64];
    if (!buildWriteFrame(0x11, (uint16_t)regAddress, (uint16_t)value, writeFrame, sizeof(writeFrame))) {
        LOG_DEBUG(LOG_TAG_COMMAND, " Failed to build Modbus write frame");
        return false;
    }
    
    LOG_DEBUG(LOG_TAG_COMMAND, " Modbus write frame: %s\n", writeFrame);
    
    // Send write command to Inverter SIM via protocol adapter
    char responseFrame[64] = {0};
    bool success = false;
    
    // Try up to 3 times with delay
    for (int attempt = 0; attempt < 3; attempt++) {
        if (adapter.writeRegister(writeFrame, responseFrame, sizeof(responseFrame))) {
            LOG_DEBUG(LOG_TAG_COMMAND, " ✓ Register write successful (attempt %d/3)\n", attempt + 1);
            LOG_DEBUG(LOG_TAG_COMMAND, " Response: %s\n", responseFrame);
            success = true;
            break;
        } else {
            LOG_DEBUG(LOG_TAG_COMMAND, " Write attempt %d/3 failed\n", attempt + 1);
            if (attempt < 2) {
                vTaskDelay(pdMS_TO_TICKS(500));  // Wait 500ms before retry
            }
        }
    }
    
    if (!success) {
        LOG_DEBUG(LOG_TAG_COMMAND, " ✗ Register write failed after 3 attempts");
    }
    
    return success;
}

bool CommandExecutor::executeGetPowerStatsCommand() {
    LOG_DEBUG(LOG_TAG_COMMAND, " Printing power management statistics...");
    PowerManagement::printStats();
    return true;
}

bool CommandExecutor::executeResetPowerStatsCommand() {
    LOG_DEBUG(LOG_TAG_COMMAND, " Resetting power management statistics...");
    PowerManagement::resetStats();
    PowerManagement::printStats();
    return true;
}

bool CommandExecutor::executeGetPeripheralStatsCommand() {
    LOG_DEBUG(LOG_TAG_COMMAND, " Printing peripheral power gating statistics...");
    PeripheralPower::printStats();
    return true;
}

bool CommandExecutor::executeResetPeripheralStatsCommand() {
    LOG_DEBUG(LOG_TAG_COMMAND, " Resetting peripheral power gating statistics...");
    PeripheralPower::resetStats();
    PeripheralPower::printStats();
    return true;
}

void CommandExecutor::sendCommandResult(const char* commandId, bool success) {
    LOG_DEBUG(LOG_TAG_COMMAND, " Sending command result to server...");
    
    if (WiFi.status() != WL_CONNECTED) {
        LOG_DEBUG(LOG_TAG_COMMAND, " WiFi not connected. Cannot send result.");
        return;
    }

    // Create WiFiClient with timeout
    WiFiClient client;
    client.setTimeout(5000);  // 5 second timeout
    
    HTTPClient http;
    http.begin(client, resultURL);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Connection", "close");  // CRITICAL: Force server to close connection
    http.setConnectTimeout(5000);  // 5 seconds connect timeout
    http.setTimeout(5000);         // 5 seconds read timeout
    http.setReuse(false);          // Don't keep TCP connection alive

    // M4 FORMAT: command_result wrapper with status and executed_at
    StaticJsonDocument<256> resultDoc;
    JsonObject commandResult = resultDoc.createNestedObject("command_result");
    commandResult["command_id"] = commandId;
    commandResult["status"] = success ? "success" : "failed";
    
    // Get current time for executed_at (ISO 8601 format)
    time_t now = time(nullptr);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));
    commandResult["executed_at"] = timestamp;

    // Pretty print for logging
    String prettyResult;
    serializeJsonPretty(resultDoc, prettyResult);
    
    LOG_DEBUG(LOG_TAG_COMMAND, "command result payload:");
    int startPos = 0;
    int endPos = prettyResult.indexOf('\n');
    while (endPos != -1) {
        LOG_DEBUG(LOG_TAG_COMMAND, "  %s", prettyResult.substring(startPos, endPos).c_str());
        startPos = endPos + 1;
        endPos = prettyResult.indexOf('\n', startPos);
    }
    if (startPos < prettyResult.length()) {
        LOG_DEBUG(LOG_TAG_COMMAND, "  %s", prettyResult.substring(startPos).c_str());
    }
    
    // Compact for actual transmission
    char resultBody[256];
    serializeJson(resultDoc, resultBody, sizeof(resultBody));

    int httpResponseCode = http.POST((uint8_t*)resultBody, strlen(resultBody));

    if (httpResponseCode == 200) {
        LOG_DEBUG(LOG_TAG_COMMAND, " ✓ Command result sent successfully");
    } else {
        LOG_DEBUG(LOG_TAG_COMMAND, " ✗ Failed to send result (HTTP %d)\n", httpResponseCode);
    }

    http.end();
}

void CommandExecutor::getCommandStats(unsigned long& totalExecuted, unsigned long& totalSuccessful,
                                      unsigned long& totalFailed) {
    totalExecuted = commandsExecuted;
    totalSuccessful = commandsSuccessful;
    totalFailed = commandsFailed;
}

void CommandExecutor::resetStats() {
    commandsExecuted = 0;
    commandsSuccessful = 0;
    commandsFailed = 0;
    LOG_DEBUG(LOG_TAG_COMMAND, " Statistics reset");
}

void CommandExecutor::printStats() {
    LOG_INFO(LOG_TAG_COMMAND, "\n========== COMMAND EXECUTOR STATISTICS ==========");
    LOG_INFO(LOG_TAG_COMMAND, "  Total Commands:      %lu\n", commandsExecuted);
    LOG_INFO(LOG_TAG_COMMAND, "  Successful:          %lu\n", commandsSuccessful);
    LOG_INFO(LOG_TAG_COMMAND, "  Failed:              %lu\n", commandsFailed);
    
    if (commandsExecuted > 0) {
        float successRate = (commandsSuccessful * 100.0f) / commandsExecuted;
        LOG_INFO(LOG_TAG_COMMAND, "  Success Rate:        %.2f\n", successRate);
    }
    
    LOG_INFO(LOG_TAG_COMMAND, "==================================================");;
}
