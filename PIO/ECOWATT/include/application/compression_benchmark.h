#ifndef COMPRESSIONBENCHMARK_H
#define COMPRESSIONBENCHMARK_H

#include "application/compression.h"
#include "peripheral/print.h"

struct SmartPerformanceStats {
    // Overall statistics
    unsigned long totalSmartCompressions = 0;
    unsigned long totalCompressionTime = 0;
    float averageAcademicRatio = 0.0f;
    float averageTraditionalRatio = 0.0f;
    size_t totalOriginalBytes = 0;
    size_t totalCompressedBytes = 0;
    
    // Quality distribution
    unsigned long excellentCompressionCount = 0;    // Academic ratio ≤ 0.5
    unsigned long goodCompressionCount = 0;         // Academic ratio ≤ 0.67
    unsigned long fairCompressionCount = 0;         // Academic ratio ≤ 0.91
    unsigned long poorCompressionCount = 0;         // Academic ratio > 0.91
    
    // Method-specific counters
    unsigned long dictionaryUsed = 0;
    unsigned long temporalUsed = 0;
    unsigned long semanticUsed = 0;
    unsigned long bitpackUsed = 0;
    
    // Adaptive learning metrics
    char currentOptimalMethod[32];
    float bestAcademicRatio = 1.0f;
    
    // Constructor to initialize char array
    SmartPerformanceStats() {
        strcpy(currentOptimalMethod, "DICTIONARY");
    }
    unsigned long fastestCompressionTime = ULONG_MAX;
    
    // Success metrics
    unsigned long losslessSuccesses = 0;
    unsigned long compressionFailures = 0;
};

struct BenchmarkResult {
    char compressionMethod[32];
    size_t numberOfSamples;
    size_t originalPayloadSize;
    size_t compressedPayloadSize;
    float compressionRatio;
    unsigned long cpuTimeMs;
    bool losslessVerified;
};

class CompressionBenchmark {
public:
    static BenchmarkResult testCompression(uint16_t* data, size_t count, const char* method);
    static void printBenchmarkReport(const BenchmarkResult& result);
    static bool verifyLosslessRecovery(uint16_t* original, const std::vector<uint16_t>& recovered, size_t count);
};

#endif