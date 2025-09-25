#ifndef DATA_COMPRESSION_H
#define DATA_COMPRESSION_H

#include <Arduino.h>
#include <vector>

#include "peripheral/print.h"

/**
 * Data Compression Class for Register Values
 * Supports Run-Length Encoding (RLE) and Delta Compression
 */
class DataCompression {
public:
    // RLE Compression Methods
    static void compressRLE(const uint16_t* values, size_t count, char* out, size_t outSize);
    static size_t decompressRLE(const char* compressed, uint16_t* out, size_t maxCount);

    // Delta Compression Methods
    static void compressDelta(const uint16_t* values, size_t count, char* out, size_t outSize);
    static size_t decompressDelta(const char* compressed, uint16_t* out, size_t maxCount);

    // Utility Methods
    static void compressRegisterData(const uint16_t* values, size_t count, char* out, size_t outSize, bool useDelta = true);
    static size_t decompressRegisterData(const char* compressed, uint16_t* out, size_t maxCount, bool isDelta = true);

    // Statistics
    static float getCompressionRatio(size_t originalSize, size_t compressedSize);
    static void printCompressionStats(const char* method, size_t original, size_t compressed);

private:
    // Helper methods
    static void encodeNumber(uint16_t value, char* out, size_t outSize);
    static uint16_t decodeNumber(const char* encoded, size_t& pos);
    static bool isRepeating(const uint16_t* values, size_t start, size_t count, size_t& runLength);
};

/**
 * Compressed Data Structure
 * Stores compressed data with metadata
 */
struct CompressedData {
    char data[128];
    bool isDelta;
    size_t originalCount;
    uint32_t timestamp;

    CompressedData() : isDelta(false), originalCount(0), timestamp(0) { data[0] = '\0'; }
    CompressedData(const char* d, bool delta, size_t count) 
        : isDelta(delta), originalCount(count), timestamp(millis()) {
        strncpy(data, d, sizeof(data) - 1);
        data[sizeof(data) - 1] = '\0';
    }
};

#endif // DATA_COMPRESSION_H