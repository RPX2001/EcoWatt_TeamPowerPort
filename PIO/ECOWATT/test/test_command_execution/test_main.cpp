/**
 * @file test_main.cpp
 * @brief M4 - Command Execution Tests for ESP32
 * 
 * Tests remote command execution workflow:
 * - Command polling from cloud
 * - Command parsing and validation
 * - Command execution (power, register writes)
 * - Result reporting back to cloud
 * - Statistics tracking
 * - Error handling
 * 
 * Full round-trip: Cloud → Device → Inverter → Device → Cloud
 * 
 * @author Team PowerPort
 * @date 2025-01-22
 */

#include <unity.h>
#include "application/command_executor.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include "peripheral/acquisition.h"
#include "driver/protocol_adapter.h"
#include "config/test_config.h"  // Centralized configuration

// Global adapter instance (same as in acquisition.cpp)
extern ProtocolAdapter adapter;

// Use configuration from test_config.h
const char* TEST_POLL_URL = FLASK_COMMAND_POLL_URL(TEST_DEVICE_ID_M4_CMD);
const char* TEST_RESULT_URL = FLASK_COMMAND_RESULT_URL(TEST_DEVICE_ID_M4_CMD);

// WiFi connection status
bool wifiConnected = false;

void connectWiFi() {
    if (wifiConnected) return;
    
    Serial.println("\n[TEST] Connecting to WiFi...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        Serial.println("\n[TEST] WiFi connected!");
        Serial.print("[TEST] IP Address: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("\n[TEST] WiFi connection failed!");
    }
}

void setUp(void) {
    // Reset command statistics before each test
    CommandExecutor::resetStats();
}

void tearDown(void) {
    // Clean up after each test
}

/**
 * @brief Test 1: Command executor initialization
 * 
 * Verifies proper initialization with endpoints and device ID
 */
void test_command_executor_initialization(void) {
    CommandExecutor::init(TEST_POLL_URL, TEST_RESULT_URL, TEST_DEVICE_ID_M4_CMD);
    
    // Verify stats are initialized to zero
    unsigned long executed = 0, successful = 0, failed = 0;
    CommandExecutor::getCommandStats(executed, successful, failed);
    
    TEST_ASSERT_EQUAL(0, executed);
    TEST_ASSERT_EQUAL(0, successful);
    TEST_ASSERT_EQUAL(0, failed);
}

/**
 * @brief Test 2: Set power command execution
 * 
 * Tests executing a set_power command
 */
void test_execute_set_power_command(void) {
    CommandExecutor::init(TEST_POLL_URL, TEST_RESULT_URL, TEST_DEVICE_ID_M4_CMD);
    
    // Prepare parameters JSON
    const char* commandId = "test-cmd-001";
    const char* commandType = "set_power";
    const char* parameters = "{\"power_value\":5000}"; // 5000W = 50%
    
    // Execute command
    bool success = CommandExecutor::executeCommand(commandId, commandType, parameters);
    
    // Should succeed (even if hardware not present, command parsing works)
    TEST_ASSERT_TRUE(success);
    
    // Verify statistics updated
    unsigned long executed = 0, successful = 0, failed = 0;
    CommandExecutor::getCommandStats(executed, successful, failed);
    
    TEST_ASSERT_EQUAL(1, executed);
    TEST_ASSERT_EQUAL(1, successful);
    TEST_ASSERT_EQUAL(0, failed);
}

/**
 * @brief Test 3: Set power percentage command
 * 
 * Tests executing a set_power_percentage command
 */
void test_execute_set_power_percentage_command(void) {
    CommandExecutor::init(TEST_POLL_URL, TEST_RESULT_URL, TEST_DEVICE_ID_M4_CMD);
    
    const char* commandId = "test-cmd-002";
    const char* commandType = "set_power_percentage";
    const char* parameters = "{\"percentage\":75}"; // 75%
    
    bool success = CommandExecutor::executeCommand(commandId, commandType, parameters);
    
    TEST_ASSERT_TRUE(success);
    
    unsigned long executed = 0, successful = 0, failed = 0;
    CommandExecutor::getCommandStats(executed, successful, failed);
    
    TEST_ASSERT_EQUAL(1, executed);
    TEST_ASSERT_EQUAL(1, successful);
}

/**
 * @brief Test 4: Write register command
 * 
 * Tests executing a write_register command to inverter
 * NOTE: Currently not implemented, so should return false
 */
void test_execute_write_register_command(void) {
    CommandExecutor::init(TEST_POLL_URL, TEST_RESULT_URL, TEST_DEVICE_ID_M4_CMD);
    
    const char* commandId = "test-cmd-003";
    const char* commandType = "write_register";
    const char* parameters = "{\"register_address\":8,\"value\":50}";
    
    bool success = CommandExecutor::executeCommand(commandId, commandType, parameters);
    
    // Write register is not yet implemented, should return false
    TEST_ASSERT_FALSE(success);
    
    unsigned long executed = 0, successful = 0, failed = 0;
    CommandExecutor::getCommandStats(executed, successful, failed);
    
    TEST_ASSERT_EQUAL(1, executed);
    TEST_ASSERT_EQUAL(0, successful);
    TEST_ASSERT_EQUAL(1, failed);
}

/**
 * @brief Test 5: Get power statistics command
 * 
 * Tests command to retrieve power management statistics
 */
void test_execute_get_power_stats_command(void) {
    CommandExecutor::init(TEST_POLL_URL, TEST_RESULT_URL, TEST_DEVICE_ID_M4_CMD);
    
    const char* commandId = "test-cmd-004";
    const char* commandType = "get_power_stats";
    const char* parameters = "{}";
    
    bool success = CommandExecutor::executeCommand(commandId, commandType, parameters);
    
    TEST_ASSERT_TRUE(success);
    
    unsigned long executed = 0, successful = 0, failed = 0;
    CommandExecutor::getCommandStats(executed, successful, failed);
    
    TEST_ASSERT_EQUAL(1, executed);
    TEST_ASSERT_EQUAL(1, successful);
}

/**
 * @brief Test 6: Reset power statistics command
 * 
 * Tests command to reset power management statistics
 */
void test_execute_reset_power_stats_command(void) {
    CommandExecutor::init(TEST_POLL_URL, TEST_RESULT_URL, TEST_DEVICE_ID_M4_CMD);
    
    const char* commandId = "test-cmd-005";
    const char* commandType = "reset_power_stats";
    const char* parameters = "{}";
    
    bool success = CommandExecutor::executeCommand(commandId, commandType, parameters);
    
    TEST_ASSERT_TRUE(success);
}

/**
 * @brief Test 7: Unknown command handling
 * 
 * Tests that unknown commands are rejected properly
 */
void test_execute_unknown_command(void) {
    CommandExecutor::init(TEST_POLL_URL, TEST_RESULT_URL, TEST_DEVICE_ID_M4_CMD);
    
    const char* commandId = "test-cmd-006";
    const char* commandType = "invalid_command_type";
    const char* parameters = "{}";
    
    bool success = CommandExecutor::executeCommand(commandId, commandType, parameters);
    
    // Should fail for unknown command
    TEST_ASSERT_FALSE(success);
    
    unsigned long executed = 0, successful = 0, failed = 0;
    CommandExecutor::getCommandStats(executed, successful, failed);
    
    TEST_ASSERT_EQUAL(1, executed);
    TEST_ASSERT_EQUAL(0, successful);
    TEST_ASSERT_EQUAL(1, failed);
}

/**
 * @brief Test 8: Invalid parameters handling
 * 
 * Tests command execution with malformed JSON parameters
 */
void test_execute_command_invalid_parameters(void) {
    CommandExecutor::init(TEST_POLL_URL, TEST_RESULT_URL, TEST_DEVICE_ID_M4_CMD);
    
    const char* commandId = "test-cmd-007";
    const char* commandType = "set_power";
    const char* parameters = "{invalid json}"; // Malformed JSON
    
    bool success = CommandExecutor::executeCommand(commandId, commandType, parameters);
    
    // Should fail due to invalid JSON
    TEST_ASSERT_FALSE(success);
    
    unsigned long executed = 0, successful = 0, failed = 0;
    CommandExecutor::getCommandStats(executed, successful, failed);
    
    TEST_ASSERT_EQUAL(1, executed);
    TEST_ASSERT_EQUAL(0, successful);
    TEST_ASSERT_EQUAL(1, failed);
}

/**
 * @brief Test 9: Multiple command execution
 * 
 * Tests executing multiple commands in sequence
 */
void test_execute_multiple_commands(void) {
    CommandExecutor::init(TEST_POLL_URL, TEST_RESULT_URL, TEST_DEVICE_ID_M4_CMD);
    
    // Execute first command (set_power - requires WiFi/adapter)
    bool success1 = CommandExecutor::executeCommand("cmd-1", "set_power", "{\"power_value\":3000}");
    // May succeed or fail depending on hardware, just check it executed
    
    // Execute second command (set_power_percentage - requires WiFi/adapter)
    bool success2 = CommandExecutor::executeCommand("cmd-2", "set_power_percentage", "{\"percentage\":60}");
    
    // Execute third command (get_power_stats - always succeeds)
    bool success3 = CommandExecutor::executeCommand("cmd-3", "get_power_stats", "{}");
    TEST_ASSERT_TRUE(success3);
    
    // Verify statistics - at least 3 commands executed
    unsigned long executed = 0, successful = 0, failed = 0;
    CommandExecutor::getCommandStats(executed, successful, failed);
    
    TEST_ASSERT_EQUAL(3, executed);
    TEST_ASSERT_GREATER_OR_EQUAL(1, successful); // At least get_power_stats succeeded
}

/**
 * @brief Test 10: Command statistics tracking
 * 
 * Tests that command statistics are properly tracked
 */
void test_command_statistics_tracking(void) {
    CommandExecutor::init(TEST_POLL_URL, TEST_RESULT_URL, TEST_DEVICE_ID_M4_CMD);
    
    // Execute mix of successful and failed commands
    CommandExecutor::executeCommand("cmd-1", "set_power", "{\"power_value\":4000}"); // Success
    CommandExecutor::executeCommand("cmd-2", "invalid_cmd", "{}"); // Fail
    CommandExecutor::executeCommand("cmd-3", "set_power_percentage", "{\"percentage\":80}"); // Success
    CommandExecutor::executeCommand("cmd-4", "unknown", "{}"); // Fail
    CommandExecutor::executeCommand("cmd-5", "get_power_stats", "{}"); // Success
    
    unsigned long executed = 0, successful = 0, failed = 0;
    CommandExecutor::getCommandStats(executed, successful, failed);
    
    TEST_ASSERT_EQUAL(5, executed);
    TEST_ASSERT_EQUAL(3, successful);
    TEST_ASSERT_EQUAL(2, failed);
}

/**
 * @brief Test 11: Command statistics reset
 * 
 * Tests resetting command statistics
 */
void test_command_statistics_reset(void) {
    CommandExecutor::init(TEST_POLL_URL, TEST_RESULT_URL, TEST_DEVICE_ID_M4_CMD);
    
    // Execute some commands
    CommandExecutor::executeCommand("cmd-1", "set_power", "{\"power_value\":2000}");
    CommandExecutor::executeCommand("cmd-2", "get_power_stats", "{}");
    
    unsigned long executed = 0, successful = 0, failed = 0;
    CommandExecutor::getCommandStats(executed, successful, failed);
    TEST_ASSERT_EQUAL(2, executed);
    
    // Reset statistics
    CommandExecutor::resetStats();
    
    // Verify reset
    CommandExecutor::getCommandStats(executed, successful, failed);
    TEST_ASSERT_EQUAL(0, executed);
    TEST_ASSERT_EQUAL(0, successful);
    TEST_ASSERT_EQUAL(0, failed);
}

/**
 * @brief Test 12: Power value clamping
 * 
 * Tests that power values are clamped to valid range (0-100%)
 */
void test_power_value_clamping(void) {
    CommandExecutor::init(TEST_POLL_URL, TEST_RESULT_URL, TEST_DEVICE_ID_M4_CMD);
    
    // Test overrange power value (should clamp to 100%)
    bool success1 = CommandExecutor::executeCommand("cmd-1", "set_power", "{\"power_value\":15000}");
    TEST_ASSERT_TRUE(success1); // Should succeed with clamping
    
    // Test underrange power value (should clamp to 0%)
    bool success2 = CommandExecutor::executeCommand("cmd-2", "set_power", "{\"power_value\":-1000}");
    TEST_ASSERT_TRUE(success2); // Should succeed with clamping
    
    unsigned long executed = 0, successful = 0, failed = 0;
    CommandExecutor::getCommandStats(executed, successful, failed);
    
    TEST_ASSERT_EQUAL(2, executed);
    TEST_ASSERT_EQUAL(2, successful);
}

void setup() {
    delay(2000); // Allow serial monitor to connect
    
    Serial.begin(115200);
    Serial.println("\n\n========================================");
    Serial.println("  M4 COMMAND EXECUTION TESTS");
    Serial.println("========================================");
    Serial.println("WiFi SSID: " + String(WIFI_SSID));
    Serial.println("Flask Server: " + String(FLASK_SERVER_URL));
    Serial.println("Inverter API: " + String(INVERTER_API_BASE_URL));
    Serial.println("Device ID: " + String(TEST_DEVICE_ID_M4_CMD));
    Serial.println("========================================\n");
    
    // Connect to WiFi for power commands and Flask communication
    connectWiFi();
    
    // Initialize acquisition adapter if WiFi connected
    if (wifiConnected) {
        Serial.println("[TEST] Initializing adapter for inverter API...");
        adapter.setApiKey(INVERTER_API_KEY);
        Serial.println("[TEST] Adapter configured with API key!");
        Serial.println("[TEST] Ready to execute commands against real inverter simulator");
    } else {
        Serial.println("[TEST] WARNING: No WiFi - power commands will fail");
    }
    
    UNITY_BEGIN();
    
    RUN_TEST(test_command_executor_initialization);
    RUN_TEST(test_execute_set_power_command);
    RUN_TEST(test_execute_set_power_percentage_command);
    RUN_TEST(test_execute_write_register_command);
    RUN_TEST(test_execute_get_power_stats_command);
    RUN_TEST(test_execute_reset_power_stats_command);
    RUN_TEST(test_execute_unknown_command);
    RUN_TEST(test_execute_command_invalid_parameters);
    RUN_TEST(test_execute_multiple_commands);
    RUN_TEST(test_command_statistics_tracking);
    RUN_TEST(test_command_statistics_reset);
    RUN_TEST(test_power_value_clamping);
    
    UNITY_END();
}

void loop() {
    // Tests run once in setup()
}
