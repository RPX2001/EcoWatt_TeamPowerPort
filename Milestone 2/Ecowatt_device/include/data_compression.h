#ifndef DATA_COMPRESSION_H
#define DATA_COMPRESSION_H

#include <Arduino.h>
#include <vector>

/**
 * Advanced Delta+RLE Compression for Register Values
 * Optimized compression algorithm with better compression ratios
 */
class DataCompression {
public:
    // Main compression methods
    static String compressRegisterData(const uint16_t* values, size_t count);
    static std::vector<uint16_t> decompressRegisterData(const String& compressed);
    
    // Statistics
    static float getCompressionRatio(size_t originalSize, size_t compressedSize);
    static void printCompressionStats(const String& method, size_t original, size_t compressed);

private:
    // Core compression engine
    static std::vector<uint8_t> compressAdvanced(const uint16_t* values, size_t count);
    static std::vector<uint16_t> decompressAdvanced(const std::vector<uint8_t>& compressed);
    
    // Optimized delta compression
    static void compressSampleOptimized(uint16_t prev, uint16_t curr, uint16_t commonValue, std::vector<uint8_t>& output);
    static size_t decompressSampleOptimized(const std::vector<uint8_t>& data, size_t pos, uint16_t prev, uint16_t commonValue, uint16_t& result);
    
    // RLE compression
    static std::vector<uint8_t> applyRLE(const std::vector<uint8_t>& data);
    static std::vector<uint8_t> decompressRLE(const std::vector<uint8_t>& data, size_t startPos);
    
    // Utilities
    static uint16_t findMostCommonValue(const uint16_t* values, size_t count);
    static String bytesToBase64(const std::vector<uint8_t>& bytes);
    static std::vector<uint8_t> base64ToBytes(const String& base64);
};

/**
 * Compressed Data Structure
 * Stores compressed data with metadata
 */
struct CompressedData {
    String data;
    String compressionType;  // Always "DELTA"
    size_t originalCount;
    uint32_t timestamp;
    
    CompressedData() : compressionType("DELTA"), originalCount(0), timestamp(0) {}
    CompressedData(const String& d, size_t count) 
        : data(d), compressionType("DELTA"), originalCount(count), timestamp(millis()) {}
};

#endif // DATA_COMPRESSION_H