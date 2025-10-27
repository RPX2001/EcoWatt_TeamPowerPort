/**
 * @file test_main.cpp
 * @brief M4 - Remote Configuration Tests for ESP32
 * 
 * Tests dynamic runtime configuration updates:
 * - Sampling frequency changes
 * - Register list updates
 * - Upload frequency changes
 * - NVS persistence across reboots
 * - Validation and error handling
 * - Idempotent updates
 * - Multiple parameter updates
 * 
 * @author Team PowerPort
 * @date 2025-01-22
 */

#include <unity.h>
#include "application/config_manager.h"
#include "application/nvs.h"
#include <Preferences.h>
#include "config/test_config.h"  // Centralized configuration

// Test endpoint - use real Flask server endpoint
const char* TEST_ENDPOINT = FLASK_CONFIG_CHECK_URL(TEST_DEVICE_ID_M4_CONFIG);
const char* TEST_DEVICE_ID = TEST_DEVICE_ID_M4_CONFIG;

// Preferences for manual NVS verification
Preferences testPrefs;

void setUp(void) {
    // Clear NVS config state before each test
    testPrefs.begin("config", false);
    testPrefs.clear();
    testPrefs.end();
}

void tearDown(void) {
    // Clean up after each test
    testPrefs.begin("config", false);
    testPrefs.clear();
    testPrefs.end();
}

/**
 * @brief Test 1: Configuration initialization
 * 
 * Verifies that ConfigManager properly initializes with endpoint and device ID
 */
void test_config_initialization(void) {
    ConfigManager::init(TEST_ENDPOINT, TEST_DEVICE_ID);
    
    const SystemConfig& config = ConfigManager::getCurrentConfig();
    
    // Should have default config from NVS
    TEST_ASSERT_NOT_NULL(config.registers);
    TEST_ASSERT_GREATER_THAN(0, config.registerCount);
    TEST_ASSERT_GREATER_THAN(0, config.pollFrequency);
    TEST_ASSERT_GREATER_THAN(0, config.uploadFrequency);
}

/**
 * @brief Test 2: Poll frequency change
 * 
 * Tests runtime update of sampling/poll frequency without reboot
 */
void test_poll_frequency_change(void) {
    ConfigManager::init(TEST_ENDPOINT, TEST_DEVICE_ID);
    
    // Get initial frequency
    const SystemConfig& configBefore = ConfigManager::getCurrentConfig();
    uint64_t initialPollFreq = configBefore.pollFrequency;
    
    // Simulate cloud change by directly updating NVS (in real scenario, checkForChanges does this)
    uint64_t newPollFreq = 5000000; // 5 seconds
    nvs::changePollFreq(newPollFreq);
    
    // Apply the change
    uint64_t appliedFreq = 0;
    bool success = ConfigManager::applyPollFrequencyChange(&appliedFreq);
    
    TEST_ASSERT_TRUE(success);
    // Compare upper and lower 32 bits separately (Unity limitation)
    TEST_ASSERT_EQUAL_UINT32((uint32_t)(newPollFreq >> 32), (uint32_t)(appliedFreq >> 32));
    TEST_ASSERT_EQUAL_UINT32((uint32_t)newPollFreq, (uint32_t)appliedFreq);
    
    // Verify config updated
    const SystemConfig& configAfter = ConfigManager::getCurrentConfig();
    TEST_ASSERT_EQUAL_UINT32((uint32_t)(newPollFreq >> 32), (uint32_t)(configAfter.pollFrequency >> 32));
    TEST_ASSERT_EQUAL_UINT32((uint32_t)newPollFreq, (uint32_t)configAfter.pollFrequency);
    TEST_ASSERT_NOT_EQUAL((uint32_t)initialPollFreq, (uint32_t)configAfter.pollFrequency);
}

/**
 * @brief Test 3: Upload frequency change
 * 
 * Tests runtime update of data upload frequency without reboot
 */
void test_upload_frequency_change(void) {
    ConfigManager::init(TEST_ENDPOINT, TEST_DEVICE_ID);
    
    // Get initial frequency
    const SystemConfig& configBefore = ConfigManager::getCurrentConfig();
    uint64_t initialUploadFreq = configBefore.uploadFrequency;
    
    // Simulate cloud change
    uint64_t newUploadFreq = 10000000; // 10 seconds
    nvs::changeUploadFreq(newUploadFreq);
    
    // Apply the change
    uint64_t appliedFreq = 0;
    bool success = ConfigManager::applyUploadFrequencyChange(&appliedFreq);
    
    TEST_ASSERT_TRUE(success);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)(newUploadFreq >> 32), (uint32_t)(appliedFreq >> 32));
    TEST_ASSERT_EQUAL_UINT32((uint32_t)newUploadFreq, (uint32_t)appliedFreq);
    
    // Verify config updated
    const SystemConfig& configAfter = ConfigManager::getCurrentConfig();
    TEST_ASSERT_EQUAL_UINT32((uint32_t)(newUploadFreq >> 32), (uint32_t)(configAfter.uploadFrequency >> 32));
    TEST_ASSERT_EQUAL_UINT32((uint32_t)newUploadFreq, (uint32_t)configAfter.uploadFrequency);
    TEST_ASSERT_NOT_EQUAL((uint32_t)initialUploadFreq, (uint32_t)configAfter.uploadFrequency);
}

/**
 * @brief Test 4: Register list update
 * 
 * Tests adding/removing registers from monitoring list at runtime
 */
void test_register_list_change(void) {
    ConfigManager::init(TEST_ENDPOINT, TEST_DEVICE_ID);
    
    // Get initial register list
    const SystemConfig& configBefore = ConfigManager::getCurrentConfig();
    size_t initialRegCount = configBefore.registerCount;
    
    // Simulate cloud change - change register selection
    // Use a different register mask
    uint16_t newRegsMask = 0b0000000000001111; // First 4 registers
    size_t newRegsCount = 4;
    bool saved = nvs::saveReadRegs(newRegsMask, newRegsCount);
    TEST_ASSERT_TRUE(saved);
    
    // Apply the change
    const RegID* newSelection = nullptr;
    size_t newCount = 0;
    bool success = ConfigManager::applyRegisterChanges(&newSelection, &newCount);
    
    TEST_ASSERT_TRUE(success);
    TEST_ASSERT_EQUAL(newRegsCount, newCount);
    TEST_ASSERT_NOT_NULL(newSelection);
    
    // Verify config updated
    const SystemConfig& configAfter = ConfigManager::getCurrentConfig();
    TEST_ASSERT_EQUAL(newRegsCount, configAfter.registerCount);
    TEST_ASSERT_EQUAL_PTR(newSelection, configAfter.registers);
}

/**
 * @brief Test 5: NVS persistence across simulated reboot
 * 
 * Tests that configuration changes persist in NVS after reboot
 */
void test_config_persistence(void) {
    ConfigManager::init(TEST_ENDPOINT, TEST_DEVICE_ID);
    
    // Make configuration changes
    uint64_t newPollFreq = 7000000; // 7 seconds
    uint64_t newUploadFreq = 14000000; // 14 seconds
    nvs::changePollFreq(newPollFreq);
    nvs::changeUploadFreq(newUploadFreq);
    
    uint64_t appliedPoll = 0, appliedUpload = 0;
    ConfigManager::applyPollFrequencyChange(&appliedPoll);
    ConfigManager::applyUploadFrequencyChange(&appliedUpload);
    
    // Simulate reboot by re-initializing ConfigManager
    ConfigManager::init(TEST_ENDPOINT, TEST_DEVICE_ID);
    
    // Verify persisted values
    const SystemConfig& config = ConfigManager::getCurrentConfig();
    TEST_ASSERT_EQUAL_UINT32((uint32_t)(newPollFreq >> 32), (uint32_t)(config.pollFrequency >> 32));
    TEST_ASSERT_EQUAL_UINT32((uint32_t)newPollFreq, (uint32_t)config.pollFrequency);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)(newUploadFreq >> 32), (uint32_t)(config.uploadFrequency >> 32));
    TEST_ASSERT_EQUAL_UINT32((uint32_t)newUploadFreq, (uint32_t)config.uploadFrequency);
}

/**
 * @brief Test 6: Idempotent updates
 * 
 * Tests that applying the same configuration twice doesn't cause errors
 */
void test_idempotent_updates(void) {
    ConfigManager::init(TEST_ENDPOINT, TEST_DEVICE_ID);
    
    uint64_t targetFreq = 8000000; // 8 seconds
    
    // First update
    nvs::changePollFreq(targetFreq);
    uint64_t appliedFreq1 = 0;
    bool success1 = ConfigManager::applyPollFrequencyChange(&appliedFreq1);
    TEST_ASSERT_TRUE(success1);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)(targetFreq >> 32), (uint32_t)(appliedFreq1 >> 32));
    TEST_ASSERT_EQUAL_UINT32((uint32_t)targetFreq, (uint32_t)appliedFreq1);
    
    // Second update with same value (should not fail)
    nvs::changePollFreq(targetFreq);
    uint64_t appliedFreq2 = 0;
    bool success2 = ConfigManager::applyPollFrequencyChange(&appliedFreq2);
    TEST_ASSERT_TRUE(success2);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)(targetFreq >> 32), (uint32_t)(appliedFreq2 >> 32));
    TEST_ASSERT_EQUAL_UINT32((uint32_t)targetFreq, (uint32_t)appliedFreq2);
    
    // Config should still be correct
    const SystemConfig& config = ConfigManager::getCurrentConfig();
    TEST_ASSERT_EQUAL_UINT32((uint32_t)(targetFreq >> 32), (uint32_t)(config.pollFrequency >> 32));
    TEST_ASSERT_EQUAL_UINT32((uint32_t)targetFreq, (uint32_t)config.pollFrequency);
}

/**
 * @brief Test 7: Multiple parameter update
 * 
 * Tests updating poll frequency, upload frequency, and registers together
 */
void test_multiple_parameter_update(void) {
    ConfigManager::init(TEST_ENDPOINT, TEST_DEVICE_ID);
    
    // Change all three parameters
    uint64_t newPollFreq = 6000000; // 6 seconds
    uint64_t newUploadFreq = 12000000; // 12 seconds
    uint16_t newRegsMask = 0b0000000000111111; // First 6 registers
    size_t newRegsCount = 6;
    
    nvs::changePollFreq(newPollFreq);
    nvs::changeUploadFreq(newUploadFreq);
    nvs::saveReadRegs(newRegsMask, newRegsCount);
    
    // Apply all changes
    uint64_t appliedPoll = 0, appliedUpload = 0;
    const RegID* appliedRegs = nullptr;
    size_t appliedRegCount = 0;
    
    bool pollSuccess = ConfigManager::applyPollFrequencyChange(&appliedPoll);
    bool uploadSuccess = ConfigManager::applyUploadFrequencyChange(&appliedUpload);
    bool regsSuccess = ConfigManager::applyRegisterChanges(&appliedRegs, &appliedRegCount);
    
    TEST_ASSERT_TRUE(pollSuccess);
    TEST_ASSERT_TRUE(uploadSuccess);
    TEST_ASSERT_TRUE(regsSuccess);
    
    // Verify all parameters updated
    const SystemConfig& config = ConfigManager::getCurrentConfig();
    TEST_ASSERT_EQUAL_UINT32((uint32_t)(newPollFreq >> 32), (uint32_t)(config.pollFrequency >> 32));
    TEST_ASSERT_EQUAL_UINT32((uint32_t)newPollFreq, (uint32_t)config.pollFrequency);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)(newUploadFreq >> 32), (uint32_t)(config.uploadFrequency >> 32));
    TEST_ASSERT_EQUAL_UINT32((uint32_t)newUploadFreq, (uint32_t)config.uploadFrequency);
    TEST_ASSERT_EQUAL(newRegsCount, config.registerCount);
}

/**
 * @brief Test 8: Get current configuration
 * 
 * Tests that getCurrentConfig returns accurate system state
 */
void test_get_current_config(void) {
    ConfigManager::init(TEST_ENDPOINT, TEST_DEVICE_ID);
    
    // Make some changes
    uint64_t testPollFreq = 9000000; // 9 seconds
    nvs::changePollFreq(testPollFreq);
    uint64_t appliedFreq = 0;
    ConfigManager::applyPollFrequencyChange(&appliedFreq);
    
    // Get config
    const SystemConfig& config = ConfigManager::getCurrentConfig();
    
    // Verify structure is valid
    TEST_ASSERT_NOT_NULL(config.registers);
    TEST_ASSERT_GREATER_THAN(0, config.registerCount);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)(testPollFreq >> 32), (uint32_t)(config.pollFrequency >> 32));
    TEST_ASSERT_EQUAL_UINT32((uint32_t)testPollFreq, (uint32_t)config.pollFrequency);
    TEST_ASSERT_GREATER_THAN(0, (uint32_t)config.uploadFrequency);
}

/**
 * @brief Test 9: Update current config directly
 * 
 * Tests the updateCurrentConfig method for in-memory config changes
 */
void test_update_current_config_directly(void) {
    ConfigManager::init(TEST_ENDPOINT, TEST_DEVICE_ID);
    
    const SystemConfig& configBefore = ConfigManager::getCurrentConfig();
    
    // Update config directly (simulating immediate change)
    uint64_t newPollFreq = 4000000; // 4 seconds
    uint64_t newUploadFreq = 8000000; // 8 seconds
    
    ConfigManager::updateCurrentConfig(
        configBefore.registers,
        configBefore.registerCount,
        newPollFreq,
        newUploadFreq
    );
    
    // Verify immediate update
    const SystemConfig& configAfter = ConfigManager::getCurrentConfig();
    TEST_ASSERT_EQUAL_UINT32((uint32_t)(newPollFreq >> 32), (uint32_t)(configAfter.pollFrequency >> 32));
    TEST_ASSERT_EQUAL_UINT32((uint32_t)newPollFreq, (uint32_t)configAfter.pollFrequency);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)(newUploadFreq >> 32), (uint32_t)(configAfter.uploadFrequency >> 32));
    TEST_ASSERT_EQUAL_UINT32((uint32_t)newUploadFreq, (uint32_t)configAfter.uploadFrequency);
}

/**
 * @brief Test 10: Configuration validation with edge values
 * 
 * Tests behavior with minimum and maximum frequency values
 */
void test_config_validation_edge_values(void) {
    ConfigManager::init(TEST_ENDPOINT, TEST_DEVICE_ID);
    
    // Test minimum frequency (1 second = 1,000,000 microseconds)
    uint64_t minFreq = 1000000;
    nvs::changePollFreq(minFreq);
    uint64_t appliedMin = 0;
    bool successMin = ConfigManager::applyPollFrequencyChange(&appliedMin);
    TEST_ASSERT_TRUE(successMin);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)(minFreq >> 32), (uint32_t)(appliedMin >> 32));
    TEST_ASSERT_EQUAL_UINT32((uint32_t)minFreq, (uint32_t)appliedMin);
    
    // Test large frequency (1 hour = 3,600,000,000 microseconds)
    uint64_t maxFreq = 3600000000;
    nvs::changePollFreq(maxFreq);
    uint64_t appliedMax = 0;
    bool successMax = ConfigManager::applyPollFrequencyChange(&appliedMax);
    TEST_ASSERT_TRUE(successMax);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)(maxFreq >> 32), (uint32_t)(appliedMax >> 32));
    TEST_ASSERT_EQUAL_UINT32((uint32_t)maxFreq, (uint32_t)appliedMax);
}

void setup() {
    delay(2000); // Allow serial monitor to connect
    
    UNITY_BEGIN();
    
    RUN_TEST(test_config_initialization);
    RUN_TEST(test_poll_frequency_change);
    RUN_TEST(test_upload_frequency_change);
    RUN_TEST(test_register_list_change);
    RUN_TEST(test_config_persistence);
    RUN_TEST(test_idempotent_updates);
    RUN_TEST(test_multiple_parameter_update);
    RUN_TEST(test_get_current_config);
    RUN_TEST(test_update_current_config_directly);
    RUN_TEST(test_config_validation_edge_values);
    
    UNITY_END();
}

void loop() {
    // Tests run once in setup()
}
