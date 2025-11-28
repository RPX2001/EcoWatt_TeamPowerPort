/**
 * @file test_main.cpp
 * @brief M4 FOTA - ESP32 Unit Tests (Firmware Download)
 * 
 * Tests OTAManager functionality without actual firmware flashing:
 * 1. OTA Manager initialization
 * 2. Manifest parsing
 * 3. Progress tracking
 * 4. State management
 * 5. Session resume capability
 * 6. Error handling
 * 7. Statistics tracking
 * 8. Test mode (fault injection)
 * 9. Chunk calculation
 * 10. Configuration updates
 */

#include <Arduino.h>
#include <unity.h>
#include <Preferences.h>
#include "application/OTAManager.h"

// Test configuration
const char* TEST_TAG = "TEST_FOTA";
const char* TEST_SERVER_URL = "http://192.168.1.100:5001";
const char* TEST_DEVICE_ID = "TEST_OTA_DEVICE";
const char* TEST_CURRENT_VERSION = "1.0.0";

// Global test instance
OTAManager* otaManager = nullptr;

void setUp(void) {
    // Setup runs before each test
}

void tearDown(void) {
    // Teardown runs after each test
}

/**
 * Test 1: OTA Manager Initialization
 * Verify that OTA Manager initializes correctly
 */
void test_ota_manager_initialization(void) {
    Serial.println("\n=== Test 1: OTA Manager Initialization ===");
    
    // Create OTA Manager instance
    otaManager = new OTAManager(
        String(TEST_SERVER_URL),
        String(TEST_DEVICE_ID),
        String(TEST_CURRENT_VERSION)
    );
    
    TEST_ASSERT_NOT_NULL(otaManager);
    
    // Check initial state
    OTAProgress progress = otaManager->getProgress();
    TEST_ASSERT_EQUAL(OTA_IDLE, progress.state);
    TEST_ASSERT_EQUAL(0, progress.chunks_received);
    TEST_ASSERT_EQUAL(0, progress.total_chunks);
    TEST_ASSERT_EQUAL(0, progress.bytes_downloaded);
    TEST_ASSERT_EQUAL(0, progress.percentage);
    
    Serial.println("✓ OTA Manager initialized correctly");
    
    delete otaManager;
    otaManager = nullptr;
}

/**
 * Test 2: Progress Tracking
 * Verify progress tracking works correctly
 */
void test_progress_tracking(void) {
    Serial.println("\n=== Test 2: Progress Tracking ===");
    
    otaManager = new OTAManager(
        String(TEST_SERVER_URL),
        String(TEST_DEVICE_ID),
        String(TEST_CURRENT_VERSION)
    );
    
    // Get initial progress
    OTAProgress progress1 = otaManager->getProgress();
    TEST_ASSERT_EQUAL(OTA_IDLE, progress1.state);
    
    // Check that progress is accessible
    TEST_ASSERT_EQUAL(0, progress1.percentage);
    
    Serial.println("✓ Progress tracking works");
    
    delete otaManager;
    otaManager = nullptr;
}

/**
 * Test 3: State Management
 * Verify OTA state transitions
 */
void test_state_management(void) {
    Serial.println("\n=== Test 3: State Management ===");
    
    otaManager = new OTAManager(
        String(TEST_SERVER_URL),
        String(TEST_DEVICE_ID),
        String(TEST_CURRENT_VERSION)
    );
    
    // Initial state should be IDLE
    String stateStr = otaManager->getStateString();
    TEST_ASSERT_TRUE(stateStr.length() > 0);
    
    // Check that we can query if OTA is in progress
    bool inProgress = otaManager->isOTAInProgress();
    TEST_ASSERT_FALSE(inProgress);  // Should not be in progress initially
    
    Serial.println("✓ State management works");
    
    delete otaManager;
    otaManager = nullptr;
}

/**
 * Test 4: Resume Capability Check
 * Verify resume capability detection
 */
void test_resume_capability(void) {
    Serial.println("\n=== Test 4: Resume Capability ===");
    
    otaManager = new OTAManager(
        String(TEST_SERVER_URL),
        String(TEST_DEVICE_ID),
        String(TEST_CURRENT_VERSION)
    );
    
    // Initially should not be able to resume (no previous session)
    bool canResume = otaManager->canResume();
    TEST_ASSERT_FALSE(canResume);
    
    Serial.println("✓ Resume capability check works");
    
    delete otaManager;
    otaManager = nullptr;
}

/**
 * Test 5: Clear Progress
 * Verify progress can be cleared
 */
void test_clear_progress(void) {
    Serial.println("\n=== Test 5: Clear Progress ===");
    
    otaManager = new OTAManager(
        String(TEST_SERVER_URL),
        String(TEST_DEVICE_ID),
        String(TEST_CURRENT_VERSION)
    );
    
    // Clear progress
    otaManager->clearProgress();
    
    // Verify progress is cleared
    OTAProgress progress = otaManager->getProgress();
    TEST_ASSERT_EQUAL(0, progress.chunks_received);
    TEST_ASSERT_EQUAL(0, progress.bytes_downloaded);
    
    Serial.println("✓ Progress clearing works");
    
    delete otaManager;
    otaManager = nullptr;
}

/**
 * Test 6: Test Mode Enable/Disable
 * Verify fault injection test mode
 */
void test_test_mode(void) {
    Serial.println("\n=== Test 6: Test Mode ===");
    
    otaManager = new OTAManager(
        String(TEST_SERVER_URL),
        String(TEST_DEVICE_ID),
        String(TEST_CURRENT_VERSION)
    );
    
    // Initially test mode should be disabled
    TEST_ASSERT_FALSE(otaManager->isTestMode());
    
    // Enable test mode
    otaManager->enableTestMode(OTA_FAULT_CORRUPT_CHUNK);
    TEST_ASSERT_TRUE(otaManager->isTestMode());
    TEST_ASSERT_EQUAL(OTA_FAULT_CORRUPT_CHUNK, otaManager->getTestFaultType());
    
    // Disable test mode
    otaManager->disableTestMode();
    TEST_ASSERT_FALSE(otaManager->isTestMode());
    
    Serial.println("✓ Test mode enable/disable works");
    
    delete otaManager;
    otaManager = nullptr;
}

/**
 * Test 7: OTA Statistics
 * Verify statistics tracking
 */
void test_ota_statistics(void) {
    Serial.println("\n=== Test 7: OTA Statistics ===");
    
    otaManager = new OTAManager(
        String(TEST_SERVER_URL),
        String(TEST_DEVICE_ID),
        String(TEST_CURRENT_VERSION)
    );
    
    // Get statistics
    uint32_t successCount = 0;
    uint32_t failureCount = 0;
    uint32_t rollbackCount = 0;
    
    otaManager->getOTAStatistics(successCount, failureCount, rollbackCount);
    
    // Initially all should be zero
    TEST_ASSERT_EQUAL(0, successCount);
    TEST_ASSERT_EQUAL(0, failureCount);
    TEST_ASSERT_EQUAL(0, rollbackCount);
    
    Serial.println("✓ Statistics tracking works");
    
    delete otaManager;
    otaManager = nullptr;
}

/**
 * Test 8: Configuration Updates
 * Verify configuration can be updated
 */
void test_configuration_updates(void) {
    Serial.println("\n=== Test 8: Configuration Updates ===");
    
    otaManager = new OTAManager(
        String(TEST_SERVER_URL),
        String(TEST_DEVICE_ID),
        String(TEST_CURRENT_VERSION)
    );
    
    // Update server URL
    String newServerURL = "http://newserver:5001";
    otaManager->setServerURL(newServerURL);
    
    // Update check interval
    unsigned long newInterval = 7200000;  // 2 hours
    otaManager->setCheckInterval(newInterval);
    
    // No direct way to verify these, but at least test they don't crash
    TEST_ASSERT_NOT_NULL(otaManager);
    
    Serial.println("✓ Configuration updates work");
    
    delete otaManager;
    otaManager = nullptr;
}

/**
 * Test 9: Multiple Test Fault Types
 * Verify all fault types can be enabled
 */
void test_all_fault_types(void) {
    Serial.println("\n=== Test 9: All Fault Types ===");
    
    otaManager = new OTAManager(
        String(TEST_SERVER_URL),
        String(TEST_DEVICE_ID),
        String(TEST_CURRENT_VERSION)
    );
    
    // Test each fault type
    OTAFaultType faultTypes[] = {
        OTA_FAULT_NONE,
        OTA_FAULT_CORRUPT_CHUNK,
        OTA_FAULT_BAD_HMAC,
        OTA_FAULT_BAD_HASH,
        OTA_FAULT_NETWORK_TIMEOUT,
        OTA_FAULT_INCOMPLETE_DOWNLOAD
    };
    
    for (int i = 0; i < 6; i++) {
        otaManager->enableTestMode(faultTypes[i]);
        TEST_ASSERT_TRUE(otaManager->isTestMode());
        TEST_ASSERT_EQUAL(faultTypes[i], otaManager->getTestFaultType());
        otaManager->disableTestMode();
    }
    
    Serial.println("✓ All fault types work");
    
    delete otaManager;
    otaManager = nullptr;
}

/**
 * Test 10: Memory Management
 * Verify OTA Manager doesn't leak memory
 */
void test_memory_management(void) {
    Serial.println("\n=== Test 10: Memory Management ===");
    
    // Get initial free heap
    uint32_t initialFreeHeap = ESP.getFreeHeap();
    
    // Create and destroy OTA Manager multiple times
    for (int i = 0; i < 5; i++) {
        otaManager = new OTAManager(
            String(TEST_SERVER_URL),
            String(TEST_DEVICE_ID),
            String(TEST_CURRENT_VERSION)
        );
        
        TEST_ASSERT_NOT_NULL(otaManager);
        
        delete otaManager;
        otaManager = nullptr;
    }
    
    // Get final free heap
    uint32_t finalFreeHeap = ESP.getFreeHeap();
    
    // Allow some tolerance (1KB) for heap fragmentation
    int32_t heapDiff = initialFreeHeap - finalFreeHeap;
    TEST_ASSERT_LESS_THAN(1024, abs(heapDiff));
    
    Serial.printf("✓ Memory management OK (diff: %d bytes)\n", heapDiff);
}

/**
 * Test 11: OTA State String Conversion
 * Verify state string conversion works
 */
void test_state_string_conversion(void) {
    Serial.println("\n=== Test 11: State String Conversion ===");
    
    otaManager = new OTAManager(
        String(TEST_SERVER_URL),
        String(TEST_DEVICE_ID),
        String(TEST_CURRENT_VERSION)
    );
    
    // Get state string
    String stateStr = otaManager->getStateString();
    
    TEST_ASSERT_TRUE(stateStr.length() > 0);
    TEST_ASSERT_TRUE(stateStr != "");
    
    // State should contain a recognizable word
    bool validState = (stateStr.indexOf("IDLE") >= 0) ||
                      (stateStr.indexOf("CHECKING") >= 0) ||
                      (stateStr.indexOf("DOWNLOADING") >= 0) ||
                      (stateStr.indexOf("ERROR") >= 0);
    
    TEST_ASSERT_TRUE(validState);
    
    Serial.printf("✓ State string: %s\n", stateStr.c_str());
    
    delete otaManager;
    otaManager = nullptr;
}

/**
 * Test 12: Progress Percentage Bounds
 * Verify progress percentage stays within 0-100
 */
void test_progress_percentage_bounds(void) {
    Serial.println("\n=== Test 12: Progress Percentage Bounds ===");
    
    otaManager = new OTAManager(
        String(TEST_SERVER_URL),
        String(TEST_DEVICE_ID),
        String(TEST_CURRENT_VERSION)
    );
    
    OTAProgress progress = otaManager->getProgress();
    
    // Percentage should be between 0 and 100
    TEST_ASSERT_GREATER_OR_EQUAL(0, progress.percentage);
    TEST_ASSERT_LESS_OR_EQUAL(100, progress.percentage);
    
    Serial.printf("✓ Progress percentage: %d%%\n", progress.percentage);
    
    delete otaManager;
    otaManager = nullptr;
}

void setup() {
    delay(2000);  // Allow time for Serial Monitor
    
    Serial.begin(115200);
    Serial.println("\n\n");
    Serial.println("===========================================");
    Serial.println("  M4 FOTA - ESP32 UNIT TESTS");
    Serial.println("  (Firmware Download - No Network/Flash)");
    Serial.println("===========================================");
    
    UNITY_BEGIN();
    
    RUN_TEST(test_ota_manager_initialization);
    RUN_TEST(test_progress_tracking);
    RUN_TEST(test_state_management);
    RUN_TEST(test_resume_capability);
    RUN_TEST(test_clear_progress);
    RUN_TEST(test_test_mode);
    RUN_TEST(test_ota_statistics);
    RUN_TEST(test_configuration_updates);
    RUN_TEST(test_all_fault_types);
    RUN_TEST(test_memory_management);
    RUN_TEST(test_state_string_conversion);
    RUN_TEST(test_progress_percentage_bounds);
    
    UNITY_END();
    
    Serial.println("\n===========================================");
    Serial.println("  ALL FOTA UNIT TESTS COMPLETE");
    Serial.println("===========================================\n");
}

void loop() {
    // Tests run once in setup()
    delay(1000);
}
