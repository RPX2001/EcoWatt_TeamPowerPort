/**
 * @file command_executor.h
 * @brief Remote command polling and execution
 * 
 * This module handles polling for remote commands from the server
 * and executing them on the ESP32 device.
 * 
 * @author Team PowerPort
 * @date 2025-10-22
 */

#ifndef COMMAND_EXECUTOR_H
#define COMMAND_EXECUTOR_H

#include <Arduino.h>
#include <ArduinoJson.h>  // For JsonObject type

/**
 * @class CommandExecutor
 * @brief Manages remote command execution following Milestone 4 format
 * 
 * This class provides a singleton-style interface for polling commands
 * from the server and executing them locally.
 */
class CommandExecutor {
public:
    /**
     * @brief Initialize the command executor
     * 
     * @param pollURL URL endpoint for polling commands
     * @param resultURL URL endpoint for sending command results
     * @param deviceID Unique device identifier
     */
    static void init(const char* pollURL, const char* resultURL, const char* deviceID);

    /**
     * @brief Check for and execute pending commands from server
     * 
     * This function polls the server for commands, executes them,
     * and sends results back to the server.
     */
    static void checkAndExecuteCommands();

    /**
     * @brief Execute a specific command (M4 format)
     * 
     * @param commandId Unique identifier for this command
     * @param action M4 action (e.g., "write_register")
     * @param m4Command JsonObject containing M4 command fields (target_register, value, etc.)
     * @return true if execution successful, false otherwise
     */
    static bool executeCommand(const char* commandId, const char* action, 
                               JsonObject& m4Command);

    /**
     * @brief Send command execution result back to server (M4 format)
     * 
     * @param commandId Command identifier
     * @param success Whether command succeeded
     */
    static void sendCommandResult(const char* commandId, bool success);

    /**
     * @brief Get command execution statistics
     * 
     * @param totalExecuted Output: total commands executed
     * @param totalSuccessful Output: total successful commands
     * @param totalFailed Output: total failed commands
     */
    static void getCommandStats(unsigned long& totalExecuted, unsigned long& totalSuccessful,
                                unsigned long& totalFailed);

    /**
     * @brief Reset command statistics
     */
    static void resetStats();

    /**
     * @brief Print command statistics to serial
     */
    static void printStats();

private:
    static char pollURL[256];
    static char resultURL[256];
    static char deviceID[64];

    // Statistics
    static unsigned long commandsExecuted;
    static unsigned long commandsSuccessful;
    static unsigned long commandsFailed;

    // Command execution handlers (M4 format)
    static bool executePowerCommand(JsonObject& m4Command);
    static bool executePowerPercentageCommand(JsonObject& m4Command);
    static bool executeWriteRegisterCommand(JsonObject& m4Command);
    static bool executeGetPowerStatsCommand();
    static bool executeResetPowerStatsCommand();
    static bool executeGetPeripheralStatsCommand();
    static bool executeResetPeripheralStatsCommand();

    // Prevent instantiation
    CommandExecutor() = delete;
    ~CommandExecutor() = delete;
    CommandExecutor(const CommandExecutor&) = delete;
    CommandExecutor& operator=(const CommandExecutor&) = delete;
};

#endif // COMMAND_EXECUTOR_H
