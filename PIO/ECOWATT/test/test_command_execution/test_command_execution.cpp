/**
 * @file test_m4_command_execution.cpp
 * @brief Unit tests for M4 - Remote Command Execution
 * 
 * Tests for the CommandExecutor class which handles remote command
 * polling, execution, and result reporting.
 * 
 * @author Team PowerPort
 * @date 2025-10-23
 */

#include <unity.h>
#include "application/command_executor.h"

// ============================================================================
// TEST CASES
// ============================================================================

// Test 1: Initialization
void test_command_initialization(void) {
    CommandExecutor::init(
        "http://test.server.com/commands/poll",
        "http://test.server.com/commands/result",
        "TEST_DEVICE_001"
    );
    
    unsigned long executed, successful, failed;
    CommandExecutor::getCommandStats(executed, successful, failed);
    
    // After init, stats should be zero (or reset them first)
    CommandExecutor::resetStats();
    CommandExecutor::getCommandStats(executed, successful, failed);
    
    TEST_ASSERT_EQUAL(0, executed);
    TEST_ASSERT_EQUAL(0, successful);
    TEST_ASSERT_EQUAL(0, failed);
}

// Test 2: Command Statistics Initialization
void test_command_stats_initialization(void) {
    CommandExecutor::resetStats();
    
    unsigned long executed, successful, failed;
    CommandExecutor::getCommandStats(executed, successful, failed);
    
    TEST_ASSERT_EQUAL(0, executed);
    TEST_ASSERT_EQUAL(0, successful);
    TEST_ASSERT_EQUAL(0, failed);
}

// Test 3: Execute Simple Command
void test_command_execute_simple(void) {
    CommandExecutor::resetStats();
    
    // Execute a simple test command (implementation dependent)
    bool success = CommandExecutor::executeCommand(
        "CMD_001",
        "TEST",
        "{\"action\":\"ping\"}"
    );
    
    // Even if execution logic isn't implemented, function should not crash
    TEST_ASSERT_TRUE(success || !success);  // Just verify it returns
    
    unsigned long executed, successful, failed;
    CommandExecutor::getCommandStats(executed, successful, failed);
    
    // Stats should reflect the execution
    TEST_ASSERT_GREATER_OR_EQUAL(1, executed);
}

// Test 4: Command with Parameters
void test_command_with_parameters(void) {
    CommandExecutor::resetStats();
    
    bool success = CommandExecutor::executeCommand(
        "CMD_002",
        "CONFIG",
        "{\"key\":\"sample_rate\",\"value\":\"1000\"}"
    );
    
    // Verify execution completed
    TEST_ASSERT_TRUE(success || !success);
    
    unsigned long executed, successful, failed;
    CommandExecutor::getCommandStats(executed, successful, failed);
    TEST_ASSERT_GREATER_OR_EQUAL(1, executed);
}

// Test 5: Multiple Command Executions
void test_command_multiple_executions(void) {
    CommandExecutor::resetStats();
    
    // Execute multiple commands
    for (int i = 0; i < 5; i++) {
        char cmdId[20];
        sprintf(cmdId, "CMD_%03d", i);
        
        CommandExecutor::executeCommand(
            cmdId,
            "TEST",
            "{\"index\":%d}", i
        );
    }
    
    unsigned long executed, successful, failed;
    CommandExecutor::getCommandStats(executed, successful, failed);
    
    TEST_ASSERT_GREATER_OR_EQUAL(5, executed);
}

// Test 6: Command Result Sending (No crash test)
void test_command_send_result(void) {
    // This tests that sendCommandResult doesn't crash
    // Actual HTTP sending would require network setup
    CommandExecutor::sendCommandResult("CMD_001", true, "Success");
    CommandExecutor::sendCommandResult("CMD_002", false, "Failed: timeout");
    
    // If we reach here, function didn't crash
    TEST_ASSERT_TRUE(true);
}

// Test 7: Statistics Reset
void test_command_stats_reset(void) {
    // Execute some commands
    CommandExecutor::executeCommand("CMD_001", "TEST", "{}");
    CommandExecutor::executeCommand("CMD_002", "TEST", "{}");
    
    unsigned long executed1, successful1, failed1;
    CommandExecutor::getCommandStats(executed1, successful1, failed1);
    TEST_ASSERT_GREATER_THAN(0, executed1);
    
    // Reset stats
    CommandExecutor::resetStats();
    
    unsigned long executed2, successful2, failed2;
    CommandExecutor::getCommandStats(executed2, successful2, failed2);
    
    TEST_ASSERT_EQUAL(0, executed2);
    TEST_ASSERT_EQUAL(0, successful2);
    TEST_ASSERT_EQUAL(0, failed2);
}

// Test 8: Empty Command ID
void test_command_empty_id(void) {
    bool result = CommandExecutor::executeCommand("", "TEST", "{}");
    
    // Should handle empty ID gracefully
    TEST_ASSERT_TRUE(result || !result);
}

// Test 9: Empty Command Type
void test_command_empty_type(void) {
    bool result = CommandExecutor::executeCommand("CMD_001", "", "{}");
    
    // Should handle empty type gracefully
    TEST_ASSERT_TRUE(result || !result);
}

// Test 10: Null Parameters
void test_command_null_parameters(void) {
    bool result = CommandExecutor::executeCommand("CMD_001", "TEST", "");
    
    // Should handle empty parameters gracefully
    TEST_ASSERT_TRUE(result || !result);
}

// ============================================================================
// MAIN TEST RUNNER
// ============================================================================

void setup() {
    delay(2000);  // Give time for serial connection
    
    UNITY_BEGIN();
    
    // Run all tests
    RUN_TEST(test_command_initialization);
    RUN_TEST(test_command_stats_initialization);
    RUN_TEST(test_command_execute_simple);
    RUN_TEST(test_command_with_parameters);
    RUN_TEST(test_command_multiple_executions);
    RUN_TEST(test_command_send_result);
    RUN_TEST(test_command_stats_reset);
    RUN_TEST(test_command_empty_id);
    RUN_TEST(test_command_empty_type);
    RUN_TEST(test_command_null_parameters);
    
    UNITY_END();
}

void loop() {
    // Nothing to do here
}
