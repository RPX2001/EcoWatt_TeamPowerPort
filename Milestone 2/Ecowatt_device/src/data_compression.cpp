#include "data_compression.h"

// RLE Compression - encodes runs of identical values
String DataCompression::compressRLE(const uint16_t* values, size_t count) {
    if (count == 0) return "";
    
    String result = "R:"; // RLE prefix
    size_t i = 0;
    
    while (i < count) {
        uint16_t currentValue = values[i];
        size_t runLength = 1;
        
        // Count consecutive identical values
        while (i + runLength < count && values[i + runLength] == currentValue) {
            runLength++;
        }
        
        // Encode: value,count|
        result += String(currentValue) + "," + String(runLength) + "|";
        i += runLength;
    }
    
    return result;
}

// RLE Decompression
std::vector<uint16_t> DataCompression::decompressRLE(const String& compressed) {
    std::vector<uint16_t> result;
    
    if (!compressed.startsWith("R:")) return result;
    
    String data = compressed.substring(2); // Remove "R:" prefix
    int pos = 0;
    
    while (pos < data.length()) {
        int commaPos = data.indexOf(',', pos);
        int pipePos = data.indexOf('|', commaPos);
        
        if (commaPos == -1 || pipePos == -1) break;
        
        uint16_t value = data.substring(pos, commaPos).toInt();
        size_t count = data.substring(commaPos + 1, pipePos).toInt();
        
        // Add 'count' copies of 'value'
        for (size_t i = 0; i < count; i++) {
            result.push_back(value);
        }
        
        pos = pipePos + 1;
    }
    
    return result;
}

// Delta Compression - stores first value + differences
String DataCompression::compressDelta(const uint16_t* values, size_t count) {
    if (count == 0) return "";
    
    String result = "D:"; // Delta prefix
    result += String(values[0]) + "|"; // Store first value
    
    for (size_t i = 1; i < count; i++) {
        int16_t delta = (int16_t)values[i] - (int16_t)values[i-1];
        result += String(delta) + ",";
    }
    
    return result;
}

// Delta Decompression
std::vector<uint16_t> DataCompression::decompressDelta(const String& compressed) {
    std::vector<uint16_t> result;
    
    if (!compressed.startsWith("D:")) return result;
    
    String data = compressed.substring(2); // Remove "D:" prefix
    int pipePos = data.indexOf('|');
    
    if (pipePos == -1) return result;
    
    // Get first value
    uint16_t currentValue = data.substring(0, pipePos).toInt();
    result.push_back(currentValue);
    
    // Process deltas
    String deltas = data.substring(pipePos + 1);
    int pos = 0;
    
    while (pos < deltas.length()) {
        int commaPos = deltas.indexOf(',', pos);
        if (commaPos == -1) break;
        
        int16_t delta = deltas.substring(pos, commaPos).toInt();
        currentValue = (uint16_t)((int16_t)currentValue + delta);
        result.push_back(currentValue);
        
        pos = commaPos + 1;
    }
    
    return result;
}

// Smart compression - chooses best method
String DataCompression::compressRegisterData(const uint16_t* values, size_t count, bool useDelta) {
    if (useDelta) {
        return compressDelta(values, count);
    } else {
        return compressRLE(values, count);
    }
}

// Smart decompression
std::vector<uint16_t> DataCompression::decompressRegisterData(const String& compressed, bool isDelta) {
    if (compressed.startsWith("D:")) {
        return decompressDelta(compressed);
    } else if (compressed.startsWith("R:")) {
        return decompressRLE(compressed);
    }
    return std::vector<uint16_t>();
}

// Calculate compression ratio
float DataCompression::getCompressionRatio(size_t originalSize, size_t compressedSize) {
    if (compressedSize == 0) return 0.0f;
    return (float)originalSize / (float)compressedSize;
}

// Print compression statistics
void DataCompression::printCompressionStats(const String& method, size_t original, size_t compressed) {
    float ratio = getCompressionRatio(original, compressed);
    float savings = ((float)(original - compressed) / (float)original) * 100.0f;
    
    Serial.println("=== " + method + " Compression Stats ===");
    Serial.println("Original size: " + String(original) + " bytes");
    Serial.println("Compressed size: " + String(compressed) + " bytes");
    Serial.println("Compression ratio: " + String(ratio, 2) + ":1");
    Serial.println("Space savings: " + String(savings, 1) + "%");
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