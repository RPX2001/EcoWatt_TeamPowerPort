#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <iomanip>
#include <sstream>
#include <cstring>

struct DataSample {
    uint8_t byte1, byte2;
    
    std::string toHex() const {
        std::stringstream ss;
        ss << std::hex << std::uppercase << std::setfill('0');
        ss << std::setw(2) << (int)byte1;
        ss << std::setw(2) << (int)byte2;
        return ss.str();
    }
    
    void printDetails() const {
        std::cout << "Byte 1 (Slave Address): 0x" << std::hex << std::uppercase << std::setfill('0') 
                  << std::setw(2) << (int)byte1 << std::dec << " (" << (int)byte1 << ")" << std::endl;
        std::cout << "Byte 2 (Function Code): 0x" << std::hex << std::uppercase << std::setfill('0') 
                  << std::setw(2) << (int)byte2 << std::dec << " (" << (int)byte2 << ")" << std::endl;
    }
};

class DeltaRLEDecompressor {
private:
    struct CompressedData {
        std::vector<uint8_t> data;
        size_t originalSize;
    };
    
public:
    std::vector<DataSample> decompress(const std::vector<uint8_t>& compressedData) {
        std::vector<DataSample> result;
        
        if (compressedData.size() < 3) {
            std::cerr << "Error: Compressed data too small" << std::endl;
            return result;
        }
        
        // Extract header: [CommonByte2][FirstByte1][FirstByte2]
        uint8_t commonByte2 = compressedData[0];
        DataSample current;
        current.byte1 = compressedData[1];
        current.byte2 = compressedData[2];
        result.push_back(current);
        
        std::cout << "Header extracted:" << std::endl;
        std::cout << "Common Byte2: 0x" << std::hex << std::uppercase << std::setfill('0') 
                  << std::setw(2) << (int)commonByte2 << std::dec << std::endl;
        std::cout << "Baseline sample extracted:" << std::endl;
        current.printDetails();
        std::cout << "Hex: " << current.toHex() << std::endl << std::endl;
        
        // First decompress RLE data
        std::vector<uint8_t> rleDecompressed = decompressRLE(compressedData, 3);
        
        // Then decompress optimized deltas
        size_t pos = 0;
        int sampleIndex = 1;
        
        while (pos < rleDecompressed.size()) {
            DataSample next = current;
            size_t newPos = decompressSampleOptimized(rleDecompressed, pos, current, next, commonByte2);
            
            if (newPos == 0) {
                std::cerr << "Error: Failed to decompress sample at position " << pos << std::endl;
                break;
            }
            
            std::cout << "Sample " << sampleIndex << " decompressed:" << std::endl;
            next.printDetails();
            std::cout << "Hex: " << next.toHex() << std::endl << std::endl;
            
            result.push_back(next);
            current = next;
            pos = newPos;
            sampleIndex++;
        }
        
        return result;
    }
    
private:
    // RLE decompression
    std::vector<uint8_t> decompressRLE(const std::vector<uint8_t>& data, size_t startPos) {
        std::vector<uint8_t> result;
        
        for (size_t i = startPos; i < data.size(); ) {
            if (data[i] == 0xFF && i + 2 < data.size()) {
                // RLE sequence: 0xFF + count + value
                uint8_t count = data[i + 1];
                uint8_t value = data[i + 2];
                for (int j = 0; j < count; j++) {
                    result.push_back(value);
                }
                i += 3;
            } else {
                // Regular byte
                result.push_back(data[i]);
                i++;
            }
        }
        
        return result;
    }
    
    // Optimized decompression
    size_t decompressSampleOptimized(const std::vector<uint8_t>& data, size_t pos,
                                   const DataSample& prev, DataSample& result, uint8_t commonByte2) {
        if (pos >= data.size()) return 0;
        
        uint8_t control = data[pos++];
        result = prev;
        
        std::cout << "Processing control: 0x" << std::hex << std::uppercase 
                  << std::setfill('0') << std::setw(2) << (int)control << std::dec << std::endl;
        
        if ((control & 0x80) == 0) {
            // Compact format: single byte with delta for byte1, byte2 = common
            int8_t delta = (int8_t)(control & 0x7F);
            if (delta > 63) delta -= 128; // Sign extend 7-bit to 8-bit
            result.byte1 = prev.byte1 + delta;
            result.byte2 = commonByte2;
            std::cout << "  Compact format - Byte1 delta: " << (int)delta 
                      << " (" << (int)prev.byte1 << " -> " << (int)result.byte1 << ")" << std::endl;
            std::cout << "  Byte2 set to common: 0x" << std::hex << (int)commonByte2 << std::dec << std::endl;
        } else {
            // Extended format: flag byte + deltas
            uint8_t flags = control;
            std::cout << "  Extended format - flags: 0x" << std::hex << (int)flags << std::dec << std::endl;
            
            if (flags & 0x01) {
                // Byte1 changed
                if (pos >= data.size()) return 0;
                int8_t delta = (int8_t)data[pos++];
                result.byte1 = prev.byte1 + delta;
                std::cout << "    Byte1 delta: " << (int)delta 
                          << " (" << (int)prev.byte1 << " -> " << (int)result.byte1 << ")" << std::endl;
            }
            if (flags & 0x02) {
                // Byte2 changed
                if (pos >= data.size()) return 0;
                int8_t delta = (int8_t)data[pos++];
                result.byte2 = prev.byte2 + delta;
                std::cout << "    Byte2 delta: " << (int)delta 
                          << " (" << (int)prev.byte2 << " -> " << (int)result.byte2 << ")" << std::endl;
            }
        }
        
        return pos;
    }
};

int main() {
    std::cout << "=== MODBUS RTU DECOMPRESSOR ===" << std::endl;
    
    // Read compressed data from file
    std::ifstream compressedFile("compressed.txt", std::ios::binary);
    if (!compressedFile.is_open()) {
        std::cerr << "Error: Could not open compressed.txt" << std::endl;
        return 1;
    }
    
    // Get file size
    compressedFile.seekg(0, std::ios::end);
    size_t fileSize = compressedFile.tellg();
    compressedFile.seekg(0, std::ios::beg);
    
    // Read compressed data
    std::vector<uint8_t> compressedData(fileSize);
    compressedFile.read(reinterpret_cast<char*>(compressedData.data()), fileSize);
    compressedFile.close();
    
    std::cout << "Loaded " << fileSize << " bytes of compressed data" << std::endl << std::endl;
    
    // Decompress
    DeltaRLEDecompressor decompressor;
    std::vector<DataSample> decompressedSamples = decompressor.decompress(compressedData);
    
    if (decompressedSamples.empty()) {
        std::cerr << "Error: Decompression failed" << std::endl;
        return 1;
    }
    
    std::cout << "Successfully decompressed " << decompressedSamples.size() << " samples" << std::endl << std::endl;
    
    // Save decompressed data to text file
    std::ofstream outputFile("decompressed_data.txt");
    if (!outputFile.is_open()) {
        std::cerr << "Error: Could not create decompressed_data.txt" << std::endl;
        return 1;
    }
    
    // Write header
    outputFile << "=== DECOMPRESSED MODBUS RTU DATA ===" << std::endl;
    outputFile << "Total samples: " << decompressedSamples.size() << std::endl;
    outputFile << "Format: [4-char Hex String] - [Byte1] [Byte2]" << std::endl;
    outputFile << std::endl;
    
    // Write each sample in both hex and detailed format
    for (size_t i = 0; i < decompressedSamples.size(); i++) {
        const DataSample& sample = decompressedSamples[i];
        
        outputFile << "Sample " << (i + 1) << ":" << std::endl;
        outputFile << "Hex: " << sample.toHex() << std::endl;
        outputFile << "Byte 1 (Slave Address): 0x" << std::hex << std::uppercase << std::setfill('0') 
                   << std::setw(2) << (int)sample.byte1 << std::dec << " (" << (int)sample.byte1 << ")" << std::endl;
        outputFile << "Byte 2 (Function Code): 0x" << std::hex << std::uppercase << std::setfill('0') 
                   << std::setw(2) << (int)sample.byte2 << std::dec << " (" << (int)sample.byte2 << ")" << std::endl;
        outputFile << std::endl;
    }
    
    // Also create a simple hex-only format for easy comparison
    outputFile << "=== HEX STRINGS ONLY (for comparison with original) ===" << std::endl;
    for (size_t i = 0; i < decompressedSamples.size(); i++) {
        outputFile << decompressedSamples[i].toHex() << std::endl;
    }
    
    outputFile.close();
    
    std::cout << "=== DECOMPRESSION COMPLETE ===" << std::endl;
    std::cout << "Decompressed data saved to: decompressed_data.txt" << std::endl;
    std::cout << "Format includes both detailed breakdown and hex strings" << std::endl;
    
    // Quick verification by comparing with original data.txt
    std::ifstream originalFile("data.txt");
    if (originalFile.is_open()) {
        std::vector<std::string> originalLines;
        std::string line;
        while (std::getline(originalFile, line)) {
            if (!line.empty()) {
                originalLines.push_back(line);
            }
        }
        originalFile.close();
        
        std::cout << "\n=== VERIFICATION ===" << std::endl;
        if (originalLines.size() == decompressedSamples.size()) {
            bool allMatch = true;
            for (size_t i = 0; i < originalLines.size(); i++) {
                if (originalLines[i] != decompressedSamples[i].toHex()) {
                    std::cout << "❌ Mismatch at line " << (i + 1) << ":" << std::endl;
                    std::cout << "   Original:     " << originalLines[i] << std::endl;
                    std::cout << "   Decompressed: " << decompressedSamples[i].toHex() << std::endl;
                    allMatch = false;
                }
            }
            if (allMatch) {
                std::cout << "✅ All " << originalLines.size() << " samples match original data perfectly!" << std::endl;
            }
        } else {
            std::cout << "❌ Sample count mismatch: " << originalLines.size() 
                      << " original vs " << decompressedSamples.size() << " decompressed" << std::endl;
        }
    }
    
    return 0;
}