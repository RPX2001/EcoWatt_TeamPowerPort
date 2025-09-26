#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include <Arduino.h>
#include <vector>

// Structure to hold compressed data with metadata
struct CompressedData {
    String data;                // The actual compressed data string
    unsigned long timestamp;    // When the data was compressed
    String compressionType;     // Type of compression used (DELTA, RLE, HYBRID)
    size_t originalCount;       // Number of original data points

    // Default constructor
    CompressedData() : timestamp(0), originalCount(0) {}
    
    // Constructor with compressed data and count
    CompressedData(const String& compressedData, size_t count) 
        : data(compressedData), timestamp(millis()), originalCount(count) {}
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