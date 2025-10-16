#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include <Arduino.h>
#include <vector>
#include <cstring>  // for memcpy, strncpy
#include "peripheral/acquisition.h"  // For RegID enum

// Structure to store RAW uncompressed sensor data
struct RawSensorData {
    uint16_t values[16];           // Raw sensor values (supports up to 16 registers)
    RegID registers[16];           // Register IDs corresponding to values
    size_t registerCount;          // Number of valid registers/values
    unsigned long timestamp;       // Timestamp when data was acquired
    
    // Constructor
    RawSensorData(const uint16_t* data, const RegID* regSelection, size_t regCount) 
        : registerCount(regCount), timestamp(millis()) {
        // Copy raw sensor values
        memcpy(values, data, regCount * sizeof(uint16_t));
        // Copy register selection
        memcpy(registers, regSelection, regCount * sizeof(RegID));
    }
    
    // Default constructor
    RawSensorData() : registerCount(0), timestamp(0) {}
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