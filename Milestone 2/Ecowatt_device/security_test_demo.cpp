#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include "simple_security.h"

// Create security instance for testing
SimpleSecurity testSecurity;

void setup() {
    Serial.begin(115200);
    delay(2000);
    
    Serial.println("\n=== EcoWatt Security Layer Test ===");
    
    // Initialize security layer with test PSK
    const char* testPSK = "4A6F486E20446F652041455336342D536563726574204B65792D323536626974";
    
    if (!testSecurity.begin(testPSK)) {
        Serial.println("Failed to initialize security layer");
        return;
    }
    
    Serial.println("Security layer initialized");
    
    // Test 1: Simple message authentication
    Serial.println("\n--- Test 1: Simple Message ---");
    String simpleMessage = "Hello, secure world!";
    String securedSimple = testSecurity.secureMessage(simpleMessage);
    
    Serial.println("Original: " + simpleMessage);
    Serial.println("Secured:  " + securedSimple);
    
    String verifiedSimple = testSecurity.unsecureMessage(securedSimple);
    if (verifiedSimple == simpleMessage) {
        Serial.println("Simple message test PASSED");
    } else {
        Serial.println("Simple message test FAILED");
    }
    
    // Test 2: JSON sensor data
    Serial.println("\n--- Test 2: JSON Sensor Data ---");
    JsonDocument sensorDoc;
    sensorDoc["device_id"] = "ESP32_Test";
    sensorDoc["timestamp"] = millis();
    sensorDoc["temperature"] = 25.4;
    sensorDoc["humidity"] = 60.2;
    sensorDoc["voltage"] = 12.6;
    
    String sensorJson;
    serializeJson(sensorDoc, sensorJson);
    
    String securedSensor = testSecurity.secureMessage(sensorJson);
    Serial.println("Original JSON: " + sensorJson);
    Serial.println("Secured size:  " + String(securedSensor.length()) + " bytes");
    
    String verifiedSensor = testSecurity.unsecureMessage(securedSensor);
    if (verifiedSensor == sensorJson) {
        Serial.println("JSON sensor test PASSED");
    } else {
        Serial.println("JSON sensor test FAILED");
        Serial.println("Expected: " + sensorJson);
        Serial.println("Got:      " + verifiedSensor);
    }
    
    // Test 3: Mock encryption
    Serial.println("\n--- Test 3: Mock Encryption ---");
    String encryptedMessage = testSecurity.secureMessage(simpleMessage, true);
    String decryptedMessage = testSecurity.unsecureMessage(encryptedMessage);
    
    Serial.println("Original:   " + simpleMessage);
    Serial.println("Encrypted:  " + encryptedMessage);
    Serial.println("Decrypted:  " + decryptedMessage);
    
    if (decryptedMessage == simpleMessage) {
        Serial.println("Mock encryption test PASSED");
    } else {
        Serial.println("Mock encryption test FAILED");
    }
    
    // Test 4: Anti-replay protection
    Serial.println("\n--- Test 4: Anti-replay Protection ---");
    String oldMessage = securedSimple; // Reuse first secured message
    String replayResult = testSecurity.unsecureMessage(oldMessage);
    
    if (replayResult.isEmpty()) {
        Serial.println("Anti-replay protection PASSED - old message rejected");
    } else {
        Serial.println("Anti-replay protection FAILED - old message accepted");
    }
    
    // Test 5: Tampered message detection
    Serial.println("\n--- Test 5: Tamper Detection ---");
    String freshMessage = testSecurity.secureMessage("Fresh message for tampering test");
    
    // Tamper with the message by changing one character
    String tamperedMessage = freshMessage;
    if (tamperedMessage.length() > 10) {
        tamperedMessage[10] = (tamperedMessage[10] == 'a') ? 'b' : 'a';
    }
    
    String tamperedResult = testSecurity.unsecureMessage(tamperedMessage);
    if (tamperedResult.isEmpty()) {
        Serial.println("Tamper detection PASSED - modified message rejected");
    } else {
        Serial.println("Tamper detection FAILED - modified message accepted");
    }
    
    Serial.println("\n=== Security Layer Tests Complete ===");
    Serial.printf("Current nonce: %u\n", testSecurity.getCurrentNonce());
}

void loop() {
    // Demonstrate continuous operation with nonce increment
    delay(5000);
    
    static int counter = 0;
    counter++;
    
    JsonDocument testDoc;
    testDoc["counter"] = counter;
    testDoc["uptime"] = millis();
    testDoc["free_heap"] = ESP.getFreeHeap();
    
    String testJson;
    serializeJson(testDoc, testJson);
    
    String secured = testSecurity.secureMessage(testJson);
    String verified = testSecurity.unsecureMessage(secured);
    
    Serial.printf("\n[%d] Nonce: %u | Secured: %d bytes | Verified: %s\n", 
        counter, testSecurity.getCurrentNonce(), secured.length(), 
        verified.isEmpty() ? "FAILED" : "OK");
}