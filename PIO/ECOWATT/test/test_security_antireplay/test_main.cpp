/**
 * @file test_security_antireplay.cpp
 * @brief Unit tests for M4 Security - Anti-Replay Protection
 * 
 * Tests:
 * 1. Duplicate nonce detection
 * 2. Old nonce rejection
 * 3. Nonce persistence across reboots
 * 4. Nonce ordering validation
 * 
 * Hardware: ESP32 with NVS storage
 */

#include <unity.h>
#include <Arduino.h>
#include <Preferences.h>
#include "application/security.h"

// Test configuration
#define TEST_DEVICE_ID "TEST_SECURITY_ESP32"
#define NVS_NAMESPACE "security_test"

// Global test variables
static Preferences preferences;
static uint32_t testNonce = 1000;

void setUp(void) {
    // Setup runs before each test
    Serial.println("\n--- Test Setup ---");
    
    // Clear NVS for clean test
    preferences.begin(NVS_NAMESPACE, false);
    preferences.clear();
    preferences.end();
    
    // Clear nonce state
    Security::clearNonceState();
    Security::resetAttackStats();
    
    // Initialize security module
    Security::init();
}

void tearDown(void) {
    // Cleanup runs after each test
    Serial.println("--- Test Teardown ---\n");
    
    // Clear test data
    preferences.begin(NVS_NAMESPACE, false);
    preferences.clear();
    preferences.end();
}

// ============================================================================
// Test 1: Duplicate Nonce Detection
// ============================================================================
void test_duplicate_nonce_rejected(void) {
    Serial.println("\n=== Test 1: Duplicate Nonce Rejection ===");
    
    uint32_t nonce = testNonce++;
    
    // First use of nonce - should be accepted
    bool firstUse = Security::validateNonce(TEST_DEVICE_ID, nonce);
    TEST_ASSERT_TRUE_MESSAGE(firstUse, "First use of nonce should be accepted");
    
    // Second use of same nonce - should be rejected
    bool secondUse = Security::validateNonce(TEST_DEVICE_ID, nonce);
    TEST_ASSERT_FALSE_MESSAGE(secondUse, "Duplicate nonce should be rejected");
    
    Serial.println("[PASS] Duplicate nonce correctly rejected");
}

// ============================================================================
// Test 2: Old Nonce Rejection
// ============================================================================
void test_old_nonce_rejected(void) {
    Serial.println("\n=== Test 2: Old Nonce Rejection ===");
    
    uint32_t nonce1 = testNonce++;
    uint32_t nonce2 = testNonce++;
    uint32_t nonce3 = testNonce++;
    
    // Use nonces in order
    TEST_ASSERT_TRUE(Security::validateNonce(TEST_DEVICE_ID, nonce1));
    TEST_ASSERT_TRUE(Security::validateNonce(TEST_DEVICE_ID, nonce2));
    TEST_ASSERT_TRUE(Security::validateNonce(TEST_DEVICE_ID, nonce3));
    
    // Try to use old nonce (nonce1) again - should be rejected
    bool oldNonceResult = Security::validateNonce(TEST_DEVICE_ID, nonce1);
    TEST_ASSERT_FALSE_MESSAGE(oldNonceResult, "Old nonce should be rejected");
    
    // Try nonce2 - should also be rejected
    oldNonceResult = Security::validateNonce(TEST_DEVICE_ID, nonce2);
    TEST_ASSERT_FALSE_MESSAGE(oldNonceResult, "Old nonce should be rejected");
    
    Serial.println("[PASS] Old nonces correctly rejected");
}

// ============================================================================
// Test 3: Nonce Ordering Validation
// ============================================================================
void test_nonce_ordering(void) {
    Serial.println("\n=== Test 3: Nonce Ordering ===");
    
    uint32_t baseNonce = testNonce;
    testNonce += 10;
    
    // Use nonces in ascending order - all should be accepted
    for (int i = 0; i < 5; i++) {
        bool result = Security::validateNonce(TEST_DEVICE_ID, baseNonce + i);
        TEST_ASSERT_TRUE_MESSAGE(result, "Sequential nonce should be accepted");
    }
    
    // Try a nonce that's too old
    bool oldResult = Security::validateNonce(TEST_DEVICE_ID, baseNonce - 1);
    TEST_ASSERT_FALSE_MESSAGE(oldResult, "Nonce older than current should be rejected");
    
    Serial.println("[PASS] Nonce ordering validated");
}

// ============================================================================
// Test 4: Nonce Window Tolerance
// ============================================================================
void test_nonce_window_tolerance(void) {
    Serial.println("\n=== Test 4: Nonce Window Tolerance ===");
    
    uint32_t currentNonce = testNonce;
    testNonce += 100;
    
    // Establish current nonce
    TEST_ASSERT_TRUE(Security::validateNonce(TEST_DEVICE_ID, currentNonce));
    
    // Try nonce slightly in future (within window) - should be accepted
    uint32_t futureNonce = currentNonce + 50;
    bool futureResult = Security::validateNonce(TEST_DEVICE_ID, futureNonce);
    TEST_ASSERT_TRUE_MESSAGE(futureResult, "Future nonce within window should be accepted");
    
    // Try nonce far in past (outside window) - should be rejected
    uint32_t pastNonce = currentNonce - 100;
    bool pastResult = Security::validateNonce(TEST_DEVICE_ID, pastNonce);
    TEST_ASSERT_FALSE_MESSAGE(pastResult, "Nonce outside window should be rejected");
    
    Serial.println("[PASS] Nonce window tolerance validated");
}

// ============================================================================
// Test 5: Nonce Persistence Across Reboot
// ============================================================================
void test_nonce_persistence(void) {
    Serial.println("\n=== Test 5: Nonce Persistence ===");
    
    uint32_t persistNonce = testNonce++;
    
    // Use nonce and ensure it's stored
    TEST_ASSERT_TRUE(Security::validateNonce(TEST_DEVICE_ID, persistNonce));
    
    // Force save to NVS
    Security::saveNonceState();
    
    // Simulate reboot by reinitializing
    Security::init();
    
    // Try to use same nonce again - should still be rejected
    bool afterReboot = Security::validateNonce(TEST_DEVICE_ID, persistNonce);
    TEST_ASSERT_FALSE_MESSAGE(afterReboot, "Nonce should persist across reboot");
    
    // Use next nonce - should be accepted
    bool nextNonce = Security::validateNonce(TEST_DEVICE_ID, persistNonce + 1);
    TEST_ASSERT_TRUE_MESSAGE(nextNonce, "Next nonce after reboot should be accepted");
    
    Serial.println("[PASS] Nonce persistence validated");
}

// ============================================================================
// Test 6: Multiple Device Nonce Isolation
// ============================================================================
void test_multiple_device_isolation(void) {
    Serial.println("\n=== Test 6: Multiple Device Isolation ===");
    
    const char* device1 = "DEVICE_001";
    const char* device2 = "DEVICE_002";
    
    uint32_t nonce = testNonce++;
    
    // Use same nonce for device1
    TEST_ASSERT_TRUE(Security::validateNonce(device1, nonce));
    
    // Use same nonce for device2 - should be accepted (different device)
    bool device2Result = Security::validateNonce(device2, nonce);
    TEST_ASSERT_TRUE_MESSAGE(device2Result, "Same nonce should be allowed for different device");
    
    // Try duplicate on device1 - should be rejected
    bool device1Duplicate = Security::validateNonce(device1, nonce);
    TEST_ASSERT_FALSE_MESSAGE(device1Duplicate, "Duplicate nonce should be rejected for device1");
    
    // Try duplicate on device2 - should be rejected
    bool device2Duplicate = Security::validateNonce(device2, nonce);
    TEST_ASSERT_FALSE_MESSAGE(device2Duplicate, "Duplicate nonce should be rejected for device2");
    
    Serial.println("[PASS] Device nonce isolation validated");
}

// ============================================================================
// Test 7: Nonce Counter Rollover
// ============================================================================
void test_nonce_rollover(void) {
    Serial.println("\n=== Test 7: Nonce Rollover ===");
    
    // Test nonce near 32-bit max value
    uint32_t maxNonce = 0xFFFFFFF0;
    
    // Use nonces near max
    TEST_ASSERT_TRUE(Security::validateNonce(TEST_DEVICE_ID, maxNonce));
    TEST_ASSERT_TRUE(Security::validateNonce(TEST_DEVICE_ID, maxNonce + 1));
    TEST_ASSERT_TRUE(Security::validateNonce(TEST_DEVICE_ID, maxNonce + 2));
    
    // Try old nonce near max - should be rejected
    bool oldMax = Security::validateNonce(TEST_DEVICE_ID, maxNonce);
    TEST_ASSERT_FALSE_MESSAGE(oldMax, "Old nonce near max should be rejected");
    
    Serial.println("[PASS] Nonce rollover handling validated");
}

// ============================================================================
// Test 8: Rapid Nonce Validation
// ============================================================================
void test_rapid_nonce_validation(void) {
    Serial.println("\n=== Test 8: Rapid Nonce Validation ===");
    
    uint32_t baseNonce = testNonce;
    testNonce += 100;
    
    unsigned long startTime = millis();
    int successCount = 0;
    
    // Rapidly validate 50 sequential nonces
    for (int i = 0; i < 50; i++) {
        if (Security::validateNonce(TEST_DEVICE_ID, baseNonce + i)) {
            successCount++;
        }
    }
    
    unsigned long duration = millis() - startTime;
    
    TEST_ASSERT_EQUAL(50, successCount);
    TEST_ASSERT_LESS_THAN(1000, duration); // Should complete within 1 second
    
    Serial.printf("[PASS] Validated 50 nonces in %lu ms\n", duration);
}

// ============================================================================
// Test 9: Nonce Storage Limits
// ============================================================================
void test_nonce_storage_limits(void) {
    Serial.println("\n=== Test 9: Nonce Storage Limits ===");
    
    // Test with many devices to check storage handling
    const int deviceCount = 10;
    char deviceId[32];
    
    for (int i = 0; i < deviceCount; i++) {
        snprintf(deviceId, sizeof(deviceId), "DEVICE_%03d", i);
        
        uint32_t nonce = testNonce + i;
        bool result = Security::validateNonce(deviceId, nonce);
        TEST_ASSERT_TRUE_MESSAGE(result, "Should handle multiple devices");
    }
    
    // Verify persistence
    Security::saveNonceState();
    Security::init();
    
    // Check all devices still tracked
    for (int i = 0; i < deviceCount; i++) {
        snprintf(deviceId, sizeof(deviceId), "DEVICE_%03d", i);
        
        uint32_t oldNonce = testNonce + i;
        bool oldResult = Security::validateNonce(deviceId, oldNonce);
        TEST_ASSERT_FALSE_MESSAGE(oldResult, "Old nonces should be rejected after reload");
        
        uint32_t newNonce = oldNonce + 1;
        bool newResult = Security::validateNonce(deviceId, newNonce);
        TEST_ASSERT_TRUE_MESSAGE(newResult, "New nonces should be accepted after reload");
    }
    
    Serial.println("[PASS] Multiple device storage validated");
}

// ============================================================================
// Test 10: Attack Statistics Tracking
// ============================================================================
void test_attack_statistics(void) {
    Serial.println("\n=== Test 10: Attack Statistics ===");
    
    uint32_t validNonce = testNonce++;
    
    // Perform valid operation
    TEST_ASSERT_TRUE(Security::validateNonce(TEST_DEVICE_ID, validNonce));
    
    // Perform multiple attacks
    for (int i = 0; i < 5; i++) {
        Security::validateNonce(TEST_DEVICE_ID, validNonce); // Duplicate - rejected
    }
    
    // Get statistics
    uint32_t validCount, replayCount;
    Security::getAttackStats(validCount, replayCount);
    
    TEST_ASSERT_EQUAL(1, validCount);
    TEST_ASSERT_EQUAL(5, replayCount);
    
    Serial.printf("[PASS] Stats - Valid: %d, Replay: %d\n", 
                  validCount, replayCount);
}

// ============================================================================
// Main Test Runner
// ============================================================================
void setup() {
    delay(2000); // Wait for serial
    
    UNITY_BEGIN();
    
    Serial.println("\n========================================");
    Serial.println("  M4 SECURITY - ANTI-REPLAY TESTS");
    Serial.println("========================================");
    
    RUN_TEST(test_duplicate_nonce_rejected);
    RUN_TEST(test_old_nonce_rejected);
    RUN_TEST(test_nonce_ordering);
    RUN_TEST(test_nonce_window_tolerance);
    RUN_TEST(test_nonce_persistence);
    RUN_TEST(test_multiple_device_isolation);
    RUN_TEST(test_nonce_rollover);
    RUN_TEST(test_rapid_nonce_validation);
    RUN_TEST(test_nonce_storage_limits);
    RUN_TEST(test_attack_statistics);
    
    UNITY_END();
}

void loop() {
    // Empty loop
}
