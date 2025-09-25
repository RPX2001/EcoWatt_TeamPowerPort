/*
@dev/raveen

*/


#include "data_compression.h"

// Main compression interface - uses advanced Delta+RLE algorithm
String DataCompression::compressRegisterData(const uint16_t* values, size_t count) {
    if (count == 0) return "";
    
    std::vector<uint8_t> compressed = compressAdvanced(values, count);
    return bytesToBase64(compressed);
}

// Main decompression interface
std::vector<uint16_t> DataCompression::decompressRegisterData(const String& compressed) {
    if (compressed.length() == 0) return std::vector<uint16_t>();
    
    std::vector<uint8_t> bytes = base64ToBytes(compressed);
    return decompressAdvanced(bytes);
}

// Advanced compression engine with optimized Delta+RLE
std::vector<uint8_t> DataCompression::compressAdvanced(const uint16_t* values, size_t count) {
    std::vector<uint8_t> result;
    if (count == 0) return result;
    
    // Find the most common value for optimization
    uint16_t commonValue = findMostCommonValue(values, count);
    
    // Header: [CommonValueHigh][CommonValueLow][FirstValueHigh][FirstValueLow]
    result.push_back((commonValue >> 8) & 0xFF);
    result.push_back(commonValue & 0xFF);
    result.push_back((values[0] >> 8) & 0xFF);
    result.push_back(values[0] & 0xFF);
    
    // Compress remaining samples with optimized approach
    std::vector<uint8_t> tempData;
    for (size_t i = 1; i < count; i++) {
        compressSampleOptimized(values[i-1], values[i], commonValue, tempData);
    }
    
    // Apply RLE compression
    std::vector<uint8_t> rleData = applyRLE(tempData);
    result.insert(result.end(), rleData.begin(), rleData.end());
    
    return result;
}

// Advanced decompression engine
std::vector<uint16_t> DataCompression::decompressAdvanced(const std::vector<uint8_t>& compressed) {
    std::vector<uint16_t> result;
    
    if (compressed.size() < 4) return result;
    
    // Extract header
    uint16_t commonValue = (compressed[0] << 8) | compressed[1];
    uint16_t firstValue = (compressed[2] << 8) | compressed[3];
    result.push_back(firstValue);
    
    // Decompress RLE data first
    std::vector<uint8_t> rleDecompressed = decompressRLE(compressed, 4);
    
    // Decompress optimized deltas
    size_t pos = 0;
    uint16_t current = firstValue;
    while (pos < rleDecompressed.size()) {
        uint16_t next;
        pos = decompressSampleOptimized(rleDecompressed, pos, current, commonValue, next);
        if (pos == 0) break; // Error
        result.push_back(next);
        current = next;
    }
    
    return result;
}

// Optimized sample compression
void DataCompression::compressSampleOptimized(uint16_t prev, uint16_t curr, uint16_t commonValue, std::vector<uint8_t>& output) {
    int16_t d1 = (int16_t)curr - (int16_t)prev;
    
    // If current value is the common value and delta is small, use compact encoding
    if (curr == commonValue && d1 >= -63 && d1 <= 63) {
        // Compact format: single byte with delta (bit 7 = 0 for compact mode)
        output.push_back(d1 & 0x7F);
    } else {
        // Extended format: flag byte + delta bytes
        uint8_t flags = 0x80; // Bit 7 = 1 for extended mode
        if (d1 != 0) flags |= 0x01;
        
        output.push_back(flags);
        if (d1 != 0) {
            output.push_back((d1 >> 8) & 0xFF); // High byte
            output.push_back(d1 & 0xFF);        // Low byte
        }
    }
}

// Optimized sample decompression
size_t DataCompression::decompressSampleOptimized(const std::vector<uint8_t>& data, size_t pos, uint16_t prev, uint16_t commonValue, uint16_t& result) {
    if (pos >= data.size()) return 0;
    
    uint8_t firstByte = data[pos++];
    result = prev;
    
    if ((firstByte & 0x80) == 0) {
        // Compact mode: single byte delta, result is commonValue
        int8_t delta = (firstByte & 0x7F);
        if (delta > 63) delta -= 128; // Sign extend
        result = commonValue;
    } else {
        // Extended mode: flag byte + delta bytes
        uint8_t flags = firstByte;
        
        if (flags & 0x01) {
            if (pos + 1 >= data.size()) return 0;
            int16_t delta = (data[pos] << 8) | data[pos + 1];
            result = prev + delta;
            pos += 2;
        }
    }
    
    return pos;
}

// RLE compression
std::vector<uint8_t> DataCompression::applyRLE(const std::vector<uint8_t>& data) {
    std::vector<uint8_t> compressed;
    
    if (data.empty()) return compressed;
    
    for (size_t i = 0; i < data.size(); ) {
        uint8_t currentByte = data[i];
        uint8_t count = 1;
        
        // Count consecutive identical bytes
        while (i + count < data.size() && data[i + count] == currentByte && count < 255) {
            count++;
        }
        
        if (count >= 3) {
            // RLE encoding: 0xFF (marker) + count + value
            compressed.push_back(0xFF);
            compressed.push_back(count);
            compressed.push_back(currentByte);
        } else {
            // Store bytes directly
            for (uint8_t j = 0; j < count; j++) {
                compressed.push_back(currentByte);
            }
        }
        
        i += count;
    }
    
    return compressed;
}

// RLE decompression
std::vector<uint8_t> DataCompression::decompressRLE(const std::vector<uint8_t>& data, size_t startPos) {
    std::vector<uint8_t> decompressed;
    
    for (size_t i = startPos; i < data.size(); ) {
        if (data[i] == 0xFF && i + 2 < data.size()) {
            // RLE encoded sequence
            uint8_t count = data[i + 1];
            uint8_t value = data[i + 2];
            
            for (uint8_t j = 0; j < count; j++) {
                decompressed.push_back(value);
            }
            i += 3;
        } else {
            // Direct byte
            decompressed.push_back(data[i]);
            i++;
        }
    }
    
    return decompressed;
}

// Find most common value for optimization
uint16_t DataCompression::findMostCommonValue(const uint16_t* values, size_t count) {
    if (count == 0) return 0;
    
    // Simple frequency analysis - use first value as default
    uint16_t mostCommon = values[0];
    int maxFreq = 1;
    
    // For small arrays, just analyze all values
    for (size_t i = 0; i < count && i < 100; i++) {
        int freq = 1;
        for (size_t j = i + 1; j < count && j < 100; j++) {
            if (values[i] == values[j]) freq++;
        }
        if (freq > maxFreq) {
            maxFreq = freq;
            mostCommon = values[i];
        }
    }
    
    return mostCommon;
}

// Base64 encoding for Arduino
String DataCompression::bytesToBase64(const std::vector<uint8_t>& bytes) {
    const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    String result = "";
    
    for (size_t i = 0; i < bytes.size(); i += 3) {
        uint32_t value = bytes[i] << 16;
        if (i + 1 < bytes.size()) value |= bytes[i + 1] << 8;
        if (i + 2 < bytes.size()) value |= bytes[i + 2];
        
        result += chars[(value >> 18) & 0x3F];
        result += chars[(value >> 12) & 0x3F];
        result += (i + 1 < bytes.size()) ? chars[(value >> 6) & 0x3F] : '=';
        result += (i + 2 < bytes.size()) ? chars[value & 0x3F] : '=';
    }
    
    return result;
}

// Base64 decoding for Arduino
std::vector<uint8_t> DataCompression::base64ToBytes(const String& base64) {
    std::vector<uint8_t> result;
    String clean = base64;
    clean.replace("\n", "");
    clean.replace("\r", "");
    
    for (int i = 0; i < clean.length(); i += 4) {
        uint32_t value = 0;
        int chars = 0;
        
        for (int j = 0; j < 4 && i + j < clean.length(); j++) {
            char c = clean[i + j];
            if (c == '=') break;
            
            uint8_t val = 0;
            if (c >= 'A' && c <= 'Z') val = c - 'A';
            else if (c >= 'a' && c <= 'z') val = c - 'a' + 26;
            else if (c >= '0' && c <= '9') val = c - '0' + 52;
            else if (c == '+') val = 62;
            else if (c == '/') val = 63;
            
            value = (value << 6) | val;
            chars++;
        }
        
        if (chars >= 2) result.push_back((value >> 16) & 0xFF);
        if (chars >= 3) result.push_back((value >> 8) & 0xFF);
        if (chars >= 4) result.push_back(value & 0xFF);
    }
    
    return result;
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