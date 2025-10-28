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

    PRINT_SECTION("COMMAND POLL");
    PRINT_PROGRESS("Checking for pending commands...");

    HTTPClient http;
    http.begin(pollURL);
    http.addHeader("Content-Type", "application/json");

    // M4 Format: Use GET request (device_id is in URL)
    int httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
        WiFiClient* stream = http.getStreamPtr();
        static char responseBuffer[512];
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

        StaticJsonDocument<512> responseDoc;
        DeserializationError error = deserializeJson(responseDoc, responseBuffer);

        if (!error && responseDoc.containsKey("command")) {
            const char* commandId = responseDoc["command"]["command_id"] | "";
            const char* commandType = responseDoc["command"]["command_type"] | "";
            
            print("  [CMD] Received: %s (ID: %s)\n", commandType, commandId);
            
            // Extract parameters as JSON string
            char parameters[256] = {0};
            if (responseDoc["command"].containsKey("parameters")) {
                serializeJson(responseDoc["command"]["parameters"], parameters, sizeof(parameters));
                print("  [INFO] Parameters: %s\n", parameters);
            }
            
            // Execute the command
            bool success = executeCommand(commandId, commandType, parameters);
            
            // Send result back to server
            char result[128];
            snprintf(result, sizeof(result), "Command %s: %s", commandType, 
                     success ? "executed successfully" : "failed");
            sendCommandResult(commandId, success, result);
            
            if (success) {
                PRINT_SUCCESS("Command executed successfully");
            } else {
                PRINT_ERROR("Command execution failed");
            }
        } else if (error) {
            PRINT_ERROR("Failed to parse JSON response: %s", error.c_str());
        } else {
            PRINT_INFO("No pending commands in queue");
        }

        http.end();
    } else {
        PRINT_ERROR("HTTP POST failed - Error code: %d", httpResponseCode);
        http.end();
    }
}

bool CommandExecutor::executeCommand(const char* commandId, const char* commandType, 
                                     const char* parameters) {
    print("[CommandExecutor] Executing: %s\n", commandType);
    print("[CommandExecutor] Parameters: %s\n", parameters);
    
    commandsExecuted++;
    bool success = false;
    
    // Route to appropriate handler
    if (strcmp(commandType, "set_power") == 0) {
        success = executePowerCommand(parameters);
    } else if (strcmp(commandType, "set_power_percentage") == 0) {
        success = executePowerPercentageCommand(parameters);
    } else if (strcmp(commandType, "write_register") == 0) {
        success = executeWriteRegisterCommand(parameters);
    } else if (strcmp(commandType, "get_power_stats") == 0) {
        success = executeGetPowerStatsCommand();
    } else if (strcmp(commandType, "reset_power_stats") == 0) {
        success = executeResetPowerStatsCommand();
    } else if (strcmp(commandType, "get_peripheral_stats") == 0) {
        success = executeGetPeripheralStatsCommand();
    } else if (strcmp(commandType, "reset_peripheral_stats") == 0) {
        success = executeResetPeripheralStatsCommand();
    } else {
        print("[CommandExecutor] Unknown command type: %s\n", commandType);
        success = false;
    }
    
    if (success) {
        commandsSuccessful++;
    } else {
        commandsFailed++;
    }
    
    return success;
}

bool CommandExecutor::executePowerCommand(const char* parameters) {
    StaticJsonDocument<256> paramDoc;
    DeserializationError error = deserializeJson(paramDoc, parameters);
    
    if (error) {
        print("[CommandExecutor] Failed to parse parameters\n");
        return false;
    }
    
    int powerValue = paramDoc["power_value"] | 0;
    
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

bool CommandExecutor::executePowerPercentageCommand(const char* parameters) {
    StaticJsonDocument<256> paramDoc;
    DeserializationError error = deserializeJson(paramDoc, parameters);
    
    if (error) {
        print("[CommandExecutor] Failed to parse parameters\n");
        return false;
    }
    
    int percentage = paramDoc["percentage"] | 0;
    
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

bool CommandExecutor::executeWriteRegisterCommand(const char* parameters) {
    StaticJsonDocument<256> paramDoc;
    DeserializationError error = deserializeJson(paramDoc, parameters);
    
    if (error) {
        print("[CommandExecutor] Failed to parse parameters\n");
        return false;
    }
    
    int regAddress = paramDoc["register_address"] | 0;
    int value = paramDoc["value"] | 0;
    
    print("[CommandExecutor] Write register command not yet implemented\n");
    print("[CommandExecutor] Would write register %d with value %d\n", regAddress, value);
    
    // TODO: Implement actual register write
    return false;
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

void CommandExecutor::sendCommandResult(const char* commandId, bool success, const char* result) {
    print("[CommandExecutor] Sending command result to server...\n");
    
    if (WiFi.status() != WL_CONNECTED) {
        print("[CommandExecutor] WiFi not connected. Cannot send result.\n");
        return;
    }

    HTTPClient http;
    http.begin(resultURL);
    http.addHeader("Content-Type", "application/json");

    StaticJsonDocument<256> resultDoc;
    resultDoc["command_id"] = commandId;
    resultDoc["status"] = success ? "completed" : "failed";
    resultDoc["result"] = result;

    char resultBody[256];
    serializeJson(resultDoc, resultBody, sizeof(resultBody));

    int httpResponseCode = http.POST((uint8_t*)resultBody, strlen(resultBody));

    if (httpResponseCode == 200) {
        print("[CommandExecutor] Command result sent successfully\n");
    } else {
        print("[CommandExecutor] Failed to send command result (HTTP %d)\n", httpResponseCode);
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
