#include "CompressionBenchmark.h"

BenchmarkResult CompressionBenchmark::testCompression(uint16_t* data, size_t count, const String& method) {
    BenchmarkResult result;
    result.compressionMethod = method;
    result.numberOfSamples = count;
    result.originalPayloadSize = count * sizeof(uint16_t);
    
    unsigned long startTime = millis();
    String compressed;
    std::vector<uint16_t> decompressed;
    
    if (method == "DELTA") {
        compressed = DataCompression::compressRegisterData(data, count);
        decompressed = DataCompression::decompressRegisterData(compressed);
    } else if (method == "RLE") {
        // Use binary RLE methods and convert to string for compatibility
        std::vector<uint8_t> binaryCompressed = DataCompression::compressBinaryRLE(data, count);
        compressed = DataCompression::base64Encode(binaryCompressed);
        decompressed = DataCompression::decompressBinaryRLE(binaryCompressed);
    } else if (method == "HYBRID") {
        // Use the main binary compression method which selects optimal method
        std::vector<uint8_t> binaryCompressed = DataCompression::compressBinary(data, count);
        compressed = DataCompression::base64Encode(binaryCompressed);
        decompressed = DataCompression::decompressBinary(binaryCompressed);
    }
    
    unsigned long endTime = millis();
    
    result.compressedPayloadSize = compressed.length();
    result.compressionRatio = (float)result.originalPayloadSize / (float)result.compressedPayloadSize;
    result.cpuTimeMs = endTime - startTime;
    result.losslessVerified = verifyLosslessRecovery(data, decompressed, count);
    
    return result;
}

void CompressionBenchmark::printBenchmarkReport(const BenchmarkResult& result) {
    Serial.println("=== COMPRESSION BENCHMARK REPORT ===");
    Serial.println("Compression Method Used: " + result.compressionMethod);
    Serial.println("Number of Samples: " + String(result.numberOfSamples));
    Serial.println("Original Payload Size: " + String(result.originalPayloadSize) + " bytes");
    Serial.println("Compressed Payload Size: " + String(result.compressedPayloadSize) + " bytes");
    Serial.println("Compression Ratio: " + String(result.compressionRatio, 2) + ":1");
    Serial.println("CPU Time: " + String(result.cpuTimeMs) + " ms");
    Serial.println("Lossless Recovery Verification: " + String(result.losslessVerified ? "PASSED" : "FAILED"));
    Serial.println("Storage Savings: " + String((1.0 - (float)result.compressedPayloadSize / (float)result.originalPayloadSize) * 100.0, 1) + "%");
    Serial.println("=====================================");
}

bool CompressionBenchmark::verifyLosslessRecovery(uint16_t* original, const std::vector<uint16_t>& recovered, size_t count) {
    if (recovered.size() != count) return false;
    
    for (size_t i = 0; i < count; i++) {
        if (original[i] != recovered[i]) return false;
    }
    return true;
}
