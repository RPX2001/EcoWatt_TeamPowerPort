#ifndef DATA_COMPRESSION_H
#define DATA_COMPRESSION_H

#include <Arduino.h>
#include <vector>

/**
 * Data Compression Class for Register Values
 * Supports Run-Length Encoding (RLE) and Delta Compression
 */
class DataCompression {
public:
    // RLE Compression Methods
    static String compressRLE(const uint16_t* values, size_t count);
    static std::vector<uint16_t> decompressRLE(const String& compressed);
    
    // Delta Compression Methods
    static String compressDelta(const uint16_t* values, size_t count);
    static std::vector<uint16_t> decompressDelta(const String& compressed);
    
    // Utility Methods
    static String compressRegisterData(const uint16_t* values, size_t count);
    static std::vector<uint16_t> decompressRegisterData(const String& compressed);
    
    // Statistics
    static float getCompressionRatio(size_t originalSize, size_t compressedSize);
    static void printCompressionStats(const String& method, size_t original, size_t compressed);

private:
    // Helper methods
    static String encodeNumber(uint16_t value);
    static uint16_t decodeNumber(const String& encoded, size_t& pos);
    static bool isRepeating(const uint16_t* values, size_t start, size_t count, size_t& runLength);
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