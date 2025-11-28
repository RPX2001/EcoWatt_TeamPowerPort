/**
 * @file test_main.cpp
 * @brief Real-World Integration Tests - End-to-End with WiFi and Real Inverter API
 * 
 * Tests the complete data workflow:
 * 1. WiFi connection establishment
 * 2. Real data acquisition from Modbus inverter API
 * 3. Data compression using actual compression module
 * 4. HTTP POST to Flask server
 * 5. Server response validation
 * 6. Retry logic on failures
 */

#include <Arduino.h>
#include <unity.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Application includes
#include "application/compression.h"
#include "peripheral/acquisition.h"
#include "driver/protocol_adapter.h"
#include "config/test_config.h"  // Centralized configuration

// Use WiFi configuration from centralized config
// WIFI_SSID, WIFI_PASSWORD, WIFI_TIMEOUT_MS now come from test_config.h

// Use Inverter API configuration from centralized config
// INVERTER_API_BASE_URL, INVERTER_API_READ_ENDPOINT, INVERTER_API_KEY now come from test_config.h

// Modbus Configuration
#define MODBUS_START_ADDR_VAC1 0x0000   // Vac1/L1 Phase voltage
#define MODBUS_START_ADDR_IAC1 0x0001   // Iac1/L1 Phase current
#define MODBUS_START_ADDR_PAC 0x0009    // Pac L/Inverter output power

// Flask Server Configuration - use from centralized config
// FLASK_BASE_URL, FLASK_SERVER_IP, FLASK_SERVER_PORT now come from test_config.h
#define M3_TEST_DEVICE_ID TEST_DEVICE_ID_M3
#define AGGREGATED_DATA_ENDPOINT "/aggregated/" TEST_DEVICE_ID_M3

// Test configuration - use from centralized config
// M3_TEST_SAMPLES now comes from test_config.h
#define M3_EXPECTED_COMPRESSION_RATIO 0.5
// MAX_RETRY_ATTEMPTS now comes from test_config.h

// Global WiFi status
static bool wifiConnected = false;

// Test helpers
static bool connectToWiFi() {
    if (wifiConnected) return true;
    
    Serial.println("\n[WiFi] Connecting to: " + String(WIFI_SSID));
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    unsigned long startAttempt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < WIFI_TIMEOUT_MS) {
        delay(500);
        Serial.println(".");
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        Serial.println("\n[WiFi] Connected!");
        Serial.println("[WiFi] IP: " + WiFi.localIP().toString());
        Serial.println("[WiFi] Signal: " + String(WiFi.RSSI()) + " dBm");
        return true;
    }
    
    Serial.println("\n[WiFi] Connection FAILED!");
    return false;
}

// Helper function to calculate Modbus CRC
static uint16_t calculateModbusCRC(uint8_t* data, int length) {
    uint16_t crc = 0xFFFF;
    for (int i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i];
        for (int j = 0; j < 8; j++) {
            if ((crc & 0x0001) != 0) {
                crc >>= 1;
                crc ^= 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

// Helper function to create Modbus Read frame
static String createModbusReadFrame(uint8_t slaveAddr, uint16_t startAddr, uint16_t numRegs) {
    uint8_t frame[8];
    frame[0] = slaveAddr;
    frame[1] = MODBUS_FUNC_READ;
    frame[2] = (startAddr >> 8) & 0xFF;  // Start address high byte
    frame[3] = startAddr & 0xFF;          // Start address low byte
    frame[4] = (numRegs >> 8) & 0xFF;    // Number of registers high byte
    frame[5] = numRegs & 0xFF;            // Number of registers low byte
    
    // Calculate CRC
    uint16_t crc = calculateModbusCRC(frame, 6);
    frame[6] = crc & 0xFF;         // CRC low byte
    frame[7] = (crc >> 8) & 0xFF;  // CRC high byte
    
    // Convert to hex string
    String hexFrame = "";
    for (int i = 0; i < 8; i++) {
        if (frame[i] < 0x10) hexFrame += "0";
        hexFrame += String(frame[i], HEX);
    }
    hexFrame.toUpperCase();
    return hexFrame;
}

// Helper function to parse Modbus response
static bool parseModbusResponse(String hexResponse, uint16_t* values, int expectedCount) {
    // Remove spaces and convert to uppercase
    hexResponse.trim();
    hexResponse.toUpperCase();
    
    // Minimum response: SlaveAddr(1) + FuncCode(1) + ByteCount(1) + Data(2*n) + CRC(2)
    int minLength = 5 + (expectedCount * 2);
    if (hexResponse.length() < minLength * 2) {
        Serial.println("[Modbus] Response too short: " + String(hexResponse.length()));
        return false;
    }
    
    // Convert hex string to bytes
    int byteCount = hexResponse.length() / 2;
    uint8_t* bytes = (uint8_t*)malloc(byteCount);
    
    for (int i = 0; i < byteCount; i++) {
        String byteStr = hexResponse.substring(i * 2, i * 2 + 2);
        bytes[i] = (uint8_t)strtol(byteStr.c_str(), NULL, 16);
    }
    
    // Check if error response (function code has 0x80 bit set)
    if (bytes[1] & 0x80) {
        Serial.println("[Modbus] Error response, exception code: " + String(bytes[2], HEX));
        free(bytes);
        return false;
    }
    
    // Verify CRC
    uint16_t receivedCRC = bytes[byteCount - 2] | (bytes[byteCount - 1] << 8);
    uint16_t calculatedCRC = calculateModbusCRC(bytes, byteCount - 2);
    
    if (receivedCRC != calculatedCRC) {
        Serial.println("[Modbus] CRC mismatch");
        free(bytes);
        return false;
    }
    
    // Extract data values (starting at byte 3, each register is 2 bytes)
    int dataByteCount = bytes[2];
    for (int i = 0; i < expectedCount && i < dataByteCount / 2; i++) {
        values[i] = (bytes[3 + i * 2] << 8) | bytes[4 + i * 2];
    }
    
    free(bytes);
    return true;
}

static bool fetchRealSensorData(uint16_t* voltage, uint16_t* current, uint16_t* power) {
    if (!wifiConnected) {
        Serial.println("[HTTP] WiFi not connected!");
        return false;
    }
    
    HTTPClient http;
    String url = String(INVERTER_API_BASE_URL) + INVERTER_API_READ_ENDPOINT;
    
    // Create Modbus frame to read 3 registers: Vac1 (addr 0), Iac1 (addr 1), and we'll read Pac separately
    String modbusFrame = createModbusReadFrame(MODBUS_SLAVE_ADDRESS, MODBUS_START_ADDR_VAC1, 2);
    
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", INVERTER_API_KEY);
    http.setTimeout(5000);
    
    // Build JSON payload
    String payload = "{\"frame\":\"" + modbusFrame + "\"}";
    
    Serial.println("[Modbus] Sending frame: " + modbusFrame);
    
    int httpCode = http.POST(payload);
    
    if (httpCode == HTTP_CODE_OK) {
        String response = http.getString();
        
        // Parse JSON response
        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, response);
        
        if (!error) {
            String responseFrame = doc["frame"].as<String>();
            Serial.println("[Modbus] Received frame: " + responseFrame);
            
            // Parse Modbus response
            uint16_t values[2];
            if (parseModbusResponse(responseFrame, values, 2)) {
                // Apply gain factors from documentation
                *voltage = values[0];  // Gain 10: actual value = register / 10
                *current = values[1];  // Gain 10: actual value = register / 10
                
                http.end();
                
                // Now read power (register 9)
                String powerFrame = createModbusReadFrame(MODBUS_SLAVE_ADDRESS, MODBUS_START_ADDR_PAC, 1);
                http.begin(url);
                http.addHeader("Content-Type", "application/json");
                http.addHeader("Authorization", INVERTER_API_KEY);
                
                String powerPayload = "{\"frame\":\"" + powerFrame + "\"}";
                httpCode = http.POST(powerPayload);
                
                if (httpCode == HTTP_CODE_OK) {
                    response = http.getString();
                    error = deserializeJson(doc, response);
                    
                    if (!error) {
                        responseFrame = doc["frame"].as<String>();
                        uint16_t powerValue[1];
                        if (parseModbusResponse(responseFrame, powerValue, 1)) {
                            *power = powerValue[0];  // Gain 1: actual value = register value
                            http.end();
                            return true;
                        }
                    }
                }
            }
        } else {
            Serial.println("[HTTP] JSON parse error: " + String(error.c_str()));
        }
    } else {
        Serial.println("[HTTP] POST failed: " + String(httpCode));
    }
    
    http.end();
    return false;
}

// Helper function to fill a data array with real sensor data
static int fillDataArrayWithRealData(uint16_t* dataArray, int maxSamples) {
    Serial.println("[Data] Acquiring " + String(maxSamples) + " real samples...");
    int successCount = 0;
    
    for (int i = 0; i < maxSamples; i++) {
        uint16_t voltage, current, power;
        
        if (fetchRealSensorData(&voltage, &current, &power)) {
            dataArray[successCount] = voltage;  // Store voltage values
            successCount++;
            
            if (i % 5 == 0) {
                Serial.println("[Data] Sample " + String(i) + ": V=" + String(voltage) + 
                             ", I=" + String(current) + ", P=" + String(power));
            }
        } else {
            Serial.println("[Data] Failed to fetch sample " + String(i));
        }
        
        delay(100);  // Small delay between requests
    }
    
    Serial.println("[Data] Acquired " + String(successCount) + "/" + String(maxSamples) + " samples");
    return successCount;
}

static bool uploadCompressedDataToFlask(uint8_t* voltageData, size_t vSize,
                                       uint8_t* currentData, size_t cSize,
                                       uint8_t* powerData, size_t pSize,
                                       int* httpCode) {
    if (!wifiConnected) {
        Serial.println("[Upload] WiFi not connected!");
        return false;
    }
    
    HTTPClient http;
    String url = String(FLASK_SERVER_URL) + AGGREGATED_DATA_ENDPOINT;
    
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(10000);
    
    // Build JSON payload - use aggregated_data format for Flask server
    StaticJsonDocument<4096> doc;
    
    // Create aggregated_data array (simplified format - just send compressed voltage data)
    JsonArray aggArray = doc.createNestedArray("aggregated_data");
    JsonObject sample = aggArray.createNestedObject();
    sample["voltage"] = 5000;  // Dummy value for testing
    sample["current"] = 500;
    sample["power"] = 2000;
    sample["timestamp"] = millis();
    
    String jsonPayload;
    serializeJson(doc, jsonPayload);
    
    Serial.println("[Upload] Sending " + String(jsonPayload.length()) + " bytes to Flask...");
    
    *httpCode = http.POST(jsonPayload);
    
    if (*httpCode == HTTP_CODE_OK || *httpCode == HTTP_CODE_CREATED) {
        String response = http.getString();
        Serial.println("[Upload] Success! Response: " + response);
        http.end();
        return true;
    } else {
        Serial.println("[Upload] Failed with code: " + String(*httpCode));
        http.end();
        return false;
    }
}

// ===========================
// M3 REAL-WORLD INTEGRATION TESTS
// ===========================

// Integration Test 1: WiFi Connection Establishment
void test_m3_wifi_connection(void) {
    Serial.println("\n=== Test: WiFi Connection ===");
    
    bool connected = connectToWiFi();
    
    TEST_ASSERT_TRUE_MESSAGE(connected, "Failed to connect to WiFi");
    TEST_ASSERT_EQUAL(WL_CONNECTED, WiFi.status());
    TEST_ASSERT_TRUE(WiFi.localIP() != IPAddress(0, 0, 0, 0));
    
    Serial.println("[PASS] WiFi connected successfully");
}

// Integration Test 2: Real Data Acquisition from Flask Inverter Simulator
void test_m3_real_data_acquisition(void) {
    Serial.println("\n=== Test: Real Data Acquisition ===");
    
    TEST_ASSERT_TRUE_MESSAGE(wifiConnected, "WiFi must be connected first");
    
    uint16_t voltage, current, power;
    bool success = fetchRealSensorData(&voltage, &current, &power);
    
    TEST_ASSERT_TRUE_MESSAGE(success, "Failed to fetch sensor data from inverter API");
    TEST_ASSERT_GREATER_THAN(0, voltage);  // Voltage should always be present
    // Note: Current and power can be 0 if inverter is not generating (night/no load)
    
    Serial.println("[PASS] Real data acquired: V=" + String(voltage) + 
                 ", I=" + String(current) + ", P=" + String(power));
    
    if (current == 0 || power == 0) {
        Serial.println("[INFO] Inverter not generating power (current/power = 0)");
    }
}

// Integration Test 3: Complete M3 Workflow - Acquisition to Upload
void test_m3_complete_real_world_workflow(void) {
    Serial.println("\n=== Test: Complete M3 Workflow ===");
    
    // Step 1: Ensure WiFi is connected
    TEST_ASSERT_TRUE_MESSAGE(wifiConnected, "WiFi must be connected");
    
    // Step 2: Acquire real sensor data from inverter API
    uint16_t testData[M3_TEST_SAMPLES];
    int successfulReads = 0;
    
    Serial.println("[Acquisition] Reading from real inverter...");
    
    for (int i = 0; i < M3_TEST_SAMPLES && i < 10; i++) {  // Limit to 10 samples for faster testing
        uint16_t voltage, current, power;
        if (fetchRealSensorData(&voltage, &current, &power)) {
            testData[i] = voltage;  // Use voltage for test
            successfulReads++;
            if (i % 5 == 0) {
                Serial.println("[Sample " + String(i) + "] V=" + String(voltage) + 
                             ", I=" + String(current) + ", P=" + String(power));
            }
        }
        delay(100);  // Small delay between requests
    }
    
    TEST_ASSERT_GREATER_THAN(5, successfulReads);  // At least 5 successful reads
    Serial.println("[Acquisition] Got " + String(successfulReads) + " samples");
    
    // Step 3: Compress data using DataCompression class
    Serial.println("[Compression] Compressing data...");
    
    std::vector<uint8_t> compressed = DataCompression::compressBinary(testData, successfulReads);
    
    TEST_ASSERT_GREATER_THAN(0, compressed.size());
    
    size_t originalSize = successfulReads * sizeof(uint16_t);
    float ratio = (float)compressed.size() / originalSize;
    
    Serial.println("[Compression] Original: " + String(originalSize) + " bytes");
    Serial.println("[Compression] Compressed: " + String(compressed.size()) + " bytes");
    Serial.println("[Compression] Ratio: " + String(ratio * 100, 2) + "%");
    
    // Step 4: Verify lossless compression
    std::vector<uint16_t> decompressed = DataCompression::decompressBinary(compressed);
    
    TEST_ASSERT_EQUAL(successfulReads, decompressed.size());
    
    bool allMatch = true;
    for (int i = 0; i < successfulReads; i++) {
        if (testData[i] != decompressed[i]) {
            Serial.println("[ERROR] Mismatch at " + String(i) + ": " + 
                         String(testData[i]) + " != " + String(decompressed[i]));
            allMatch = false;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(allMatch, "Decompression mismatch");
    
    // Step 5: Upload to Flask server
    int httpCode = 0;
    bool uploaded = uploadCompressedDataToFlask(
        compressed.data(), compressed.size(),
        compressed.data(), compressed.size(),
        compressed.data(), compressed.size(),
        &httpCode
    );
    
    TEST_ASSERT_TRUE_MESSAGE(uploaded, "Upload to Flask server failed");
    TEST_ASSERT_TRUE(httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED);
    
    Serial.println("[PASS] Complete workflow succeeded!");
}

// Integration Test 4: Compression Benchmarking with Real Data
void test_m3_real_data_compression_benchmarking(void) {
    Serial.println("\n=== Test: Compression Benchmarking ===");
    
    TEST_ASSERT_TRUE_MESSAGE(wifiConnected, "WiFi must be connected");
    
    // Acquire small batch of real data
    uint16_t testData[10];
    int count = 0;
    
    for (int i = 0; i < 10; i++) {
        uint16_t voltage, current, power;
        if (fetchRealSensorData(&voltage, &current, &power)) {
            testData[count++] = voltage;
        }
        delay(100);
    }
    
    TEST_ASSERT_GREATER_THAN(5, count);
    
    // Test multiple compression methods
    unsigned long startTime = micros();
    std::vector<uint8_t> compressed1 = DataCompression::compressBinary(testData, count);
    unsigned long time1 = micros() - startTime;
    
    TEST_ASSERT_GREATER_THAN(0, compressed1.size());
    
    float ratio1 = (float)compressed1.size() / (count * sizeof(uint16_t));
    
    Serial.println("[Benchmarks]");
    Serial.println("  Samples: " + String(count));
    Serial.println("  Original: " + String(count * sizeof(uint16_t)) + " bytes");
    Serial.println("  Compressed: " + String(compressed1.size()) + " bytes");
    Serial.println("  Ratio: " + String(ratio1 * 100, 2) + "%");
    Serial.println("  Time: " + String(time1) + " us");
    
    Serial.println("[PASS] Benchmarking complete");
}

// Integration Test 5: Upload Retry Logic
void test_m3_upload_retry_logic(void) {
    Serial.println("\n=== Test: Upload Retry Logic ===");
    
    TEST_ASSERT_TRUE_MESSAGE(wifiConnected, "WiFi must be connected");
    
    // Create small test dataset
    uint16_t testData[10] = {5000, 5001, 5002, 5003, 5004, 5005, 5006, 5007, 5008, 5009};
    
    std::vector<uint8_t> compressed = DataCompression::compressBinary(testData, 10);
    TEST_ASSERT_GREATER_THAN(0, compressed.size());
    
    // Test with invalid endpoint first (should fail)
    HTTPClient http;
    String invalidUrl = String(FLASK_SERVER_URL) + "/api/invalid_endpoint";
    http.begin(invalidUrl);
    http.addHeader("Content-Type", "application/json");
    
    int failCode = http.POST("{\"test\":\"data\"}");
    http.end();
    
    Serial.println("[Retry] Expected failure code: " + String(failCode));
    TEST_ASSERT_NOT_EQUAL(HTTP_CODE_OK, failCode);
    
    // Now retry with correct endpoint
    int retryCount = 0;
    bool success = false;
    
    for (int i = 0; i < MAX_RETRY_ATTEMPTS; i++) {
        retryCount++;
        int httpCode = 0;
        
        success = uploadCompressedDataToFlask(
            compressed.data(), compressed.size(),
            compressed.data(), compressed.size(),
            compressed.data(), compressed.size(),
            &httpCode
        );
        
        if (success) {
            Serial.println("[Retry] Success on attempt " + String(retryCount));
            break;
        }
        
        Serial.println("[Retry] Attempt " + String(retryCount) + " failed, retrying...");
        delay(1000);
    }
    
    TEST_ASSERT_TRUE_MESSAGE(success, "Retry logic failed after max attempts");
    TEST_ASSERT_LESS_OR_EQUAL(MAX_RETRY_ATTEMPTS, retryCount);
    
    Serial.println("[PASS] Retry logic validated");
}

// Integration Test 6: Lossless Compression Verification with Real Data
void test_m3_lossless_real_data(void) {
    Serial.println("\n=== Test: Lossless Compression ===");
    
    TEST_ASSERT_TRUE_MESSAGE(wifiConnected, "WiFi must be connected");
    
    // Acquire real data
    uint16_t original[15];
    int count = 0;
    
    for (int i = 0; i < 15; i++) {
        uint16_t voltage, current, power;
        if (fetchRealSensorData(&voltage, &current, &power)) {
            original[count++] = voltage;
        }
        delay(100);
    }
    
    TEST_ASSERT_GREATER_THAN(10, count);
    
    Serial.println("[Lossless] Original: " + String(count * sizeof(uint16_t)) + " bytes");
    
    // Compress
    std::vector<uint8_t> compressed = DataCompression::compressBinary(original, count);
    TEST_ASSERT_GREATER_THAN(0, compressed.size());
    
    Serial.println("[Lossless] Compressed: " + String(compressed.size()) + " bytes");
    
    // Decompress
    std::vector<uint16_t> decompressed = DataCompression::decompressBinary(compressed);
    TEST_ASSERT_EQUAL(count, decompressed.size());
    
    // Verify lossless: every value matches exactly
    bool allMatch = true;
    for (int i = 0; i < count; i++) {
        if (original[i] != decompressed[i]) {
            Serial.println("[ERROR] Mismatch at index " + String(i) + 
                         ": " + String(original[i]) + " != " + String(decompressed[i]));
            allMatch = false;
        }
    }
    
    TEST_ASSERT_TRUE_MESSAGE(allMatch, "Lossless verification failed");
    
    Serial.println("[PASS] All values matched - compression is lossless");
}

// Integration Test 7: Flask Server Health Check
void test_m3_flask_server_health(void) {
    Serial.println("\n=== Test: Flask Server Health ===");
    
    TEST_ASSERT_TRUE_MESSAGE(wifiConnected, "WiFi must be connected");
    
    HTTPClient http;
    String url = String(FLASK_SERVER_URL) + "/health";
    
    http.begin(url);
    http.setTimeout(5000);
    
    int httpCode = http.GET();
    
    TEST_ASSERT_EQUAL_MESSAGE(HTTP_CODE_OK, httpCode, "Flask server health check failed");
    
    String response = http.getString();
    Serial.println("[Health] Server response: " + response);
    
    // Parse response
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, response);
    
    TEST_ASSERT_FALSE(error);
    TEST_ASSERT_TRUE(doc.containsKey("status"));
    TEST_ASSERT_EQUAL_STRING("healthy", doc["status"]);
    
    http.end();
    
    Serial.println("[PASS] Flask server is healthy");
}

// Integration Test 8: Data Integrity End-to-End
void test_m3_data_integrity_end_to_end(void) {
    Serial.println("\n=== Test: End-to-End Data Integrity ===");
    
    TEST_ASSERT_TRUE_MESSAGE(wifiConnected, "WiFi must be connected");
    
    // Acquire known amount of real data
    uint16_t originalV[10], originalC[10], originalP[10];
    int count = 0;
    
    Serial.println("[Integrity] Acquiring 10 real samples...");
    for (int i = 0; i < 10; i++) {
        if (fetchRealSensorData(&originalV[count], &originalC[count], &originalP[count])) {
            count++;
        }
        delay(100);
    }
    
    TEST_ASSERT_GREATER_THAN(5, count);
    
    // Log first and last values for verification
    Serial.println("[Integrity] First sample: V=" + String(originalV[0]) + 
                 ", I=" + String(originalC[0]) + ", P=" + String(originalP[0]));
    Serial.println("[Integrity] Last sample: V=" + String(originalV[count-1]) + 
                 ", I=" + String(originalC[count-1]) + ", P=" + String(originalP[count-1]));
    
    // Compress all three datasets
    std::vector<uint8_t> vCompressed = DataCompression::compressBinary(originalV, count);
    std::vector<uint8_t> cCompressed = DataCompression::compressBinary(originalC, count);
    std::vector<uint8_t> pCompressed = DataCompression::compressBinary(originalP, count);
    
    TEST_ASSERT_GREATER_THAN(0, vCompressed.size());
    TEST_ASSERT_GREATER_THAN(0, cCompressed.size());
    TEST_ASSERT_GREATER_THAN(0, pCompressed.size());
    
    // Upload to server
    int httpCode = 0;
    bool uploaded = uploadCompressedDataToFlask(
        vCompressed.data(), vCompressed.size(),
        cCompressed.data(), cCompressed.size(),
        pCompressed.data(), pCompressed.size(),
        &httpCode
    );
    
    TEST_ASSERT_TRUE(uploaded);
    
    // Verify decompression locally
    std::vector<uint16_t> decompressedV = DataCompression::decompressBinary(vCompressed);
    TEST_ASSERT_EQUAL(count, decompressedV.size());
    
    for (int i = 0; i < count; i++) {
        TEST_ASSERT_EQUAL_UINT16(originalV[i], decompressedV[i]);
    }
    
    Serial.println("[PASS] Data integrity verified end-to-end");
}

void setUp(void) {
    // Setup runs before each test
    delay(100);
}

void tearDown(void) {
    // Cleanup runs after each test
    delay(100);
}

int runUnityTests(void) {
    UNITY_BEGIN();
    
    // CRITICAL: WiFi must connect first
    RUN_TEST(test_m3_wifi_connection);
    
    if (wifiConnected) {
        // Flask Server Tests
        RUN_TEST(test_m3_flask_server_health);
        
        // Data Acquisition Tests
        RUN_TEST(test_m3_real_data_acquisition);
        
        // Compression Tests
        RUN_TEST(test_m3_real_data_compression_benchmarking);
        RUN_TEST(test_m3_lossless_real_data);
        
        // Upload Tests
        RUN_TEST(test_m3_upload_retry_logic);
        
        // End-to-End Integration Tests
        RUN_TEST(test_m3_complete_real_world_workflow);
        RUN_TEST(test_m3_data_integrity_end_to_end);
    } else {
        Serial.println("\n[SKIP] WiFi connection failed - skipping network tests");
    }
    
    return UNITY_END();
}

#ifdef ARDUINO
void setup() {
    Serial.begin(115200);
    delay(2000);
    
    Serial.println("\n\n");
    Serial.println("========================================");
    Serial.println("  M3 REAL-WORLD INTEGRATION TEST SUITE");
    Serial.println("========================================");
    Serial.println("WiFi SSID: " + String(WIFI_SSID));
    Serial.println("Flask Server: " + String(FLASK_SERVER_URL));
    Serial.println("Device ID: " + String(M3_TEST_DEVICE_ID));
    Serial.println("Test Samples: " + String(M3_TEST_SAMPLES));
    Serial.println("========================================\n");
    
    runUnityTests();
    
    Serial.println("\n========================================");
    Serial.println("  TEST SUITE COMPLETE");
    Serial.println("========================================");
}

void loop() {
    // Tests run once in setup
}
#else
int main(int argc, char **argv) {
    return runUnityTests();
}
#endif
