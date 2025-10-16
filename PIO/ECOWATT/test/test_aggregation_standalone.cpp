/**
 * @file test_aggregation_standalone.cpp
 * @brief Standalone test for aggregation functions
 * 
 * Tests all aggregation methods including:
 * - Statistical calculations (mean, median, min, max, stddev)
 * - Downsampling with different window sizes
 * - Adaptive downsampling
 * - Outlier detection and removal
 * - Data stability analysis
 * 
 * Compile with: g++ -std=c++11 -Wall -Wextra -I../include test_aggregation_standalone.cpp ../src/application/aggregation.cpp -o test_aggregation
 */

#include <cstdint>
#include <cstdio>
#include <cmath>
#include <vector>
#include <algorithm>

// Include aggregation header
#include "application/aggregation.h"

using namespace DataAggregation;

// Simple test counter
int tests_passed = 0;
int tests_failed = 0;

#define TEST_ASSERT(condition, message) \
    if (!(condition)) { \
        printf("❌ FAIL: %s\n", message); \
        tests_failed++; \
        return; \
    }

#define TEST_ASSERT_EQUAL(expected, actual, message) \
    if ((expected) != (actual)) { \
        printf("❌ FAIL: %s (expected %u, got %u)\n", message, (unsigned)(expected), (unsigned)(actual)); \
        tests_failed++; \
        return; \
    }

#define TEST_ASSERT_NEAR(expected, actual, tolerance, message) \
    if (abs((int)(expected) - (int)(actual)) > (int)(tolerance)) { \
        printf("❌ FAIL: %s (expected %u±%u, got %u)\n", message, (unsigned)(expected), (unsigned)(tolerance), (unsigned)(actual)); \
        tests_failed++; \
        return; \
    }

void printSuccess(const char* testName) {
    printf("✓ %s\n", testName);
    tests_passed++;
}

// Test data sets
const uint16_t DATA_STABLE[] = {100, 101, 99, 100, 102, 100, 101, 99, 100, 100};
const uint16_t DATA_WITH_OUTLIERS[] = {100, 102, 99, 101, 500, 100, 98, 101, 103, 100};
const uint16_t DATA_TRENDING[] = {100, 110, 120, 130, 140, 150, 160, 170, 180, 190};
const uint16_t DATA_VARYING[] = {2429, 177, 73, 4331, 70, 605, 2500, 150, 80, 4200};


// ==================== TEST CASES ====================

/**
 * Test 1: Calculate Statistics
 */
void test_calculate_stats() {
    printf("\n[TEST 1] Calculate Statistics\n");
    
    uint16_t data[] = {100, 200, 150, 175, 125};
    
    AggregatedStats stats = calculateStats(data, 5);
    
    TEST_ASSERT_EQUAL(5, stats.count, "Count should be 5");
    TEST_ASSERT_EQUAL(100, stats.min, "Min should be 100");
    TEST_ASSERT_EQUAL(200, stats.max, "Max should be 200");
    TEST_ASSERT_EQUAL(100, stats.range, "Range should be 100");
    TEST_ASSERT_EQUAL(100, stats.first, "First should be 100");
    TEST_ASSERT_EQUAL(125, stats.last, "Last should be 125");
    TEST_ASSERT_EQUAL(150, stats.mean, "Mean should be 150");
    TEST_ASSERT_EQUAL(150, stats.median, "Median should be 150");
    
    printf("  Mean: %u, Median: %u, Min: %u, Max: %u\n", 
           stats.mean, stats.median, stats.min, stats.max);
    printf("  Range: %u, StdDev: %u, Sum: %u\n", 
           stats.range, stats.stddev, stats.sum);
    
    printSuccess("Calculate Statistics");
}

/**
 * Test 2: Aggregation Methods
 */
void test_aggregation_methods() {
    printf("\n[TEST 2] Aggregation Methods\n");
    
    uint16_t data[] = {100, 200, 150, 175, 125};
    
    uint16_t mean = aggregate(data, 5, AGG_MEAN);
    uint16_t median = aggregate(data, 5, AGG_MEDIAN);
    uint16_t min = aggregate(data, 5, AGG_MIN);
    uint16_t max = aggregate(data, 5, AGG_MAX);
    uint16_t first = aggregate(data, 5, AGG_FIRST);
    uint16_t last = aggregate(data, 5, AGG_LAST);
    
    TEST_ASSERT_EQUAL(150, mean, "Mean aggregation");
    TEST_ASSERT_EQUAL(150, median, "Median aggregation");
    TEST_ASSERT_EQUAL(100, min, "Min aggregation");
    TEST_ASSERT_EQUAL(200, max, "Max aggregation");
    TEST_ASSERT_EQUAL(100, first, "First aggregation");
    TEST_ASSERT_EQUAL(125, last, "Last aggregation");
    
    printf("  AGG_MEAN: %u\n", mean);
    printf("  AGG_MEDIAN: %u\n", median);
    printf("  AGG_MIN: %u, AGG_MAX: %u\n", min, max);
    printf("  AGG_FIRST: %u, AGG_LAST: %u\n", first, last);
    
    printSuccess("Aggregation Methods");
}

/**
 * Test 3: Downsampling
 */
void test_downsampling() {
    printf("\n[TEST 3] Downsampling\n");
    
    // Create 450 samples (15 minutes at 2 sec/sample)
    const size_t inputSize = 450;
    uint16_t* input = new uint16_t[inputSize];
    
    // Fill with simulated data
    for (size_t i = 0; i < inputSize; i++) {
        input[i] = 2400 + (i % 100); // Varying data
    }
    
    // Downsample to 30 samples (window size = 15)
    const size_t outputSize = 30;
    uint16_t* output = new uint16_t[outputSize];
    
    size_t resultSize = downsample(input, inputSize, output, 15, AGG_MEAN);
    
    TEST_ASSERT_EQUAL(30, resultSize, "Should produce 30 output samples");
    
    printf("  Input: %zu samples → Output: %zu samples (window=15)\n", 
           inputSize, resultSize);
    printf("  First output values: %u, %u, %u\n", output[0], output[1], output[2]);
    printf("  Compression ratio: %.1f:1\n", (float)inputSize / (float)resultSize);
    
    delete[] input;
    delete[] output;
    
    printSuccess("Downsampling");
}

/**
 * Test 4: Adaptive Downsampling
 */
void test_adaptive_downsampling() {
    printf("\n[TEST 4] Adaptive Downsampling\n");
    
    // Create 450 samples
    const size_t inputSize = 450;
    uint16_t* input = new uint16_t[inputSize];
    
    for (size_t i = 0; i < inputSize; i++) {
        input[i] = 2400 + (uint16_t)(sin(i * 0.1) * 50);
    }
    
    // Adaptively downsample to 50 samples
    const size_t targetSize = 50;
    uint16_t* output = new uint16_t[targetSize];
    
    size_t resultSize = adaptiveDownsample(input, inputSize, output, targetSize, AGG_MEAN);
    
    TEST_ASSERT(resultSize <= targetSize, "Should not exceed target size");
    TEST_ASSERT(resultSize >= targetSize - 1, "Should be close to target size");
    
    printf("  Input: %zu samples → Target: %zu → Actual: %zu samples\n", 
           inputSize, targetSize, resultSize);
    printf("  Auto-calculated window size: %zu\n", inputSize / resultSize);
    
    delete[] input;
    delete[] output;
    
    printSuccess("Adaptive Downsampling");
}

/**
 * Test 5: Stability Detection
 */
void test_stability_detection() {
    printf("\n[TEST 5] Stability Detection\n");
    
    uint16_t stableData[10];
    uint16_t varyingData[10];
    
    for (size_t i = 0; i < 10; i++) {
        stableData[i] = DATA_STABLE[i];
        varyingData[i] = DATA_VARYING[i];
    }
    
    bool stable = isStable(stableData, 10, 10); // 10% threshold
    bool varying = isStable(varyingData, 10, 10);
    
    TEST_ASSERT(stable == true, "Stable data should be detected as stable");
    TEST_ASSERT(varying == false, "Varying data should not be stable");
    
    AggregatedStats stats1 = calculateStats(stableData, 10);
    AggregatedStats stats2 = calculateStats(varyingData, 10);
    
    printf("  Stable data: CV = %.2f%% → %s\n", 
           ((float)stats1.stddev / stats1.mean) * 100.0f,
           stable ? "STABLE ✓" : "NOT STABLE");
    printf("  Varying data: CV = %.2f%% → %s\n", 
           ((float)stats2.stddev / stats2.mean) * 100.0f,
           varying ? "STABLE" : "NOT STABLE ✓");
    
    printSuccess("Stability Detection");
}

/**
 * Test 6: Outlier Detection
 */
void test_outlier_detection() {
    printf("\n[TEST 6] Outlier Detection\n");
    
    uint16_t data[10];
    bool isOutlier[10];
    
    for (size_t i = 0; i < 10; i++) {
        data[i] = DATA_WITH_OUTLIERS[i];
    }
    
    size_t outlierCount = detectOutliers(data, 10, isOutlier);
    
    TEST_ASSERT(outlierCount > 0, "Should detect at least one outlier");
    TEST_ASSERT(isOutlier[4] == true, "Value 500 should be detected as outlier");
    
    printf("  Detected %zu outlier(s):\n", outlierCount);
    for (size_t i = 0; i < 10; i++) {
        if (isOutlier[i]) {
            printf("    Index %zu: %u (OUTLIER)\n", i, data[i]);
        }
    }
    
    printSuccess("Outlier Detection");
}

/**
 * Test 7: Outlier Removal
 */
void test_outlier_removal() {
    printf("\n[TEST 7] Outlier Removal\n");
    
    uint16_t data[10];
    uint16_t cleaned[10];
    
    for (size_t i = 0; i < 10; i++) {
        data[i] = DATA_WITH_OUTLIERS[i];
    }
    
    size_t cleanedCount = removeOutliers(data, 10, cleaned);
    
    TEST_ASSERT(cleanedCount < 10, "Should remove at least one value");
    TEST_ASSERT(cleanedCount >= 8, "Should keep most values");
    
    printf("  Original: %zu values → Cleaned: %zu values\n", (size_t)10, cleanedCount);
    printf("  Removed: %zu outlier(s)\n", 10 - cleanedCount);
    
    // Verify outlier is not in cleaned data
    bool hasOutlier = false;
    for (size_t i = 0; i < cleanedCount; i++) {
        if (cleaned[i] == 500) {
            hasOutlier = true;
            break;
        }
    }
    
    TEST_ASSERT(hasOutlier == false, "Cleaned data should not contain outlier");
    
    printSuccess("Outlier Removal");
}

/**
 * Test 8: Smart Aggregation
 */
void test_smart_aggregation() {
    printf("\n[TEST 8] Smart Aggregation\n");
    
    uint16_t stableData[10];
    uint16_t varyingData[10];
    
    for (size_t i = 0; i < 10; i++) {
        stableData[i] = DATA_STABLE[i];
        varyingData[i] = DATA_VARYING[i];
    }
    
    uint16_t smartStable = aggregate(stableData, 10, AGG_SMART);
    uint16_t smartVarying = aggregate(varyingData, 10, AGG_SMART);
    
    printf("  Stable data: Smart aggregation = %u\n", smartStable);
    printf("  Varying data: Smart aggregation = %u\n", smartVarying);
    
    // Smart aggregation should produce reasonable results
    TEST_ASSERT(smartStable >= 95 && smartStable <= 105, "Smart stable should be near 100");
    
    printSuccess("Smart Aggregation");
}

/**
 * Test 9: Large Dataset Downsampling
 */
void test_large_dataset_downsampling() {
    printf("\n[TEST 9] Large Dataset Downsampling (450 samples)\n");
    
    // Simulate 15 minutes of data (450 samples at 2s intervals)
    const size_t inputSize = 450;
    uint16_t* input = new uint16_t[inputSize];
    
    // Create realistic sensor data with trends and noise
    for (size_t i = 0; i < inputSize; i++) {
        float baseline = 2400.0f;
        float trend = i * 0.5f; // Slight upward trend
        float noise = sin(i * 0.2) * 20.0f; // Sinusoidal variation
        input[i] = (uint16_t)(baseline + trend + noise);
    }
    
    // Downsample to 30 samples (30 second intervals)
    const size_t targetSize = 30;
    uint16_t* output = new uint16_t[targetSize];
    
    size_t resultSize = adaptiveDownsample(input, inputSize, output, targetSize, AGG_MEAN);
    
    TEST_ASSERT(resultSize > 0, "Should produce output");
    
    printf("  Original: %zu samples (15 min at 2s)\n", inputSize);
    printf("  Downsampled: %zu samples (15 min at 30s)\n", resultSize);
    printf("  Data reduction: %.1f%%\n", (1.0f - (float)resultSize / inputSize) * 100.0f);
    printf("  Sample values: %u, %u, %u, ..., %u\n", 
           output[0], output[1], output[2], output[resultSize-1]);
    
    delete[] input;
    delete[] output;
    
    printSuccess("Large Dataset Downsampling");
}

/**
 * Test 10: Combined Compression + Aggregation
 */
void test_combined_compression_aggregation() {
    printf("\n[TEST 10] Combined Compression + Aggregation\n");
    
    const size_t inputSize = 450;
    uint16_t* input = new uint16_t[inputSize];
    
    // Create data
    for (size_t i = 0; i < inputSize; i++) {
        input[i] = 2400 + (uint16_t)(sin(i * 0.1) * 50);
    }
    
    // Step 1: Downsample from 450 to 50 samples
    uint16_t* downsampled = new uint16_t[50];
    size_t downsampledSize = adaptiveDownsample(input, inputSize, downsampled, 50, AGG_MEAN);
    
    // Calculate sizes
    size_t originalBytes = inputSize * sizeof(uint16_t);
    size_t aggregatedBytes = downsampledSize * sizeof(uint16_t);
    
    printf("  Original: %zu bytes (%zu samples)\n", originalBytes, inputSize);
    printf("  After aggregation: %zu bytes (%zu samples)\n", aggregatedBytes, downsampledSize);
    printf("  Reduction: %.1f%%\n", (1.0f - (float)aggregatedBytes / originalBytes) * 100.0f);
    printf("  Combined with compression would yield even better results!\n");
    
    TEST_ASSERT(aggregatedBytes < originalBytes, "Aggregation should reduce size");
    
    delete[] input;
    delete[] downsampled;
    
    printSuccess("Combined Compression + Aggregation");
}


// ==================== MAIN ====================

int main() {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║  ECOWATT AGGREGATION ALGORITHM TEST SUITE                 ║\n");
    printf("║  Testing data aggregation and downsampling methods        ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    
    test_calculate_stats();
    test_aggregation_methods();
    test_downsampling();
    test_adaptive_downsampling();
    test_stability_detection();
    test_outlier_detection();
    test_outlier_removal();
    test_smart_aggregation();
    test_large_dataset_downsampling();
    test_combined_compression_aggregation();
    
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║  TEST RESULTS                                             ║\n");
    printf("║  Passed: %-3d  Failed: %-3d                                ║\n", tests_passed, tests_failed);
    printf("╚════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    return tests_failed > 0 ? 1 : 0;
}
