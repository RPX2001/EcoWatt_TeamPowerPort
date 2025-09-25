
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <map>

struct DataSample {
    uint8_t byte1, byte2;
    
    // Parse from 4-character hex string
    bool parseFromHex(const std::string& line) {
        if (line.length() != 4) return false;
        
        try {
            byte1 = std::stoi(line.substr(0, 2), nullptr, 16);
            byte2 = std::stoi(line.substr(2, 2), nullptr, 16);
            return true;
        } catch (...) {
            return false;
        }
    }
    
    std::string toHex() const {
        std::stringstream ss;
        ss << std::hex << std::uppercase << std::setfill('0');
        ss << std::setw(2) << (int)byte1;
        ss << std::setw(2) << (int)byte2;
        return ss.str();
    }
};

class DeltaRLECompressor {
private:
    struct CompressedData {
        std::vector<uint8_t> data;
        size_t originalSize;
    };
    
public:
    CompressedData compress(const std::vector<DataSample>& samples) {
        CompressedData result;
        result.originalSize = samples.size() * sizeof(DataSample);
        
        if (samples.empty()) return result;
        
        // Find the most common byte2 value (likely 0x03)
        std::map<uint8_t, int> byte2Freq;
        for (const auto& sample : samples) {
            byte2Freq[sample.byte2]++;
        }
        
        uint8_t commonByte2 = 0x03;
        int maxCount = 0;
        for (const auto& pair : byte2Freq) {
            if (pair.second > maxCount) {
                maxCount = pair.second;
                commonByte2 = pair.first;
            }
        }
        
        // Header: [CommonByte2][FirstByte1][FirstByte2]
        result.data.push_back(commonByte2);
        result.data.push_back(samples[0].byte1);
        result.data.push_back(samples[0].byte2);
        
        // Compress remaining samples with optimized approach
        std::vector<uint8_t> tempData;
        for (size_t i = 1; i < samples.size(); i++) {
            compressSampleOptimized(samples[i-1], samples[i], commonByte2, tempData);
        }
        
        // Apply RLE compression
        std::vector<uint8_t> rleData = applyRLE(tempData);
        result.data.insert(result.data.end(), rleData.begin(), rleData.end());
        
        return result;
    }
    
    std::vector<DataSample> decompress(const CompressedData& compressed) {
        std::vector<DataSample> result;
        
        if (compressed.data.size() < 3) return result;
        
        // Extract header
        uint8_t commonByte2 = compressed.data[0];
        DataSample current;
        current.byte1 = compressed.data[1];
        current.byte2 = compressed.data[2];
        result.push_back(current);
        
        // Decompress RLE data first
        std::vector<uint8_t> rleDecompressed = decompressRLE(compressed.data, 3);
        
        // Decompress optimized deltas
        size_t pos = 0;
        while (pos < rleDecompressed.size()) {
            DataSample next = current;
            pos = decompressSampleOptimized(rleDecompressed, pos, current, commonByte2, next);
            if (pos == 0) break; // Error
            result.push_back(next);
            current = next;
        }
        
        return result;
    }
    
private:
    void compressSampleOptimized(const DataSample& prev, const DataSample& curr, 
                                uint8_t commonByte2, std::vector<uint8_t>& output) {
        int8_t d1 = curr.byte1 - prev.byte1;
        int8_t d2 = curr.byte2 - prev.byte2;
        
        // If byte2 is the common value and byte1 delta is small, use compact encoding
        if (curr.byte2 == commonByte2 && d1 >= -63 && d1 <= 63) {
            // Compact format: single byte with delta (bit 7 = 0 for compact mode)
            output.push_back(d1 & 0x7F);
        } else {
            // Extended format: flag byte + deltas
            uint8_t flags = 0x80; // Bit 7 = 1 for extended mode
            if (d1 != 0) flags |= 0x01;
            if (d2 != 0) flags |= 0x02;
            
            output.push_back(flags);
            if (d1 != 0) output.push_back(d1);
            if (d2 != 0) output.push_back(d2);
        }
    }
    
    std::vector<uint8_t> applyRLE(const std::vector<uint8_t>& data) {
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

    void compressSample(const DataSample& prev, const DataSample& curr, 
                       std::vector<uint8_t>& output) {
        // Calculate deltas for 2 bytes only
        int8_t d1 = curr.byte1 - prev.byte1;
        int8_t d2 = curr.byte2 - prev.byte2;
        
        // Use flags to indicate which fields changed
        uint8_t flags = 0;
        if (d1 != 0) flags |= 0x01;
        if (d2 != 0) flags |= 0x02;
        
        output.push_back(flags);
        
        // Store only non-zero deltas
        if (d1 != 0) output.push_back(d1);
        if (d2 != 0) output.push_back(d2);
    }
    
    std::vector<uint8_t> decompressRLE(const std::vector<uint8_t>& data, size_t startPos) {
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
    
    size_t decompressSampleOptimized(const std::vector<uint8_t>& data, size_t pos,
                                   const DataSample& prev, uint8_t commonByte2, DataSample& result) {
        if (pos >= data.size()) return 0;
        
        uint8_t firstByte = data[pos++];
        result = prev;
        
        if ((firstByte & 0x80) == 0) {
            // Compact mode: single byte delta for byte1, byte2 stays as commonByte2
            int8_t delta = (firstByte & 0x7F);
            if (delta > 63) delta -= 128; // Sign extend
            result.byte1 = prev.byte1 + delta;
            result.byte2 = commonByte2;
        } else {
            // Extended mode: flag byte + deltas
            uint8_t flags = firstByte;
            
            if (flags & 0x01) {
                if (pos >= data.size()) return 0;
                result.byte1 = prev.byte1 + (int8_t)data[pos++];
            }
            if (flags & 0x02) {
                if (pos >= data.size()) return 0;
                result.byte2 = prev.byte2 + (int8_t)data[pos++];
            }
        }
        
        return pos;
    }

    size_t decompressSample(const std::vector<uint8_t>& data, size_t pos,
                           const DataSample& prev, DataSample& result) {
        if (pos >= data.size()) return 0;
        
        uint8_t flags = data[pos++];
        result = prev;
        
        if (flags & 0x01) {
            if (pos >= data.size()) return 0;
            result.byte1 = prev.byte1 + (int8_t)data[pos++];
        }
        if (flags & 0x02) {
            if (pos >= data.size()) return 0;
            result.byte2 = prev.byte2 + (int8_t)data[pos++];
        }
        
        return pos;
    }
};

class Benchmarker {
public:
    struct BenchmarkResult {
        std::string compressionMethod;
        size_t numberOfSamples;
        size_t originalPayloadSize;
        size_t compressedPayloadSize;
        double compressionRatio;
        double cpuTimeMs;
        bool losslessRecoveryVerified;
        std::string aggregationStats;
    };
    
    static BenchmarkResult runBenchmark(const std::vector<DataSample>& samples) {
        BenchmarkResult result;
        result.compressionMethod = "Delta Encoding";
        result.numberOfSamples = samples.size();
        
        // Calculate original size in bits (4 hex chars = 2 bytes = 16 bits per sample)
        size_t originalSizeBits = samples.size() * 16;
        result.originalPayloadSize = originalSizeBits / 8; // Convert to bytes for display
        
        DeltaRLECompressor compressor;
        
        // Measure compression time
        auto start = std::chrono::high_resolution_clock::now();
        auto compressed = compressor.compress(samples);
        auto end = std::chrono::high_resolution_clock::now();
        
        result.cpuTimeMs = std::chrono::duration<double, std::milli>(end - start).count();
        result.compressedPayloadSize = compressed.data.size();
        
        // Calculate compression ratio as compressed bits / original bits
        size_t compressedSizeBits = compressed.data.size() * 8;
        result.compressionRatio = (double)compressedSizeBits / originalSizeBits;
        
        // Verify lossless recovery
        auto decompressed = compressor.decompress(compressed);
        result.losslessRecoveryVerified = (decompressed.size() == samples.size());
        
        if (result.losslessRecoveryVerified) {
            for (size_t i = 0; i < samples.size(); i++) {
                if (samples[i].toHex() != decompressed[i].toHex()) {
                    result.losslessRecoveryVerified = false;
                    break;
                }
            }
        }
        
        // Calculate aggregation stats for the 2-byte samples
        if (!samples.empty()) {
            uint8_t minByte1 = samples[0].byte1;
            uint8_t maxByte1 = samples[0].byte1;
            uint8_t minByte2 = samples[0].byte2;
            uint8_t maxByte2 = samples[0].byte2;
            uint64_t sumByte1 = 0, sumByte2 = 0;
            
            for (const auto& sample : samples) {
                minByte1 = std::min(minByte1, sample.byte1);
                maxByte1 = std::max(maxByte1, sample.byte1);
                minByte2 = std::min(minByte2, sample.byte2);
                maxByte2 = std::max(maxByte2, sample.byte2);
                sumByte1 += sample.byte1;
                sumByte2 += sample.byte2;
            }
            
            uint8_t avgByte1 = sumByte1 / samples.size();
            uint8_t avgByte2 = sumByte2 / samples.size();
            
            std::stringstream ss;
            ss << "Byte1 - Min: 0x" << std::hex << std::uppercase << (int)minByte1
               << ", Avg: 0x" << (int)avgByte1
               << ", Max: 0x" << (int)maxByte1
               << " | Byte2 - Min: 0x" << (int)minByte2
               << ", Avg: 0x" << (int)avgByte2
               << ", Max: 0x" << (int)maxByte2;
            result.aggregationStats = ss.str();
        }
        
        return result;
    }
    
    static void printReport(const BenchmarkResult& result) {
        std::cout << "=== COMPRESSION BENCHMARK REPORT ===" << std::endl;
        std::cout << "Compression Method Used: " << result.compressionMethod << std::endl;
        std::cout << "Number of Samples: " << result.numberOfSamples << std::endl;
        std::cout << "Original Payload Size: " << result.originalPayloadSize << " bytes" << std::endl;
        std::cout << "Compressed Payload Size: " << result.compressedPayloadSize << " bytes" << std::endl;
        std::cout << "Compression Ratio: " << std::fixed << std::setprecision(2) 
                  << result.compressionRatio << ":1" << std::endl;
        std::cout << "CPU Time: " << std::fixed << std::setprecision(3) 
                  << result.cpuTimeMs << " ms" << std::endl;
        std::cout << "Lossless Recovery Verification: " 
                  << (result.losslessRecoveryVerified ? "PASSED" : "FAILED") << std::endl;
        std::cout << "Aggregation (min/avg/max): " << result.aggregationStats << std::endl;
        std::cout << "====================================" << std::endl;
    }
};

int main() {
    // Read data from file
    std::vector<DataSample> samples;
    std::ifstream file("data.txt");
    
    if (!file.is_open()) {
        std::cerr << "Error: Could not open data.txt" << std::endl;
        return 1;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        
        DataSample sample;
        if (sample.parseFromHex(line)) {
            samples.push_back(sample);
        } else {
            std::cerr << "Warning: Could not parse line: " << line << std::endl;
        }
    }
    file.close();
    
    if (samples.empty()) {
        std::cerr << "Error: No valid samples found" << std::endl;
        return 1;
    }
    
    std::cout << "Loaded " << samples.size() << " samples from data.txt" << std::endl;
    
    // Run benchmark
    auto benchmark = Benchmarker::runBenchmark(samples);
    Benchmarker::printReport(benchmark);
    
    // Compress and save to file
    DeltaRLECompressor compressor;
    auto compressed = compressor.compress(samples);
    
    std::ofstream outFile("compressed.txt", std::ios::binary);
    if (outFile.is_open()) {
        outFile.write(reinterpret_cast<const char*>(compressed.data.data()), 
                     compressed.data.size());
        outFile.close();
        std::cout << "\nCompressed data saved to compressed.txt" << std::endl;
    } else {
        std::cerr << "Error: Could not create compressed.txt" << std::endl;
    }
    
    // Verify by decompressing
    std::cout << "\nVerifying compression integrity..." << std::endl;
    auto decompressed = compressor.decompress(compressed);
    
    if (decompressed.size() == samples.size()) {
        bool identical = true;
        for (size_t i = 0; i < samples.size(); i++) {
            if (samples[i].toHex() != decompressed[i].toHex()) {
                identical = false;
                std::cout << "Mismatch at sample " << i << ":" << std::endl;
                std::cout << "  Original:    " << samples[i].toHex() << std::endl;
                std::cout << "  Decompressed: " << decompressed[i].toHex() << std::endl;
                break;
            }
        }
        
        if (identical) {
            std::cout << "✓ All samples match perfectly after decompression!" << std::endl;
        }
    } else {
        std::cout << "✗ Sample count mismatch: " << samples.size() 
                  << " vs " << decompressed.size() << std::endl;
    }
    
    return 0;
}