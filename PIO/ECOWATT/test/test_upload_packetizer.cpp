/**
 * @file test_upload_packetizer.cpp
 * @brief Unit tests for M3 Part 3 - Upload Packetizer
 * 
 * Tests for the DataUploader class which manages ring buffer operations,
 * queue management, and payload preparation for cloud upload.
 * 
 * @author Team PowerPort
 * @date 2025-10-23
 */

#include <unity.h>
#include "application/data_uploader.h"
#include "application/compression.h"
#include "peripheral/acquisition.h"

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

/**
 * @brief Create a sample compressed data entry for testing
 */
SmartCompressedData createSampleCompressedData(unsigned long timestamp, size_t dataSize) {
    SmartCompressedData data;
    
    data.timestamp = timestamp;
    strncpy(data.compressionMethod, "BIT_PACKED", sizeof(data.compressionMethod) - 1);
    data.compressionMethod[sizeof(data.compressionMethod) - 1] = '\0';
    
    data.registerCount = 3;
    data.registers[0] = REG_VAC1;
    data.registers[1] = REG_IAC1;
    data.registers[2] = REG_PAC;
    
    // Create sample binary data
    data.binaryData.clear();
    for (size_t i = 0; i < dataSize; i++) {
        data.binaryData.push_back(i & 0xFF);
    }
    
    data.originalSize = dataSize * 2;  // Simulating uint16_t data
    data.academicRatio = (float)dataSize / (float)(dataSize * 2);
    data.traditionalRatio = (float)(dataSize * 2) / (float)dataSize;
    data.compressionTime = 100 + (timestamp % 50);
    data.losslessVerified = true;
    
    return data;
}

// ============================================================================
// TEST CASES
// ============================================================================

// Test 1: Initialization
void test_uploader_initialization(void) {
    DataUploader::init("http://test.server.com/upload", "TEST_DEVICE_001");
    
    TEST_ASSERT_EQUAL_STRING("TEST_DEVICE_001", DataUploader::getDeviceID());
    TEST_ASSERT_TRUE(DataUploader::isQueueEmpty());
    TEST_ASSERT_EQUAL(0, DataUploader::getQueueSize());
}

// Test 2: Add Single Entry to Queue
void test_uploader_addSingleEntry(void) {
    DataUploader::clearQueue();
    
    SmartCompressedData data = createSampleCompressedData(1000, 50);
    
    bool success = DataUploader::addToQueue(data);
    
    TEST_ASSERT_TRUE(success);
    TEST_ASSERT_EQUAL(1, DataUploader::getQueueSize());
    TEST_ASSERT_FALSE(DataUploader::isQueueEmpty());
}

// Test 3: Add Multiple Entries
void test_uploader_addMultipleEntries(void) {
    DataUploader::clearQueue();
    
    for (int i = 0; i < 5; i++) {
        SmartCompressedData data = createSampleCompressedData(1000 + i * 100, 50 + i * 5);
        bool success = DataUploader::addToQueue(data);
        TEST_ASSERT_TRUE(success);
    }
    
    TEST_ASSERT_EQUAL(5, DataUploader::getQueueSize());
    TEST_ASSERT_FALSE(DataUploader::isQueueEmpty());
    TEST_ASSERT_FALSE(DataUploader::isQueueFull());
}

// Test 4: Queue Full Detection
void test_uploader_queueFullDetection(void) {
    DataUploader::clearQueue();
    
    // Fill buffer to capacity (20 entries)
    for (int i = 0; i < 20; i++) {
        SmartCompressedData data = createSampleCompressedData(1000 + i * 100, 50);
        bool success = DataUploader::addToQueue(data);
        TEST_ASSERT_TRUE(success);
    }
    
    TEST_ASSERT_EQUAL(20, DataUploader::getQueueSize());
    TEST_ASSERT_TRUE(DataUploader::isQueueFull());
    
    // Try to add one more (should fail)
    SmartCompressedData extraData = createSampleCompressedData(5000, 50);
    bool shouldFail = DataUploader::addToQueue(extraData);
    
    TEST_ASSERT_FALSE(shouldFail);
    TEST_ASSERT_EQUAL(20, DataUploader::getQueueSize());  // Should still be 20
}

// Test 5: Clear Queue
void test_uploader_clearQueue(void) {
    DataUploader::clearQueue();
    
    // Add some entries
    for (int i = 0; i < 10; i++) {
        SmartCompressedData data = createSampleCompressedData(1000 + i * 100, 50);
        DataUploader::addToQueue(data);
    }
    
    TEST_ASSERT_EQUAL(10, DataUploader::getQueueSize());
    
    // Clear the queue
    DataUploader::clearQueue();
    
    TEST_ASSERT_EQUAL(0, DataUploader::getQueueSize());
    TEST_ASSERT_TRUE(DataUploader::isQueueEmpty());
}

// Test 6: Queue Size Tracking
void test_uploader_queueSizeTracking(void) {
    DataUploader::clearQueue();
    
    TEST_ASSERT_EQUAL(0, DataUploader::getQueueSize());
    
    // Add entries one by one and verify size
    for (int i = 1; i <= 15; i++) {
        SmartCompressedData data = createSampleCompressedData(1000 + i * 100, 50);
        DataUploader::addToQueue(data);
        TEST_ASSERT_EQUAL(i, DataUploader::getQueueSize());
    }
}

// Test 7: Upload Statistics Initialization
void test_uploader_statsInitialization(void) {
    DataUploader::resetStats();
    
    unsigned long uploads, failures;
    size_t bytesUploaded;
    
    DataUploader::getUploadStats(uploads, failures, bytesUploaded);
    
    TEST_ASSERT_EQUAL(0, uploads);
    TEST_ASSERT_EQUAL(0, failures);
    TEST_ASSERT_EQUAL(0, bytesUploaded);
}

// Test 8: Device ID Configuration
void test_uploader_deviceIDConfiguration(void) {
    DataUploader::init("http://server.com/api/upload", "ESP32_DEVICE_XYZ");
    
    const char* deviceID = DataUploader::getDeviceID();
    
    TEST_ASSERT_EQUAL_STRING("ESP32_DEVICE_XYZ", deviceID);
}

// Test 9: URL Configuration
void test_uploader_urlConfiguration(void) {
    DataUploader::setUploadURL("http://newserver.com/api/v2/upload");
    
    // We can't directly test the URL, but we can verify init works
    DataUploader::init("http://testserver.com/upload", "TEST_DEV");
    
    // Verify initialization didn't crash and device ID is set
    TEST_ASSERT_EQUAL_STRING("TEST_DEV", DataUploader::getDeviceID());
}

// Test 10: Data Integrity in Queue
void test_uploader_dataIntegrityInQueue(void) {
    DataUploader::clearQueue();
    
    // Add data with specific characteristics
    SmartCompressedData data1 = createSampleCompressedData(12345, 60);
    strncpy(data1.compressionMethod, "BIT_PACKED", sizeof(data1.compressionMethod) - 1);
    data1.originalSize = 120;
    
    SmartCompressedData data2 = createSampleCompressedData(67890, 80);
    strncpy(data2.compressionMethod, "RAW_BINARY", sizeof(data2.compressionMethod) - 1);
    data2.originalSize = 160;
    
    DataUploader::addToQueue(data1);
    DataUploader::addToQueue(data2);
    
    TEST_ASSERT_EQUAL(2, DataUploader::getQueueSize());
    
    // Data should be preserved in queue (we can't directly inspect,
    // but verifying size remains correct after operations)
    TEST_ASSERT_EQUAL(2, DataUploader::getQueueSize());
}

// ============================================================================
// MAIN TEST RUNNER
// ============================================================================

void setup() {
    delay(2000);  // Give time for serial connection
    
    UNITY_BEGIN();
    
    // Run all tests
    RUN_TEST(test_uploader_initialization);
    RUN_TEST(test_uploader_addSingleEntry);
    RUN_TEST(test_uploader_addMultipleEntries);
    RUN_TEST(test_uploader_queueFullDetection);
    RUN_TEST(test_uploader_clearQueue);
    RUN_TEST(test_uploader_queueSizeTracking);
    RUN_TEST(test_uploader_statsInitialization);
    RUN_TEST(test_uploader_deviceIDConfiguration);
    RUN_TEST(test_uploader_urlConfiguration);
    RUN_TEST(test_uploader_dataIntegrityInQueue);
    
    UNITY_END();
}

void loop() {
    // Nothing to do here
}
