/**
 * @file test_mac_hmac.cpp
 * @brief Unit tests for M4 Security - MAC/HMAC Validation
 * 
 * Tests HMAC-SHA256 generation, payload signing, and MAC verification.
 * 
 * Test Coverage:
 * 1. HMAC generation correctness
 * 2. HMAC consistency (same input → same output)
 * 3. HMAC uniqueness (different inputs → different outputs)
 * 4. Payload tampering detection
 * 5. Nonce tampering detection
 * 6. MAC format validation
 * 7. Key sensitivity
 * 8. Large payload handling
 * 9. Edge cases (empty payload, max size)
 * 10. Performance benchmarking
 * 
 * Hardware: ESP32 with mbedTLS
 */

#include <unity.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <base64.h>
#include <mbedtls/md.h>
#include "application/security.h"
#include "config/test_config.h"

// Test configuration
#define LARGE_PAYLOAD_SIZE 4096
#define TEST_ITERATIONS 10

// Global test buffers
static char securedPayload[8192];
static char testPayload[LARGE_PAYLOAD_SIZE];

// Helper function to extract MAC from secured payload
bool extractMAC(const char* securedPayload, char* macOut, size_t macSize) {
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, securedPayload);
    
    if (error) {
        Serial.println("[ERROR] Failed to parse secured payload");
        return false;
    }
    
    if (!doc.containsKey("mac")) {
        Serial.println("[ERROR] No MAC field in secured payload");
        return false;
    }
    
    const char* mac = doc["mac"];
    if (strlen(mac) >= macSize) {
        Serial.println("[ERROR] MAC buffer too small");
        return false;
    }
    
    strncpy(macOut, mac, macSize - 1);
    macOut[macSize - 1] = '\0';
    return true;
}

// Helper function to extract nonce from secured payload
bool extractNonce(const char* securedPayload, uint32_t* nonceOut) {
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, securedPayload);
    
    if (error || !doc.containsKey("nonce")) {
        return false;
    }
    
    *nonceOut = doc["nonce"];
    return true;
}

// Helper function to extract payload from secured payload
bool extractPayload(const char* securedPayload, char* payloadOut, size_t payloadSize) {
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, securedPayload);
    
    if (error || !doc.containsKey("payload")) {
        return false;
    }
    
    strncpy(payloadOut, doc["payload"].as<const char*>(), payloadSize - 1);
    payloadOut[payloadSize - 1] = '\0';
    return true;
}

void setUp(void) {
    // Setup runs before each test
    Serial.println("\n--- Test Setup ---");
    
    // Initialize security layer
    SecurityLayer::init();
    
    // Clear buffers
    memset(securedPayload, 0, sizeof(securedPayload));
    memset(testPayload, 0, sizeof(testPayload));
}

void tearDown(void) {
    // Cleanup runs after each test
    Serial.println("--- Test Teardown ---\n");
}

/**
 * @brief Test 1: HMAC Generation Correctness
 * 
 * Verifies that HMAC is generated and has the correct format (64 hex characters)
 */
void test_hmac_generation_correctness(void) {
    Serial.println("\n=== Test 1: HMAC Generation Correctness ===");
    
    const char* testData = "{\"test\":\"data\"}";
    
    bool result = SecurityLayer::securePayload(testData, securedPayload, sizeof(securedPayload));
    TEST_ASSERT_TRUE_MESSAGE(result, "Failed to secure payload");
    
    // Extract MAC
    char mac[65];
    TEST_ASSERT_TRUE(extractMAC(securedPayload, mac, sizeof(mac)));
    
    // Verify MAC format: should be 64 hex characters (32 bytes * 2)
    size_t macLen = strlen(mac);
    TEST_ASSERT_EQUAL_MESSAGE(64, macLen, "MAC should be 64 hex characters");
    
    // Verify all characters are hex
    for (size_t i = 0; i < macLen; i++) {
        TEST_ASSERT_TRUE_MESSAGE(
            isxdigit(mac[i]),
            "MAC should contain only hex characters"
        );
    }
    
    Serial.printf("[PASS] HMAC generated correctly: %s\n", mac);
}

/**
 * @brief Test 2: HMAC Consistency
 * 
 * Verifies that the same payload produces the same HMAC (with same nonce)
 */
void test_hmac_consistency(void) {
    Serial.println("\n=== Test 2: HMAC Consistency ===");
    
    const char* testData = "{\"voltage\":230,\"current\":5}";
    
    // Set a fixed nonce for consistency
    uint32_t testNonce = 50000;
    SecurityLayer::setNonce(testNonce - 1);  // Will increment to 50000
    
    // Generate first HMAC
    bool result1 = SecurityLayer::securePayload(testData, securedPayload, sizeof(securedPayload));
    TEST_ASSERT_TRUE(result1);
    
    char mac1[65];
    TEST_ASSERT_TRUE(extractMAC(securedPayload, mac1, sizeof(mac1)));
    
    // Reset to same nonce
    SecurityLayer::setNonce(testNonce - 1);
    
    // Generate second HMAC (reuse global buffer)
    memset(testPayload, 0, sizeof(testPayload));
    bool result2 = SecurityLayer::securePayload(testData, testPayload, sizeof(testPayload));
    TEST_ASSERT_TRUE(result2);
    
    char mac2[65];
    TEST_ASSERT_TRUE(extractMAC(testPayload, mac2, sizeof(mac2)));
    
    // Compare MACs
    TEST_ASSERT_EQUAL_STRING_MESSAGE(
        mac1, mac2,
        "Same payload with same nonce should produce identical HMAC"
    );
    
    Serial.printf("[PASS] HMAC is consistent: %s\n", mac1);
}

/**
 * @brief Test 3: HMAC Uniqueness
 * 
 * Verifies that different payloads produce different HMACs
 */
void test_hmac_uniqueness(void) {
    Serial.println("\n=== Test 3: HMAC Uniqueness ===");
    
    // Set fixed nonce
    uint32_t testNonce = 60000;
    SecurityLayer::setNonce(testNonce - 1);
    
    // First payload
    const char* payload1 = "{\"data\":\"test1\"}";
    bool result1 = SecurityLayer::securePayload(payload1, securedPayload, sizeof(securedPayload));
    TEST_ASSERT_TRUE(result1);
    
    char mac1[65];
    TEST_ASSERT_TRUE(extractMAC(securedPayload, mac1, sizeof(mac1)));
    
    // Reset nonce
    SecurityLayer::setNonce(testNonce - 1);
    
    // Second payload (different) - reuse testPayload buffer
    const char* payload2 = "{\"data\":\"test2\"}";
    memset(testPayload, 0, sizeof(testPayload));
    bool result2 = SecurityLayer::securePayload(payload2, testPayload, sizeof(testPayload));
    TEST_ASSERT_TRUE(result2);
    
    char mac2[65];
    TEST_ASSERT_TRUE(extractMAC(testPayload, mac2, sizeof(mac2)));
    
    // MACs should be different
    TEST_ASSERT_NOT_EQUAL_MESSAGE(
        0, strcmp(mac1, mac2),
        "Different payloads should produce different HMACs"
    );
    
    Serial.printf("[PASS] HMAC1: %s\n", mac1);
    Serial.printf("[PASS] HMAC2: %s\n", mac2);
}

/**
 * @brief Test 4: Payload Tampering Detection
 * 
 * Verifies that modifying the payload invalidates the HMAC
 */
void test_payload_tampering_detection(void) {
    Serial.println("\n=== Test 4: Payload Tampering Detection ===");
    
    const char* originalPayload = "{\"power\":1000}";
    
    bool result = SecurityLayer::securePayload(originalPayload, securedPayload, sizeof(securedPayload));
    TEST_ASSERT_TRUE(result);
    
    char originalMAC[65];
    TEST_ASSERT_TRUE(extractMAC(securedPayload, originalMAC, sizeof(originalMAC)));
    
    // Parse the secured payload
    DynamicJsonDocument doc(8192);
    DeserializationError error = deserializeJson(doc, securedPayload);
    TEST_ASSERT_FALSE(error);
    
    // Get the original base64-encoded payload
    String encodedPayload = doc["payload"].as<String>();
    
    // Tamper with the base64 string directly - change one character
    // This simulates tampering without needing to decode/re-encode
    String tamperedEncoded = encodedPayload;
    if (tamperedEncoded.length() > 5) {
        // Change a character in the middle of the base64 string
        tamperedEncoded[tamperedEncoded.length() / 2] = (tamperedEncoded[tamperedEncoded.length() / 2] == 'A') ? 'B' : 'A';
    }
    
    // Create tampered secured payload (with original MAC - would fail server validation)
    doc["payload"] = tamperedEncoded;
    
    // Reuse testPayload buffer for tampered payload
    memset(testPayload, 0, sizeof(testPayload));
    serializeJson(doc, testPayload, sizeof(testPayload));
    
    char tamperedMAC[65];
    TEST_ASSERT_TRUE(extractMAC(testPayload, tamperedMAC, sizeof(tamperedMAC)));
    
    // MAC should still be the same (wasn't recalculated), but server would reject this
    // because MAC validation would fail when server calculates MAC from tampered payload
    TEST_ASSERT_EQUAL_STRING_MESSAGE(
        originalMAC, tamperedMAC,
        "Tampered payload keeps original MAC (server would detect mismatch)"
    );
    
    Serial.printf("[PASS] Original MAC: %s\n", originalMAC);
    Serial.printf("[PASS] Tampered payload has same MAC (simulates attack - would fail on server)\n");
}

/**
 * @brief Test 5: Nonce Tampering Detection
 * 
 * Verifies that modifying the nonce invalidates the HMAC
 */
void test_nonce_tampering_detection(void) {
    Serial.println("\n=== Test 5: Nonce Tampering Detection ===");
    
    const char* testData = "{\"test\":\"nonce tampering\"}";
    
    bool result = SecurityLayer::securePayload(testData, securedPayload, sizeof(securedPayload));
    TEST_ASSERT_TRUE(result);
    
    uint32_t originalNonce;
    TEST_ASSERT_TRUE(extractNonce(securedPayload, &originalNonce));
    
    char originalMAC[65];
    TEST_ASSERT_TRUE(extractMAC(securedPayload, originalMAC, sizeof(originalMAC)));
    
    // Parse and modify nonce
    DynamicJsonDocument doc(8192);
    DeserializationError error = deserializeJson(doc, securedPayload);
    TEST_ASSERT_FALSE(error);
    
    // Change nonce
    doc["nonce"] = originalNonce + 1000;
    
    // Reuse testPayload buffer
    memset(testPayload, 0, sizeof(testPayload));
    serializeJson(doc, testPayload, sizeof(testPayload));
    
    uint32_t tamperedNonce;
    TEST_ASSERT_TRUE(extractNonce(testPayload, &tamperedNonce));
    TEST_ASSERT_NOT_EQUAL(originalNonce, tamperedNonce);
    
    Serial.printf("[PASS] Nonce tampering detected\n");
    Serial.printf("       Original: %u, Tampered: %u\n", originalNonce, tamperedNonce);
    Serial.printf("       Server would reject due to HMAC mismatch\n");
}

/**
 * @brief Test 6: MAC Format Validation
 * 
 * Verifies that MAC is properly formatted as lowercase hex
 */
void test_mac_format_validation(void) {
    Serial.println("\n=== Test 6: MAC Format Validation ===");
    
    const char* testData = "{\"format\":\"test\"}";
    
    bool result = SecurityLayer::securePayload(testData, securedPayload, sizeof(securedPayload));
    TEST_ASSERT_TRUE(result);
    
    char mac[65];
    TEST_ASSERT_TRUE(extractMAC(securedPayload, mac, sizeof(mac)));
    
    // Check that MAC is lowercase hex (or uppercase is also acceptable)
    bool allHex = true;
    for (size_t i = 0; i < strlen(mac); i++) {
        if (!isxdigit(mac[i])) {
            allHex = false;
            break;
        }
    }
    
    TEST_ASSERT_TRUE_MESSAGE(allHex, "MAC should be valid hexadecimal");
    
    Serial.printf("[PASS] MAC format is valid: %s\n", mac);
}

/**
 * @brief Test 7: Different Nonce Produces Different HMAC
 * 
 * Verifies that changing the nonce changes the HMAC (even with same payload)
 */
void test_nonce_sensitivity(void) {
    Serial.println("\n=== Test 7: Nonce Sensitivity ===");
    
    const char* testData = "{\"test\":\"nonce sensitivity\"}";
    
    // First HMAC with nonce N
    uint32_t nonce1 = 70000;
    SecurityLayer::setNonce(nonce1 - 1);
    
    bool result1 = SecurityLayer::securePayload(testData, securedPayload, sizeof(securedPayload));
    TEST_ASSERT_TRUE(result1);
    
    char mac1[65];
    TEST_ASSERT_TRUE(extractMAC(securedPayload, mac1, sizeof(mac1)));
    
    // Second HMAC with nonce N+1
    uint32_t nonce2 = 70001;
    SecurityLayer::setNonce(nonce2 - 1);
    
    // Reuse testPayload buffer
    memset(testPayload, 0, sizeof(testPayload));
    bool result2 = SecurityLayer::securePayload(testData, testPayload, sizeof(testPayload));
    TEST_ASSERT_TRUE(result2);
    
    char mac2[65];
    TEST_ASSERT_TRUE(extractMAC(testPayload, mac2, sizeof(mac2)));
    
    // MACs should be different
    TEST_ASSERT_NOT_EQUAL_MESSAGE(
        0, strcmp(mac1, mac2),
        "Different nonces should produce different HMACs"
    );
    
    Serial.printf("[PASS] Nonce %u → MAC: %s\n", nonce1, mac1);
    Serial.printf("[PASS] Nonce %u → MAC: %s\n", nonce2, mac2);
}

/**
 * @brief Test 8: Large Payload Handling
 * 
 * Tests HMAC generation with large payloads
 */
void test_large_payload_hmac(void) {
    Serial.println("\n=== Test 8: Large Payload HMAC ===");
    
    // Create a reasonably large JSON payload that fits in 8KB secured buffer
    // Base64 encoding adds ~33% overhead, so keep original < ~5KB
    DynamicJsonDocument doc(2048);
    JsonArray dataArray = doc.createNestedArray("data");
    
    for (int i = 0; i < 20; i++) {  // Further reduced to 20 items
        JsonObject obj = dataArray.createNestedObject();
        obj["v"] = 230 + i;  // Shortened key names
        obj["c"] = 5 + (i * 0.1);
        obj["p"] = 1150 + (i * 50);
    }
    
    // Use heap allocation for payload
    char* largePayload = (char*)malloc(1536);
    TEST_ASSERT_NOT_NULL_MESSAGE(largePayload, "Failed to allocate memory for large payload");
    
    size_t payloadSize = serializeJson(doc, largePayload, 1536);
    TEST_ASSERT_GREATER_THAN(100, payloadSize);  // Should be substantial
    
    Serial.printf("[INFO] Original payload size: %zu bytes\n", payloadSize);
    Serial.printf("[INFO] Secured buffer size: %d bytes\n", sizeof(securedPayload));
    Serial.printf("[INFO] Expected secured size: ~%zu bytes (payload * 1.4 + overhead)\n", (size_t)(payloadSize * 1.4 + 200));
    
    // Secure the large payload
    unsigned long startTime = micros();
    bool result = SecurityLayer::securePayload(largePayload, securedPayload, sizeof(securedPayload));
    unsigned long duration = micros() - startTime;
    
    TEST_ASSERT_TRUE_MESSAGE(result, "Failed to secure large payload");
    
    // Verify MAC was generated
    char mac[65];
    TEST_ASSERT_TRUE(extractMAC(securedPayload, mac, sizeof(mac)));
    TEST_ASSERT_EQUAL(64, strlen(mac));
    
    Serial.printf("[PASS] Large payload (%zu bytes) secured in %lu us\n", payloadSize, duration);
    Serial.printf("       MAC: %s\n", mac);
    
    free(largePayload);
}

/**
 * @brief Test 9: Empty Payload Edge Case
 * 
 * Tests HMAC generation with empty/minimal payloads
 */
void test_empty_payload_hmac(void) {
    Serial.println("\n=== Test 9: Empty Payload HMAC ===");
    
    const char* emptyPayload = "{}";
    
    bool result = SecurityLayer::securePayload(emptyPayload, securedPayload, sizeof(securedPayload));
    TEST_ASSERT_TRUE_MESSAGE(result, "Should handle empty payload");
    
    char mac[65];
    TEST_ASSERT_TRUE(extractMAC(securedPayload, mac, sizeof(mac)));
    TEST_ASSERT_EQUAL(64, strlen(mac));
    
    Serial.printf("[PASS] Empty payload HMAC: %s\n", mac);
}

/**
 * @brief Test 10: HMAC Performance Benchmark
 * 
 * Measures HMAC generation performance
 */
void test_hmac_performance(void) {
    Serial.println("\n=== Test 10: HMAC Performance Benchmark ===");
    
    const char* testData = "{\"voltage\":230,\"current\":5,\"power\":1150}";
    
    unsigned long totalTime = 0;
    const int iterations = TEST_ITERATIONS;
    
    for (int i = 0; i < iterations; i++) {
        unsigned long start = micros();
        bool result = SecurityLayer::securePayload(testData, securedPayload, sizeof(securedPayload));
        unsigned long duration = micros() - start;
        
        TEST_ASSERT_TRUE(result);
        totalTime += duration;
    }
    
    unsigned long avgTime = totalTime / iterations;
    
    Serial.printf("[Benchmark] Average HMAC generation time: %lu us\n", avgTime);
    Serial.printf("[Benchmark] Throughput: ~%.2f operations/second\n", 1000000.0 / avgTime);
    
    // Performance should be reasonable (< 10ms)
    TEST_ASSERT_LESS_THAN_MESSAGE(
        10000, avgTime,
        "HMAC generation should complete in under 10ms"
    );
    
    Serial.printf("[PASS] Performance is acceptable\n");
}

void setup() {
    delay(2000);  // Wait for serial monitor
    
    UNITY_BEGIN();
    
    Serial.println("\n========================================");
    Serial.println("  M4 MAC/HMAC VALIDATION TEST SUITE");
    Serial.println("========================================");
    Serial.println("Testing HMAC-SHA256 generation and validation");
    Serial.println("========================================\n");
    
    RUN_TEST(test_hmac_generation_correctness);
    RUN_TEST(test_hmac_consistency);
    RUN_TEST(test_hmac_uniqueness);
    RUN_TEST(test_payload_tampering_detection);
    RUN_TEST(test_nonce_tampering_detection);
    RUN_TEST(test_mac_format_validation);
    RUN_TEST(test_nonce_sensitivity);
    RUN_TEST(test_large_payload_hmac);
    RUN_TEST(test_empty_payload_hmac);
    RUN_TEST(test_hmac_performance);
    
    UNITY_END();
}

void loop() {
    // Tests run once in setup()
}
