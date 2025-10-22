/**
 * @file test_m3_integration.cpp
 * @brief M3 Real-World Integration Tests - End-to-End with WiFi and Flask Server
 * 
 * Tests the complete workflow for Milestone 3:
 * 1. WiFi connection establishment
 * 2. Real data acquisition from inverter simulator endpoint
 * 3. Buffering samples for test interval
 * 4. Compression with benchmarking metrics
 * 5. HTTP POST to Flask server
 * 6. Server response validation
 * 7. Retry logic on failures
 */

#include <unity.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "../../src/application/protocol_adapter.h"
#include "../../src/application/data_acquisition_scheduler.h"
#include "../../src/application/buffer.h"
#include "../../src/application/compression.h"
#include "../../src/application/upload_packetizer.h"
#include <string.h>

// WiFi Configuration
#define WIFI_SSID "Galaxy A32B46A"
#define WIFI_PASSWORD "aubz5724"
#define WIFI_TIMEOUT_MS 20000

// Real Inverter API Configuration (from EN4440 API Documentation)
#define INVERTER_API_BASE_URL "http://20.15.114.131:8080"
#define INVERTER_READ_ENDPOINT "/api/inverter/read"
#define INVERTER_API_KEY "NjhhZWIwNDU1ZDdmMzg3MzNiMTQ5YTFmOjY4YWViMDQ1NWQ3ZjM4NzMzYjE0OWExNQ=="  // TODO: Replace with actual API key from spreadsheet

// Modbus Configuration
#define MODBUS_SLAVE_ADDRESS 0x11  // Slave ID = 17
#define MODBUS_FUNC_READ 0x03      // Read Holding Registers
#define MODBUS_START_ADDR_VAC1 0x0000   // Vac1/L1 Phase voltage
#define MODBUS_START_ADDR_IAC1 0x0001   // Iac1/L1 Phase current
#define MODBUS_START_ADDR_PAC 0x0009    // Pac L/Inverter output power

// Flask Server Configuration
#define FLASK_SERVER_IP "192.168.242.249"
#define FLASK_SERVER_PORT 5000
#define FLASK_BASE_URL "http://192.168.242.249:5000"
#define AGGREGATED_DATA_ENDPOINT "/api/aggregated_data"

// Test configuration
#define M3_TEST_DEVICE_ID "TEST_ESP32_INTEGRATION"
#define M3_TEST_SAMPLES 60  // 1 minute of data for faster testing (normally 900 for 15 min)
#define M3_EXPECTED_COMPRESSION_RATIO 0.5
#define MAX_RETRY_ATTEMPTS 3

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
        Serial.print(".");
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
    String url = String(INVERTER_API_BASE_URL) + INVERTER_READ_ENDPOINT;
    
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

static bool fillBufferWithRealData(int sampleCount) {
    Serial.println("[Buffer] Acquiring " + String(sampleCount) + " real samples...");
    int successCount = 0;
    
    for (int i = 0; i < sampleCount; i++) {
        uint16_t voltage, current, power;
        
        if (fetchRealSensorData(&voltage, &current, &power)) {
            addSample(voltage, current, power);
            successCount++;
            
            if (i % 10 == 0) {
                Serial.println("[Buffer] Sample " + String(i) + ": V=" + String(voltage) + 
                             ", I=" + String(current) + ", P=" + String(power));
            }
        } else {
            Serial.println("[Buffer] Failed to fetch sample " + String(i));
        }
        
        delay(100);  // Small delay between requests
    }
    
    Serial.println("[Buffer] Acquired " + String(successCount) + "/" + String(sampleCount) + " samples");
    return successCount >= (sampleCount * 0.8);  // Allow 20% failure rate
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
    String url = String(FLASK_BASE_URL) + AGGREGATED_DATA_ENDPOINT;
    
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(10000);
    
    // Build JSON payload
    StaticJsonDocument<4096> doc;
    doc["device_id"] = M3_TEST_DEVICE_ID;
    doc["timestamp"] = millis();
    
    // Encode compressed data as base64 or hex
    JsonArray vArray = doc.createNestedArray("voltage_compressed");
    for (size_t i = 0; i < vSize; i++) {
        vArray.add(voltageData[i]);
    }
    
    JsonArray cArray = doc.createNestedArray("current_compressed");
    for (size_t i = 0; i < cSize; i++) {
        cArray.add(currentData[i]);
    }
    
    JsonArray pArray = doc.createNestedArray("power_compressed");
    for (size_t i = 0; i < pSize; i++) {
        pArray.add(powerData[i]);
    }
    
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
    
    TEST_ASSERT_TRUE_MESSAGE(success, "Failed to fetch sensor data from Flask");
    TEST_ASSERT_GREATER_THAN(0, voltage);
    TEST_ASSERT_GREATER_THAN(0, current);
    TEST_ASSERT_GREATER_THAN(0, power);
    
    Serial.println("[PASS] Real data acquired: V=" + String(voltage) + 
                 ", I=" + String(current) + ", P=" + String(power));
}

// Integration Test 3: Complete M3 Workflow - Acquisition to Upload
void test_m3_complete_real_world_workflow(void) {
    Serial.println("\n=== Test: Complete M3 Workflow ===");
    
    // Step 1: Ensure WiFi is connected
    TEST_ASSERT_TRUE_MESSAGE(wifiConnected, "WiFi must be connected");
    
    // Step 2: Initialize all M3 components
    initBuffer();
    initCompression();
    Serial.println("[Init] All components initialized");
    
    // Step 3: Acquire real sensor data
    bool dataAcquired = fillBufferWithRealData(M3_TEST_SAMPLES);
    TEST_ASSERT_TRUE_MESSAGE(dataAcquired, "Failed to acquire sufficient real data");
    
    int sampleCount = getBufferSampleCount();
    Serial.println("[Buffer] Acquired " + String(sampleCount) + " samples");
    TEST_ASSERT_GREATER_THAN(M3_TEST_SAMPLES * 0.8, sampleCount);
    
    // Step 4: Get buffer data for compression
    uint16_t* voltages = (uint16_t*)malloc(sampleCount * sizeof(uint16_t));
    uint16_t* currents = (uint16_t*)malloc(sampleCount * sizeof(uint16_t));
    uint16_t* powers = (uint16_t*)malloc(sampleCount * sizeof(uint16_t));
    
    TEST_ASSERT_NOT_NULL(voltages);
    TEST_ASSERT_NOT_NULL(currents);
    TEST_ASSERT_NOT_NULL(powers);
    
    getAllSamples(voltages, currents, powers);
    
    // Step 5: Compress data and benchmark
    Serial.println("[Compression] Compressing data...");
    CompressionResult voltageResult = compressArray(voltages, sampleCount);
    CompressionResult currentResult = compressArray(currents, sampleCount);
    CompressionResult powerResult = compressArray(powers, sampleCount);
    
    TEST_ASSERT_NOT_NULL(voltageResult.data);
    TEST_ASSERT_NOT_NULL(currentResult.data);
    TEST_ASSERT_NOT_NULL(powerResult.data);
    
    size_t originalSize = sampleCount * sizeof(uint16_t);
    float vRatio = (float)voltageResult.size / originalSize;
    float cRatio = (float)currentResult.size / originalSize;
    float pRatio = (float)powerResult.size / originalSize;
    
    Serial.println("[Compression] Voltage: " + String(originalSize) + " -> " + 
                 String(voltageResult.size) + " (" + String(vRatio * 100, 2) + "%)");
    Serial.println("[Compression] Current: " + String(originalSize) + " -> " + 
                 String(currentResult.size) + " (" + String(cRatio * 100, 2) + "%)");
    Serial.println("[Compression] Power: " + String(originalSize) + " -> " + 
                 String(powerResult.size) + " (" + String(pRatio * 100, 2) + "%)");
    
    // Step 6: Upload to Flask server
    int httpCode = 0;
    bool uploaded = uploadCompressedDataToFlask(
        voltageResult.data, voltageResult.size,
        currentResult.data, currentResult.size,
        powerResult.data, powerResult.size,
        &httpCode
    );
    
    TEST_ASSERT_TRUE_MESSAGE(uploaded, "Upload to Flask server failed");
    TEST_ASSERT_TRUE(httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED);
    
    Serial.println("[PASS] Complete workflow succeeded!");
    
    // Cleanup
    free(voltages);
    free(currents);
    free(powers);
    free(voltageResult.data);
    free(currentResult.data);
    free(powerResult.data);
    clearBuffer();
}

// Integration Test 4: Compression Benchmarking with Real Data
void test_m3_real_data_compression_benchmarking(void) {
    Serial.println("\n=== Test: Compression Benchmarking ===");
    
    initCompression();
    resetCompressionStatistics();
    
    // Acquire small batch of real data
    initBuffer();
    bool acquired = fillBufferWithRealData(20);
    TEST_ASSERT_TRUE(acquired);
    
    uint16_t testData[20];
    uint16_t dummy[20];
    getAllSamples(testData, dummy, dummy);
    
    // Compress and gather benchmark metrics
    CompressionResult result = compressArray(testData, 20);
    CompressionStatistics stats = getCompressionStatistics();
    
    // Verify all 7 benchmark metrics are captured
    TEST_ASSERT_GREATER_THAN(0, stats.totalCompressedSize);
    TEST_ASSERT_GREATER_THAN(0, stats.totalOriginalSize);
    TEST_ASSERT_GREATER_THAN(0.0f, stats.averageCompressionRatio);
    TEST_ASSERT_GREATER_THAN(0, stats.totalCompressionTime);
    TEST_ASSERT_GREATER_THAN(0, stats.compressionCount);
    TEST_ASSERT_LESS_THAN(1.0f, stats.bestCompressionRatio);
    TEST_ASSERT_GREATER_THAN(0.0f, stats.worstCompressionRatio);
    
    Serial.println("[Benchmarks]");
    Serial.println("  Original: " + String(stats.totalOriginalSize) + " bytes");
    Serial.println("  Compressed: " + String(stats.totalCompressedSize) + " bytes");
    Serial.println("  Avg Ratio: " + String(stats.averageCompressionRatio * 100, 2) + "%");
    Serial.println("  Time: " + String(stats.totalCompressionTime) + " ms");
    Serial.println("  Best: " + String(stats.bestCompressionRatio * 100, 2) + "%");
    Serial.println("  Worst: " + String(stats.worstCompressionRatio * 100, 2) + "%");
    
    Serial.println("[PASS] Benchmarking complete");
    
    free(result.data);
    clearBuffer();
}

// Integration Test 5: Upload Retry Logic
void test_m3_upload_retry_logic(void) {
    Serial.println("\n=== Test: Upload Retry Logic ===");
    
    TEST_ASSERT_TRUE_MESSAGE(wifiConnected, "WiFi must be connected");
    
    initCompression();
    
    // Create small test dataset
    uint16_t testData[10] = {5000, 5001, 5002, 5003, 5004, 5005, 5006, 5007, 5008, 5009};
    
    CompressionResult result = compressArray(testData, 10);
    TEST_ASSERT_NOT_NULL(result.data);
    
    // Test with invalid endpoint first (should fail)
    HTTPClient http;
    String invalidUrl = String(FLASK_BASE_URL) + "/api/invalid_endpoint";
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
            result.data, result.size,
            result.data, result.size,
            result.data, result.size,
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
    
    free(result.data);
}

// Integration Test 6: Lossless Compression Verification with Real Data
void test_m3_lossless_real_data(void) {
    Serial.println("\n=== Test: Lossless Compression ===");
    
    TEST_ASSERT_TRUE_MESSAGE(wifiConnected, "WiFi must be connected");
    
    initCompression();
    initBuffer();
    
    // Acquire real data
    bool acquired = fillBufferWithRealData(30);
    TEST_ASSERT_TRUE(acquired);
    
    uint16_t original[30];
    uint16_t dummy[30];
    getAllSamples(original, dummy, dummy);
    
    // Compress
    CompressionResult result = compressArray(original, 30);
    TEST_ASSERT_NOT_NULL(result.data);
    
    Serial.println("[Lossless] Original: " + String(30 * sizeof(uint16_t)) + " bytes");
    Serial.println("[Lossless] Compressed: " + String(result.size) + " bytes");
    
    // Decompress
    uint16_t decompressed[30];
    bool success = decompressArray(result.data, result.size, decompressed, 30);
    TEST_ASSERT_TRUE(success);
    
    // Verify lossless: every value matches exactly
    bool allMatch = true;
    for (int i = 0; i < 30; i++) {
        if (original[i] != decompressed[i]) {
            Serial.println("[ERROR] Mismatch at index " + String(i) + 
                         ": " + String(original[i]) + " != " + String(decompressed[i]));
            allMatch = false;
        }
    }
    
    TEST_ASSERT_TRUE_MESSAGE(allMatch, "Lossless verification failed");
    
    Serial.println("[PASS] All values matched - compression is lossless");
    
    free(result.data);
    clearBuffer();
}

// Integration Test 7: Flask Server Health Check
void test_m3_flask_server_health(void) {
    Serial.println("\n=== Test: Flask Server Health ===");
    
    TEST_ASSERT_TRUE_MESSAGE(wifiConnected, "WiFi must be connected");
    
    HTTPClient http;
    String url = String(FLASK_BASE_URL) + "/health";
    
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
    
    initBuffer();
    initCompression();
    
    // Acquire known amount of real data
    Serial.println("[Integrity] Acquiring 15 real samples...");
    bool acquired = fillBufferWithRealData(15);
    TEST_ASSERT_TRUE(acquired);
    
    uint16_t originalV[15], originalC[15], originalP[15];
    getAllSamples(originalV, originalC, originalP);
    
    // Log first and last values for verification
    Serial.println("[Integrity] First sample: V=" + String(originalV[0]) + 
                 ", I=" + String(originalC[0]) + ", P=" + String(originalP[0]));
    Serial.println("[Integrity] Last sample: V=" + String(originalV[14]) + 
                 ", I=" + String(originalC[14]) + ", P=" + String(originalP[14]));
    
    // Compress
    CompressionResult vResult = compressArray(originalV, 15);
    CompressionResult cResult = compressArray(originalC, 15);
    CompressionResult pResult = compressArray(originalP, 15);
    
    TEST_ASSERT_NOT_NULL(vResult.data);
    TEST_ASSERT_NOT_NULL(cResult.data);
    TEST_ASSERT_NOT_NULL(pResult.data);
    
    // Upload to server
    int httpCode = 0;
    bool uploaded = uploadCompressedDataToFlask(
        vResult.data, vResult.size,
        cResult.data, cResult.size,
        pResult.data, pResult.size,
        &httpCode
    );
    
    TEST_ASSERT_TRUE(uploaded);
    
    // Verify decompression locally
    uint16_t decompressedV[15];
    bool success = decompressArray(vResult.data, vResult.size, decompressedV, 15);
    TEST_ASSERT_TRUE(success);
    TEST_ASSERT_EQUAL_UINT16_ARRAY(originalV, decompressedV, 15);
    
    Serial.println("[PASS] Data integrity verified end-to-end");
    
    free(vResult.data);
    free(cResult.data);
    free(pResult.data);
    clearBuffer();
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
    Serial.println("Flask Server: " + String(FLASK_BASE_URL));
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
