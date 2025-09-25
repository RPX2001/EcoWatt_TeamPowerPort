#include "application/compression.h"

// RLE Compression - encodes runs of identical values
void DataCompression::compressRLE(const uint16_t* values, size_t count, char* out, size_t outSize) {
    if (count == 0) { out[0] = '\0'; return; }
    size_t pos = 0;
    int n = snprintf(out, outSize, "R:");
    pos += (n > 0) ? n : 0;
    size_t i = 0;
    while (i < count && pos < outSize - 1) {
        uint16_t currentValue = values[i];
        size_t runLength = 1;
        while (i + runLength < count && values[i + runLength] == currentValue) {
            runLength++;
        }
        n = snprintf(out + pos, outSize - pos, "%u,%u|", currentValue, (unsigned)runLength);
        pos += (n > 0) ? n : 0;
        i += runLength;
    }
    out[outSize-1] = '\0';
}

// RLE Decompression
size_t DataCompression::decompressRLE(const char* compressed, uint16_t* out, size_t maxCount) {
    if (!compressed || strncmp(compressed, "R:", 2) != 0) return 0;
    const char* data = compressed + 2;
    size_t outIdx = 0;
    while (*data && outIdx < maxCount) {
        uint16_t value = 0;
        size_t count = 0;
        int n = 0;
        if (sscanf(data, "%hu,%zu|%n", &value, &count, &n) == 2) {
            for (size_t i = 0; i < count && outIdx < maxCount; i++) {
                out[outIdx++] = value;
            }
            data += n;
        } else {
            break;
        }
    }
    return outIdx;
}

// Delta Compression - stores first value + differences
void DataCompression::compressDelta(const uint16_t* values, size_t count, char* out, size_t outSize) {
    if (count == 0) { out[0] = '\0'; return; }
    size_t pos = 0;
    int n = snprintf(out, outSize, "D:%u|", values[0]);
    pos += (n > 0) ? n : 0;
    for (size_t i = 1; i < count && pos < outSize - 1; i++) {
        int16_t delta = (int16_t)values[i] - (int16_t)values[i-1];
        n = snprintf(out + pos, outSize - pos, "%d ", delta);
        pos += (n > 0) ? n : 0;
    }
    out[outSize-1] = '\0';
}

// Delta Decompression
size_t DataCompression::decompressDelta(const char* compressed, uint16_t* out, size_t maxCount) {
    if (!compressed || strncmp(compressed, "D:", 2) != 0) return 0;
    const char* data = compressed + 2;
    const char* pipe = strchr(data, '|');
    if (!pipe) return 0;
    uint16_t currentValue = (uint16_t)atoi(data);
    size_t outIdx = 0;
    out[outIdx++] = currentValue;
    const char* deltas = pipe + 1;
    while (*deltas && outIdx < maxCount) {
        int16_t delta = 0;
        int n = 0;
        if (sscanf(deltas, "%hd %n", &delta, &n) == 1) {
            currentValue = (uint16_t)((int16_t)currentValue + delta);
            out[outIdx++] = currentValue;
            deltas += n;
        } else {
            break;
        }
    }
    return outIdx;
}

// Smart compression - chooses best method
void DataCompression::compressRegisterData(const uint16_t* values, size_t count, char* out, size_t outSize, bool useDelta) {
    if (useDelta) {
        compressDelta(values, count, out, outSize);
    } else {
        compressRLE(values, count, out, outSize);
    }
}

// Smart decompression
size_t DataCompression::decompressRegisterData(const char* compressed, uint16_t* out, size_t maxCount, bool isDelta) {
    if (!compressed) return 0;
    if (strncmp(compressed, "D:", 2) == 0) {
        return decompressDelta(compressed, out, maxCount);
    } else if (strncmp(compressed, "R:", 2) == 0) {
        return decompressRLE(compressed, out, maxCount);
    }
    return 0;
}

// Calculate compression ratio
float DataCompression::getCompressionRatio(size_t originalSize, size_t compressedSize) {
    if (compressedSize == 0) return 0.0f;
    return (float)originalSize / (float)compressedSize;
}

// Print compression statistics
void DataCompression::printCompressionStats(const char* method, size_t original, size_t compressed) {
    float ratio = getCompressionRatio(original, compressed);
    float savings = ((float)(original - compressed) / (float)original) * 100.0f;

    print("=== %s Compression Stats ===\n", method);
    print("Original size: %d bytes\n", original);
    print("Compressed size: %d bytes\n", compressed);
    print("Compression ratio: %.2f:1\n", ratio);
    print("Space savings: %.1f%%\n", savings);
}

// Helper: Check for repeating values (for RLE optimization)
bool DataCompression::isRepeating(const uint16_t* values, size_t start, size_t count, size_t& runLength) {
    if (start >= count) return false;
    
    uint16_t value = values[start];
    runLength = 1;
    
    while (start + runLength < count && values[start + runLength] == value) {
        runLength++;
    }
    
    return runLength > 1;
}