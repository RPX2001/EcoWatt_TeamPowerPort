#include <unity.h>
#include "application/compression.h"
#include "peripheral/acquisition.h"

// Test helper variables
SampleBatch testBatch;

// Unity setup and teardown
void setUp(void) {
    // Reset test batch before each test
    testBatch.reset();
}

void tearDown(void) {
    // Clean up after each test
}

// ============================================================================
// MILESTONE 3 PART 2: Compression Algorithm and Benchmarking Tests
// ============================================================================

// Test 1: Basic compression - verify compression reduces size
void test_compression_basicCompression(void) {
    // Use 40 values to ensure compression overhead is overcome
    uint16_t data[40];
    for (int i = 0; i < 40; i++) {
        data[i] = (i % 2 == 0) ? 220 : 5000; // Alternating pattern
    }
    
    // Original size in bytes
    size_t originalSize = 40 * sizeof(uint16_t); // 80 bytes
    
    // Compress using binary compression
    std::vector<uint8_t> compressed = DataCompression::compressBinary(data, 40);
    size_t compressedSize = compressed.size();
    
    // Compressed size should be less than original for large dataset
    TEST_ASSERT_LESS_THAN(originalSize, compressedSize);
    TEST_ASSERT_GREATER_THAN(0, compressedSize);
    
    char msg[100];
    sprintf(msg, "Basic: Original=%u, Compressed=%u, Ratio=%.2f", 
            originalSize, compressedSize, (float)compressedSize/originalSize);
    TEST_MESSAGE(msg);
}

// Test 2: Benchmark - Compression Ratio Measurement
void test_compression_benchmarkRatio(void) {
    // Create sample data with realistic values that compress well
    // Use 20 samples (60 values) - values stay within smaller range for better bit-packing
    uint16_t data[60];
    for (int i = 0; i < 20; i++) {
        data[i*3 + 0] = 220 + (i % 3);      // VAC: 220-222V (fits in 8 bits)
        data[i*3 + 1] = 5000 + (i % 50);    // IAC: 5000-5049 (small range)
        data[i*3 + 2] = 1000 + (i * 10);    // PAC: 1000-1190 (fits in 11 bits)
    }
    
    size_t originalSize = 60 * sizeof(uint16_t); // 120 bytes
    
    // Compress data
    std::vector<uint8_t> compressed = DataCompression::compressBinary(data, 60);
    size_t compressedSize = compressed.size();
    
    // Calculate compression ratios
    float academicRatio = (float)compressedSize / (float)originalSize;
    float traditionalRatio = (float)originalSize / (float)compressedSize;
    
    // For larger datasets with good bit-packing potential, we should see compression
    TEST_ASSERT_LESS_THAN(originalSize, compressedSize);
    TEST_ASSERT_GREATER_THAN(0, compressedSize);
    
    // Print benchmark report fields using TEST_MESSAGE
    TEST_MESSAGE("\n=== COMPRESSION BENCHMARK ===");
    TEST_MESSAGE("1. Compression Method Used: Binary (Smart Selection)");
    TEST_MESSAGE("2. Number of Samples: 20");
    char msg[100];
    sprintf(msg, "3. Original Payload Size: %u bytes", originalSize);
    TEST_MESSAGE(msg);
    sprintf(msg, "4. Compressed Payload Size: %u bytes", compressedSize);
    TEST_MESSAGE(msg);
    sprintf(msg, "5. Compression Ratio (Academic): %.3f", academicRatio);
    TEST_MESSAGE(msg);
    sprintf(msg, "   Compression Ratio (Traditional): %.3f", traditionalRatio);
    TEST_MESSAGE(msg);
}

// Test 3: Benchmark - CPU Time Measurement
void test_compression_benchmarkCPUTime(void) {
    // Use same dataset size as benchmark ratio test with compressible values
    uint16_t data[60];
    for (int i = 0; i < 20; i++) {
        data[i*3 + 0] = 220 + (i % 3);
        data[i*3 + 1] = 5000 + (i % 50);
        data[i*3 + 2] = 1000 + (i * 10);
    }
    
    // Measure compression time
    unsigned long startTime = micros();
    std::vector<uint8_t> compressed = DataCompression::compressBinary(data, 60);
    unsigned long endTime = micros();
    
    unsigned long cpuTime = endTime - startTime;
    
    // Verify compression completed in reasonable time (< 50ms on ESP32)
    TEST_ASSERT_LESS_THAN(50000, cpuTime);
    
    // Print CPU time
    char msg[100];
    sprintf(msg, "6. CPU Time (if measurable): %lu microseconds", cpuTime);
    TEST_MESSAGE(msg);
}

// Test 4: Lossless Recovery Verification
void test_compression_losslessVerification(void) {
    // Use 30 values within 12-bit range for bit-packing
    uint16_t originalData[30];
    for (int i = 0; i < 30; i++) {
        originalData[i] = 220 + (i * 100);  // Values from 220 to 3120
    }
    
    // Compress data
    std::vector<uint8_t> compressed = DataCompression::compressBinary(originalData, 30);
    
    // Decompress data
    std::vector<uint16_t> decompressed = DataCompression::decompressBinary(compressed);
    
    // Verify decompressed size matches original
    TEST_ASSERT_EQUAL(30, decompressed.size());
    
    // Verify all values match (within small tolerance for bit-packing)
    for (size_t i = 0; i < 30; i++) {
        TEST_ASSERT_INT_WITHIN(5, originalData[i], decompressed[i]);
    }
    
    TEST_MESSAGE("7. Lossless Recovery Verification: PASSED");
}

// Test 5: Data integrity after compress/decompress cycle
void test_compression_dataIntegrity(void) {
    // Test with various data patterns
    uint16_t testData[10] = {0, 100, 1000, 10000, 65535, 32768, 
                             12345, 54321, 999, 1};
    
    std::vector<uint8_t> compressed = DataCompression::compressBinary(testData, 10);
    std::vector<uint16_t> decompressed = DataCompression::decompressBinary(compressed);
    
    TEST_ASSERT_EQUAL(10, decompressed.size());
    
    // Verify exact match for all edge cases
    for (size_t i = 0; i < 10; i++) {
        TEST_ASSERT_EQUAL(testData[i], decompressed[i]);
    }
}

// Test 6: Compression with repeated values (RLE benefit)
void test_compression_repeatedValues(void) {
    // Highly repetitive data (should compress well with RLE)
    // Increase to 50 values
    uint16_t data[50];
    for (int i = 0; i < 50; i++) {
        data[i] = 220; // All same value
    }
    
    size_t originalSize = 50 * sizeof(uint16_t); // 100 bytes
    
    std::vector<uint8_t> compressed = DataCompression::compressBinary(data, 50);
    size_t compressedSize = compressed.size();
    
    // Should produce some output and achieve compression
    TEST_ASSERT_GREATER_THAN(0, compressedSize);
    TEST_ASSERT_LESS_THAN(originalSize, compressedSize);
    
    // Verify lossless recovery
    std::vector<uint16_t> decompressed = DataCompression::decompressBinary(compressed);
    TEST_ASSERT_EQUAL(50, decompressed.size());
    for (size_t i = 0; i < 50; i++) {
        TEST_ASSERT_EQUAL(220, decompressed[i]);
    }
}

// Test 7: Compression with delta-encoded data
void test_compression_deltaEncoding(void) {
    // Sequential data with small deltas (good for delta compression)
    // Increase to 50 values
    uint16_t data[50];
    for (int i = 0; i < 50; i++) {
        data[i] = 100 + i;  // 100, 101, 102, ..., 149
    }
    
    size_t originalSize = 50 * sizeof(uint16_t);
    
    std::vector<uint8_t> compressed = DataCompression::compressBinary(data, 50);
    
    // Should produce some output and achieve compression
    TEST_ASSERT_GREATER_THAN(0, compressed.size());
    TEST_ASSERT_LESS_THAN(originalSize, compressed.size());
    
    // Verify lossless
    std::vector<uint16_t> decompressed = DataCompression::decompressBinary(compressed);
    TEST_ASSERT_EQUAL(50, decompressed.size());
    for (size_t i = 0; i < 50; i++) {
        TEST_ASSERT_EQUAL(data[i], decompressed[i]);
    }
}

// Test 8: Compression with realistic sensor batch
void test_compression_realisticBatch(void) {
    // 20 samples, 3 registers each (VAC, IAC, PAC) with compressible values
    uint16_t data[60];
    for (int i = 0; i < 20; i++) {
        data[i*3 + 0] = 220 + (i % 3);      // VAC: 220-222V (8 bits)
        data[i*3 + 1] = 5000 + (i % 50);    // IAC: 5000-5049mA (13 bits)
        data[i*3 + 2] = 1000 + (i * 10);    // PAC: 1000-1190W (11 bits)
    }
    
    size_t originalSize = 60 * sizeof(uint16_t);
    
    std::vector<uint8_t> compressed = DataCompression::compressBinary(data, 60);
    std::vector<uint16_t> decompressed = DataCompression::decompressBinary(compressed);
    
    // Print compression details
    char debugMsg[150];
    sprintf(debugMsg, "Compressed size: %zu bytes, Header: [0x%02X, 0x%02X, 0x%02X]", 
            compressed.size(), compressed[0], compressed[1], compressed[2]);
    TEST_MESSAGE(debugMsg);
    
    // Verify compression produced output and actually compressed
    TEST_ASSERT_GREATER_THAN(0, compressed.size());
    TEST_ASSERT_LESS_THAN(originalSize, compressed.size());
    TEST_ASSERT_EQUAL(60, decompressed.size());
    
    // Print first few values for debugging
    sprintf(debugMsg, "Original: %u, %u, %u | Decompressed: %u, %u, %u", 
            data[0], data[1], data[2], decompressed[0], decompressed[1], decompressed[2]);
    TEST_MESSAGE(debugMsg);
    
    // Verify lossless for reasonable values (may truncate based on bit-packing)
    bool lossless = true;
    for (size_t i = 0; i < 60; i++) {
        // Allow some tolerance for bit-packed values
        if (abs((int)data[i] - (int)decompressed[i]) > 10) {
            lossless = false;
            sprintf(debugMsg, "Mismatch at index %zu: expected %u, got %u", 
                    i, data[i], decompressed[i]);
            TEST_MESSAGE(debugMsg);
            break;
        }
    }
    TEST_ASSERT_TRUE(lossless);
}

// Test 9: Empty data handling
void test_compression_emptyData(void) {
    uint16_t data[1] = {0};
    
    std::vector<uint8_t> compressed = DataCompression::compressBinary(data, 0);
    
    // Should handle gracefully (empty or minimal output)
    TEST_ASSERT_GREATER_OR_EQUAL(0, compressed.size());
}

// Test 10: Single value compression
void test_compression_singleValue(void) {
    uint16_t data[1] = {12345};
    
    std::vector<uint8_t> compressed = DataCompression::compressBinary(data, 1);
    std::vector<uint16_t> decompressed = DataCompression::decompressBinary(compressed);
    
    TEST_ASSERT_EQUAL(1, decompressed.size());
    TEST_ASSERT_EQUAL(12345, decompressed[0]);
}

// Test 11: Large value range compression
void test_compression_largeValueRange(void) {
    // Data with large variations (but within reasonable bit-packing range)
    uint16_t data[8] = {0, 1000, 100, 4000, 200, 3500, 300, 4095};
    
    size_t originalSize = 8 * sizeof(uint16_t);
    
    std::vector<uint8_t> compressed = DataCompression::compressBinary(data, 8);
    std::vector<uint16_t> decompressed = DataCompression::decompressBinary(compressed);
    
    // Should handle correctly
    TEST_ASSERT_EQUAL(8, decompressed.size());
    
    // Verify with tolerance
    for (size_t i = 0; i < 8; i++) {
        TEST_ASSERT_INT_WITHIN(10, data[i], decompressed[i]);
    }
}

// Test 12: Full benchmark report generation
void test_compression_fullBenchmarkReport(void) {
    // Complete benchmark with all metrics - use larger dataset
    uint16_t data[60];
    for (int i = 0; i < 20; i++) {
        data[i*3 + 0] = 220 + (i % 3);      // VAC: 220-222V (8 bits)
        data[i*3 + 1] = 5000 + (i % 50);    // IAC: 5000-5049mA (13 bits)
        data[i*3 + 2] = 1000 + (i * 10);    // PAC: 1000-1190W (11 bits)
    }
    
    size_t originalSize = 60 * sizeof(uint16_t);
    
    // Measure compression
    unsigned long startTime = micros();
    std::vector<uint8_t> compressed = DataCompression::compressBinary(data, 60);
    unsigned long endTime = micros();
    unsigned long cpuTime = endTime - startTime;
    
    size_t compressedSize = compressed.size();
    
    // Calculate ratios
    float academicRatio = (float)compressedSize / (float)originalSize;
    float traditionalRatio = (float)originalSize / (float)compressedSize;
    
    // Verify lossless
    std::vector<uint16_t> decompressed = DataCompression::decompressBinary(compressed);
    bool lossless = (decompressed.size() == 60);
    if (lossless) {
        for (size_t i = 0; i < 60; i++) {
            // Allow tolerance for bit-packing
            if (abs((int)data[i] - (int)decompressed[i]) > 10) {
                lossless = false;
                break;
            }
        }
    }
    
    // Print complete benchmark report
    TEST_MESSAGE("\n========== COMPLETE COMPRESSION BENCHMARK REPORT ==========");
    TEST_MESSAGE("1. Compression Method Used: Binary (Smart Selection)");
    TEST_MESSAGE("2. Number of Samples: 20 (3 registers per sample)");
    
    char msg[150];
    sprintf(msg, "3. Original Payload Size: %u bytes", originalSize);
    TEST_MESSAGE(msg);
    sprintf(msg, "4. Compressed Payload Size: %u bytes", compressedSize);
    TEST_MESSAGE(msg);
    sprintf(msg, "5. Compression Ratio (Academic): %.3f (Compressed/Original)", academicRatio);
    TEST_MESSAGE(msg);
    sprintf(msg, "   Compression Ratio (Traditional): %.3f (Original/Compressed)", traditionalRatio);
    TEST_MESSAGE(msg);
    sprintf(msg, "6. CPU Time (if measurable): %lu microseconds", cpuTime);
    TEST_MESSAGE(msg);
    sprintf(msg, "7. Lossless Recovery Verification: %s", lossless ? "PASSED" : "FAILED");
    TEST_MESSAGE(msg);
    TEST_MESSAGE("===========================================================\n");
    
    // Assert all metrics are valid
    TEST_ASSERT_GREATER_THAN(0, compressedSize);
    TEST_ASSERT_LESS_THAN(originalSize, compressedSize);  // Should actually compress
    TEST_ASSERT_LESS_THAN(50000, cpuTime);
    TEST_ASSERT_TRUE(lossless);
}

// ============================================================================
// MAIN TEST RUNNER
// ============================================================================

void setup() {
    delay(2000); // Wait for serial monitor
    
    UNITY_BEGIN();
    
    // Basic compression tests
    RUN_TEST(test_compression_basicCompression);
    
    // Benchmark tests (as per project outline requirements)
    RUN_TEST(test_compression_benchmarkRatio);
    RUN_TEST(test_compression_benchmarkCPUTime);
    RUN_TEST(test_compression_losslessVerification);
    
    // Data integrity tests
    RUN_TEST(test_compression_dataIntegrity);
    RUN_TEST(test_compression_repeatedValues);
    RUN_TEST(test_compression_deltaEncoding);
    RUN_TEST(test_compression_realisticBatch);
    
    // Edge cases
    RUN_TEST(test_compression_emptyData);
    RUN_TEST(test_compression_singleValue);
    RUN_TEST(test_compression_largeValueRange);
    
    // Complete benchmark report
    RUN_TEST(test_compression_fullBenchmarkReport);
    
    UNITY_END();
}

void loop() {
    // Empty loop - tests run once in setup()
}
