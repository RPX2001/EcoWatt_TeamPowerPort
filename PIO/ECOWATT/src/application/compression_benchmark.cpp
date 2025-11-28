#include "application/compression_benchmark.h"
#include "peripheral/logger.h"


/**
 * @fn BenchmarkResult CompressionBenchmark::testCompression(uint16_t* data, size_t count, const char* method)
 * 
 * @brief Test a specific compression method and return benchmark results.
 * 
 * @param data Pointer to the original data array.
 * @param count Number of samples in the data array.
 * @param method Compression method to test ("DELTA", "RLE", "HYBRID").
 * 
 * @return BenchmarkResult struct containing performance metrics.
 */
BenchmarkResult CompressionBenchmark::testCompression(uint16_t* data, size_t count, const char* method) 
{
    BenchmarkResult result;
    strncpy(result.compressionMethod, method, sizeof(result.compressionMethod) - 1);
    result.compressionMethod[sizeof(result.compressionMethod) - 1] = '\0';
    result.numberOfSamples = count;
    result.originalPayloadSize = count * sizeof(uint16_t);
    
    unsigned long startTime = millis();
    char compressed[256];
    std::vector<uint16_t> decompressed;
    
    if (strcmp(method, "DELTA") == 0) 
    {
        DataCompression::compressRegisterData(data, count, compressed, sizeof(compressed));
        uint16_t tempData[32];
        size_t decompressedCount = DataCompression::decompressRegisterData(compressed, tempData, 32);
        decompressed.clear();
        for (size_t i = 0; i < decompressedCount; i++) 
        {
            decompressed.push_back(tempData[i]);
        }
    } 
    else if (strcmp(method, "RLE") == 0) 
    {
        // Use binary RLE methods and convert to string for compatibility
        std::vector<uint8_t> binaryCompressed = DataCompression::compressBinaryRLE(data, count);
        char base64Buffer[256];
        DataCompression::base64Encode(binaryCompressed, base64Buffer, sizeof(base64Buffer));
        strcpy(compressed, base64Buffer);
        decompressed = DataCompression::decompressBinaryRLE(binaryCompressed);
    } 
    else if (strcmp(method, "HYBRID") == 0) 
    {
        // Use the main binary compression method which selects optimal method
        std::vector<uint8_t> binaryCompressed = DataCompression::compressBinary(data, count);
        char base64Buffer[256];
        DataCompression::base64Encode(binaryCompressed, base64Buffer, sizeof(base64Buffer));
        strcpy(compressed, base64Buffer);
        decompressed = DataCompression::decompressBinary(binaryCompressed);
    }
    
    unsigned long endTime = millis();
    
    result.compressedPayloadSize = strlen(compressed);
    result.compressionRatio = (float)result.originalPayloadSize / (float)result.compressedPayloadSize;
    result.cpuTimeMs = endTime - startTime;
    result.losslessVerified = verifyLosslessRecovery(data, decompressed, count);
    
    return result;
}


/**
 * @fn void CompressionBenchmark::printBenchmarkReport(const BenchmarkResult& result)
 * 
 * @brief Print a formatted benchmark report to the console.
 * 
 * @param result BenchmarkResult struct containing performance metrics.
 */
void CompressionBenchmark::printBenchmarkReport(const BenchmarkResult& result) 
{
    LOG_INFO(LOG_TAG_COMPRESS, "=== COMPRESSION BENCHMARK REPORT ===");;
    LOG_INFO(LOG_TAG_COMPRESS, "Compression Method Used: %s\n", result.compressionMethod);
    LOG_INFO(LOG_TAG_COMPRESS, "Number of Samples: %lu\n", result.numberOfSamples);
    LOG_INFO(LOG_TAG_COMPRESS, "Original Payload Size: %lu bytes\n", result.originalPayloadSize);
    LOG_INFO(LOG_TAG_COMPRESS, "Compressed Payload Size: %lu bytes\n", result.compressedPayloadSize);
    LOG_INFO(LOG_TAG_COMPRESS, "Compression Ratio: %.2f:1\n", result.compressionRatio);
    LOG_INFO(LOG_TAG_COMPRESS, "CPU Time: %lu ms\n", result.cpuTimeMs);
    LOG_INFO(LOG_TAG_COMPRESS, "Lossless Recovery Verification: %s\n", result.losslessVerified ? "PASSED" : "FAILED");
    LOG_INFO(LOG_TAG_COMPRESS, "Storage Savings: %.1f%%\n", (1.0 - (float)result.compressedPayloadSize / (float)result.originalPayloadSize) * 100.0);
    LOG_INFO(LOG_TAG_COMPRESS, "=====================================");;
}


/**
 * @fn bool CompressionBenchmark::verifyLosslessRecovery(uint16_t* original, const std::vector<uint16_t>& recovered, size_t count)
 * 
 * @brief Verify that the decompressed data matches the original data.
 * 
 * @param original Pointer to the original data array.
 * @param recovered Vector containing the decompressed data.
 * @param count Number of samples to verify.
 * 
 * @return true if data matches, false otherwise.
 */
bool CompressionBenchmark::verifyLosslessRecovery(uint16_t* original, const std::vector<uint16_t>& recovered, size_t count) 
{
    if (recovered.size() != count) return false;
    
    for (size_t i = 0; i < count; i++) 
    {
        if (original[i] != recovered[i]) return false;
    }
    return true;
}