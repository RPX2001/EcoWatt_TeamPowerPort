#ifndef COMPRESSIONBENCHMARK_H
#define COMPRESSIONBENCHMARK_H

#include "DataCompression.h"

struct BenchmarkResult {
    String compressionMethod;
    size_t numberOfSamples;
    size_t originalPayloadSize;
    size_t compressedPayloadSize;
    float compressionRatio;
    unsigned long cpuTimeMs;
    bool losslessVerified;
};

class CompressionBenchmark {
public:
    static BenchmarkResult testCompression(uint16_t* data, size_t count, const String& method);
    static void printBenchmarkReport(const BenchmarkResult& result);
    static bool verifyLosslessRecovery(uint16_t* original, const std::vector<uint16_t>& recovered, size_t count);
};

#endif
