/**
 * @file command_executor.cpp
 * @brief Implementation of remote command execution
 * 
 * @author Team PowerPort
 * @date 2025-10-22
 */

#include "application/command_executor.h"
#include "peripheral/print.h"
#include "peripheral/formatted_print.h"
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
// Redefine print macro after all ArduinoJson includes
#define print(...) debug.log(__VA_ARGS__)

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
    
    print("[CommandExecutor] Initialized\n");
    print("[CommandExecutor] Poll URL: %s\n", pollURL);
    print("[CommandExecutor] Result URL: %s\n", resultURL);
    print("[CommandExecutor] Device: %s\n", deviceID);
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
                    
                    print("  [CMD] Received M4 command: %s (ID: %s)\n", action, commandId);
                    
                    // Parse M4 parameters
                    const char* targetRegister = m4Command["target_register"] | "";
                    int value = m4Command["value"] | 0;
                    int regAddress = m4Command["register_address"] | -1;  // Flask adds this
                    
                    if (strlen(targetRegister) > 0) {
                        print("  [INFO] Target Register: %s", targetRegister);
                        if (regAddress >= 0) {
                            print(" (address: %d)", regAddress);
                        }
                        print(", Value: %d\n", value);
                    }
                    
                    // Execute the command
                    bool success = executeCommand(commandId, action, m4Command);
                    
                    // Send M4-compliant result back to server
                    sendCommandResult(commandId, success);
                    
                    if (success) {
                        PRINT_SUCCESS("Command executed successfully");
                    } else {
                        PRINT_ERROR("Command execution failed");
                    }
                } else {
                    PRINT_ERROR("Invalid M4 format - missing 'command' object");
                }
            }
        } else if (error) {
            PRINT_ERROR("Failed to parse JSON response: %s", error.c_str());
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
        PRINT_WARNING("Command poll failed - network error (code: %d)", httpResponseCode);
        http.end();
    } else {
        // HTTP error response (4xx, 5xx)
        PRINT_ERROR("HTTP GET failed - Error code: %d", httpResponseCode);
        http.end();
    }
}

bool CommandExecutor::executeCommand(const char* commandId, const char* action, 
                                     JsonObject& m4Command) {
    print("[CommandExecutor] Executing M4 action: %s\n", action);
    
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
        print("[CommandExecutor] Unknown M4 action: %s\n", action);
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
    
    print("[CommandExecutor] Setting power to %d W (%d%%)\n", powerValue, powerPercentage);
    
    bool result = setPower(powerPercentage);
    
    if (result) {
        print("[CommandExecutor] Power set successfully\n");
    } else {
        print("[CommandExecutor] Failed to set power\n");
    }
    
    return result;
}

bool CommandExecutor::executePowerPercentageCommand(JsonObject& m4Command) {
    int percentage = m4Command["value"] | 0;
    
    // Clamp to valid range
    if (percentage < 0) percentage = 0;
    if (percentage > 100) percentage = 100;
    
    print("[CommandExecutor] Setting power percentage to %d%%\n", percentage);
    
    bool result = setPower(percentage);
    
    if (result) {
        print("[CommandExecutor] Power percentage set successfully\n");
    } else {
        print("[CommandExecutor] Failed to set power percentage\n");
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
            print("[CommandExecutor] Invalid register address: %s\n", targetRegister);
            return false;
        }
    }
    
    print("[CommandExecutor] Writing register %d (%s) with value %d\n", 
          regAddress, strlen(targetRegister) > 0 ? targetRegister : "unnamed", value);
    
    // Build Modbus write frame using existing acquisition function
    char writeFrame[64];
    if (!buildWriteFrame(0x11, (uint16_t)regAddress, (uint16_t)value, writeFrame, sizeof(writeFrame))) {
        print("[CommandExecutor] Failed to build Modbus write frame\n");
        return false;
    }
    
    print("[CommandExecutor] Modbus write frame: %s\n", writeFrame);
    
    // Send write command to Inverter SIM via protocol adapter
    char responseFrame[64] = {0};
    bool success = false;
    
    // Try up to 3 times with delay
    for (int attempt = 0; attempt < 3; attempt++) {
        if (adapter.writeRegister(writeFrame, responseFrame, sizeof(responseFrame))) {
            print("[CommandExecutor] ✓ Register write successful (attempt %d/3)\n", attempt + 1);
            print("[CommandExecutor] Response: %s\n", responseFrame);
            success = true;
            break;
        } else {
            print("[CommandExecutor] Write attempt %d/3 failed\n", attempt + 1);
            if (attempt < 2) {
                vTaskDelay(pdMS_TO_TICKS(500));  // Wait 500ms before retry
            }
        }
    }
    
    if (!success) {
        print("[CommandExecutor] ✗ Register write failed after 3 attempts\n");
    }
    
    return success;
}

bool CommandExecutor::executeGetPowerStatsCommand() {
    print("[CommandExecutor] Printing power management statistics...\n");
    PowerManagement::printStats();
    return true;
}

bool CommandExecutor::executeResetPowerStatsCommand() {
    print("[CommandExecutor] Resetting power management statistics...\n");
    PowerManagement::resetStats();
    PowerManagement::printStats();
    return true;
}

bool CommandExecutor::executeGetPeripheralStatsCommand() {
    print("[CommandExecutor] Printing peripheral power gating statistics...\n");
    PeripheralPower::printStats();
    return true;
}

bool CommandExecutor::executeResetPeripheralStatsCommand() {
    print("[CommandExecutor] Resetting peripheral power gating statistics...\n");
    PeripheralPower::resetStats();
    PeripheralPower::printStats();
    return true;
}

void CommandExecutor::sendCommandResult(const char* commandId, bool success) {
    print("[CommandExecutor] Sending M4 command result to server...\n");
    
    if (WiFi.status() != WL_CONNECTED) {
        print("[CommandExecutor] WiFi not connected. Cannot send result.\n");
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

    char resultBody[256];
    serializeJson(resultDoc, resultBody, sizeof(resultBody));

    print("[CommandExecutor] M4 result payload: %s\n", resultBody);

    int httpResponseCode = http.POST((uint8_t*)resultBody, strlen(resultBody));

    if (httpResponseCode == 200) {
        print("[CommandExecutor] ✓ M4 command result sent successfully\n");
    } else {
        print("[CommandExecutor] ✗ Failed to send result (HTTP %d)\n", httpResponseCode);
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
    print("[CommandExecutor] Statistics reset\n");
}

void CommandExecutor::printStats() {
    print("\n========== COMMAND EXECUTOR STATISTICS ==========\n");
    print("  Total Commands:      %lu\n", commandsExecuted);
    print("  Successful:          %lu\n", commandsSuccessful);
    print("  Failed:              %lu\n", commandsFailed);
    
    if (commandsExecuted > 0) {
        float successRate = (commandsSuccessful * 100.0f) / commandsExecuted;
        print("  Success Rate:        %.2f%%\n", successRate);
    }
    
    print("==================================================\n\n");
}
