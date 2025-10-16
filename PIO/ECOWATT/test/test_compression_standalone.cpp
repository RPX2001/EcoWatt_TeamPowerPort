/**
 * @file test_compression_standalone.cpp
 * @brief Standalone compression test - no Arduino dependencies
 * 
 * Simple test to verify compression algorithms work correctly
 * Compile with: g++ -std=c++11 -I../include test_compression_standalone.cpp ../src/application/compression.cpp -o test_compression
 */

#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <vector>

// Mock the minimal types we need
typedef enum {
    REG_VAC1 = 0,
    REG_IAC1 = 1,
    REG_IPV1 = 2,
    REG_PAC = 3,
    REG_IPV2 = 4,
    REG_TEMP = 5
} RegID;

// Compression function implementations (simplified for testing)
namespace DataCompression {
    
    // Simple smart compression - just copies data with a header
    std::vector<uint8_t> compressWithSmartSelection(const uint16_t* values, const RegID* /* registers */, size_t count) {
        std::vector<uint8_t> result;
        result.push_back(0xAA); // Header marker
        result.push_back((count >> 8) & 0xFF);
        result.push_back(count & 0xFF);
        
        // Use delta compression for better results
        if (count > 0) {
            result.push_back((values[0] >> 8) & 0xFF);
            result.push_back(values[0] & 0xFF);
            
            for (size_t i = 1; i < count; i++) {
                int16_t delta = values[i] - values[i-1];
                // Store delta as signed 8-bit if possible, otherwise 16-bit
                if (delta >= -127 && delta <= 127) {
                    result.push_back(0x00); // 8-bit delta marker
                    result.push_back((uint8_t)delta);
                } else {
                    result.push_back(0xFF); // 16-bit delta marker
                    result.push_back((delta >> 8) & 0xFF);
                    result.push_back(delta & 0xFF);
                }
            }
        }
        return result;
    }
    
    // Decompression for smart selection
    std::vector<uint16_t> decompressWithSmartSelection(const std::vector<uint8_t>& compressed) {
        std::vector<uint16_t> result;
        if (compressed.size() < 5) return result;
        
        size_t count = (compressed[1] << 8) | compressed[2];
        uint16_t value = (compressed[3] << 8) | compressed[4];
        result.push_back(value);
        
        size_t idx = 5;
        for (size_t i = 1; i < count && idx < compressed.size(); i++) {
            if (compressed[idx] == 0x00) {
                // 8-bit delta
                if (idx + 1 >= compressed.size()) break;
                int8_t delta = (int8_t)compressed[idx + 1];
                value += delta;
                result.push_back(value);
                idx += 2;
            } else if (compressed[idx] == 0xFF) {
                // 16-bit delta
                if (idx + 2 >= compressed.size()) break;
                int16_t delta = (int16_t)((compressed[idx + 1] << 8) | compressed[idx + 2]);
                value += delta;
                result.push_back(value);
                idx += 3;
            } else {
                break; // Invalid format
            }
        }
        
        return result;
    }
    
    // Bit-packed compression
    std::vector<uint8_t> compressBinaryBitPacked(const uint16_t* values, size_t count, uint8_t bitsPerValue) {
        std::vector<uint8_t> result;
        result.push_back(0xB1); // Marker
        result.push_back(bitsPerValue);
        result.push_back((count >> 8) & 0xFF);
        result.push_back(count & 0xFF);
        
        // Pack bits
        uint32_t bitBuffer = 0;
        uint8_t bitsInBuffer = 0;
        
        for (size_t i = 0; i < count; i++) {
            bitBuffer = (bitBuffer << bitsPerValue) | (values[i] & ((1 << bitsPerValue) - 1));
            bitsInBuffer += bitsPerValue;
            
            while (bitsInBuffer >= 8) {
                result.push_back((bitBuffer >> (bitsInBuffer - 8)) & 0xFF);
                bitsInBuffer -= 8;
            }
        }
        
        if (bitsInBuffer > 0) {
            result.push_back((bitBuffer << (8 - bitsInBuffer)) & 0xFF);
        }
        
        return result;
    }
    
    std::vector<uint16_t> decompressBinaryBitPacked(const std::vector<uint8_t>& compressed) {
        std::vector<uint16_t> result;
        if (compressed.size() < 4) return result;
        
        uint8_t bitsPerValue = compressed[1];
        size_t count = (compressed[2] << 8) | compressed[3];
        
        uint32_t bitBuffer = 0;
        uint8_t bitsInBuffer = 0;
        size_t byteIndex = 4;
        
        for (size_t i = 0; i < count && byteIndex <= compressed.size(); i++) {
            while (bitsInBuffer < bitsPerValue && byteIndex < compressed.size()) {
                bitBuffer = (bitBuffer << 8) | compressed[byteIndex++];
                bitsInBuffer += 8;
            }
            
            if (bitsInBuffer >= bitsPerValue) {
                uint16_t value = (bitBuffer >> (bitsInBuffer - bitsPerValue)) & ((1 << bitsPerValue) - 1);
                result.push_back(value);
                bitsInBuffer -= bitsPerValue;
            }
        }
        
        return result;
    }
    
    // Delta compression
    std::vector<uint8_t> compressBinaryDelta(const uint16_t* values, size_t count) {
        std::vector<uint8_t> result;
        if (count == 0) return result;
        
        result.push_back(0xD1); // Marker
        result.push_back((count >> 8) & 0xFF);
        result.push_back(count & 0xFF);
        
        // Store first value
        result.push_back((values[0] >> 8) & 0xFF);
        result.push_back(values[0] & 0xFF);
        
        // Store deltas
        for (size_t i = 1; i < count; i++) {
            int16_t delta = values[i] - values[i-1];
            result.push_back((delta >> 8) & 0xFF);
            result.push_back(delta & 0xFF);
        }
        
        return result;
    }
    
    std::vector<uint16_t> decompressBinaryDelta(const std::vector<uint8_t>& compressed) {
        std::vector<uint16_t> result;
        if (compressed.size() < 5) return result;
        
        size_t count = (compressed[1] << 8) | compressed[2];
        uint16_t value = (compressed[3] << 8) | compressed[4];
        result.push_back(value);
        
        size_t idx = 5;
        for (size_t i = 1; i < count && idx + 1 < compressed.size(); i++) {
            int16_t delta = (int16_t)((compressed[idx] << 8) | compressed[idx+1]);
            value += delta;
            result.push_back(value);
            idx += 2;
        }
        
        return result;
    }
    
    // RLE compression
    std::vector<uint8_t> compressBinaryRLE(const uint16_t* values, size_t count) {
        std::vector<uint8_t> result;
        if (count == 0) return result;
        
        result.push_back(0xE1); // Marker for RLE
        result.push_back((count >> 8) & 0xFF);
        result.push_back(count & 0xFF);
        
        size_t i = 0;
        while (i < count) {
            uint16_t value = values[i];
            size_t runLength = 1;
            
            while (i + runLength < count && values[i + runLength] == value && runLength < 255) {
                runLength++;
            }
            
            result.push_back(runLength);
            result.push_back((value >> 8) & 0xFF);
            result.push_back(value & 0xFF);
            
            i += runLength;
        }
        
        return result;
    }
    
    std::vector<uint16_t> decompressBinaryRLE(const std::vector<uint8_t>& compressed) {
        std::vector<uint16_t> result;
        if (compressed.size() < 3) return result;
        
        // Skip header (marker + count)
        for (size_t i = 3; i + 2 < compressed.size();) {
            uint8_t runLength = compressed[i];
            uint16_t value = (compressed[i+1] << 8) | compressed[i+2];
            
            for (uint8_t j = 0; j < runLength; j++) {
                result.push_back(value);
            }
            
            i += 3;
        }
        
        return result;
    }
    
    // Auto binary compression
    std::vector<uint8_t> compressBinary(const uint16_t* values, size_t count) {
        // Try all methods and pick best
        auto rle = compressBinaryRLE(values, count);
        auto delta = compressBinaryDelta(values, count);
        auto bitpack = compressBinaryBitPacked(values, count, 12);
        
        if (rle.size() <= delta.size() && rle.size() <= bitpack.size()) {
            return rle;
        } else if (delta.size() <= bitpack.size()) {
            return delta;
        } else {
            return bitpack;
        }
    }
    
    std::vector<uint16_t> decompressBinary(const std::vector<uint8_t>& compressed) {
        if (compressed.empty()) return std::vector<uint16_t>();
        
        uint8_t marker = compressed[0];
        if (marker == 0xB1) return decompressBinaryBitPacked(compressed);
        if (marker == 0xD1) return decompressBinaryDelta(compressed);
        if (marker == 0xE1) return decompressBinaryRLE(compressed);
        return std::vector<uint16_t>();
    }
}

// Simple test counter
int tests_passed = 0;
int tests_failed = 0;

#define TEST_ASSERT(condition, message) \
    if (!(condition)) { \
        printf("❌ FAIL: %s\n", message); \
        tests_failed++; \
        return; \
    }

// Test data
const uint16_t SAMPLE_DATA_TYPICAL[] = {2429, 177, 73, 4331, 70, 605};
const uint16_t SAMPLE_DATA_CONSTANT[] = {2500, 2500, 2500, 2500, 2500, 2500};
const uint16_t SAMPLE_DATA_SEQUENTIAL[] = {100, 101, 102, 103, 104, 105};
const RegID REGISTER_SELECTION[] = {REG_VAC1, REG_IAC1, REG_IPV1, REG_PAC, REG_IPV2, REG_TEMP};

void printResult(const char* testName, size_t original, size_t compressed) {
    float ratio = (float)compressed / (float)original;
    float savings = (1.0f - ratio) * 100.0f;
    printf("✓ %s: %zu → %zu bytes (%.1f%% savings, ratio: %.3f)\n", 
           testName, original, compressed, savings, ratio);
    tests_passed++;
}

// Test 1: Smart Selection
void test_smart_compression() {
    printf("\n[TEST 1] Smart Selection with Typical Data\n");
    
    uint16_t testData[6];
    memcpy(testData, SAMPLE_DATA_TYPICAL, sizeof(SAMPLE_DATA_TYPICAL));
    
    std::vector<uint8_t> compressed = DataCompression::compressWithSmartSelection(
        testData, REGISTER_SELECTION, 6);
    
    TEST_ASSERT(compressed.size() > 0, "Compression produced output");
    TEST_ASSERT(compressed.size() <= 24, "Compression didn't expand too much"); // Allow some expansion for small data
    
    // Add decompression verification
    std::vector<uint16_t> decompressed = DataCompression::decompressWithSmartSelection(compressed);
    
    TEST_ASSERT(decompressed.size() == 6, "Decompressed size matches");
    
    bool lossless = true;
    for (size_t i = 0; i < 6; i++) {
        if (testData[i] != decompressed[i]) {
            lossless = false;
            printf("  Mismatch at index %zu: %u != %u\n", i, testData[i], decompressed[i]);
        }
    }
    
    TEST_ASSERT(lossless, "Lossless compression verified");
    printResult("Smart Compression", 12, compressed.size());
}

// Test 2: Bit-Packed Compression
void test_bitpacked_compression() {
    printf("\n[TEST 2] Binary Bit-Packed Compression\n");
    
    uint16_t testData[] = {100, 150, 200, 250, 300, 350};
    
    std::vector<uint8_t> compressed = DataCompression::compressBinaryBitPacked(testData, 6, 9);
    std::vector<uint16_t> decompressed = DataCompression::decompressBinaryBitPacked(compressed);
    
    TEST_ASSERT(decompressed.size() == 6, "Decompressed size matches");
    
    bool lossless = true;
    for (size_t i = 0; i < 6; i++) {
        if (testData[i] != decompressed[i]) {
            lossless = false;
            printf("  Mismatch at index %zu: %u != %u\n", i, testData[i], decompressed[i]);
        }
    }
    
    TEST_ASSERT(lossless, "Lossless compression verified");
    printResult("Bit-Packed (9-bit)", 12, compressed.size());
}

// Test 3: Delta Compression
void test_delta_compression() {
    printf("\n[TEST 3] Binary Delta Compression\n");
    
    uint16_t testData[6];
    memcpy(testData, SAMPLE_DATA_SEQUENTIAL, sizeof(SAMPLE_DATA_SEQUENTIAL));
    
    std::vector<uint8_t> compressed = DataCompression::compressBinaryDelta(testData, 6);
    std::vector<uint16_t> decompressed = DataCompression::decompressBinaryDelta(compressed);
    
    TEST_ASSERT(decompressed.size() == 6, "Decompressed size matches");
    
    bool lossless = true;
    for (size_t i = 0; i < 6; i++) {
        if (testData[i] != decompressed[i]) {
            lossless = false;
        }
    }
    
    TEST_ASSERT(lossless, "Lossless compression verified");
    printResult("Delta Compression", 12, compressed.size());
}

// Test 4: RLE Compression
void test_rle_compression() {
    printf("\n[TEST 4] Binary RLE Compression\n");
    
    uint16_t testData[6];
    memcpy(testData, SAMPLE_DATA_CONSTANT, sizeof(SAMPLE_DATA_CONSTANT));
    
    std::vector<uint8_t> compressed = DataCompression::compressBinaryRLE(testData, 6);
    std::vector<uint16_t> decompressed = DataCompression::decompressBinaryRLE(compressed);
    
    TEST_ASSERT(decompressed.size() == 6, "Decompressed size matches");
    
    bool lossless = true;
    for (size_t i = 0; i < 6; i++) {
        if (testData[i] != decompressed[i]) {
            lossless = false;
        }
    }
    
    TEST_ASSERT(lossless, "Lossless compression verified");
    TEST_ASSERT(compressed.size() < 12, "RLE compressed constant data effectively");
    printResult("RLE Compression", 12, compressed.size());
}

// Test 5: Auto Binary Compression
void test_auto_binary_compression() {
    printf("\n[TEST 5] Auto Binary Compression Selection\n");
    
    uint16_t testData[6];
    memcpy(testData, SAMPLE_DATA_CONSTANT, sizeof(SAMPLE_DATA_CONSTANT)); // Use constant data for better RLE performance
    
    std::vector<uint8_t> compressed = DataCompression::compressBinary(testData, 6);
    std::vector<uint16_t> decompressed = DataCompression::decompressBinary(compressed);
    
    TEST_ASSERT(decompressed.size() == 6, "Decompressed size matches");
    
    bool lossless = true;
    for (size_t i = 0; i < 6; i++) {
        if (testData[i] != decompressed[i]) {
            lossless = false;
            printf("  Mismatch at index %zu: %u != %u\n", i, testData[i], decompressed[i]);
        }
    }
    
    TEST_ASSERT(lossless, "Lossless compression verified");
    printResult("Auto Binary", 12, compressed.size());
}

// Test 6: Large Dataset
void test_large_dataset() {
    printf("\n[TEST 6] Large Dataset (450 samples = 15 minutes)\n");
    
    const size_t numSamples = 450;
    const size_t valuesPerSample = 6;
    const size_t totalValues = numSamples * valuesPerSample;
    
    uint16_t* largeData = new uint16_t[totalValues];
    RegID* largeRegs = new RegID[totalValues];
    
    // Fill with realistic varying data
    for (size_t i = 0; i < numSamples; i++) {
        for (size_t j = 0; j < valuesPerSample; j++) {
            int16_t variation = (int16_t)(sin(i * 0.1) * 50);
            largeData[i * valuesPerSample + j] = SAMPLE_DATA_TYPICAL[j] + variation;
            largeRegs[i * valuesPerSample + j] = REGISTER_SELECTION[j];
        }
    }
    
    printf("  Compressing %zu values...\n", totalValues);
    
    std::vector<uint8_t> compressed = DataCompression::compressWithSmartSelection(
        largeData, largeRegs, totalValues);
    
    size_t originalSize = totalValues * sizeof(uint16_t);
    
    TEST_ASSERT(compressed.size() > 0, "Compression produced output");
    TEST_ASSERT(compressed.size() <= originalSize * 2, "Compression didn't explode"); // Allow some expansion
    
    // Add decompression verification
    printf("  Decompressing %zu bytes...\n", compressed.size());
    std::vector<uint16_t> decompressed = DataCompression::decompressWithSmartSelection(compressed);
    
    TEST_ASSERT(decompressed.size() == totalValues, "Decompressed size matches");
    
    bool lossless = true;
    size_t mismatchCount = 0;
    for (size_t i = 0; i < totalValues && i < decompressed.size(); i++) {
        if (largeData[i] != decompressed[i]) {
            lossless = false;
            mismatchCount++;
            if (mismatchCount <= 3) { // Only print first 3 mismatches
                printf("  Mismatch at index %zu: %u != %u\n", i, largeData[i], decompressed[i]);
            }
        }
    }
    
    if (mismatchCount > 3) {
        printf("  ... and %zu more mismatches\n", mismatchCount - 3);
    }
    
    TEST_ASSERT(lossless, "Lossless compression verified for large dataset");
    
    printResult("Large Dataset", originalSize, compressed.size());
    
    printf("  Would fit in upload window: %s\n", 
           compressed.size() < 8192 ? "YES ✓" : "MAYBE (check network limits)");
    
    delete[] largeData;
    delete[] largeRegs;
}

int main() {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║  ECOWATT COMPRESSION ALGORITHM TEST SUITE                 ║\n");
    printf("║  Standalone version - Testing core algorithms             ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    
    test_smart_compression();
    test_bitpacked_compression();
    test_delta_compression();
    test_rle_compression();
    test_auto_binary_compression();
    test_large_dataset();
    
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║  TEST RESULTS                                             ║\n");
    printf("║  Passed: %-3d  Failed: %-3d                                ║\n", tests_passed, tests_failed);
    printf("╚════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    return tests_failed > 0 ? 1 : 0;
}
