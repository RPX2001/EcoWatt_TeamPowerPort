/**
 * @file security_test.cpp
 * @brief Security Testing Utilities for EcoWatt Device
 * 
 * Tests anti-replay protection, tamper detection, MAC verification,
 * and encryption correctness.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "application/security.h"
#include "peripheral/print.h"

// Test configuration
#define TEST_SERVER_URL "http://20.15.114.131:5000/process"
#define TEST_DEVICE_ID "ESP32_EcoWatt_Smart"

// Test results tracking
struct SecurityTestResults {
    uint8_t totalTests;
    uint8_t passedTests;
    uint8_t failedTests;
    
    SecurityTestResults() : totalTests(0), passedTests(0), failedTests(0) {}
    
    void recordPass() {
        totalTests++;
        passedTests++;
    }
    
    void recordFail() {
        totalTests++;
        failedTests++;
    }
    
    void printSummary() {
        print("\n");
        print("╔════════════════════════════════════════╗\n");
        print("║    SECURITY TEST RESULTS SUMMARY       ║\n");
        print("╠════════════════════════════════════════╣\n");
        print("║ Total Tests:  %-24d ║\n", totalTests);
        print("║ Passed:       %-24d ║\n", passedTests);
        print("║ Failed:       %-24d ║\n", failedTests);
        print("║ Success Rate: %-23.1f%% ║\n", 
              totalTests > 0 ? (float)passedTests / totalTests * 100.0f : 0.0f);
        print("╚════════════════════════════════════════╝\n");
    }
};

SecurityTestResults testResults;

/**
 * @brief Send HTTP POST request for testing
 */
bool sendTestPayload(const char* jsonPayload, int expectedStatusCode = 200) {
    if (WiFi.status() != WL_CONNECTED) {
        print("WiFi not connected!\n");
        return false;
    }
    
    HTTPClient http;
    http.begin(TEST_SERVER_URL);
    http.addHeader("Content-Type", "application/json");
    
    int httpCode = http.POST((uint8_t*)jsonPayload, strlen(jsonPayload));
    
    bool result = (httpCode == expectedStatusCode);
    
    print("HTTP Response: %d (expected %d) - %s\n", 
          httpCode, expectedStatusCode, 
          result ? "PASS" : "FAIL");
    
    if (httpCode > 0) {
        String response = http.getString();
        print("Response: %s\n", response.c_str());
    }
    
    http.end();
    return result;
}

/**
 * @brief Test 1: Valid secured payload should be accepted
 */
void test_valid_payload() {
    print("\n[TEST 1] Valid Secured Payload\n");
    print("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    
    // Create valid test payload
    const char* originalPayload = "{\"device_id\":\"ESP32_Test\",\"test_data\":\"valid\"}";
    
    char securedPayload[512];
    bool success = SecurityLayer::securePayload(originalPayload, securedPayload, sizeof(securedPayload));
    
    if (!success) {
        print("❌ Failed to create secured payload\n");
        testResults.recordFail();
        return;
    }
    
    print("Secured payload created: %s\n", securedPayload);
    
    // Send to server - should accept (200)
    if (sendTestPayload(securedPayload, 200)) {
        print("✅ PASS: Server accepted valid payload\n");
        testResults.recordPass();
    } else {
        print("❌ FAIL: Server rejected valid payload\n");
        testResults.recordFail();
    }
}

/**
 * @brief Test 2: Replay attack - duplicate nonce should be rejected
 */
void test_replay_attack() {
    print("\n[TEST 2] Replay Attack Detection\n");
    print("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    
    // Create payload
    const char* originalPayload = "{\"device_id\":\"ESP32_Test\",\"test_data\":\"replay\"}";
    
    char securedPayload[512];
    bool success = SecurityLayer::securePayload(originalPayload, securedPayload, sizeof(securedPayload));
    
    if (!success) {
        print("❌ Failed to create secured payload\n");
        testResults.recordFail();
        return;
    }
    
    // Send first time - should succeed
    print("Sending payload first time...\n");
    if (!sendTestPayload(securedPayload, 200)) {
        print("❌ FAIL: First attempt should succeed\n");
        testResults.recordFail();
        return;
    }
    
    // Send SAME payload again (replay attack) - should be rejected
    print("\nSending SAME payload again (replay attack)...\n");
    if (sendTestPayload(securedPayload, 401)) {
        print("✅ PASS: Server correctly rejected replayed payload\n");
        testResults.recordPass();
    } else {
        print("❌ FAIL: Server accepted replayed payload (SECURITY BREACH!)\n");
        testResults.recordFail();
    }
}

/**
 * @brief Test 3: Tampered MAC should be rejected
 */
void test_tampered_mac() {
    print("\n[TEST 3] Tampered MAC Detection\n");
    print("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    
    // Create valid payload
    const char* originalPayload = "{\"device_id\":\"ESP32_Test\",\"test_data\":\"tamper\"}";
    
    char securedPayload[512];
    bool success = SecurityLayer::securePayload(originalPayload, securedPayload, sizeof(securedPayload));
    
    if (!success) {
        print("❌ Failed to create secured payload\n");
        testResults.recordFail();
        return;
    }
    
    // Parse and tamper with MAC
    String payloadStr = String(securedPayload);
    int macPos = payloadStr.indexOf("\"mac\":\"");
    if (macPos > 0) {
        // Flip one bit in the MAC by changing a hex digit
        int digitPos = macPos + 7;  // Position after "mac":"
        char original = securedPayload[digitPos];
        securedPayload[digitPos] = (original == 'a') ? 'b' : 'a';  // Change one digit
        
        print("Tampered payload: %s\n", securedPayload);
        print("Changed MAC digit at position %d: '%c' -> '%c'\n", 
              digitPos, original, securedPayload[digitPos]);
    }
    
    // Send tampered payload - should be rejected (401)
    if (sendTestPayload(securedPayload, 401)) {
        print("✅ PASS: Server correctly rejected tampered MAC\n");
        testResults.recordPass();
    } else {
        print("❌ FAIL: Server accepted tampered MAC (SECURITY BREACH!)\n");
        testResults.recordFail();
    }
}

/**
 * @brief Test 4: Invalid JSON structure should be rejected
 */
void test_invalid_json() {
    print("\n[TEST 4] Invalid JSON Structure\n");
    print("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    
    // Send malformed JSON
    const char* invalidPayload = "{\"nonce\":12345,\"payload\":\"test\",\"mac\":\"invalid}"; // Missing closing quote
    
    print("Sending invalid JSON: %s\n", invalidPayload);
    
    // Should be rejected (400 or 401)
    HTTPClient http;
    http.begin(TEST_SERVER_URL);
    http.addHeader("Content-Type", "application/json");
    int httpCode = http.POST((uint8_t*)invalidPayload, strlen(invalidPayload));
    http.end();
    
    if (httpCode == 400 || httpCode == 401 || httpCode < 0) {
        print("✅ PASS: Server correctly rejected invalid JSON (code: %d)\n", httpCode);
        testResults.recordPass();
    } else {
        print("❌ FAIL: Server accepted invalid JSON\n");
        testResults.recordFail();
    }
}

/**
 * @brief Test 5: Missing security fields should be rejected
 */
void test_missing_fields() {
    print("\n[TEST 5] Missing Security Fields\n");
    print("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    
    // Payload missing MAC field
    const char* incompletePayload = "{\"nonce\":12345,\"payload\":\"dGVzdA==\"}";
    
    print("Sending payload without MAC: %s\n", incompletePayload);
    
    // Should be rejected (401)
    if (sendTestPayload(incompletePayload, 401)) {
        print("✅ PASS: Server correctly rejected incomplete payload\n");
        testResults.recordPass();
    } else {
        print("❌ FAIL: Server accepted incomplete payload\n");
        testResults.recordFail();
    }
}

/**
 * @brief Test 6: Old nonce should be rejected
 */
void test_old_nonce() {
    print("\n[TEST 6] Old Nonce Rejection\n");
    print("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    
    // Send a valid payload with current nonce
    const char* payload1 = "{\"device_id\":\"ESP32_Test\",\"seq\":1}";
    char secured1[512];
    SecurityLayer::securePayload(payload1, secured1, sizeof(secured1));
    
    print("Sending payload with current nonce...\n");
    sendTestPayload(secured1, 200);
    
    // Now try to send payload with OLDER nonce by temporarily decreasing nonce
    uint32_t currentNonce = SecurityLayer::getCurrentNonce();
    SecurityLayer::setNonce(currentNonce - 5);  // Go back 5 nonces
    
    const char* payload2 = "{\"device_id\":\"ESP32_Test\",\"seq\":2}";
    char secured2[512];
    SecurityLayer::securePayload(payload2, secured2, sizeof(secured2));
    
    print("\nSending payload with OLD nonce (current - 4)...\n");
    
    // Restore nonce
    SecurityLayer::setNonce(currentNonce);
    
    // Should be rejected (401)
    if (sendTestPayload(secured2, 401)) {
        print("✅ PASS: Server correctly rejected old nonce\n");
        testResults.recordPass();
    } else {
        print("❌ FAIL: Server accepted old nonce\n");
        testResults.recordFail();
    }
}

/**
 * @brief Run all security tests
 */
void runAllSecurityTests() {
    print("\n");
    print("╔════════════════════════════════════════╗\n");
    print("║  ECOWATT SECURITY TEST SUITE           ║\n");
    print("╠════════════════════════════════════════╣\n");
    print("║  Testing Security Layer Implementation ║\n");
    print("║  Device: %-29s ║\n", TEST_DEVICE_ID);
    print("║  Server: %-29s ║\n", TEST_SERVER_URL);
    print("╚════════════════════════════════════════╝\n");
    
    // Initialize security layer
    SecurityLayer::init();
    
    // Run all tests
    test_valid_payload();
    delay(1000);
    
    test_replay_attack();
    delay(1000);
    
    test_tampered_mac();
    delay(1000);
    
    test_invalid_json();
    delay(1000);
    
    test_missing_fields();
    delay(1000);
    
    test_old_nonce();
    delay(1000);
    
    // Print summary
    testResults.printSummary();
}

/**
 * @brief Security test mode entry point
 * Call from serial command or setup()
 */
void enterSecurityTestMode() {
    print("\n🔒 Entering Security Test Mode...\n\n");
    
    // Check WiFi
    if (WiFi.status() != WL_CONNECTED) {
        print("❌ WiFi not connected! Cannot run security tests.\n");
        return;
    }
    
    print("✓ WiFi connected\n");
    print("✓ IP Address: %s\n", WiFi.localIP().toString().c_str());
    print("\n");
    
    // Run tests
    runAllSecurityTests();
    
    print("\n🔒 Security Test Mode Complete\n");
}
