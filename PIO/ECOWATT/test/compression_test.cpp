/**
 * @file compression_test.cpp
 * @brief Compression Algorithm Benchmark and Testing Tool
 * 
 * Tests all compression methods with different datasets:
 * - Dataset 1: Constant values (worst for delta, best for RLE)
 * - Dataset 2: Linear ramp (best for delta)
 * - Dataset 3: Realistic inverter data
 * - Dataset 4: Random noise (worst case)
 * 
 * Verifies lossless compression and calculates accurate metrics.
 */

#include <Arduino.h>
#include <vector>
#include "application/compression.h"
#include "peripheral/acquisition.h"
#include "peripheral/print.h"

// Test datasets
namespace TestData {
    // Dataset 1: Constant values (70 samples = 10 values × 7 samples)
    uint16_t constantData[] = {
        2400, 180, 50, 4200, 70, 600, 70, 35, 100, 1500,  // Sample 1
        2400, 180, 50, 4200, 70, 600, 70, 35, 100, 1500,  // Sample 2
        2400, 180, 50, 4200, 70, 600, 70, 35, 100, 1500,  // Sample 3
        2400, 180, 50, 4200, 70, 600, 70, 35, 100, 1500,  // Sample 4
        2400, 180, 50, 4200, 70, 600, 70, 35, 100, 1500,  // Sample 5
        2400, 180, 50, 4200, 70, 600, 70, 35, 100, 1500,  // Sample 6
        2400, 180, 50, 4200, 70, 600, 70, 35, 100, 1500   // Sample 7
    };
    
    // Dataset 2: Linear ramp (increasing values)
    uint16_t linearData[] = {
        2400, 180, 50, 4200, 70, 600, 70, 35, 100, 1500,  // Sample 1
        2410, 181, 51, 4210, 71, 601, 71, 36, 101, 1505,  // Sample 2 (+10)
        2420, 182, 52, 4220, 72, 602, 72, 37, 102, 1510,  // Sample 3 (+10)
        2430, 183, 53, 4230, 73, 603, 73, 38, 103, 1515,  // Sample 4 (+10)
        2440, 184, 54, 4240, 74, 604, 74, 39, 104, 1520,  // Sample 5 (+10)
        2450, 185, 55, 4250, 75, 605, 75, 40, 105, 1525,  // Sample 6 (+10)
        2460, 186, 56, 4260, 76, 606, 76, 41, 106, 1530   // Sample 7 (+10)
    };
    
    // Dataset 3: Realistic inverter data (from actual logs)
    uint16_t realisticData[] = {
        2429, 177, 73, 4331, 70, 605, 67, 32, 98, 1450,   // Sample 1
        2308, 168, 69, 4115, 67, 575, 63, 30, 95, 1420,   // Sample 2
        2550, 186, 77, 4547, 74, 635, 72, 35, 102, 1480,  // Sample 3
        2380, 150, 65, 3800, 55, 590, 60, 28, 90, 1400,   // Sample 4
        2480, 195, 80, 4800, 85, 620, 75, 38, 105, 1500,  // Sample 5
        2429, 177, 73, 4331, 70, 605, 67, 32, 98, 1450,   // Sample 6 (repeat)
        2308, 168, 69, 4115, 67, 575, 63, 30, 95, 1420    // Sample 7 (repeat)
    };
    
    // Dataset 4: Random noise (worst case)
    uint16_t randomData[] = {
        2847, 123, 98, 3992, 88, 712, 55, 42, 134, 1687,  // Sample 1
        1923, 234, 31, 5102, 23, 489, 91, 18, 67, 1234,   // Sample 2
        3401, 156, 77, 2876, 102, 823, 44, 51, 189, 1876, // Sample 3
        2134, 198, 52, 4523, 67, 601, 78, 29, 95, 1098,   // Sample 4
        2789, 144, 89, 3678, 91, 534, 62, 37, 142, 1543,  // Sample 5
        1876, 211, 43, 4901, 74, 678, 85, 21, 76, 1321,   // Sample 6
        3156, 189, 61, 3234, 56, 745, 69, 44, 113, 1789   // Sample 7
    };
    
    const size_t DATASET_SIZE = 70;  // 10 registers × 7 samples
}

// Register selection for testing (all 10 registers)
RegID testRegisters[] = {
    REG_VAC1, REG_IAC1, REG_FAC1, REG_VPV1, REG_VPV2,
    REG_IPV1, REG_IPV2, REG_TEMP, REG_POW, REG_PAC
};
const size_t REGISTER_COUNT = 10;

/**
 * @brief Calculate compression metrics
 */
struct CompressionMetrics {
    size_t originalSize;
    size_t compressedSize;
    float academicRatio;      // compressed / original
    float traditionalRatio;   // original / compressed
    float savingsPercent;
    unsigned long timeUs;
    bool losslessVerified;
    
    void calculate() {
        if (originalSize > 0) {
            academicRatio = (float)compressedSize / (float)originalSize;
            traditionalRatio = (compressedSize > 0) ? (float)originalSize / (float)compressedSize : 0.0f;
            savingsPercent = (1.0f - academicRatio) * 100.0f;
        } else {
            academicRatio = 1.0f;
            traditionalRatio = 0.0f;
            savingsPercent = 0.0f;
        }
    }
    
    void print(const char* methodName) {
        print("  Method: %s\n", methodName);
        print("  Original Size:     %zu bytes\n", originalSize);
        print("  Compressed Size:   %zu bytes\n", compressedSize);
        print("  Academic Ratio:    %.4f (%.1f%%)\n", academicRatio, academicRatio * 100.0f);
        print("  Traditional Ratio: %.2f:1\n", traditionalRatio);
        print("  Compression Savings: %.1f%%\n", savingsPercent);
        print("  Processing Time:   %lu µs (%.2f ms)\n", timeUs, timeUs / 1000.0f);
        print("  Lossless Test:     %s\n", losslessVerified ? "✅ PASSED" : "❌ FAILED");
    }
};

/**
 * @brief Test a specific compression method
 */
CompressionMetrics testCompressionMethod(
    const char* methodName,
    uint16_t* data,
    size_t dataSize,
    std::vector<uint8_t> (*compressionFunc)(uint16_t*, const RegID*, size_t)
) {
    CompressionMetrics metrics;
    metrics.originalSize = dataSize * sizeof(uint16_t);
    metrics.losslessVerified = false;
    
    print("\n┌─────────────────────────────────────────┐\n");
    print("│ Testing: %-31s │\n", methodName);
    print("└─────────────────────────────────────────┘\n");
    
    // Measure compression time
    unsigned long startTime = micros();
    std::vector<uint8_t> compressed = compressionFunc(data, testRegisters, dataSize);
    metrics.timeUs = micros() - startTime;
    
    metrics.compressedSize = compressed.size();
    metrics.calculate();
    
    // Verify lossless compression
    if (!compressed.empty()) {
        std::vector<uint16_t> decompressed = DataCompression::decompressBinary(compressed);
        
        if (decompressed.size() == dataSize) {
            metrics.losslessVerified = true;
            for (size_t i = 0; i < dataSize; i++) {
                if (decompressed[i] != data[i]) {
                    metrics.losslessVerified = false;
                    print("  ❌ Mismatch at index %zu: original=%u, decompressed=%u\n",
                          i, data[i], decompressed[i]);
                    break;
                }
            }
        } else {
            print("  ❌ Size mismatch: original=%zu, decompressed=%zu\n",
                  dataSize, decompressed.size());
        }
    } else {
        print("  ❌ Compression failed (empty result)\n");
    }
    
    // Print metrics
    metrics.print(methodName);
    
    return metrics;
}

/**
 * @brief Test all compression methods on a dataset
 */
void testDataset(const char* datasetName, uint16_t* data, size_t dataSize) {
    print("\n");
    print("╔═════════════════════════════════════════════════════════╗\n");
    print("║ DATASET: %-47s ║\n", datasetName);
    print("╠═════════════════════════════════════════════════════════╣\n");
    print("║ Size: %zu samples (%zu bytes)                          ║\n", 
          dataSize, dataSize * sizeof(uint16_t));
    print("╚═════════════════════════════════════════════════════════╝\n");
    
    // Print first sample for reference
    print("\nFirst sample values:\n");
    for (size_t i = 0; i < REGISTER_COUNT && i < dataSize; i++) {
        print("  %s: %u\n", REGISTER_MAP[testRegisters[i]].name, data[i]);
    }
    
    // Test each compression method
    CompressionMetrics metrics[5];
    
    // 1. Dictionary + Bitmask
    metrics[0] = testCompressionMethod(
        "Dictionary + Bitmask",
        data, dataSize,
        DataCompression::compressWithDictionary
    );
    
    // 2. Temporal Delta
    metrics[1] = testCompressionMethod(
        "Temporal Delta",
        data, dataSize,
        DataCompression::compressWithTemporalDelta
    );
    
    // 3. Semantic RLE
    metrics[2] = testCompressionMethod(
        "Semantic RLE",
        data, dataSize,
        DataCompression::compressWithSemanticRLE
    );
    
    // 4. Binary compression (auto-select)
    metrics[3] = testCompressionMethod(
        "Binary Auto-Select",
        data, dataSize,
        [](uint16_t* d, const RegID* r, size_t s) { 
            return DataCompression::compressBinary(d, s); 
        }
    );
    
    // 5. Smart Selection (chooses best)
    metrics[4] = testCompressionMethod(
        "Smart Selection",
        data, dataSize,
        DataCompression::compressWithSmartSelection
    );
    
    // Summary
    print("\n╔═════════════════════════════════════════════════════════╗\n");
    print("║ SUMMARY FOR: %-42s ║\n", datasetName);
    print("╠═════════════════════════════════════════════════════════╣\n");
    
    // Find best method
    size_t bestIdx = 0;
    for (size_t i = 1; i < 5; i++) {
        if (metrics[i].compressedSize > 0 && 
            metrics[i].compressedSize < metrics[bestIdx].compressedSize) {
            bestIdx = i;
        }
    }
    
    const char* methodNames[] = {
        "Dictionary", "Delta", "RLE", "Binary", "Smart"
    };
    
    print("║ Best Method:       %-36s ║\n", methodNames[bestIdx]);
    print("║ Best Ratio:        %.2f:1 (%.1f%% savings)            ║\n",
          metrics[bestIdx].traditionalRatio, metrics[bestIdx].savingsPercent);
    print("║ All Lossless:      %-36s ║\n",
          (metrics[0].losslessVerified && metrics[1].losslessVerified && 
           metrics[2].losslessVerified && metrics[3].losslessVerified && 
           metrics[4].losslessVerified) ? "✅ YES" : "❌ NO");
    print("╚═════════════════════════════════════════════════════════╝\n");
}

/**
 * @brief Run complete compression benchmark suite
 */
void runCompressionBenchmark() {
    print("\n");
    print("╔═════════════════════════════════════════════════════════╗\n");
    print("║       ECOWATT COMPRESSION BENCHMARK SUITE               ║\n");
    print("╠═════════════════════════════════════════════════════════╣\n");
    print("║ Testing all compression methods with 4 datasets         ║\n");
    print("║ Verifying lossless compression and calculating metrics  ║\n");
    print("╚═════════════════════════════════════════════════════════╝\n");
    
    // Test all datasets
    testDataset("Constant Values", TestData::constantData, TestData::DATASET_SIZE);
    delay(500);
    
    testDataset("Linear Ramp", TestData::linearData, TestData::DATASET_SIZE);
    delay(500);
    
    testDataset("Realistic Inverter Data", TestData::realisticData, TestData::DATASET_SIZE);
    delay(500);
    
    testDataset("Random Noise", TestData::randomData, TestData::DATASET_SIZE);
    
    print("\n");
    print("╔═════════════════════════════════════════════════════════╗\n");
    print("║              BENCHMARK COMPLETE                         ║\n");
    print("╚═════════════════════════════════════════════════════════╝\n");
}

/**
 * @brief Compression benchmark mode entry point
 */
void enterCompressionBenchmarkMode() {
    print("\n📊 Entering Compression Benchmark Mode...\n\n");
    runCompressionBenchmark();
    print("\n📊 Benchmark Complete\n");
}
