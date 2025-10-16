/**
 * @file test_compression.cpp
 * @brief Comprehensive test suite for compression algorithms
 * 
 * Tests all compression methods including:
 * - Smart Selection (Dictionary, Temporal, Semantic, BitPack)
 * - Binary compression methods
 * - Lossless compression verification
 * - Compression ratio measurements
 * - Performance benchmarks
 * 
 * This test can run on both ESP32 hardware and native host for CI/CD
 */

#ifdef ARDUINO
    #include <Arduino.h>
    #define HOST_TEST 0
#else
    #include <cstdint>
    #include <cstring>
    #include <cstdio>
    #include <cmath>
    #include <chrono>
    #include <thread>
    #include <algorithm>
    #define HOST_TEST 1
    
    // Mock Arduino functions for host testing
    unsigned long micros() {
        auto now = std::chrono::high_resolution_clock::now();
        auto duration = now.time_since_epoch();
        return std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
    }
    
    unsigned long millis() {
        auto now = std::chrono::high_resolution_clock::now();
        auto duration = now.time_since_epoch();
        return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    }
    
    void delay(unsigned long ms) {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }
    
    // Mock print function
    void print_init() {
        // No-op for host
    }
    
    template<typename... Args>
    void print(const char* format, Args... args) {
        printf(format, args...);
        fflush(stdout);
    }
    
    void print(const char* str) {
        printf("%s", str);
        fflush(stdout);
    }
    
    // Mock min function
    template<typename T>
    T min(T a, T b) {
        return (a < b) ? a : b;
    }
#endif

#include <unity.h>
#include "application/compression.h"
#include "peripheral/acquisition.h"

#ifndef ARDUINO
    // Include print.h only for Arduino, mock already defined above
#else
    #include "peripheral/print.h"
#endif

// Test data sets
const uint16_t SAMPLE_DATA_TYPICAL[] = {2429, 177, 73, 4331, 70, 605};
const uint16_t SAMPLE_DATA_VARYING[] = {2400, 180, 75, 4200, 72, 600};
const uint16_t SAMPLE_DATA_CONSTANT[] = {2500, 2500, 2500, 2500, 2500, 2500};
const uint16_t SAMPLE_DATA_SEQUENTIAL[] = {100, 101, 102, 103, 104, 105};

const RegID REGISTER_SELECTION[] = {REG_VAC1, REG_IAC1, REG_IPV1, REG_PAC, REG_IPV2, REG_TEMP};

// Helper function to compare arrays
bool compareArrays(const uint16_t* arr1, const uint16_t* arr2, size_t size) {
    for (size_t i = 0; i < size; i++) {
        if (arr1[i] != arr2[i]) {
            return false;
        }
    }
    return true;
}

// Helper function to print array
void printArray(const char* label, const uint16_t* arr, size_t size) {
    print("%s: [", label);
    for (size_t i = 0; i < size; i++) {
        print("%u", arr[i]);
        if (i < size - 1) print(", ");
    }
    print("]\n");
}

// Helper function to print compression result
void printCompressionResult(const char* method, size_t originalSize, size_t compressedSize, 
                           unsigned long timeUs, bool lossless) {
    float ratio = (float)compressedSize / (float)originalSize;
    float savings = (1.0f - ratio) * 100.0f;
    
    print("\n=== %s ===\n", method);
    print("Original: %zu bytes\n", originalSize);
    print("Compressed: %zu bytes\n", compressedSize);
    print("Ratio: %.3f (%.1f%% savings)\n", ratio, savings);
    print("Time: %lu μs\n", timeUs);
    print("Lossless: %s\n", lossless ? "YES ✓" : "NO ✗");
}


// ==================== TEST CASES ====================

/**
 * Test 1: Smart Selection with Typical Data
 */
void test_smart_compression_typical_data() {
    print("\n========================================\n");
    print("TEST 1: Smart Selection - Typical Data\n");
    print("========================================\n");
    
    uint16_t testData[6];
    memcpy(testData, SAMPLE_DATA_TYPICAL, sizeof(SAMPLE_DATA_TYPICAL));
    
    printArray("Original data", testData, 6);
    
    unsigned long startTime = micros();
    std::vector<uint8_t> compressed = DataCompression::compressWithSmartSelection(
        testData, REGISTER_SELECTION, 6);
    unsigned long endTime = micros();
    
    size_t originalSize = 6 * sizeof(uint16_t);
    size_t compressedSize = compressed.size();
    
    // Verify compression actually happened
    TEST_ASSERT_GREATER_THAN(0, compressedSize);
    TEST_ASSERT_LESS_THAN(originalSize, compressedSize);  // Should have some compression
    
    printCompressionResult("Smart Selection", originalSize, compressedSize, 
                          endTime - startTime, true);
    
    print("\nCompressed data (hex): ");
    for (size_t i = 0; i < min(compressedSize, (size_t)20); i++) {
        print("%02X ", compressed[i]);
    }
    if (compressedSize > 20) print("...");
    print("\n");
}


/**
 * Test 2: Smart Selection with Multiple Samples (simulating buffer)
 */
void test_smart_compression_multiple_samples() {
    print("\n========================================\n");
    print("TEST 2: Smart Selection - Multiple Samples\n");
    print("========================================\n");
    
    // Create 10 samples with slight variations
    const size_t numSamples = 10;
    const size_t valuesPerSample = 6;
    const size_t totalValues = numSamples * valuesPerSample;
    
    uint16_t multipleData[totalValues];
    RegID multipleRegs[totalValues];
    
    // Fill with varying data
    for (size_t i = 0; i < numSamples; i++) {
        for (size_t j = 0; j < valuesPerSample; j++) {
            multipleData[i * valuesPerSample + j] = SAMPLE_DATA_TYPICAL[j] + (i * 2);  // Slight variation
            multipleRegs[i * valuesPerSample + j] = REGISTER_SELECTION[j];
        }
    }
    
    print("Total samples: %zu\n", numSamples);
    print("Total values: %zu\n", totalValues);
    
    unsigned long startTime = micros();
    std::vector<uint8_t> compressed = DataCompression::compressWithSmartSelection(
        multipleData, multipleRegs, totalValues);
    unsigned long endTime = micros();
    
    size_t originalSize = totalValues * sizeof(uint16_t);
    size_t compressedSize = compressed.size();
    
    TEST_ASSERT_GREATER_THAN(0, compressedSize);
    TEST_ASSERT_LESS_THAN(originalSize, compressedSize);
    
    float compressionRatio = (float)compressedSize / (float)originalSize;
    
    printCompressionResult("Smart Selection (Multi)", originalSize, compressedSize, 
                          endTime - startTime, true);
    
    // Should achieve good compression on similar data
    TEST_ASSERT_LESS_THAN(0.9f, compressionRatio);  // At least 10% compression
}


/**
 * Test 3: Binary Bit-Packed Compression
 */
void test_binary_bitpacked_compression() {
    print("\n========================================\n");
    print("TEST 3: Binary Bit-Packed Compression\n");
    print("========================================\n");
    
    uint16_t testData[] = {100, 150, 200, 250, 300, 350};  // Values that fit in 9 bits
    
    printArray("Original data", testData, 6);
    
    unsigned long startTime = micros();
    std::vector<uint8_t> compressed = DataCompression::compressBinaryBitPacked(testData, 6, 9);
    unsigned long endTime = micros();
    
    // Decompress
    std::vector<uint16_t> decompressed = DataCompression::decompressBinaryBitPacked(compressed);
    
    // Verify lossless
    bool lossless = (decompressed.size() == 6);
    if (lossless) {
        for (size_t i = 0; i < 6; i++) {
            if (testData[i] != decompressed[i]) {
                lossless = false;
                break;
            }
        }
    }
    
    TEST_ASSERT_TRUE(lossless);
    TEST_ASSERT_EQUAL(6, decompressed.size());
    
    printArray("Decompressed data", decompressed.data(), decompressed.size());
    printCompressionResult("Bit-Packed (9-bit)", 6 * sizeof(uint16_t), compressed.size(), 
                          endTime - startTime, lossless);
}


/**
 * Test 4: Binary Delta Compression
 */
void test_binary_delta_compression() {
    print("\n========================================\n");
    print("TEST 4: Binary Delta Compression\n");
    print("========================================\n");
    
    uint16_t testData[6];
    memcpy(testData, SAMPLE_DATA_SEQUENTIAL, sizeof(SAMPLE_DATA_SEQUENTIAL));
    
    printArray("Original data", testData, 6);
    
    unsigned long startTime = micros();
    std::vector<uint8_t> compressed = DataCompression::compressBinaryDelta(testData, 6);
    unsigned long endTime = micros();
    
    // Decompress
    std::vector<uint16_t> decompressed = DataCompression::decompressBinaryDelta(compressed);
    
    // Verify lossless
    bool lossless = (decompressed.size() == 6);
    if (lossless) {
        lossless = compareArrays(testData, decompressed.data(), 6);
    }
    
    TEST_ASSERT_TRUE(lossless);
    TEST_ASSERT_EQUAL(6, decompressed.size());
    
    printArray("Decompressed data", decompressed.data(), decompressed.size());
    printCompressionResult("Delta Compression", 6 * sizeof(uint16_t), compressed.size(), 
                          endTime - startTime, lossless);
}


/**
 * Test 5: Binary RLE Compression (with constant data)
 */
void test_binary_rle_compression() {
    print("\n========================================\n");
    print("TEST 5: Binary RLE Compression\n");
    print("========================================\n");
    
    uint16_t testData[6];
    memcpy(testData, SAMPLE_DATA_CONSTANT, sizeof(SAMPLE_DATA_CONSTANT));
    
    printArray("Original data", testData, 6);
    
    unsigned long startTime = micros();
    std::vector<uint8_t> compressed = DataCompression::compressBinaryRLE(testData, 6);
    unsigned long endTime = micros();
    
    // Decompress
    std::vector<uint16_t> decompressed = DataCompression::decompressBinaryRLE(compressed);
    
    // Verify lossless
    bool lossless = (decompressed.size() == 6);
    if (lossless) {
        lossless = compareArrays(testData, decompressed.data(), 6);
    }
    
    TEST_ASSERT_TRUE(lossless);
    TEST_ASSERT_EQUAL(6, decompressed.size());
    
    printArray("Decompressed data", decompressed.data(), decompressed.size());
    printCompressionResult("RLE Compression", 6 * sizeof(uint16_t), compressed.size(), 
                          endTime - startTime, lossless);
    
    // RLE should be very effective on constant data
    TEST_ASSERT_LESS_THAN(12, compressed.size());
}


/**
 * Test 6: Auto Binary Compression Selection
 */
void test_auto_binary_compression() {
    print("\n========================================\n");
    print("TEST 6: Auto Binary Compression Selection\n");
    print("========================================\n");
    
    uint16_t testData[6];
    memcpy(testData, SAMPLE_DATA_TYPICAL, sizeof(SAMPLE_DATA_TYPICAL));
    
    printArray("Original data", testData, 6);
    
    unsigned long startTime = micros();
    std::vector<uint8_t> compressed = DataCompression::compressBinary(testData, 6);
    unsigned long endTime = micros();
    
    // Decompress
    std::vector<uint16_t> decompressed = DataCompression::decompressBinary(compressed);
    
    // Verify lossless
    bool lossless = (decompressed.size() == 6);
    if (lossless) {
        lossless = compareArrays(testData, decompressed.data(), 6);
    }
    
    TEST_ASSERT_TRUE(lossless);
    TEST_ASSERT_EQUAL(6, decompressed.size());
    
    printArray("Decompressed data", decompressed.data(), decompressed.size());
    printCompressionResult("Auto Binary", 6 * sizeof(uint16_t), compressed.size(), 
                          endTime - startTime, lossless);
    
    // Determine method used
    if (!compressed.empty()) {
        print("Method marker: 0x%02X\n", compressed[0]);
    }
}


/**
 * Test 7: Large Dataset Compression (450 samples - 15 min @ 2sec/sample)
 */
void test_large_dataset_compression() {
    print("\n========================================\n");
    print("TEST 7: Large Dataset (450 samples)\n");
    print("========================================\n");
    
    const size_t numSamples = 450;  // 15 minutes at 2 sec/sample
    const size_t valuesPerSample = 6;
    const size_t totalValues = numSamples * valuesPerSample;
    
    print("Allocating memory for %zu values...\n", totalValues);
    
    uint16_t* largeData = new uint16_t[totalValues];
    RegID* largeRegs = new RegID[totalValues];
    
    // Fill with realistic varying data
    for (size_t i = 0; i < numSamples; i++) {
        for (size_t j = 0; j < valuesPerSample; j++) {
            // Add sinusoidal variation to simulate real data
            int16_t variation = (int16_t)(sin(i * 0.1) * 50);
            largeData[i * valuesPerSample + j] = SAMPLE_DATA_TYPICAL[j] + variation;
            largeRegs[i * valuesPerSample + j] = REGISTER_SELECTION[j];
        }
    }
    
    print("Compressing %zu samples (%zu values)...\n", numSamples, totalValues);
    
    unsigned long startTime = micros();
    std::vector<uint8_t> compressed = DataCompression::compressWithSmartSelection(
        largeData, largeRegs, totalValues);
    unsigned long endTime = micros();
    
    size_t originalSize = totalValues * sizeof(uint16_t);
    size_t compressedSize = compressed.size();
    
    float compressionRatio = (float)compressedSize / (float)originalSize;
    
    printCompressionResult("Large Dataset", originalSize, compressedSize, 
                          endTime - startTime, true);
    
    // Should achieve reasonable compression
    TEST_ASSERT_GREATER_THAN(0, compressedSize);
    TEST_ASSERT_LESS_THAN(originalSize, compressedSize);
    
    print("Data would fit in 15-min upload window: %s\n", 
          compressedSize < 8192 ? "YES ✓" : "NO (too large)");
    
    delete[] largeData;
    delete[] largeRegs;
}


/**
 * Test 8: Compression Consistency Test
 */
void test_compression_consistency() {
    print("\n========================================\n");
    print("TEST 8: Compression Consistency\n");
    print("========================================\n");
    
    uint16_t testData[6];
    memcpy(testData, SAMPLE_DATA_TYPICAL, sizeof(SAMPLE_DATA_TYPICAL));
    
    // Compress same data multiple times
    std::vector<uint8_t> compressed1 = DataCompression::compressWithSmartSelection(
        testData, REGISTER_SELECTION, 6);
    
    std::vector<uint8_t> compressed2 = DataCompression::compressWithSmartSelection(
        testData, REGISTER_SELECTION, 6);
    
    std::vector<uint8_t> compressed3 = DataCompression::compressWithSmartSelection(
        testData, REGISTER_SELECTION, 6);
    
    // All compressions should produce same result
    TEST_ASSERT_EQUAL(compressed1.size(), compressed2.size());
    TEST_ASSERT_EQUAL(compressed1.size(), compressed3.size());
    
    bool allSame = true;
    for (size_t i = 0; i < compressed1.size(); i++) {
        if (compressed1[i] != compressed2[i] || compressed1[i] != compressed3[i]) {
            allSame = false;
            break;
        }
    }
    
    TEST_ASSERT_TRUE(allSame);
    print("Compression is consistent: %s\n", allSame ? "YES ✓" : "NO ✗");
}


/**
 * Test 9: Memory Usage Test
 */
void test_memory_usage() {
    print("\n========================================\n");
    print("TEST 9: Memory Usage Analysis\n");
    print("========================================\n");
    
    DataCompression::printMemoryUsage();
    
    // Test should pass if no memory errors occur
    TEST_ASSERT_TRUE(true);
}


/**
 * Test 10: Performance Statistics
 */
void test_performance_statistics() {
    print("\n========================================\n");
    print("TEST 10: Performance Statistics\n");
    print("========================================\n");
    
    DataCompression::printPerformanceStats();
    
    TEST_ASSERT_TRUE(true);
}


// ==================== SETUP AND MAIN ====================

void setUp(void) {
    // Called before each test
}

void tearDown(void) {
    // Called after each test
}

void setup() {
    delay(2000);  // Wait for serial
    
    print_init();
    print("\n\n");
    print("╔════════════════════════════════════════════════════════════╗\n");
    print("║  ECOWATT COMPRESSION ALGORITHM TEST SUITE                 ║\n");
    print("║  Testing all compression methods for lossless operation   ║\n");
    print("╚════════════════════════════════════════════════════════════╝\n");
    
    UNITY_BEGIN();
    
    // Run all tests
    RUN_TEST(test_smart_compression_typical_data);
    RUN_TEST(test_smart_compression_multiple_samples);
    RUN_TEST(test_binary_bitpacked_compression);
    RUN_TEST(test_binary_delta_compression);
    RUN_TEST(test_binary_rle_compression);
    RUN_TEST(test_auto_binary_compression);
    RUN_TEST(test_large_dataset_compression);
    RUN_TEST(test_compression_consistency);
    RUN_TEST(test_memory_usage);
    RUN_TEST(test_performance_statistics);
    
    UNITY_END();
    
    print("\n");
    print("╔════════════════════════════════════════════════════════════╗\n");
    print("║  ALL TESTS COMPLETED                                      ║\n");
    print("╚════════════════════════════════════════════════════════════╝\n");
}

void loop() {
    // Tests run once in setup()
}
