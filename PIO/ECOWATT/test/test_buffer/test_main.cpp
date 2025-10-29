#include <unity.h>
#include "application/compression.h"
// data_pipeline.h does not exist - SampleBatch is defined in compression.h

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
// MILESTONE 3 PART 1: Buffer Implementation Tests
// ============================================================================

// Test 1: SampleBatch initialization
void test_sampleBatch_initialization(void) {
    SampleBatch batch;
    
    TEST_ASSERT_EQUAL(0, batch.sampleCount);
    TEST_ASSERT_EQUAL(0, batch.registerCount);
}

// Test 2: Add single sample to batch
void test_sampleBatch_addSingleSample(void) {
    uint16_t values[3] = {100, 200, 300};
    unsigned long timestamp = 1000;
    
    testBatch.addSample(values, timestamp, 3);
    
    TEST_ASSERT_EQUAL(1, testBatch.sampleCount);
    TEST_ASSERT_EQUAL(3, testBatch.registerCount);
    TEST_ASSERT_EQUAL(timestamp, testBatch.timestamps[0]);
    
    // Verify sample values
    TEST_ASSERT_EQUAL(100, testBatch.samples[0][0]);
    TEST_ASSERT_EQUAL(200, testBatch.samples[0][1]);
    TEST_ASSERT_EQUAL(300, testBatch.samples[0][2]);
}

// Test 3: Add multiple samples (not full)
void test_sampleBatch_addMultipleSamples(void) {
    uint16_t values1[2] = {100, 200};
    uint16_t values2[2] = {300, 400};
    uint16_t values3[2] = {500, 600};
    
    testBatch.addSample(values1, 1000, 2);
    testBatch.addSample(values2, 2000, 2);
    testBatch.addSample(values3, 3000, 2);
    
    TEST_ASSERT_EQUAL(3, testBatch.sampleCount);
    TEST_ASSERT_EQUAL(2, testBatch.registerCount);
    TEST_ASSERT_FALSE(testBatch.isFull());
    
    // Verify all samples
    TEST_ASSERT_EQUAL(100, testBatch.samples[0][0]);
    TEST_ASSERT_EQUAL(200, testBatch.samples[0][1]);
    TEST_ASSERT_EQUAL(300, testBatch.samples[1][0]);
    TEST_ASSERT_EQUAL(400, testBatch.samples[1][1]);
    TEST_ASSERT_EQUAL(500, testBatch.samples[2][0]);
    TEST_ASSERT_EQUAL(600, testBatch.samples[2][1]);
}

// Test 4: Fill batch to capacity (MAX_SAMPLES = 7)
void test_sampleBatch_fillToCapacity(void) {
    uint16_t values[2] = {111, 222};
    
    // Add 7 samples (MAX_SAMPLES)
    for (int i = 0; i < 7; i++) {
        testBatch.addSample(values, 1000 + i * 100, 2);
    }
    
    TEST_ASSERT_EQUAL(7, testBatch.sampleCount);
    TEST_ASSERT_TRUE(testBatch.isFull());
}

// Test 5: Handle buffer overflow (add beyond MAX_SAMPLES)
void test_sampleBatch_overflowHandling(void) {
    uint16_t values[2] = {111, 222};
    
    // Add 7 samples (fill to capacity)
    for (int i = 0; i < 7; i++) {
        testBatch.addSample(values, 1000 + i * 100, 2);
    }
    
    TEST_ASSERT_TRUE(testBatch.isFull());
    size_t countBefore = testBatch.sampleCount;
    
    // Try to add one more sample (should not overflow)
    testBatch.addSample(values, 2000, 2);
    
    // Should still be at capacity (implementation may ignore or handle differently)
    TEST_ASSERT_EQUAL(7, testBatch.sampleCount);
}

// Test 6: Reset batch
void test_sampleBatch_reset(void) {
    uint16_t values[3] = {100, 200, 300};
    
    // Add samples
    testBatch.addSample(values, 1000, 3);
    testBatch.addSample(values, 2000, 3);
    
    TEST_ASSERT_EQUAL(2, testBatch.sampleCount);
    
    // Reset
    testBatch.reset();
    
    TEST_ASSERT_EQUAL(0, testBatch.sampleCount);
    TEST_ASSERT_EQUAL(0, testBatch.registerCount);
}

// Test 7: Convert to linear array
void test_sampleBatch_toLinearArray(void) {
    uint16_t values1[3] = {100, 200, 300};
    uint16_t values2[3] = {400, 500, 600};
    
    testBatch.addSample(values1, 1000, 3);
    testBatch.addSample(values2, 2000, 3);
    
    // Create output buffer (2 samples * 3 registers = 6 values)
    uint16_t output[6];
    testBatch.toLinearArray(output);
    
    // Verify linear order: [sample0_reg0, sample0_reg1, sample0_reg2, sample1_reg0, ...]
    TEST_ASSERT_EQUAL(100, output[0]);
    TEST_ASSERT_EQUAL(200, output[1]);
    TEST_ASSERT_EQUAL(300, output[2]);
    TEST_ASSERT_EQUAL(400, output[3]);
    TEST_ASSERT_EQUAL(500, output[4]);
    TEST_ASSERT_EQUAL(600, output[5]);
}

// Test 8: Timestamp tracking
void test_sampleBatch_timestampTracking(void) {
    uint16_t values[2] = {100, 200};
    
    testBatch.addSample(values, 1000, 2);
    testBatch.addSample(values, 2500, 2);
    testBatch.addSample(values, 3750, 2);
    
    TEST_ASSERT_EQUAL(1000, testBatch.timestamps[0]);
    TEST_ASSERT_EQUAL(2500, testBatch.timestamps[1]);
    TEST_ASSERT_EQUAL(3750, testBatch.timestamps[2]);
}

// Test 9: Handle different register counts (modular separation)
void test_sampleBatch_differentRegisterCounts(void) {
    uint16_t values3[3] = {100, 200, 300};
    uint16_t values5[5] = {10, 20, 30, 40, 50};
    
    // First batch with 3 registers
    testBatch.addSample(values3, 1000, 3);
    TEST_ASSERT_EQUAL(3, testBatch.registerCount);
    
    // Reset and use different count
    testBatch.reset();
    testBatch.addSample(values5, 2000, 5);
    TEST_ASSERT_EQUAL(5, testBatch.registerCount);
}

// Test 10: Maximum register count (MAX_REGISTERS = 10)
void test_sampleBatch_maxRegisters(void) {
    uint16_t values[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    
    testBatch.addSample(values, 1000, 10);
    
    TEST_ASSERT_EQUAL(10, testBatch.registerCount);
    
    // Verify all 10 values
    for (int i = 0; i < 10; i++) {
        TEST_ASSERT_EQUAL(i + 1, testBatch.samples[0][i]);
    }
}

// Test 11: Empty batch behavior
void test_sampleBatch_emptyBehavior(void) {
    TEST_ASSERT_EQUAL(0, testBatch.sampleCount);
    TEST_ASSERT_FALSE(testBatch.isFull());
    
    // Linear array from empty batch should be safe
    uint16_t output[10];
    testBatch.toLinearArray(output);
    // No assertion - just ensure no crash
}

// ============================================================================
// MAIN TEST RUNNER
// ============================================================================

void setup() {
    delay(2000); // Wait for serial monitor
    
    UNITY_BEGIN();
    
    // SampleBatch basic tests
    RUN_TEST(test_sampleBatch_initialization);
    RUN_TEST(test_sampleBatch_addSingleSample);
    RUN_TEST(test_sampleBatch_addMultipleSamples);
    RUN_TEST(test_sampleBatch_fillToCapacity);
    RUN_TEST(test_sampleBatch_overflowHandling);
    RUN_TEST(test_sampleBatch_reset);
    
    // SampleBatch conversion and tracking
    RUN_TEST(test_sampleBatch_toLinearArray);
    RUN_TEST(test_sampleBatch_timestampTracking);
    
    // SampleBatch modular behavior
    RUN_TEST(test_sampleBatch_differentRegisterCounts);
    RUN_TEST(test_sampleBatch_maxRegisters);
    RUN_TEST(test_sampleBatch_emptyBehavior);
    
    UNITY_END();
}

void loop() {
    // Empty loop - tests run once in setup()
}
