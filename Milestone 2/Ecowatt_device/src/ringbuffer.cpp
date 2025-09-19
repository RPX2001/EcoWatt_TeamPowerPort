#include "RingBuffer.h"

// Example specialization: String with size 64
template class RingBuffer<String, 64>;

template <typename T, size_t N>
RingBuffer<T, N>::RingBuffer() : head(0), tail(0), full(false) {}

template <typename T, size_t N>
void RingBuffer<T, N>::push(const T& item) {
  buffer[head] = item;
  head = (head + 1) % N;
  if (full) {
    tail = (tail + 1) % N; // overwrite oldest
  }
  if (head == tail) {
    full = true;
  }
}

template <typename T, size_t N>
std::vector<T> RingBuffer<T, N>::drain_all() {
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

template <typename T, size_t N>
bool RingBuffer<T, N>::not_empty() const { return !empty(); }

template <typename T, size_t N>
size_t RingBuffer<T, N>::size() const {
  if (full) return N;
  if (head >= tail) return head - tail;
  return N + head - tail;
}

template <typename T, size_t N>
bool RingBuffer<T, N>::empty() const {
  return (!full && (head == tail));
}

template <typename T, size_t N>
void RingBuffer<T, N>::clear() {
  head = tail;
  full = false;
}
