/**
 * Fault Injection Test for EcoWatt ESP32
 * 
 * Tests the protocol adapter's error handling against various fault types
 * injected by the Flask server's fault_injector.py module.
 * 
 * Tests:
 * 1. CRC corruption detection
 * 2. Truncated frame handling
 * 3. Garbage data rejection
 * 4. Timeout recovery
 * 5. Modbus exception handling
 * 
 * Author: EcoWatt Team
 * Date: October 22, 2025
 */

#include "fault_injection_test.h"
#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "../include/driver/protocol_adapter.h"
#include "../include/application/diagnostics.h"

// Flask server configuration
const char* FLASK_SERVER = "http://192.168.1.100:5001";  // Update with your server IP

// Test results structure
struct FaultTestResults {
    int tests_run = 0;
    int tests_passed = 0;
    int tests_failed = 0;
    
    // Per-fault-type results
    int crc_detected = 0;
    int truncated_detected = 0;
    int garbage_detected = 0;
    int timeout_recovered = 0;
    int exception_handled = 0;
    
    void print() {
        Serial.println("\n╔════════════════════════════════════════╗");
        Serial.println("║   FAULT INJECTION TEST RESULTS         ║");
        Serial.println("╠════════════════════════════════════════╣");
        Serial.printf("║ Tests Run:    %3d                      ║\n", tests_run);
        Serial.printf("║ Tests Passed: %3d                      ║\n", tests_passed);
        Serial.printf("║ Tests Failed: %3d                      ║\n", tests_failed);
        Serial.println("╠════════════════════════════════════════╣");
        Serial.printf("║ CRC Errors Detected:       %3d         ║\n", crc_detected);
        Serial.printf("║ Truncated Frames Detected: %3d         ║\n", truncated_detected);
        Serial.printf("║ Garbage Data Rejected:     %3d         ║\n", garbage_detected);
        Serial.printf("║ Timeouts Recovered:        %3d         ║\n", timeout_recovered);
        Serial.printf("║ Exceptions Handled:        %3d         ║\n", exception_handled);
        Serial.println("╚════════════════════════════════════════╝\n");
    }
};

// Global test results
static FaultTestResults results;

// External diagnostics instance
extern Diagnostics diagnostics;

// HTTP helper functions
bool enableFaultInjection(const char* fault_type, int probability = 100, int duration = 0) {
    HTTPClient http;
    String url = String(FLASK_SERVER) + "/fault/enable";
    
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    
    StaticJsonDocument<256> doc;
    doc["fault_type"] = fault_type;
    doc["probability"] = probability;
    doc["duration"] = duration;
    
    String requestBody;
    serializeJson(doc, requestBody);
    
    int httpCode = http.POST(requestBody);
    
    if (httpCode == 200) {
        String response = http.getString();
        Serial.printf("✓ Fault injection enabled: %s\n", fault_type);
        http.end();
        return true;
    } else {
        Serial.printf("✗ Failed to enable fault injection: HTTP %d\n", httpCode);
        http.end();
        return false;
    }
}

bool disableFaultInjection() {
    HTTPClient http;
    String url = String(FLASK_SERVER) + "/fault/disable";
    
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    
    int httpCode = http.POST("");
    
    if (httpCode == 200) {
        Serial.println("✓ Fault injection disabled");
        http.end();
        return true;
    } else {
        Serial.printf("✗ Failed to disable fault injection: HTTP %d\n", httpCode);
        http.end();
        return false;
    }
}

void getFaultStatus() {
    HTTPClient http;
    String url = String(FLASK_SERVER) + "/fault/status";
    
    http.begin(url);
    int httpCode = http.GET();
    
    if (httpCode == 200) {
        String response = http.getString();
        
        StaticJsonDocument<512> doc;
        DeserializationError error = deserializeJson(doc, response);
        
        if (!error) {
            Serial.println("\n=== Fault Injection Status ===");
            Serial.printf("Enabled: %s\n", doc["enabled"].as<bool>() ? "YES" : "NO");
            Serial.printf("Fault Type: %s\n", doc["fault_type"].as<const char*>());
            Serial.printf("Probability: %d%%\n", doc["probability"].as<int>());
            Serial.printf("Faults Injected: %d\n", doc["faults_injected"].as<int>());
            Serial.printf("Requests Processed: %d\n", doc["requests_processed"].as<int>());
            Serial.println("==============================\n");
        }
    }
    
    http.end();
}

// Test: CRC corruption detection
bool test_crc_corruption() {
    Serial.println("\n--- Test 1: CRC Corruption Detection ---");
    
    // Enable CRC fault injection
    if (!enableFaultInjection("corrupt_crc", 100, 10)) {
        return false;
    }
    
    // Get diagnostics baseline
    uint32_t initial_crc_errors = diagnostics.getCRCErrors();
    
    // Make 5 requests (all should fail CRC)
    ProtocolAdapter adapter;
    int detected = 0;
    
    for (int i = 0; i < 5; i++) {
        uint16_t buffer[10];
        int result = adapter.readRegisters(0x09, 0x04, 10, buffer);
        
        if (result == -2) {  // CRC error code
            detected++;
        }
        
        delay(100);
    }
    
    // Check diagnostics
    uint32_t final_crc_errors = diagnostics.getCRCErrors();
    uint32_t crc_errors_increment = final_crc_errors - initial_crc_errors;
    
    disableFaultInjection();
    
    Serial.printf("CRC errors detected: %d/5\n", detected);
    Serial.printf("Diagnostics incremented by: %d\n", crc_errors_increment);
    
    results.crc_detected = detected;
    bool passed = (detected >= 4 && crc_errors_increment >= 4);  // Allow 1 miss due to probability
    
    Serial.printf("Result: %s\n", passed ? "PASS ✓" : "FAIL ✗");
    return passed;
}

// Test: Truncated frame detection
bool test_truncated_frame() {
    Serial.println("\n--- Test 2: Truncated Frame Detection ---");
    
    if (!enableFaultInjection("truncate", 100, 10)) {
        return false;
    }
    
    uint32_t initial_malformed = diagnostics.getMalformedFrames();
    
    ProtocolAdapter adapter;
    int detected = 0;
    
    for (int i = 0; i < 5; i++) {
        uint16_t buffer[10];
        int result = adapter.readRegisters(0x09, 0x04, 10, buffer);
        
        if (result == -3) {  // Malformed frame error
            detected++;
        }
        
        delay(100);
    }
    
    uint32_t final_malformed = diagnostics.getMalformedFrames();
    uint32_t malformed_increment = final_malformed - initial_malformed;
    
    disableFaultInjection();
    
    Serial.printf("Truncated frames detected: %d/5\n", detected);
    Serial.printf("Diagnostics incremented by: %d\n", malformed_increment);
    
    results.truncated_detected = detected;
    bool passed = (detected >= 4 && malformed_increment >= 4);
    
    Serial.printf("Result: %s\n", passed ? "PASS ✓" : "FAIL ✗");
    return passed;
}

// Test: Garbage data rejection
bool test_garbage_rejection() {
    Serial.println("\n--- Test 3: Garbage Data Rejection ---");
    
    if (!enableFaultInjection("garbage", 100, 10)) {
        return false;
    }
    
    uint32_t initial_malformed = diagnostics.getMalformedFrames();
    
    ProtocolAdapter adapter;
    int detected = 0;
    
    for (int i = 0; i < 5; i++) {
        uint16_t buffer[10];
        int result = adapter.readRegisters(0x09, 0x04, 10, buffer);
        
        if (result < 0) {  // Any error
            detected++;
        }
        
        delay(100);
    }
    
    uint32_t final_malformed = diagnostics.getMalformedFrames();
    
    disableFaultInjection();
    
    Serial.printf("Garbage frames rejected: %d/5\n", detected);
    
    results.garbage_detected = detected;
    bool passed = (detected >= 4);
    
    Serial.printf("Result: %s\n", passed ? "PASS ✓" : "FAIL ✗");
    return passed;
}

// Test: Timeout handling
bool test_timeout_handling() {
    Serial.println("\n--- Test 4: Timeout Handling ---");
    
    if (!enableFaultInjection("timeout", 100, 10)) {
        return false;
    }
    
    uint32_t initial_timeouts = diagnostics.getTimeouts();
    
    ProtocolAdapter adapter;
    int recovered = 0;
    
    for (int i = 0; i < 3; i++) {  // Only 3 tests (each takes 5+ seconds)
        uint16_t buffer[10];
        unsigned long start = millis();
        int result = adapter.readRegisters(0x09, 0x04, 10, buffer);
        unsigned long elapsed = millis() - start;
        
        Serial.printf("Request %d: result=%d, elapsed=%lums\n", i+1, result, elapsed);
        
        if (result == -1 && elapsed >= 5000) {  // Timeout detected
            recovered++;
        }
        
        delay(100);
    }
    
    uint32_t final_timeouts = diagnostics.getTimeouts();
    
    disableFaultInjection();
    
    Serial.printf("Timeouts detected and recovered: %d/3\n", recovered);
    
    results.timeout_recovered = recovered;
    bool passed = (recovered >= 2);
    
    Serial.printf("Result: %s\n", passed ? "PASS ✓" : "FAIL ✗");
    return passed;
}

// Test: Modbus exception handling
bool test_exception_handling() {
    Serial.println("\n--- Test 5: Modbus Exception Handling ---");
    
    if (!enableFaultInjection("exception", 100, 10)) {
        return false;
    }
    
    ProtocolAdapter adapter;
    int handled = 0;
    
    for (int i = 0; i < 5; i++) {
        uint16_t buffer[10];
        int result = adapter.readRegisters(0x09, 0x04, 10, buffer);
        
        if (result == -4) {  // Exception code
            handled++;
        }
        
        delay(100);
    }
    
    disableFaultInjection();
    
    Serial.printf("Exceptions handled: %d/5\n", handled);
    
    results.exception_handled = handled;
    bool passed = (handled >= 4);
    
    Serial.printf("Result: %s\n", passed ? "PASS ✓" : "FAIL ✗");
    return passed;
}

// Run all fault injection tests
void runFaultInjectionTests() {
    Serial.println("\n");
    Serial.println("╔════════════════════════════════════════╗");
    Serial.println("║   FAULT INJECTION TEST SUITE           ║");
    Serial.println("╠════════════════════════════════════════╣");
    Serial.printf("║ Flask Server: %-24s ║\n", FLASK_SERVER);
    Serial.println("╚════════════════════════════════════════╝\n");
    
    // Initialize diagnostics if not already
    diagnostics.init();
    
    // Reset statistics
    HTTPClient http;
    String url = String(FLASK_SERVER) + "/fault/reset";
    http.begin(url);
    http.POST("");
    http.end();
    
    // Run tests
    results = FaultTestResults();
    
    bool test1 = test_crc_corruption();
    results.tests_run++;
    if (test1) results.tests_passed++;
    else results.tests_failed++;
    
    bool test2 = test_truncated_frame();
    results.tests_run++;
    if (test2) results.tests_passed++;
    else results.tests_failed++;
    
    bool test3 = test_garbage_rejection();
    results.tests_run++;
    if (test3) results.tests_passed++;
    else results.tests_failed++;
    
    bool test4 = test_timeout_handling();
    results.tests_run++;
    if (test4) results.tests_passed++;
    else results.tests_failed++;
    
    bool test5 = test_exception_handling();
    results.tests_run++;
    if (test5) results.tests_passed++;
    else results.tests_failed++;
    
    // Print results
    results.print();
    
    // Get final fault injection stats
    getFaultStatus();
}

// Enter fault injection test mode (for main.cpp integration)
void enterFaultInjectionTestMode() {
    Serial.println("\n🧪 Entering Fault Injection Test Mode...\n");
    
    // Wait for user confirmation
    Serial.println("Make sure Flask server is running at:");
    Serial.println(FLASK_SERVER);
    Serial.println("\nPress any key to start tests...");
    
    while (!Serial.available()) {
        delay(100);
    }
    
    while (Serial.available()) {
        Serial.read();  // Clear buffer
    }
    
    // Run tests
    runFaultInjectionTests();
    
    Serial.println("\n✓ Fault injection tests complete!");
    Serial.println("Exiting test mode...\n");
}
