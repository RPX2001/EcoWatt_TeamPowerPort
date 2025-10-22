#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include <Arduino.h>
#include <vector>
#include "peripheral/acquisition.h"

struct SmartCompressedData {
    std::vector<uint8_t> binaryData;
    RegID registers[16];            // Register selection used
    size_t registerCount;           // Number of registers
    char compressionMethod[32];     // Method used by smart selection
    unsigned long timestamp;        // Sample timestamp
    size_t originalSize;            // Original data size in bytes
    float academicRatio;            // Academic compression ratio (compressed/original)
    float traditionalRatio;         // Traditional compression ratio (original/compressed)
    unsigned long compressionTime;  // Time taken to compress (μs)
    bool losslessVerified;          // Whether lossless compression was verified
    
    // Constructor
    SmartCompressedData(const std::vector<uint8_t>& data, const RegID* regSelection, size_t regCount, const char* method) 
        : binaryData(data), registerCount(regCount), timestamp(millis()) {
        strncpy(compressionMethod, method, sizeof(compressionMethod) - 1);
        compressionMethod[sizeof(compressionMethod) - 1] = '\0';
        
        // Copy register selection
        memcpy(registers, regSelection, regCount * sizeof(RegID));
        
        // Calculate metrics
        originalSize = regCount * sizeof(uint16_t);
        academicRatio = (binaryData.size() > 0) ? (float)binaryData.size() / (float)originalSize : 1.0f;
        traditionalRatio = (binaryData.size() > 0) ? (float)originalSize / (float)binaryData.size() : 0.0f;
        losslessVerified = false;
    }
    
    // Default constructor
    SmartCompressedData() : registerCount(0), timestamp(0), originalSize(0), 
                           academicRatio(1.0f), traditionalRatio(0.0f), 
                           compressionTime(0), losslessVerified(false) {
        compressionMethod[0] = '\0';
    }
};

// Template class with inline implementations
template <typename T, size_t N>
class RingBuffer {
  public:
    RingBuffer() : head(0), tail(0), full(false) {}

    void push(const T& item) {
      buffer[head] = item;
      head = (head + 1) % N;
      if (full) {
        tail = (tail + 1) % N; // overwrite oldest
      }
      if (head == tail) {
        full = true;
      }
    }

    std::vector<T> drain_all() {
      std::vector<T> out;
      if (empty()) return out;

      size_t idx = tail;
      size_t count = size();
      for (size_t i = 0; i < count; i++) {
        out.push_back(buffer[idx]);
        idx = (idx + 1) % N;
      }
      clear();
      return out;
    }

    bool not_empty() const { return !empty(); }

    size_t size() const {
      if (full) return N;
      if (head >= tail) return head - tail;
      return N + head - tail;
    }

    bool empty() const {
      return (!full && (head == tail));
    }

    void clear() {
      head = tail;
      full = false;
    }

  private:
    T buffer[N];
    size_t head;
    size_t tail;
    bool full;
};

#endif // RINGBUFFER_H