/**
 * @file test_main.cpp
 * @brief M4 Security - Nonce Generation Tests (ESP32)
 * 
 * Tests:
 * 1. Nonce initialization
 * 2. Nonce increment
 * 3. Nonce persistence
 * 4. Nonce rollover handling
 * 5. Manual nonce setting
 * 6. Nonce persistence across reboot simulation
 * 7. Nonce range validation
 * 8. Multiple increments
 */

#include <Arduino.h>
#include <unity.h>
#include <Preferences.h>
#include "application/security.h"

// Test configuration
const char* TEST_TAG = "TEST_NONCE";
Preferences testPrefs;

// Helper function to clear nonce from NVS
void clearNonceFromNVS() {
    testPrefs.begin("security", false);
    testPrefs.remove("nonce");
    testPrefs.end();
}

// Helper function to read nonce from NVS
uint32_t readNonceFromNVS() {
    testPrefs.begin("security", true);  // Read-only
    uint32_t nonce = testPrefs.getUInt("nonce", 0);
    testPrefs.end();
    return nonce;
}

// Helper function to write nonce to NVS
void writeNonceToNVS(uint32_t nonce) {
    testPrefs.begin("security", false);
    testPrefs.putUInt("nonce", nonce);
    testPrefs.end();
}

void setUp(void) {
    // Setup runs before each test
}

void tearDown(void) {
    // Teardown runs after each test
}

/**
 * Test 1: Nonce Initialization
 * Verify that nonce initializes to default value (10000) when not present in NVS
 */
void test_nonce_initialization(void) {
    Serial.println("\n=== Test 1: Nonce Initialization ===");
    
    // Clear any existing nonce from NVS
    clearNonceFromNVS();
    
    // Initialize security layer
    SecurityLayer::init();
    
    // Check that nonce initialized to default value (10000) in memory
    uint32_t currentNonce = SecurityLayer::getCurrentNonce();
    TEST_ASSERT_EQUAL_UINT32(10000, currentNonce);
    
    // Note: Nonce is not saved to NVS until first increment
    // This is efficient - we only write to NVS when nonce changes
    
    // Now perform an operation to increment and save
    char payload[] = "{\"test\":1}";
    char securedPayload[2048];
    SecurityLayer::securePayload(payload, securedPayload, sizeof(securedPayload));
    
    // Now it should be saved to NVS
    uint32_t nvsNonce = readNonceFromNVS();
    TEST_ASSERT_EQUAL_UINT32(10001, nvsNonce);
    
    Serial.println("✓ Nonce initialized to 10000 and saved on first use");
}

/**
 * Test 2: Nonce Increment
 * Verify that nonce increments by 1 on each secure operation
 */
void test_nonce_increment(void) {
    Serial.println("\n=== Test 2: Nonce Increment ===");
    
    // Set known starting nonce
    SecurityLayer::setNonce(20000);
    uint32_t startNonce = SecurityLayer::getCurrentNonce();
    TEST_ASSERT_EQUAL_UINT32(20000, startNonce);
    
    // Create secured payload (this should increment nonce)
    char payload[] = "{\"device_id\":\"TEST\",\"voltage\":3300}";
    char securedPayload[2048];
    
    bool result = SecurityLayer::securePayload(payload, securedPayload, sizeof(securedPayload));
    TEST_ASSERT_TRUE(result);
    
    // Check that nonce incremented
    uint32_t newNonce = SecurityLayer::getCurrentNonce();
    TEST_ASSERT_EQUAL_UINT32(20001, newNonce);
    
    Serial.printf("✓ Nonce incremented: %u -> %u\n", startNonce, newNonce);
}

/**
 * Test 3: Nonce Persistence
 * Verify that nonce value is saved to NVS after increment
 */
void test_nonce_persistence(void) {
    Serial.println("\n=== Test 3: Nonce Persistence ===");
    
    // Set known nonce
    SecurityLayer::setNonce(30000);
    
    // Perform operation to increment
    char payload[] = "{\"test\":123}";
    char securedPayload[2048];
    SecurityLayer::securePayload(payload, securedPayload, sizeof(securedPayload));
    
    // Get current nonce from memory
    uint32_t memoryNonce = SecurityLayer::getCurrentNonce();
    
    // Read nonce from NVS
    uint32_t nvsNonce = readNonceFromNVS();
    
    // They should match
    TEST_ASSERT_EQUAL_UINT32(memoryNonce, nvsNonce);
    TEST_ASSERT_EQUAL_UINT32(30001, nvsNonce);
    
    Serial.printf("✓ Nonce persisted to NVS: %u\n", nvsNonce);
}

/**
 * Test 4: Nonce Rollover Handling
 * Verify behavior when nonce approaches UINT32_MAX
 */
void test_nonce_rollover(void) {
    Serial.println("\n=== Test 4: Nonce Rollover ===");
    
    // Set nonce to near maximum
    uint32_t nearMax = UINT32_MAX - 5;
    SecurityLayer::setNonce(nearMax);
    
    uint32_t currentNonce = SecurityLayer::getCurrentNonce();
    TEST_ASSERT_EQUAL_UINT32(nearMax, currentNonce);
    
    // Create secured payload multiple times
    char payload[] = "{\"test\":1}";
    char securedPayload[2048];
    
    for (int i = 0; i < 3; i++) {
        bool result = SecurityLayer::securePayload(payload, securedPayload, sizeof(securedPayload));
        TEST_ASSERT_TRUE(result);
    }
    
    // Check final nonce value
    uint32_t finalNonce = SecurityLayer::getCurrentNonce();
    TEST_ASSERT_EQUAL_UINT32(nearMax + 3, finalNonce);
    
    Serial.printf("✓ Nonce near rollover: %u -> %u\n", nearMax, finalNonce);
}

/**
 * Test 5: Manual Nonce Setting
 * Verify that setNonce() correctly updates nonce value and saves to NVS
 */
void test_manual_nonce_setting(void) {
    Serial.println("\n=== Test 5: Manual Nonce Setting ===");
    
    // Set various nonce values
    uint32_t testValues[] = {1000, 50000, 100000, 999999};
    
    for (uint32_t testValue : testValues) {
        SecurityLayer::setNonce(testValue);
        
        // Check memory
        uint32_t memNonce = SecurityLayer::getCurrentNonce();
        TEST_ASSERT_EQUAL_UINT32(testValue, memNonce);
        
        // Check NVS
        uint32_t nvsNonce = readNonceFromNVS();
        TEST_ASSERT_EQUAL_UINT32(testValue, nvsNonce);
    }
    
    Serial.println("✓ Manual nonce setting works correctly");
}

/**
 * Test 6: Nonce Persistence Across Reboot Simulation
 * Simulate reboot by reinitializing security layer
 */
void test_nonce_persistence_across_reboot(void) {
    Serial.println("\n=== Test 6: Nonce Persistence Across Reboot ===");
    
    // Set specific nonce and save
    uint32_t testNonce = 55555;
    SecurityLayer::setNonce(testNonce);
    
    // Verify it's in NVS
    uint32_t nvsNonce = readNonceFromNVS();
    TEST_ASSERT_EQUAL_UINT32(testNonce, nvsNonce);
    
    // Simulate reboot: Reinitialize security layer
    SecurityLayer::init();
    
    // Check that nonce was loaded from NVS
    uint32_t loadedNonce = SecurityLayer::getCurrentNonce();
    TEST_ASSERT_EQUAL_UINT32(testNonce, loadedNonce);
    
    Serial.printf("✓ Nonce persisted across reboot: %u\n", loadedNonce);
}

/**
 * Test 7: Nonce Range Validation
 * Verify nonce stays within uint32_t range
 */
void test_nonce_range_validation(void) {
    Serial.println("\n=== Test 7: Nonce Range Validation ===");
    
    // Test minimum value
    SecurityLayer::setNonce(0);
    TEST_ASSERT_EQUAL_UINT32(0, SecurityLayer::getCurrentNonce());
    
    // Test maximum value
    SecurityLayer::setNonce(UINT32_MAX);
    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, SecurityLayer::getCurrentNonce());
    
    // Test mid-range
    SecurityLayer::setNonce(2147483648U);  // 2^31
    TEST_ASSERT_EQUAL_UINT32(2147483648U, SecurityLayer::getCurrentNonce());
    
    Serial.println("✓ Nonce range validation passed");
}

/**
 * Test 8: Multiple Increments
 * Verify sequential increments work correctly
 */
void test_multiple_increments(void) {
    Serial.println("\n=== Test 8: Multiple Increments ===");
    
    // Set starting nonce
    uint32_t startNonce = 40000;
    SecurityLayer::setNonce(startNonce);
    
    // Perform 10 secure operations
    char payload[] = "{\"seq\":0}";
    char securedPayload[2048];
    
    for (int i = 0; i < 10; i++) {
        bool result = SecurityLayer::securePayload(payload, securedPayload, sizeof(securedPayload));
        TEST_ASSERT_TRUE(result);
        
        // Verify nonce incremented correctly
        uint32_t expectedNonce = startNonce + i + 1;
        uint32_t actualNonce = SecurityLayer::getCurrentNonce();
        TEST_ASSERT_EQUAL_UINT32(expectedNonce, actualNonce);
    }
    
    // Final check
    uint32_t finalNonce = SecurityLayer::getCurrentNonce();
    TEST_ASSERT_EQUAL_UINT32(startNonce + 10, finalNonce);
    
    Serial.printf("✓ Multiple increments: %u -> %u\n", startNonce, finalNonce);
}

/**
 * Test 9: Nonce Monotonicity
 * Verify nonce always increases, never decreases
 */
void test_nonce_monotonicity(void) {
    Serial.println("\n=== Test 9: Nonce Monotonicity ===");
    
    SecurityLayer::setNonce(60000);
    
    uint32_t previousNonce = SecurityLayer::getCurrentNonce();
    char payload[] = "{\"data\":1}";
    char securedPayload[2048];
    
    // Perform 20 operations
    for (int i = 0; i < 20; i++) {
        SecurityLayer::securePayload(payload, securedPayload, sizeof(securedPayload));
        uint32_t currentNonce = SecurityLayer::getCurrentNonce();
        
        // Nonce should always be greater than previous
        TEST_ASSERT_GREATER_THAN(previousNonce, currentNonce);
        previousNonce = currentNonce;
    }
    
    Serial.println("✓ Nonce monotonicity verified (always increasing)");
}

/**
 * Test 10: Nonce After Failed Operation
 * Verify nonce behavior when secure operation fails
 */
void test_nonce_after_failed_operation(void) {
    Serial.println("\n=== Test 10: Nonce After Failed Operation ===");
    
    SecurityLayer::setNonce(70000);
    uint32_t initialNonce = SecurityLayer::getCurrentNonce();
    
    // Try to secure with invalid parameters (null pointer)
    char payload[] = "{\"test\":1}";
    bool result = SecurityLayer::securePayload(payload, nullptr, 0);
    
    // Operation should fail
    TEST_ASSERT_FALSE(result);
    
    // Nonce should NOT have incremented (operation failed early)
    uint32_t afterFailNonce = SecurityLayer::getCurrentNonce();
    TEST_ASSERT_EQUAL_UINT32(initialNonce, afterFailNonce);
    
    // Now try valid operation
    char securedPayload[2048];
    result = SecurityLayer::securePayload(payload, securedPayload, sizeof(securedPayload));
    TEST_ASSERT_TRUE(result);
    
    // Nonce should have incremented
    uint32_t afterSuccessNonce = SecurityLayer::getCurrentNonce();
    TEST_ASSERT_EQUAL_UINT32(initialNonce + 1, afterSuccessNonce);
    
    Serial.println("✓ Nonce only increments on successful operations");
}

void setup() {
    delay(2000);  // Allow time for Serial Monitor
    
    Serial.begin(115200);
    Serial.println("\n\n");
    Serial.println("===========================================");
    Serial.println("  M4 SECURITY - NONCE GENERATION TESTS");
    Serial.println("===========================================");
    
    UNITY_BEGIN();
    
    RUN_TEST(test_nonce_initialization);
    RUN_TEST(test_nonce_increment);
    RUN_TEST(test_nonce_persistence);
    RUN_TEST(test_nonce_rollover);
    RUN_TEST(test_manual_nonce_setting);
    RUN_TEST(test_nonce_persistence_across_reboot);
    RUN_TEST(test_nonce_range_validation);
    RUN_TEST(test_multiple_increments);
    RUN_TEST(test_nonce_monotonicity);
    RUN_TEST(test_nonce_after_failed_operation);
    
    UNITY_END();
    
    Serial.println("\n===========================================");
    Serial.println("  ALL NONCE GENERATION TESTS COMPLETE");
    Serial.println("===========================================\n");
}

void loop() {
    // Tests run once in setup()
    delay(1000);
}
